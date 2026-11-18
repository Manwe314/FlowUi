#include "FlowUi/Public/managers/FontManager.hpp"
#include "FlowUi/Private/storageSystem/StorageBinding.hpp"
#include "FlowUi/Private/storageSystem/capacity/InitialMemoryCapacity.hpp"
#include "FlowUi/Public/Resources.hpp"
#include <fstream>
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontBakeProfiles.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuAssetValidation.hpp"
#include "FlowUi/Private/storageSystem/ManagerStateAccess.hpp"
#include "FlowUi/Private/storageSystem/ResourceKeyNormalization.hpp"

namespace Font = FlowUi::Font;

namespace FlowUi {
#if FLOW_UI_DEV_MODE
std::uint64_t FontManager::devRevision() const noexcept {
	return controller_ ? controller_->catalogRevision : 0;
}

std::size_t FontManager::devFontCount() const noexcept {
	if (!controller_) return 0;
	std::size_t count = 0;
	for (const auto& family : controller_->families) count += family.faces.size();
	return count;
}

Result<bool> FontManager::visitDevFonts(void* userData, DevFontVisitor visitor) const noexcept try {
	if (!controller_ || !visitor) return false;
	for (FontFamilyId familyId = 0; familyId < controller_->families.size(); ++familyId) {
		const auto& family = controller_->families[familyId];
		for (const auto& familyFace : family.faces) {
			if (!visitor(userData, DevFontView{
				.fontId = familyFace.fontId,
				.familyId = familyId,
				.familyName = family.name,
				.weight = familyFace.weight,
				.style = familyFace.style,
				.face = getFontById(familyFace.fontId),
			})) return false;
		}
	}
	return true;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontLookupFamily));
}

FontManager::DevAtlasView FontManager::devAtlas() const noexcept {
	if (!controller_) return {};
	const auto layers = dev_atlas_layers();
	return DevAtlasView{
		.texture = layers.empty() ? TextureHandle{} : layers.front().texture,
		.imageView = layers.empty() ? detail::storage::ImageViewHandle{} : layers.front().image_view,
		.width = controller_->borrowedAtlas.width,
		.height = controller_->borrowedAtlas.height,
		.layersUsed = controller_->borrowedAtlas.layersUsed,
		.layersCapacity = controller_->borrowedAtlas.layersCapacity,
	};
}

std::span<const FontManager::DevAtlasLayerView> FontManager::dev_atlas_layers() const noexcept {
	return controller_ ? std::span<const DevAtlasLayerView>(controller_->dev_atlas_layers)
		: std::span<const DevAtlasLayerView>{};
}
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void FontManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (!controller_) return;
	try {
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup,
			controller_->familyIdByName, controller_->capacity_demand);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::fonts_id_lookup,
			controller_->fontIndexById, controller_->capacity_demand);
		devSystems::append_capacity_demand(sink,
										   FlowUi::detail::InitialCapacityTarget::fonts_name_lookup,
										   controller_->fontIdByName, controller_->capacity_demand);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::fonts_atlas_layers,
			controller_->atlasLayerPixels, controller_->capacity_demand);
		devSystems::DevContainerMemoryAccumulator memory{};
		memory.liveBytes += controller_->families.size() * sizeof(detail::manager_storage::FontFamilyRecord);
		memory.capacityBytes += memory.liveBytes;
		memory.objectCount += controller_->families.size();
		memory.capacityCount += controller_->families.size();
		memory.addNodeContainer(controller_->familyIdByName);
		memory.liveBytes += controller_->fonts.size() * sizeof(Font::FontFaceData);
		memory.capacityBytes += controller_->fonts.size() * sizeof(Font::FontFaceData);
		memory.objectCount += controller_->fonts.size();
		memory.capacityCount += controller_->fonts.size();
		memory.addNodeContainer(controller_->fontIndexById);
		memory.addNodeContainer(controller_->fontIdByName);
		memory.add(controller_->atlasLayerPixels);
		uint64_t face_maximum = 0;
		for (const auto& family : controller_->families)
			face_maximum = std::max<uint64_t>(face_maximum, family.faces.size());
		(void)sink.append(devSystems::MemoryValueSample{
			.source =
				FlowUi::detail::initial_capacity_definitions
					[static_cast<size_t>(FlowUi::detail::InitialCapacityTarget::fonts_family_faces)]
						.id(),
			.objectCount = face_maximum,
			.flags = devSystems::MemorySampleFlag::TuningAggregate,
			.value_fields = uint16_t(devSystems::MemoryValueField::Objects)});
		for (const auto& family : controller_->families) {
			memory.add(family.name);
			memory.add(family.faces);
		}
		for (const auto& layer : controller_->atlasLayerPixels) memory.add(layer);
		for (const auto& font : controller_->fonts) {
			memory.add(font.name);
			memory.add(font.metadata);
			memory.add(font.variants);
			for (const auto& variant : font.variants) {
				memory.add(variant.glyphs);
				memory.addNodeContainer(variant.kerningPairs);
			}
		}
		devSystems::appendManagerSample(sink, devSystems::memory_sources::kFonts.id, memory);
		devSystems::DevContainerMemoryAccumulator atlasPixels{};
		atlasPixels.add(controller_->atlasLayerPixels);
		for (const auto& layer : controller_->atlasLayerPixels) atlasPixels.add(layer);
		atlasPixels.flags = devSystems::MemorySampleFlag::None;
		devSystems::appendManagerSample(
			sink, devSystems::memory_sources::kFontAtlasCpuPixels.id, atlasPixels);
	} catch (...) {}
}

void FontManager::setDevMemoryRecorder(devSystems::DevMemoryRecorder* recorder) noexcept {
	devMemoryRecorder_ = recorder;
	if (controller_) controller_->setDevMemoryRecorder(recorder);
}
#endif

namespace manager_storage = detail::manager_storage;
namespace key_storage = detail::managerStorage;
namespace storage = detail::storage;

namespace {

std::string toLowerAscii(std::string text) {
	for (char& character : text)
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	return text;
}
[[nodiscard]] bool isArfontPath(const std::filesystem::path& path) {
	return toLowerAscii(detail::requireApiValue(path_to_utf8(path.extension()))) == ".arfont";
}
[[nodiscard]] std::vector<unsigned char> readFontBytes(const std::filesystem::path& path) {
	std::error_code pathError;
	if (!std::filesystem::is_regular_file(path, pathError)) {
		throw FlowUiException(
			makeError(pathError ? ErrorCode::AssetOpenFailed : ErrorCode::AssetNotFound,
					  ErrorSite::FontLoad));
	}
	auto bytes = read_file_bytes(path);
	if (!bytes)
		throw FlowUiException(bytes.error());
	return std::move(*bytes);
}

std::string makeUniqueFontName(
	std::string baseName,
	const std::unordered_map<std::string, FontManager::FontId>& existingNames) {
	if (baseName.empty()) {
		baseName = "font";
	}

	if (existingNames.find(baseName) == existingNames.end()) {
		return baseName;
	}

	for (uint32_t suffix = 1; suffix < std::numeric_limits<uint32_t>::max(); ++suffix) {
		const std::string candidate = baseName + "_" + std::to_string(suffix);
		if (existingNames.find(candidate) == existingNames.end()) {
			return candidate;
		}
	}
	detail::terminateForFatalError(makeError(ErrorCode::IdentitySpaceExhausted, ErrorSite::FontRegisterFamily));
}

} // namespace

void FontManager::init(storage::StorageBinding& storageBinding, uint32_t atlasSize) {
	auto& storageSystem = storageBinding.system();
	destroy();
	const storage::StringId name = storageSystem.intern("flowui.font.catalog");
	const storage::ResourceKey key{storage::ResourceDomain::Font, name, InvalidWindowId};
	const storage::ManagerRecordHandle handle = manager_storage::createState<manager_storage::FontCatalogController>(
		storageSystem, key, storage::ResourceKind::FontFamily, name,
		std::ref(storageSystem), atlasSize);
	storage_ = &storageBinding;
	controllerHandle_ = handle.packed();
	controller_ = manager_storage::state<manager_storage::FontCatalogController>(
		storage_, handle, storage::ResourceKind::FontFamily);
	if (!controller_) {
		destroy();
		throw FlowUiException(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::FontManagerInitialize));
	}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	controller_->capacity_demand.bind(storageSystem.memory_demand_control());
#endif
	detail::reserve_initial_capacity(
		controller_->familyIdByName,
		storageSystem.initial_memory_capacity(
			FlowUi::detail::InitialCapacityTarget::fonts_family_lookup));
	detail::reserve_initial_capacity(controller_->fontIndexById,
									 storageSystem.initial_memory_capacity(
										 FlowUi::detail::InitialCapacityTarget::fonts_id_lookup));
	detail::reserve_initial_capacity(controller_->fontIdByName,
									 storageSystem.initial_memory_capacity(
										 FlowUi::detail::InitialCapacityTarget::fonts_name_lookup));
	detail::reserve_initial_capacity(
		controller_->atlasLayerPixels,
		storageSystem.initial_memory_capacity(
			FlowUi::detail::InitialCapacityTarget::fonts_atlas_layers));
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	controller_->setDevMemoryRecorder(devMemoryRecorder_);
#endif
}

Result<FontManager::FontFamilyId> FontManager::createFamily(const FontFamilyCreateInfo& createInfo) noexcept try {
	if (!controller_ || !storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontRegisterFamily));
	std::string familyName = createInfo.name.empty() ? std::string("Default") : createInfo.name;
	if (controller_->familyIdByName.find(familyName) != controller_->familyIdByName.end()) {
		return unexpectedError(makeError(ErrorCode::FontFamilyAlreadyExists, ErrorSite::FontRegisterFamily));
	}

	const size_t oldFontCount = controller_->fonts.size();
	const size_t oldLayerCount = controller_->atlasLayerPixels.size();
	const FontId oldNextFontId = controller_->nextFontId;
	const uint64_t oldCatalogRevision = controller_->catalogRevision;
	const auto oldAtlasImage = controller_->atlasImage;
	const auto oldAtlasView = controller_->atlasView;
	const FontFamilyId familyId = static_cast<FontFamilyId>(controller_->families.size());
	controller_->familyIdByName.reserve(controller_->familyIdByName.size() + 1u);
	manager_storage::FontFamilyRecord family{};
	family.id = familyId;
	family.name = std::move(familyName);
	family.faces.reserve(std::max<uint64_t>(
		createInfo.faces.size(), storage_->system().initial_memory_capacity(
									 FlowUi::detail::InitialCapacityTarget::fonts_family_faces)));
	storage_->system().noteManagerMutation(InvalidWindowId);
	controller_->families.push_back(std::move(family));
	try {
		controller_->familyIdByName.emplace(controller_->families.back().name, familyId);
		controller_->familyTransaction = true;
		for (const FontFaceCreateInfo& faceInfo : createInfo.faces) {
			auto added = addFamilyFace(familyId, faceInfo);
			if (!added) throw FlowUiException(added.error());
		}
		controller_->familyTransaction = false;
	} catch (const FlowUiException& exception) {
		controller_->familyTransaction = false;
		for (size_t index = oldFontCount; index < controller_->fonts.size(); ++index) {
			FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
										FlowUi::detail::InitialCapacityTarget::fonts_id_lookup,
										controller_->fontIndexById.size());
			controller_->fontIndexById.erase(controller_->fonts[index].id);
			FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
										FlowUi::detail::InitialCapacityTarget::fonts_name_lookup,
										controller_->fontIdByName.size());
			controller_->fontIdByName.erase(controller_->fonts[index].name);
		}
		controller_->fonts.resize(oldFontCount);
		controller_->nextFontId = oldNextFontId;
		FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
									FlowUi::detail::InitialCapacityTarget::fonts_atlas_layers,
									controller_->atlasLayerPixels.size());
		controller_->discard_atlas_layers(oldLayerCount);
		if (controller_->atlasImage == oldAtlasImage && controller_->atlasView == oldAtlasView)
			controller_->catalogRevision = oldCatalogRevision;
		FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
									FlowUi::detail::InitialCapacityTarget::fonts_family_lookup,
									controller_->familyIdByName.size());
		controller_->familyIdByName.erase(controller_->families.back().name);
		controller_->families.pop_back();
		return unexpectedError(exception.error());
	} catch (...) {
		controller_->familyTransaction = false;
		for (size_t index = oldFontCount; index < controller_->fonts.size(); ++index) {
			FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
										FlowUi::detail::InitialCapacityTarget::fonts_id_lookup,
										controller_->fontIndexById.size());
			controller_->fontIndexById.erase(controller_->fonts[index].id);
			FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
										FlowUi::detail::InitialCapacityTarget::fonts_name_lookup,
										controller_->fontIdByName.size());
			controller_->fontIdByName.erase(controller_->fonts[index].name);
		}
		controller_->fonts.resize(oldFontCount);
		controller_->nextFontId = oldNextFontId;
		FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
									FlowUi::detail::InitialCapacityTarget::fonts_atlas_layers,
									controller_->atlasLayerPixels.size());
		controller_->discard_atlas_layers(oldLayerCount);
		if (controller_->atlasImage == oldAtlasImage && controller_->atlasView == oldAtlasView)
			controller_->catalogRevision = oldCatalogRevision;
		FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
									FlowUi::detail::InitialCapacityTarget::fonts_family_lookup,
									controller_->familyIdByName.size());
		controller_->familyIdByName.erase(controller_->families.back().name);
		controller_->families.pop_back();
		throw;
	}
	++controller_->catalogRevision;
	return familyId;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontRegisterFamily));
}

Result<FontManager::FontFamilyId> FontManager::createFamily(ResourceKey key, const FontFamilyCreateInfo& createInfo) noexcept try {
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontRegisterFamily));
	storage::ResourceKey normalized{};
	try {
		normalized = key_storage::normalizeResourceKey(
			storage_->system(), key, ResourceDomain::Font, key_storage::ResourceScope::AppShared);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	FontFamilyCreateInfo normalizedInfo = createInfo;
	normalizedInfo.name = std::string(storage_->system().string(normalized.name));
	return createFamily(normalizedInfo);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontRegisterFamily));
}

Result<FontManager::FontFamilyId> FontManager::getFamilyId(std::string_view familyName) const noexcept try {
	if (!controller_ || !storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontLookupFamily));
	const auto it = controller_->familyIdByName.find(std::string(familyName));
	return (it != controller_->familyIdByName.end()) ? it->second : std::numeric_limits<FontFamilyId>::max();
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontLookupFamily));
}

Result<FontManager::FontFamilyId> FontManager::getFamilyId(ResourceKey key) const noexcept try {
	if (!controller_ || !storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontLookupFamily));
	const storage::ResourceKey normalized = key_storage::normalizeResourceKey(
		storage_->system(), key, ResourceDomain::Font, key_storage::ResourceScope::AppShared);
	return getFamilyId(storage_->system().string(normalized.name));
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontLookupFamily));
}

Result<FontManager::FontId> FontManager::addFamilyFace(FontFamilyId familyId, const FontFaceCreateInfo& createInfo) noexcept try {
	if (!controller_ || !storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontRegisterFamily));
	if (familyId >= controller_->families.size()) {
		return unexpectedError(makeError(ErrorCode::FontFamilyNotFound, ErrorSite::FontRegisterFamily, familyId));
	}

	controller_->families[familyId].faces.reserve(controller_->families[familyId].faces.size() + 1u);
	if (!controller_->familyTransaction)
		storage_->system().noteManagerMutation(InvalidWindowId);
	FontId fontId = 0;
	try {
		fontId = loadFontFace(createInfo);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (const std::bad_alloc&) {
		throw;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::FontBakeFailed, ErrorSite::FontRegisterFamily));
	}
	controller_->families[familyId].faces.push_back(manager_storage::FontFamilyFaceRecord{
		.fontId = fontId,
		.weight = createInfo.weight,
		.style = createInfo.style,
	});
	if (!controller_->familyTransaction) {
		++controller_->catalogRevision;
	}
	return fontId;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontRegisterFamily));
}

Result<FontManager::FontId> FontManager::addFamilyFace(std::string_view familyName, const FontFaceCreateInfo& createInfo) noexcept try {
	const auto familyResult = getFamilyId(familyName);
	if (!familyResult) return unexpectedError(familyResult.error());
	const FontFamilyId familyId = *familyResult;
	if (familyId == std::numeric_limits<FontFamilyId>::max()) {
		return unexpectedError(makeError(ErrorCode::FontFamilyNotFound, ErrorSite::FontRegisterFamily));
	}
	return addFamilyFace(familyId, createInfo);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontRegisterFamily));
}

Result<FontManager::FontId> FontManager::addFamilyFace(ResourceKey key, const FontFaceCreateInfo& createInfo) noexcept try {
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontRegisterFamily));
	storage::ResourceKey normalized{};
	try {
		normalized = key_storage::normalizeResourceKey(
			storage_->system(), key, ResourceDomain::Font, key_storage::ResourceScope::AppShared);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	return addFamilyFace(storage_->system().string(normalized.name), createInfo);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontRegisterFamily));
}

Result<FontManager::FontId> FontManager::resolveFont(FontFamilyId familyId, uint32_t weight, FontStyle style) const noexcept try {
	if (!controller_ || !storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontLookupFamily));
	if (familyId >= controller_->families.size()) {
		return 0;
	}

	const manager_storage::FontFamilyRecord& family = controller_->families[familyId];
	const manager_storage::FontFamilyFaceRecord* bestFace = nullptr;
	uint32_t bestDistance = std::numeric_limits<uint32_t>::max();

	for (const manager_storage::FontFamilyFaceRecord& face : family.faces) {
		if (face.style != style) {
			continue;
		}
		const uint32_t distance = (face.weight > weight) ? (face.weight - weight) : (weight - face.weight);
		if (!bestFace || distance < bestDistance) {
			bestFace = &face;
			bestDistance = distance;
		}
	}

	if (bestFace) {
		return bestFace->fontId;
	}
	if (!family.faces.empty()) {
		return family.faces.front().fontId;
	}
	return 0;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontLookupFamily));
}

Result<FontManager::FontId> FontManager::resolveFont(std::string_view familyName, uint32_t weight, FontStyle style) const noexcept try {
	if (!controller_ || !storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontLookupFamily));
	const auto familyResult = getFamilyId(familyName);
	if (!familyResult) return unexpectedError(familyResult.error());
	const FontFamilyId familyId = *familyResult;
	if (familyId == std::numeric_limits<FontFamilyId>::max()) return FontId{0};
	return resolveFont(familyId, weight, style);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontLookupFamily));
}

Result<FontManager::FontId> FontManager::resolveFont(ResourceKey key, uint32_t weight, FontStyle style) const noexcept try {
	if (!controller_ || !storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontLookupFamily));
	const storage::ResourceKey normalized = key_storage::normalizeResourceKey(
		storage_->system(), key, ResourceDomain::Font, key_storage::ResourceScope::AppShared);
	return resolveFont(storage_->system().string(normalized.name), weight, style);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::FontLookupFamily));
}

FontManager::FontId FontManager::loadFontFace(const FontFaceCreateInfo& createInfo) {
	if (!createInfo.baked_data.empty()) {
		return registerBakedFont({}, createInfo.name, createInfo.baked_data);
	}
	if (createInfo.path.empty()) {
		throw FlowUiException(makeError(ErrorCode::AssetPathEmpty, ErrorSite::FontLoad));
	}
	if (isArfontPath(createInfo.path)) {
		return registerBakedFont(createInfo.path, createInfo.name);
	}
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	return registerRuntimeFont(createInfo);
#else
	return loadFont(createInfo.path, createInfo.pixelSize);
#endif
}

FontManager::FontId FontManager::loadFont(const std::filesystem::path& path, float px) {
	if (!controller_) {
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontLoad));
	}
	const std::filesystem::path fontPath(path);
	if (fontPath.empty()) {
		throw FlowUiException(makeError(ErrorCode::AssetPathEmpty, ErrorSite::FontLoad));
	}

	if (isArfontPath(fontPath)) {
		return registerBakedFont(path);
	}

#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	FontFaceCreateInfo createInfo{};
	createInfo.path = fontPath;
	createInfo.pixelSize = px;
	return registerRuntimeFont(createInfo);
#else
	throw FlowUiException(makeError(ErrorCode::AssetFormatUnsupported, ErrorSite::FontLoad));
#endif
}

FontManager::FontId FontManager::registerRuntimeFont(const FontFaceCreateInfo& createInfo) {
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	if (!controller_)
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontBake));
	if (createInfo.path.empty())
		throw FlowUiException(makeError(ErrorCode::AssetPathEmpty, ErrorSite::FontBake));
	if (controller_->atlasSizeHint == 0u ||
		controller_->atlasSizeHint > static_cast<uint32_t>(std::numeric_limits<int>::max()))
		throw FlowUiException(
			makeError(ErrorCode::RendererConfigurationInvalid, ErrorSite::FontBake));
	const auto bytes = readFontBytes(createInfo.path);
	const std::string faceName = createInfo.name.empty()
									 ? detail::requireApiValue(path_to_utf8(createInfo.path.stem()))
									 : createInfo.name;
	const auto profile = detail::font::runtimeFontBakeProfile;
	auto output = controller_->provider.bake(detail::font::FontBakeRequest{
		.sourceBytes = std::as_bytes(std::span(bytes)),
		.faceName = faceName,
		.pixelSize = createInfo.pixelSize,
		.pixelRange = profile.pixelRange,
		.angleThreshold = profile.angleThreshold,
		.miterLimit = profile.miterLimit,
		.pageWidth = controller_->atlasSizeHint,
		.pageHeight = controller_->atlasSizeHint,
		.workerThreadCount = std::max(1u, std::thread::hardware_concurrency()),
		.weight = createInfo.weight,
		.charsetMode = detail::font::FontCharsetMode::RuntimeAscii});
	if (!output)
		throw FlowUiException(output.error());
	return registerCpuFont(std::move(output->asset), createInfo.path, faceName,
						   ErrorSite::FontBake);
#else
	throw FlowUiException(makeError(ErrorCode::FontBakeUnavailable, ErrorSite::FontBake, 0u,
									createInfo.baked_data.size()));
#endif
}

FontManager::FontId FontManager::registerBakedFont(const std::filesystem::path& path,
												   std::string_view requestedName,
												   std::span<const std::byte> bakedData) {
	if (!controller_)
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontParse));
	std::vector<unsigned char> ownedBytes;
	if (bakedData.empty()) {
		if (!isArfontPath(path))
			throw FlowUiException(
				makeError(ErrorCode::AssetFormatUnsupported, ErrorSite::FontParse));
		ownedBytes = readFontBytes(path);
		bakedData = std::as_bytes(std::span(ownedBytes));
	}
	auto decoded = controller_->provider.loadBaked(bakedData);
	if (!decoded)
		throw FlowUiException(decoded.error());
	return registerCpuFont(std::move(*decoded), path, requestedName, ErrorSite::FontParse);
}

FontManager::FontId FontManager::registerCpuFont(detail::font::FontCpuAsset asset,
												 const std::filesystem::path& sourcePath,
												 std::string_view requestedName,
												 ErrorSite operationSite) {
	const auto valid = detail::font::validateFontCpuAsset(asset);
	if (!valid)
		throw FlowUiException(valid.error());
	const auto pageSize =
		detail::rgba8ByteSize(controller_->atlasSizeHint, controller_->atlasSizeHint);
	if (!pageSize.valid)
		throw FlowUiException(
			makeError(ErrorCode::RendererConfigurationInvalid, ErrorSite::FontPublishAtlas));
	if (asset.atlas.width > controller_->atlasSizeHint ||
		asset.atlas.height > controller_->atlasSizeHint)
		throw FlowUiException(
			makeError(ErrorCode::FontAtlasCapacityExceeded, ErrorSite::FontPublishAtlas));
	if (controller_->nextFontId == std::numeric_limits<FontId>::max())
		throw FlowUiException(makeError(ErrorCode::FontIdSpaceExhausted, operationSite));
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevExternalMemoryScope decodedMemory(
		devMemoryRecorder_, devSystems::memory_sources::kFontDecode.id, asset.atlas.pixels.size());
#endif
	std::vector<uint8_t> pagePixels(pageSize.value, 0u);
	const size_t pageRowBytes = static_cast<size_t>(controller_->atlasSizeHint) * 4u;
	const size_t sourceRowBytes = static_cast<size_t>(asset.atlas.width) * 4u;
	for (uint32_t rowIndex = 0; rowIndex < asset.atlas.height; ++rowIndex)
		std::memcpy(pagePixels.data() + static_cast<size_t>(rowIndex) * pageRowBytes,
					asset.atlas.pixels.data() + static_cast<size_t>(rowIndex) * sourceRowBytes,
					sourceRowBytes);
#if FLOW_UI_DEV_MODE
	if (asset.atlas.width != controller_->atlasSizeHint ||
		asset.atlas.height != controller_->atlasSizeHint) {
		static constexpr auto source =
			devSystems::makeDevErrorSource("flowui.font.atlas_size_adjusted");
		const uint64_t decodedSize =
			(static_cast<uint64_t>(asset.atlas.width) << 32u) | asset.atlas.height;
		const uint64_t configuredSize =
			(static_cast<uint64_t>(controller_->atlasSizeHint) << 32u) | controller_->atlasSizeHint;
		devSystems::recordGlobalDevDiagnostic(makeError(ErrorCode::AssetPayloadInvalid,
														ErrorSite::FontParse, decodedSize,
														configuredSize),
											  source);
	}
#endif
	Font::FontFaceData fontFace{};
	fontFace.id = controller_->nextFontId;
	fontFace.sourcePath = sourcePath;
	fontFace.metadata = std::move(asset.metadata);
	fontFace.atlasLayer = static_cast<uint32_t>(controller_->atlasLayerPixels.size());
	fontFace.atlasWidth = controller_->atlasSizeHint;
	fontFace.atlasHeight = controller_->atlasSizeHint;
	fontFace.sourceAtlasWidth = asset.atlas.width;
	fontFace.sourceAtlasHeight = asset.atlas.height;
	fontFace.imageType = detail::font::legacyImageType(asset.fieldKind);
	fontFace.variants.reserve(asset.variants.size());
	for (auto& source : asset.variants) {
		Font::FontVariantData variant;
		variant.flags = source.flags;
		variant.weight = source.weight;
		variant.fallbackGlyphIndex =
			source.fallbackGlyphIndex < source.glyphs.size() ? source.fallbackGlyphIndex : 0u;
		variant.fontSizePx = source.fontSizePx;
		variant.distanceRange = source.distanceRange;
		variant.distanceRangeMiddle = source.distanceRangeMiddle;
		variant.emSize = source.emSize;
		variant.ascender = source.ascender;
		variant.descender = source.descender;
		variant.lineHeight = source.lineHeight;
		variant.underlineY = source.underlineY;
		variant.underlineThickness = source.underlineThickness;
		variant.name = std::move(source.name);
		variant.metadata = std::move(source.metadata);
		variant.glyphs = std::move(source.glyphs);
		variant.unicodeToGlyphIndex.reserve(variant.glyphs.size());
		for (size_t glyphIndex = 0; glyphIndex < variant.glyphs.size(); ++glyphIndex) {
			const auto codepoint = variant.glyphs[glyphIndex].codepoint;
			if (codepoint != 0u)
				variant.unicodeToGlyphIndex.emplace(codepoint, static_cast<uint32_t>(glyphIndex));
		}
		variant.kerningPairs.reserve(source.kerningPairs.size());
		for (const auto& pair : source.kerningPairs)
			variant.kerningPairs[Font::FontVariantData::kerningKey(
				pair.leftCodepoint, pair.rightCodepoint)] = pair.advance;
		fontFace.variants.emplace_back(std::move(variant));
	}
	std::string preferredName(requestedName);
	if (preferredName.empty())
		preferredName = !fontFace.variants.front().name.empty()
							? fontFace.variants.front().name
							: detail::requireApiValue(path_to_utf8(sourcePath.stem()));
	fontFace.name = makeUniqueFontName(std::move(preferredName), controller_->fontIdByName);
	const size_t newIndex = controller_->fonts.size();
	const FontId committedId = fontFace.id;
	const uint32_t assignedLayer = fontFace.atlasLayer;
	const std::string committedName = fontFace.name;
	controller_->fontIndexById.reserve(controller_->fontIndexById.size() + 1u);
	controller_->fontIdByName.reserve(controller_->fontIdByName.size() + 1u);
	// Allocate the stable deque node before GPU upload. Publication afterwards only moves
	// nonthrowing owned records and increments the committed ID.
	static_assert(std::is_nothrow_move_assignable_v<Font::FontFaceData>);
	controller_->fonts.emplace_back();
	try {
		controller_->fontIndexById.emplace(committedId, newIndex);
		controller_->fontIdByName.emplace(committedName, committedId);
		controller_->uploadLayerTransactional(assignedLayer, pagePixels);
		controller_->fonts.back() = std::move(fontFace);
		++controller_->nextFontId;
	} catch (...) {
		FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
									FlowUi::detail::InitialCapacityTarget::fonts_name_lookup,
									controller_->fontIdByName.size());
		controller_->fontIdByName.erase(committedName);
		FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
									FlowUi::detail::InitialCapacityTarget::fonts_id_lookup,
									controller_->fontIndexById.size());
		controller_->fontIndexById.erase(committedId);
		controller_->fonts.pop_back();
		if (controller_->atlasLayerPixels.size() > assignedLayer) {
			FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
										FlowUi::detail::InitialCapacityTarget::fonts_atlas_layers,
										controller_->atlasLayerPixels.size());
			controller_->discard_atlas_layers(assignedLayer);
		}
		throw;
	}
	return committedId;
}

const Font::FontFaceData* FontManager::getFontById(FontId fontId) const noexcept {
	if (!controller_) return nullptr;
	const auto it = controller_->fontIndexById.find(fontId);
	if (it == controller_->fontIndexById.end()) {
		return nullptr;
	}
	const size_t index = it->second;
	return (index < controller_->fonts.size()) ? &controller_->fonts[index] : nullptr;
}

Result<std::reference_wrapper<const Font::AtlasArrayResource>> FontManager::getAtlasResource() const noexcept {
	if (!controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontLookupFamily));
	return std::cref(controller_->borrowedAtlas);
}

manager_storage::FontFrameView FontManager::frameView(const storage::FrameToken& frame) const {
	if (!controller_ || !storage_) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::FontLookupFamily));
	std::array<storage::ResourceUse, 3> uses{};
	size_t count = 0;
	if (controller_->atlasImage) uses[count++] = storage::useOf(controller_->atlasImage);
	if (controller_->atlasView) uses[count++] = storage::useOf(controller_->atlasView);
	if (controller_->atlasSampler) uses[count++] = storage::useOf(controller_->atlasSampler);
	if (count > 0)
		storage_->system().trackUses(frame, std::span(uses).first(count));
	return manager_storage::FontFrameView{
		.families = &controller_->families,
		.fonts = &controller_->fonts,
		.familyCount = controller_->families.size(),
		.fontCount = controller_->fonts.size(),
		.atlas = controller_->borrowedAtlas,
		.atlasImage = controller_->atlasImage,
		.atlasView = controller_->atlasView,
		.atlasSampler = controller_->atlasSampler,
		.publicationRevision = controller_->catalogRevision,
	};
}

void FontManager::destroy() noexcept {
	if (storage_) {
		try {
			const storage::StringId name = storage_->system().intern("flowui.font.catalog");
			(void)storage_->system().removeManagerRecord(
				storage::ResourceKey{storage::ResourceDomain::Font, name, InvalidWindowId},
				storage::ResourceKind::FontFamily);
		} catch (...) {
		}
	}
	controller_ = nullptr;
	controllerHandle_ = 0;
	storage_ = nullptr;
}

} // namespace FlowUi
