#include "devSystems/devTooling/catalogue/DevCatalogues.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <filesystem>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#include "managers/ActionManager.hpp"
#include "managers/FontManager.hpp"
#include "managers/IconManager.hpp"
#include "managers/ImageManager.hpp"
#include "managers/ThemeManager.hpp"

namespace FlowUi::devMode {
namespace storage = detail::storage;

namespace {

DevResourceStatus resourceStatus(storage::ResourceState state) noexcept {
	switch (state) {
	case storage::ResourceState::Queued:
	case storage::ResourceState::Decoding:
	case storage::ResourceState::Uploading: return DevResourceStatus::Loading;
	case storage::ResourceState::Ready: return DevResourceStatus::Ready;
	case storage::ResourceState::Failed: return DevResourceStatus::Failed;
	case storage::ResourceState::Retiring: return DevResourceStatus::Retiring;
	case storage::ResourceState::Invalid:
	default: return DevResourceStatus::Unloaded;
	}
}

template <typename Entry>
std::size_t vectorBytes(const std::vector<Entry>& entries) noexcept {
	return entries.capacity() * sizeof(Entry);
}

std::uint64_t combinedRevision(std::uint64_t left, std::uint64_t right) noexcept {
	return left ^ (right + 0x9e3779b97f4a7c15ull + (left << 6u) + (left >> 2u));
}

} // namespace

DevCatalogLease::DevCatalogLease(
	storage::IStorageSystem* storage,
	std::vector<TextureHandle> textures) noexcept
	: storage_(storage), textures_(std::move(textures)) {}

DevCatalogLease::~DevCatalogLease() { release(); }

DevCatalogLease::DevCatalogLease(DevCatalogLease&& other) noexcept
	: storage_(std::exchange(other.storage_, nullptr)),
	  textures_(std::move(other.textures_)) {}

DevCatalogLease& DevCatalogLease::operator=(DevCatalogLease&& other) noexcept {
	if (this == &other) return *this;
	release();
	storage_ = std::exchange(other.storage_, nullptr);
	textures_ = std::move(other.textures_);
	return *this;
}

void DevCatalogLease::release() noexcept {
	if (storage_) {
		for (TextureHandle texture : textures_) {
			try { storage_->releaseTextureFromDevInspection(texture); } catch (...) {}
		}
	}
	textures_.clear();
	storage_ = nullptr;
}

void DevCatalogues::bindManagers(
	storage::IStorageSystem* storageSystem,
	ThemeManager* themes,
	ActionManager* actions,
	FontManager* fonts,
	IconManager* icons,
	ImageManager* images,
	DevSchemaRegistry* schemaRegistry) noexcept {
	storage_ = storageSystem;
	themeManager_ = themes;
	actionManager_ = actions;
	fontManager_ = fonts;
	iconManager_ = icons;
	imageManager_ = images;
	schemaRegistry_ = schemaRegistry;
	invalidateAllCaches();
}

std::span<const DevImageCatalogEntry> DevCatalogues::queryImages() noexcept {
	const std::uint64_t revision = imageManager_ ? imageManager_->devRevision() : 0;
	if (imageRevision_ != revision) {
		try { rebuildImageCache(); imageRevision_ = revision; } catch (...) { cachedImages_.clear(); }
	}
	return cachedImages_;
}

std::span<const DevIconCatalogEntry> DevCatalogues::queryIcons() noexcept {
	const std::uint64_t revision = iconManager_ ? iconManager_->devRevision() : 0;
	if (iconRevision_ != revision) {
		try { rebuildIconCache(); iconRevision_ = revision; } catch (...) { cachedIcons_.clear(); }
	}
	return cachedIcons_;
}

std::span<const DevAtlasCatalogEntry> DevCatalogues::queryAtlases() noexcept {
	const std::uint64_t revision = combinedRevision(
		fontManager_ ? fontManager_->devRevision() : 0,
		iconManager_ ? iconManager_->devRevision() : 0);
	if (atlasRevision_ != revision) {
		try { rebuildAtlasCache(); atlasRevision_ = revision; } catch (...) { cachedAtlases_.clear(); }
	}
	return cachedAtlases_;
}

std::span<const DevFontCatalogEntry> DevCatalogues::queryFonts() noexcept {
	const std::uint64_t revision = fontManager_ ? fontManager_->devRevision() : 0;
	if (cachedRevisions_.fontRevision != revision) {
		try { rebuildFontCache(); cachedRevisions_.fontRevision = revision; }
		catch (...) { cachedFonts_.clear(); }
	}
	return cachedFonts_;
}

std::span<const DevActionCatalogEntry> DevCatalogues::queryActions() noexcept {
	const std::uint64_t revision = actionManager_ ? actionManager_->devRevision() : 0;
	if (actionRevision_ != revision) {
		try { rebuildActionCache(); actionRevision_ = revision; } catch (...) { cachedActions_.clear(); }
	}
	return cachedActions_;
}

std::span<const DevThemeCatalogEntry> DevCatalogues::queryThemes() noexcept {
	const DevSchemaView schema = schemaRegistry_ ? schemaRegistry_->view() : DevSchemaView{};
	const std::uint64_t revision = combinedRevision(
		themeManager_ ? themeManager_->devRevision() : 0,
		schema ? schema->generation : 0);
	if (themeRevision_ != revision) {
		try { rebuildThemeCache(); themeRevision_ = revision; } catch (...) { cachedThemes_.clear(); }
	}
	return cachedThemes_;
}

std::span<const DevElementCatalogEntry> DevCatalogues::queryElements(
	const devSystems::tooling::DevTreeSnapshot* currentTree) noexcept {
	const DevSchemaView schema = schemaRegistry_ ? schemaRegistry_->view() : DevSchemaView{};
	const std::uint64_t schemaRevision = schema ? schema->generation : 0;
	const std::uint64_t treeRevision = currentTree ? currentTree->generation : 0;
	if (cachedRevisions_.schemaRevision != schemaRevision ||
		cachedRevisions_.treeRevision != treeRevision || cachedElementTree_ != currentTree) {
		try {
			rebuildElementCache(currentTree);
			cachedRevisions_.schemaRevision = schemaRevision;
			cachedRevisions_.treeRevision = treeRevision;
			cachedElementTree_ = currentTree;
		} catch (...) { cachedElements_.clear(); }
	}
	return cachedElements_;
}

void DevCatalogues::rebuildImageCache() {
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timingRecorder_, devSystems::TimingCategory::DevTool,
		devSystems::TimingZoneRole::DevToolWork, "flowui.dev.catalogues.images.rebuild");
	cachedImages_.clear();
	if (!imageManager_) return;
	cachedImages_.reserve(imageManager_->devImageCount());
	imageManager_->visitDevImages(&cachedImages_, [](void* context, const ImageManager::DevImageView& image) {
		auto& entries = *static_cast<std::vector<DevImageCatalogEntry>*>(context);
		entries.push_back(DevImageCatalogEntry{
			.key = ResourceKey{.name = image.key, .domain = ResourceDomain::Image},
			.textureHandle = image.texture,
			.sourcePathOrDebugName = image.sourcePath,
		});
		return true;
	});
	for (DevImageCatalogEntry& entry : cachedImages_) {
		const storage::DevTextureMetadata devMetadata = storage_
			? storage_->devTextureMetadata(entry.textureHandle) : storage::DevTextureMetadata{};
		const storage::TextureMetadata& metadata = devMetadata.texture;
		entry.width = metadata.sourceWidth > 0 ? static_cast<std::uint32_t>(metadata.sourceWidth) : 0;
		entry.height = metadata.sourceHeight > 0 ? static_cast<std::uint32_t>(metadata.sourceHeight) : 0;
		entry.formatVulkan = devMetadata.formatVulkan;
		entry.gpuMemoryBytes = devMetadata.gpuMemoryBytes;
		entry.status = devMetadata.published
			? resourceStatus(metadata.state) : DevResourceStatus::Retiring;
		entry.isAnonymous = devMetadata.key.name == 0;
	}
	std::ranges::sort(cachedImages_, {}, [](const auto& entry) { return entry.key.name; });
}

void DevCatalogues::rebuildIconCache() {
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timingRecorder_, devSystems::TimingCategory::DevTool,
		devSystems::TimingZoneRole::DevToolWork, "flowui.dev.catalogues.icons.rebuild");
	cachedIcons_.clear();
	if (!iconManager_) return;
	cachedIcons_.reserve(iconManager_->devIconCount());
	iconManager_->visitDevIcons(&cachedIcons_, [](void* context, const IconManager::DevIconView& icon) {
		auto& entries = *static_cast<std::vector<DevIconCatalogEntry>*>(context);
		entries.push_back(DevIconCatalogEntry{
			.iconKey = ResourceKey{.name = icon.key, .domain = ResourceDomain::Icon},
			.iconName = icon.key,
			.targetWidth = icon.targetWidth,
			.targetHeight = icon.targetHeight,
			.atlasTexture = icon.texture,
			.atlasUv = DevUvRect{
				.x = icon.uv0x, .y = icon.uv0y,
				.width = icon.uv1x - icon.uv0x,
				.height = icon.uv1y - icon.uv0y,
			},
			.atlasLayer = icon.atlasPage,
		});
		return true;
	});
	std::ranges::sort(cachedIcons_, {}, [](const auto& entry) { return entry.iconName; });
}

void DevCatalogues::rebuildAtlasCache() {
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timingRecorder_, devSystems::TimingCategory::DevTool,
		devSystems::TimingZoneRole::DevToolWork, "flowui.dev.catalogues.atlases.rebuild");
	cachedAtlases_.clear();
	const std::size_t iconCount = iconManager_ ? iconManager_->devAtlasCount() : 0;
	cachedAtlases_.reserve(iconCount + (fontManager_ ? 1u : 0u));
	if (fontManager_) {
		const FontManager::DevAtlasView atlas = fontManager_->devAtlas();
		if (atlas.imageView || atlas.layersCapacity != 0) {
			cachedAtlases_.push_back(DevAtlasCatalogEntry{
				.atlasKey = ResourceKey{.name = "flowui.font.atlas", .domain = ResourceDomain::Font},
				.debugName = "FlowUi font MSDF atlas",
				.kind = DevAtlasKind::FontMsdfAtlas,
				.atlasTexture = atlas.texture,
				.atlasImageView = atlas.imageView,
				.width = atlas.width,
				.height = atlas.height,
				.layerCount = atlas.layersCapacity,
				.occupancyRatio = atlas.layersCapacity == 0 ? 0.0f :
					static_cast<float>(atlas.layersUsed) / static_cast<float>(atlas.layersCapacity),
				.allocatedRegions = atlas.layersUsed,
				.gpuMemoryBytes = static_cast<std::uint64_t>(atlas.width) * atlas.height *
					atlas.layersCapacity * 4u,
			});
		}
	}
	if (iconManager_) {
		iconManager_->visitDevAtlases(&cachedAtlases_, [](void* context, const IconManager::DevAtlasView& atlas) {
			auto& entries = *static_cast<std::vector<DevAtlasCatalogEntry>*>(context);
			const std::uint64_t area = static_cast<std::uint64_t>(atlas.width) * atlas.height;
			entries.push_back(DevAtlasCatalogEntry{
				.atlasKey = ResourceKey{.name = "flowui.icon.atlas", .domain = ResourceDomain::Icon},
				.debugName = "FlowUi icon vector atlas",
				.kind = DevAtlasKind::IconVectorAtlas,
				.atlasTexture = atlas.texture,
				.width = atlas.width,
				.height = atlas.height,
				.occupancyRatio = area == 0 ? 0.0f : static_cast<float>(atlas.usedArea) / static_cast<float>(area),
				.allocatedRegions = atlas.allocatedRegions,
				.gpuMemoryBytes = area * 4u,
			});
			return true;
		});
	}
	if (storage_) {
		for (DevAtlasCatalogEntry& entry : cachedAtlases_) {
			if (!entry.atlasTexture) continue;
			const storage::DevTextureMetadata metadata =
				storage_->devTextureMetadata(entry.atlasTexture);
			if (metadata.gpuMemoryBytes != 0) entry.gpuMemoryBytes = metadata.gpuMemoryBytes;
		}
	}
}

void DevCatalogues::rebuildFontCache() {
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timingRecorder_, devSystems::TimingCategory::DevTool,
		devSystems::TimingZoneRole::DevToolWork, "flowui.dev.catalogues.fonts.rebuild");
	cachedFonts_.clear();
	if (!fontManager_) return;
	cachedFonts_.reserve(fontManager_->devFontCount());
	fontManager_->visitDevFonts(&cachedFonts_, [](void* context, const FontManager::DevFontView& font) {
		auto& entries = *static_cast<std::vector<DevFontCatalogEntry>*>(context);
		const Font::FontVariantData* variant = font.face ? font.face->defaultVariant() : nullptr;
		entries.push_back(DevFontCatalogEntry{
			.fontHandle = font.fontId,
			.familyName = font.familyName,
			.faceName = font.face ? std::string_view(font.face->name) : std::string_view{},
			.sourcePath = {},
			.weight = static_cast<std::uint16_t>(std::min<std::uint32_t>(font.weight, UINT16_MAX)),
			.isItalic = font.style == FontStyle::Italic,
			.emSizePoints = variant ? variant->fontSizePx : 0.0f,
			.ascent = variant ? variant->ascender : 0.0f,
			.descent = variant ? variant->descender : 0.0f,
			.lineGap = variant ? variant->lineHeight - variant->ascender + variant->descender : 0.0f,
			.loadedGlyphCount = variant ? static_cast<std::uint32_t>(variant->glyphs.size()) : 0,
			.atlasLayer = font.face ? font.face->atlasLayer : 0,
		});
		return true;
	});
	std::ranges::sort(cachedFonts_, {}, [](const auto& entry) { return entry.fontHandle; });
}

void DevCatalogues::rebuildActionCache() {
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timingRecorder_, devSystems::TimingCategory::DevTool,
		devSystems::TimingZoneRole::DevToolWork, "flowui.dev.catalogues.actions.rebuild");
	cachedActions_.clear();
	if (!actionManager_) return;
	cachedActions_.reserve(actionManager_->devActionCount());
	actionManager_->visitDevActions(&cachedActions_, [](void* context, const ActionManager::DevActionView& action) {
		auto& entries = *static_cast<std::vector<DevActionCatalogEntry>*>(context);
		entries.push_back(DevActionCatalogEntry{
			.actionId = action.debug.appId,
			.debugName = action.debug.debugName,
			.kind = action.debug.kind == ActionCallKind::Ui
				? DevActionKind::UiActionRecipe : DevActionKind::AppActionBinding,
			.isBound = action.debug.bound,
			.availabilityFlags = static_cast<std::uint8_t>(
				action.debug.availability.enabled ? 1u : 0u),
			.callableTypeHash = action.callableTypeHash,
			.resultTypeHash = action.resultTypeHash,
			.lifetimeInvocations = action.debug.invocationCount,
			.definitionSource = action.debug.definitionSource.file,
		});
		return true;
	});
	std::ranges::sort(cachedActions_, {}, [](const auto& entry) { return entry.actionId.value; });
}

void DevCatalogues::rebuildThemeCache() {
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timingRecorder_, devSystems::TimingCategory::DevTool,
		devSystems::TimingZoneRole::DevToolWork, "flowui.dev.catalogues.themes.rebuild");
	cachedThemes_.clear();
	if (!themeManager_) return;
	cachedThemes_.reserve(themeManager_->devThemeCount());
	struct Context {
		std::vector<DevThemeCatalogEntry>* entries = nullptr;
		DevSchemaView schema{};
	} context{&cachedThemes_, schemaRegistry_ ? schemaRegistry_->view() : DevSchemaView{}};
	themeManager_->visitDevCatalogueThemes(&context, [](void* raw, const ThemeManager::DevThemePayloadView& theme) noexcept {
		auto& context = *static_cast<Context*>(raw);
		std::size_t fields = 0;
		if (context.schema) {
			if (const DevTypeSchema* type = context.schema->findType(theme.type)) fields = type->fields.count;
		}
		context.entries->push_back(DevThemeCatalogEntry{
			.themeTypeId = theme.type,
			.typeName = theme.typeName,
			.variantName = theme.variant,
			.isActive = theme.active,
			.revision = theme.revision,
			.payloadPointer = theme.payload,
			.payloadSizeBytes = theme.payloadSize,
			.reflectedFieldCount = fields,
		});
		return true;
	});
	std::ranges::sort(cachedThemes_, [](const auto& left, const auto& right) {
		return std::tie(left.typeName, left.variantName) < std::tie(right.typeName, right.variantName);
	});
}

void DevCatalogues::rebuildElementCache(
	const devSystems::tooling::DevTreeSnapshot* currentTree) {
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timingRecorder_, devSystems::TimingCategory::DevTool,
		devSystems::TimingZoneRole::DevToolWork, "flowui.dev.catalogues.elements.rebuild");
	cachedElements_.clear();
	const DevSchemaView schema = schemaRegistry_ ? schemaRegistry_->view() : DevSchemaView{};
	if (!schema) return;
	std::unordered_map<std::uint64_t, std::uint32_t> liveCounts;
	if (currentTree) {
		liveCounts.reserve(currentTree->flow.nodes.size());
		for (const devSystems::tooling::DevFlowNode& node : currentTree->flow.nodes) {
			if (node.definition) ++liveCounts[node.definition.value];
		}
	}
	cachedElements_.reserve(schema->elements.size());
	for (const DevElementSchema& element : schema->elements) {
		const DevTypeSchema* parameters = schema->type(element.parametersType);
		const DevTypeSchema* state = schema->type(element.stateType);
		std::string_view sourceFile{};
		if (parameters) {
			const auto fields = schema->fieldsOf(element.parametersType);
			if (!fields.empty()) sourceFile = schema->string(fields.front().source.file);
		}
		cachedElements_.push_back(DevElementCatalogEntry{
			.definition = element.definitionId,
			.definitionName = schema->string(element.displayName),
			.parametersTypeId = parameters ? parameters->id : 0,
			.stateTypeId = state ? state->id : 0,
			.reflectedFieldCount = parameters ? parameters->fields.count : 0,
			.activeInstanceCount = liveCounts[element.definitionId.value],
			.isBakeable = parameters && parameters->edit != DevEditCapability::Unsupported,
			.headerSourceFile = sourceFile,
		});
	}
	std::ranges::sort(cachedElements_, {}, [](const auto& entry) { return entry.definition.value; });
}

DevCatalogLease DevCatalogues::acquireInspectionLease() noexcept {
	if (!storage_) return {};
	std::vector<TextureHandle> retained;
	auto retain = [&](TextureHandle texture) {
		if (!texture || std::ranges::find(retained, texture) != retained.end()) return;
		if (!storage_->retainTextureForDevInspection(texture)) return;
		try { retained.push_back(texture); }
		catch (...) {
			try { storage_->releaseTextureFromDevInspection(texture); } catch (...) {}
			throw;
		}
	};
	try {
		retained.reserve(cachedImages_.size() + cachedIcons_.size() + cachedAtlases_.size());
		for (const auto& entry : cachedImages_) retain(entry.textureHandle);
		for (const auto& entry : cachedIcons_) retain(entry.atlasTexture);
		for (const auto& entry : cachedAtlases_) retain(entry.atlasTexture);
		return DevCatalogLease(storage_, std::move(retained));
	} catch (...) {
		for (TextureHandle texture : retained) {
			try { storage_->releaseTextureFromDevInspection(texture); } catch (...) {}
		}
		return {};
	}
}

void DevCatalogues::invalidateAllCaches() noexcept {
	cachedElementTree_ = nullptr;
	imageRevision_ = iconRevision_ = atlasRevision_ = actionRevision_ = themeRevision_ = UINT64_MAX;
	cachedRevisions_ = DevCatalogueRevisions{
		.storageRevision = UINT64_MAX,
		.fontRevision = UINT64_MAX,
		.schemaRevision = UINT64_MAX,
		.treeRevision = UINT64_MAX,
	};
}

std::size_t DevCatalogues::memoryFootprintBytes() const noexcept {
	return vectorBytes(cachedImages_) + vectorBytes(cachedIcons_) + vectorBytes(cachedAtlases_) +
		vectorBytes(cachedFonts_) + vectorBytes(cachedActions_) + vectorBytes(cachedThemes_) +
		vectorBytes(cachedElements_);
}

} // namespace FlowUi::devMode

#endif
