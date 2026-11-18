#include "FlowUi/Public/managers/IconManager.hpp"
#include "FlowUi/Private/storageSystem/StorageBinding.hpp"
#include "FlowUi/Private/storageSystem/capacity/InitialMemoryCapacity.hpp"
#include "FlowUi/Public/Resources.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

#if FLOWUI_INCLUDE_ICON_MANAGER

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include "FlowUi/Private/resourceService/IconCacheController.hpp"
#include "FlowUi/Private/resourceService/iconRasterProvider/IconRasterCalculations.hpp"
#include "FlowUi/Private/storageSystem/ManagerStateAccess.hpp"
#include "FlowUi/Private/storageSystem/ResourceKeyNormalization.hpp"

namespace FlowUi {
#if FLOW_UI_DEV_MODE
std::uint64_t IconManager::devRevision() const noexcept {
	return storage_ ? storage_->system().managerSharedRevision() : 0;
}

std::size_t IconManager::devIconCount() const noexcept {
	return controller_ ? controller_->documentsByKey.size() + controller_->variantsByKeyAndSize.size() : 0;
}

std::size_t IconManager::devAtlasCount() const noexcept {
	return controller_ ? controller_->atlasPages.size() : 0;
}

Result<bool> IconManager::visitDevIcons(void* userData, DevIconVisitor visitor) const noexcept try {
	if (!controller_ || !visitor) return false;
	for (const auto& [key, document] : controller_->documentsByKey) {
		bool materialized = false;
		for (const auto& [variantKey, variant] : controller_->variantsByKeyAndSize) {
			if (variantKey.nameKey != key) continue;
			materialized = true;
			if (!visitor(userData, DevIconView{
				.key = key,
				.targetWidth = variant.key.requestedWidth,
				.targetHeight = variant.key.requestedHeight,
				.texture = variant.texture,
				.uv0x = variant.uv0x, .uv0y = variant.uv0y,
				.uv1x = variant.uv1x, .uv1y = variant.uv1y,
				.atlasPage = variant.pageIndex,
			})) return false;
		}
		if (!materialized && !visitor(userData, DevIconView{
			.key = key,
			.targetWidth = static_cast<std::uint32_t>(std::max(0.0f, document.intrinsicWidth)),
			.targetHeight = static_cast<std::uint32_t>(std::max(0.0f, document.intrinsicHeight)),
		})) return false;
	}
	return true;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconLookup));
}

Result<bool> IconManager::visitDevAtlases(void* userData, DevAtlasVisitor visitor) const noexcept try {
	if (!controller_ || !visitor) return false;
	for (std::uint32_t pageIndex = 0; pageIndex < controller_->atlasPages.size(); ++pageIndex) {
		const AtlasPage& page = controller_->atlasPages[pageIndex];
		TextureHandle texture{};
		std::uint32_t regions = 0;
		for (const auto& [_, variant] : controller_->variantsByKeyAndSize) {
			if (variant.pageIndex != pageIndex) continue;
			if (!texture) texture = variant.texture;
			++regions;
		}
		if (!visitor(userData, DevAtlasView{
			.texture = texture,
			.page = pageIndex,
			.width = page.width,
			.height = page.height,
			.usedArea = page.usedArea,
			.allocatedRegions = regions,
		})) return false;
	}
	return true;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconLookup));
}

Result<IconManager::DevIconView> IconManager::devRasterize(
	std::string_view key, uint32_t width, uint32_t height) noexcept {
	if (!controller_ || !storage_)
		return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconPublishAtlas));
	if (key.empty() || !width || !height || width > 512 || height > 512)
		return unexpectedError(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconPublishAtlas));
	try {
		const auto registered = contains(key);
		if (!registered) return unexpectedError(registered.error());
		if (!*registered) return unexpectedError(makeError(ErrorCode::IconNotFound, ErrorSite::IconPublishAtlas));
		const bool existing = controller_->variantsByKeyAndSize.contains(makeVariantKey(key, width, height));
		const auto& variant = ensureVariantForRequest(key, width, height, true);
		return DevIconView{.key = variant.key.nameKey,
			.targetWidth = variant.key.requestedWidth,
			.targetHeight = variant.key.requestedHeight,
			.texture = variant.texture,
			.uv0x = variant.uv0x, .uv0y = variant.uv0y,
			.uv1x = variant.uv1x, .uv1y = variant.uv1y,
			.atlasPage = variant.pageIndex, .newlyRasterized = !existing};
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::IconPublishAtlas));
	}
}

Result<bool> IconManager::devRemoveRaster(TextureHandle texture) noexcept {
	if (!controller_ || !storage_)
		return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconPublishAtlas));
	try {
		const auto found = std::ranges::find_if(controller_->variantsByKeyAndSize,
			[texture](const auto& item) { return item.second.texture == texture; });
		if (found == controller_->variantsByKeyAndSize.end()) return false;
		const auto& variant = found->second;
		controller_->retiredRegions.reserve(controller_->retiredRegions.size() + 1);
		storage_->system().noteManagerMutation(InvalidWindowId);
		storage_->system().releaseAnonymousTexture(variant.texture);
		controller_->retiredRegions.emplace_back(
			detail::manager_storage::IconRetiredRegion{variant.texture, variant.pageIndex, variant.paddedRect});
		controller_->variantsByKeyAndSize.erase(found);
		return true;
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::IconPublishAtlas));
	}
}
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void IconManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (!controller_) return;
	try {
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::icons_documents,
			controller_->documentsByKey, controller_->capacity_demand);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::icons_requests,
			controller_->requestTextureByKey, controller_->capacity_demand);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::icons_reverse_lookup,
			controller_->requestedKeyByTexture, controller_->capacity_demand);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::icons_variants,
			controller_->variantsByKeyAndSize, controller_->capacity_demand);
		devSystems::append_capacity_demand(sink, FlowUi::detail::InitialCapacityTarget::icons_pages,
										   controller_->atlasPages, controller_->capacity_demand);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::icons_retired_regions,
			controller_->retiredRegions, controller_->capacity_demand);
		devSystems::DevContainerMemoryAccumulator memory{};
		memory.addNodeContainer(controller_->documentsByKey);
		memory.addNodeContainer(controller_->requestTextureByKey);
		memory.addNodeContainer(controller_->requestedKeyByTexture);
		memory.addNodeContainer(controller_->variantsByKeyAndSize);
		memory.add(controller_->atlasPages);
		memory.add(controller_->retiredRegions);
		for (const auto& page : controller_->atlasPages)
			FLOWUI_MEMORY_DEMAND_RECORD(
				controller_->capacity_demand,
				FlowUi::detail::InitialCapacityTarget::icons_free_rectangles,
				page.freeRects.size());
		(void)sink.append(devSystems::MemoryValueSample{
			.source = FlowUi::detail::initial_capacity_definitions
						  [static_cast<size_t>(
							   FlowUi::detail::InitialCapacityTarget::icons_free_rectangles)]
							  .id(),
			.objectCount = controller_->capacity_demand.consume(
				FlowUi::detail::InitialCapacityTarget::icons_free_rectangles, 0),
			.flags = devSystems::MemorySampleFlag::TuningAggregate,
			.value_fields = uint16_t(devSystems::MemoryValueField::Objects)});
		for (const auto& [key, _] : controller_->documentsByKey) memory.add(key);
		for (const auto& [key, _] : controller_->requestTextureByKey) memory.add(key);
		for (const auto& [_, key] : controller_->requestedKeyByTexture) memory.add(key);
		for (const auto& page : controller_->atlasPages) memory.add(page.freeRects);
		devSystems::appendManagerSample(sink, devSystems::memory_sources::kIcons.id, memory);
		devSystems::DevContainerMemoryAccumulator documents{};
		documents.addNodeContainer(controller_->documentsByKey);
		for (const auto& [key, _] : controller_->documentsByKey) documents.add(key);
		devSystems::appendManagerSample(
			sink, devSystems::memory_sources::kIconSvgDocuments.id, documents);
		devSystems::DevContainerMemoryAccumulator atlasMetadata{};
		atlasMetadata.add(controller_->atlasPages);
		for (const auto& page : controller_->atlasPages) atlasMetadata.add(page.freeRects);
		devSystems::appendManagerSample(
			sink, devSystems::memory_sources::kIconAtlasMetadata.id, atlasMetadata);
	} catch (...) {}
}
#endif

namespace manager_storage = detail::manager_storage;
namespace key_storage = detail::managerStorage;
namespace storage = detail::storage;

namespace {

constexpr uint32_t MissingIconDiagnostic = 1u;
constexpr uint32_t IconGenerationDiagnostic = 2u;

storage::ResourceKey iconKey(storage::ActiveStorageSystem& storageSystem, ResourceKey key) {
	return key_storage::normalizeResourceKey(
		storageSystem, key, ResourceDomain::Icon,
		key_storage::ResourceScope::AppShared);
}

struct IconSourceBlobGuard {
	storage::ActiveStorageSystem* storageSystem;
	storage::BlobHandle source;
	~IconSourceBlobGuard() noexcept {
		try {
			if (source)
				storageSystem->releaseBlob(source);
		} catch (...) {}
	}
};

} // namespace

uint32_t IconManager::frameAge(uint32_t currentFrame, uint32_t lastUsedFrame) {
	// Unsigned subtraction is wrap-safe in modulo-2^32 arithmetic.
	// Example: lastUsed=UINT32_MAX-5, current=2 => age=8.
	return currentFrame - lastUsedFrame;
}

IconManager::VariantKey IconManager::makeVariantKey(std::string_view key, uint32_t requestedWidth, uint32_t requestedHeight) const
{
	VariantKey variantKey{};
	variantKey.nameKey = std::string(key);
	variantKey.requestedWidth = std::max<uint32_t>(1u, requestedWidth);
	variantKey.requestedHeight = std::max<uint32_t>(1u, requestedHeight);
	return variantKey;
}

void IconManager::advanceFrameCounter() {
	++controller_->frameCounter;
	if (controller_->frameCounter != 0u) {
		return;
	}

	// Overflow policy: renormalize timestamps to maintain stable ordering semantics.
	for (auto& [_, variant] : controller_->variantsByKeyAndSize) {
		variant.lastUsedFrame = 0u;
	}
	for (AtlasPage& page : controller_->atlasPages) {
		page.lastUsedFrame = 0u;
	}
}

void IconManager::touchVariant(VariantEntry& variant, uint32_t frameIndex) {
	variant.lastUsedFrame = frameIndex;
	variant.referencedThisFrame = true;
}

void IconManager::resetVariantFrameMarks() {
	for (auto& [_, variant] : controller_->variantsByKeyAndSize) {
		variant.referencedThisFrame = false;
	}
}

IconManager::AtlasPage IconManager::createAtlasPage(uint32_t pageIndex) const
{
	(void)pageIndex;
	if (!storage_ || !controller_ || !controller_->atlasSampler) {
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconPublishAtlas));
	}

	AtlasPage page{};
	detail::reserve_initial_capacity(
		page.freeRects, storage_->system().initial_memory_capacity(
							FlowUi::detail::InitialCapacityTarget::icons_free_rectangles));
	page.width = std::max<uint32_t>(1u, controller_->atlasSize);
	page.height = std::max<uint32_t>(1u, controller_->atlasSize);
	const auto pageBytes = detail::rgba8ByteSize(page.width, page.height);
	if (!pageBytes.valid)
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconPublishAtlas));
	page.freeRects.emplace_back(AtlasRect{
		.x = 0u,
		.y = 0u,
		.w = page.width,
		.h = page.height,
	});
	page.usedArea = 0u;
	page.lastUsedFrame = controller_->frameCounter;

	storage::BlobHandle zeroBlob{};
	try {
		const storage::StringId name = storage_->system().intern("flowui.icon.atlas.page");
		page.image = storage_->system().createImage(storage::ImageDesc{
			.width = page.width,
			.height = page.height,
			.format = storage::PixelFormat::Rgba8Srgb,
			.usage = storage::ImageUsage::Sampled | storage::ImageUsage::TransferDestination,
			.sharing = storage::ResourceSharing::AppShared,
			.debugName = name,
		});
		page.view = storage_->system().createImageView(
			page.image, storage::ImageViewDesc{
							.format = storage::PixelFormat::Rgba8Srgb,
							.debugName = name,
						});
		std::vector<std::byte> zeroes(pageBytes.value, std::byte{0});
		zeroBlob = storage_->system().createBlob(zeroes, name);
		(void)storage_->system().enqueueUpload(storage::UploadRequest{
			.byteCount = zeroes.size(),
			.imageRegion = storage::ImageRegion{.width = page.width, .height = page.height},
			.source = zeroBlob,
			.destinationImage = page.image,
			.destination = storage::UploadDestination::Image,
			.releaseSourceWhenComplete = true,
		});
		storage_->system().flushUploads();
		zeroBlob = {};
	} catch (...) {
		if (zeroBlob)
			storage_->system().releaseBlob(zeroBlob);
		if (page.view)
			storage_->system().releaseImageView(page.view);
		if (page.image)
			storage_->system().releaseImage(page.image);
		throw;
	}

	return page;
}

void IconManager::destroyAtlasPage(AtlasPage& page) {
	if (storage_ && page.view)
		storage_->system().releaseImageView(page.view);
	if (storage_ && page.image)
		storage_->system().releaseImage(page.image);
	page.image = {};
	page.view = {};
	page.width = 0u;
	page.height = 0u;
	page.usedArea = 0u;
	page.lastUsedFrame = 0u;
	page.freeRects.clear();
}

void IconManager::destroyAtlasPages() {
	for (AtlasPage& page : controller_->atlasPages) {
		destroyAtlasPage(page);
	}
	FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
								FlowUi::detail::InitialCapacityTarget::icons_pages,
								controller_->atlasPages.size());
	controller_->atlasPages.clear();
}

bool IconManager::tryAllocateInPage(
	AtlasPage& page,
	uint32_t contentWidth,
	uint32_t contentHeight,
	uint32_t padding,
	AtlasAllocation& outAllocation)
{
	if (contentWidth == 0u || contentHeight == 0u || page.freeRects.empty()) {
		return false;
	}

	const uint32_t pad = padding;
	const auto padded = detail::icon::paddedIconExtent(contentWidth, contentHeight, padding);
	if (!padded.valid)
		return false;
	const uint32_t neededWidth = padded.width;
	const uint32_t neededHeight = padded.height;

	size_t bestIndex = std::numeric_limits<size_t>::max();
	uint64_t bestWaste = std::numeric_limits<uint64_t>::max();

	for (size_t i = 0; i < page.freeRects.size(); ++i) {
		const AtlasRect& rect = page.freeRects[i];
		if (rect.w < neededWidth || rect.h < neededHeight) {
			continue;
		}
		const uint64_t waste =
			static_cast<uint64_t>(rect.w) * static_cast<uint64_t>(rect.h) -
			static_cast<uint64_t>(neededWidth) * static_cast<uint64_t>(neededHeight);
		if (waste < bestWaste) {
			bestWaste = waste;
			bestIndex = i;
			if (waste == 0u) {
				break;
			}
		}
	}

	if (bestIndex == std::numeric_limits<size_t>::max()) {
		return false;
	}

	const AtlasRect selected = page.freeRects[bestIndex];
	page.freeRects.erase(page.freeRects.begin() + static_cast<std::ptrdiff_t>(bestIndex));

	const AtlasRect paddedRect{
		.x = selected.x,
		.y = selected.y,
		.w = neededWidth,
		.h = neededHeight,
	};
	const AtlasRect contentRect{
		.x = paddedRect.x + pad,
		.y = paddedRect.y + pad,
		.w = contentWidth,
		.h = contentHeight,
	};

	// Simple guillotine split into right strip and bottom strip.
	if (selected.w > neededWidth) {
		page.freeRects.emplace_back(AtlasRect{
			.x = selected.x + neededWidth,
			.y = selected.y,
			.w = selected.w - neededWidth,
			.h = neededHeight,
		});
	}
	if (selected.h > neededHeight) {
		page.freeRects.emplace_back(AtlasRect{
			.x = selected.x,
			.y = selected.y + neededHeight,
			.w = selected.w,
			.h = selected.h - neededHeight,
		});
	}

	page.usedArea += static_cast<uint64_t>(paddedRect.w) * static_cast<uint64_t>(paddedRect.h);

	outAllocation.pageIndex = std::numeric_limits<uint32_t>::max();
	outAllocation.paddedRect = paddedRect;
	outAllocation.contentRect = contentRect;
	return true;
}

void IconManager::releasePageRegion(AtlasPage& page, const AtlasRect& paddedRect) {
	if (paddedRect.w == 0u || paddedRect.h == 0u) {
		return;
	}
	page.freeRects.push_back(paddedRect);
	const uint64_t freedArea = static_cast<uint64_t>(paddedRect.w) * static_cast<uint64_t>(paddedRect.h);
	page.usedArea = (freedArea > page.usedArea) ? 0u : (page.usedArea - freedArea);
}

void IconManager::mergeFreeRects(AtlasPage& page)
{
	bool mergedAny = true;
	while (mergedAny) {
		mergedAny = false;
		for (size_t i = 0; i < page.freeRects.size() && !mergedAny; ++i) {
			for (size_t j = i + 1; j < page.freeRects.size(); ++j) {
				AtlasRect& a = page.freeRects[i];
				AtlasRect& b = page.freeRects[j];

				// Vertical merge: same x/w, touching y edge.
				if (a.x == b.x && a.w == b.w && (a.y + a.h == b.y || b.y + b.h == a.y))
				{
					const uint32_t top = std::min(a.y, b.y);
					a = AtlasRect{ .x = a.x, .y = top, .w = a.w, .h = a.h + b.h };
					page.freeRects.erase(page.freeRects.begin() + static_cast<std::ptrdiff_t>(j));
					mergedAny = true;
					break;
				}

				// Horizontal merge: same y/h, touching x edge.
				if (a.y == b.y && a.h == b.h && (a.x + a.w == b.x || b.x + b.w == a.x)) {
					const uint32_t left = std::min(a.x, b.x);
					a = AtlasRect{ .x = left, .y = a.y, .w = a.w + b.w, .h = a.h };
					page.freeRects.erase(page.freeRects.begin() + static_cast<std::ptrdiff_t>(j));
					mergedAny = true;
					break;
				}
			}
		}
	}
}

void IconManager::recalcAtlasUvs(VariantEntry& variant, const AtlasPage& page) const {
	const float invW = page.width > 0u ? (1.0f / static_cast<float>(page.width)) : 0.0f;
	const float invH = page.height > 0u ? (1.0f / static_cast<float>(page.height)) : 0.0f;
	variant.uv0x = static_cast<float>(variant.contentRect.x) * invW;
	variant.uv0y = static_cast<float>(variant.contentRect.y) * invH;
	variant.uv1x = static_cast<float>(variant.contentRect.x + variant.contentRect.w) * invW;
	variant.uv1y = static_cast<float>(variant.contentRect.y + variant.contentRect.h) * invH;
}

const std::string* IconManager::findRequestedKeyByTextureHandle(TextureHandle texture) const
{
	const auto it = controller_->requestedKeyByTexture.find(texture.packed());
	if (it == controller_->requestedKeyByTexture.end()) {
		return nullptr;
	}
	return &it->second;
}

void IconManager::uploadRasterToAtlasPage(
	const AtlasPage& page,
	const TransientRasterResult& raster,
	const AtlasRect& contentRect) {
	if (!storage_) {
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconPublishAtlas));
	}
	if (!page.image) {
		detail::terminateForFatalError(makeError(ErrorCode::RendererNativeResourceInvalid, ErrorSite::IconPublishAtlas));
	}
	const auto imageBytes = detail::rgba8ByteSize(raster.image.width, raster.image.height);
	if (!imageBytes.valid || raster.image.pixels.size() != imageBytes.value) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconPublishAtlas));
	}
	if (contentRect.x > page.width || raster.image.width > page.width - contentRect.x ||
		contentRect.y > page.height || raster.image.height > page.height - contentRect.y) {
		detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::IconPublishAtlas));
	}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevExternalMemoryScope rasterMemory(
		devMemoryRecorder_, devSystems::memory_sources::kIconRaster.id, imageBytes.value);
#endif
	storage::BlobHandle blob{};
	try {
		const storage::StringId name = storage_->system().intern("flowui.icon.atlas.region");
		blob = storage_->system().createBlob(raster.image.pixels, name);
		(void)storage_->system().enqueueUpload(storage::UploadRequest{
			.byteCount = raster.image.pixels.size(),
			.imageRegion =
				storage::ImageRegion{
					.x = contentRect.x,
					.y = contentRect.y,
					.width = raster.image.width,
					.height = raster.image.height,
				},
			.source = blob,
			.destinationImage = page.image,
			.destination = storage::UploadDestination::Image,
			.releaseSourceWhenComplete = true,
		});
		storage_->system().flushUploads();
		blob = {};
	} catch (...) {
		if (blob)
			storage_->system().releaseBlob(blob);
		throw;
	}
}

bool IconManager::tryAllocateAtlasRegion(
	uint32_t contentWidth,
	uint32_t contentHeight,
	AtlasAllocation& outAllocation) {
	outAllocation = AtlasAllocation{};

	auto tryAcrossPages = [&](bool mergeFirst) -> bool {
		for (uint32_t pageIndex = 0u; pageIndex < controller_->atlasPages.size(); ++pageIndex) {
			AtlasPage& page = controller_->atlasPages[pageIndex];
			if (mergeFirst && page.freeRects.size() > 1u) {
				mergeFreeRects(page);
			}

			AtlasAllocation candidate{};
			if (!tryAllocateInPage(page, contentWidth, contentHeight, controller_->atlasPadding, candidate)) {
				continue;
			}
			candidate.pageIndex = pageIndex;
			page.lastUsedFrame = controller_->frameCounter;
			outAllocation = candidate;
			return true;
		}
		return false;
	};

	if (tryAcrossPages(false)) {
		return true;
	}
	if (tryAcrossPages(true)) {
		return true;
	}

	if (controller_->atlasPages.size() < controller_->maxAtlasPages) {
		const uint32_t newPageIndex = static_cast<uint32_t>(controller_->atlasPages.size());
		controller_->atlasPages.push_back(createAtlasPage(newPageIndex));
		AtlasAllocation candidate{};
		if (tryAllocateInPage(controller_->atlasPages.back(), contentWidth, contentHeight, controller_->atlasPadding, candidate)) {
			candidate.pageIndex = newPageIndex;
			controller_->atlasPages.back().lastUsedFrame = controller_->frameCounter;
			outAllocation = candidate;
			return true;
		}
	}

	if (controller_->atlasPages.empty()) {
		return false;
	}

	while (capacityPolicy_ == CacheCapacityPolicy::EvictAndRetry &&
		evictLeastRecentlyUsedVariant()) {
		if (tryAcrossPages(false)) {
			return true;
		}
		if (tryAcrossPages(true)) {
			return true;
		}
	}

	return false;
}

bool IconManager::evictLeastRecentlyUsedVariant() {
	auto victimIt = controller_->variantsByKeyAndSize.end();
	uint32_t bestAge = 0u;

	for (auto it = controller_->variantsByKeyAndSize.begin(); it != controller_->variantsByKeyAndSize.end(); ++it) {
		const VariantEntry& candidate = it->second;
		if (candidate.referencedThisFrame) {
			continue;
		}
		const uint32_t age = frameAge(controller_->frameCounter, candidate.lastUsedFrame);
		if (victimIt == controller_->variantsByKeyAndSize.end() || age > bestAge) {
			victimIt = it;
			bestAge = age;
		}
	}

	if (victimIt == controller_->variantsByKeyAndSize.end()) {
		return false;
	}

	const VariantEntry victim = victimIt->second;
	controller_->retiredRegions.reserve(controller_->retiredRegions.size() + 1);
	storage_->system().noteManagerMutation(InvalidWindowId);
	storage_->system().releaseAnonymousTexture(victim.texture);
	controller_->retiredRegions.push_back({victim.texture, victim.pageIndex, victim.paddedRect});
	FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
								FlowUi::detail::InitialCapacityTarget::icons_variants,
								controller_->variantsByKeyAndSize.size());
	controller_->variantsByKeyAndSize.erase(victimIt);
	return true;
}

IconManager::VariantEntry* IconManager::findBestCachedVariant(
	std::string_view nameKey,
	uint32_t requestedWidth,
	uint32_t requestedHeight) {
	const uint32_t maxSizeGap = std::max<uint32_t>(1u, controller_->sizeReuseTolerance);

	VariantEntry* bestAbove = nullptr;
	uint64_t bestAboveGap = std::numeric_limits<uint64_t>::max();
	uint64_t bestAboveArea = std::numeric_limits<uint64_t>::max();

	VariantEntry* bestLower = nullptr;
	uint64_t bestLowerGap = std::numeric_limits<uint64_t>::max();
	uint64_t bestLowerArea = 0u;

	for (auto& [_, variant] : controller_->variantsByKeyAndSize) {
		if (variant.key.nameKey != nameKey) {
			continue;
		}

		const uint32_t variantWidth = variant.key.requestedWidth;
		const uint32_t variantHeight = variant.key.requestedHeight;

		if (variantWidth == requestedWidth && variantHeight == requestedHeight) {
			return &variant;
		}

		if (variantWidth >= requestedWidth && variantHeight >= requestedHeight) {
			const uint32_t widthGap = variantWidth - requestedWidth;
			const uint32_t heightGap = variantHeight - requestedHeight;
			if (widthGap > maxSizeGap || heightGap > maxSizeGap) {
				continue;
			}

			const uint64_t gap =
				static_cast<uint64_t>(widthGap) +
				static_cast<uint64_t>(heightGap);
			const uint64_t area = static_cast<uint64_t>(variantWidth) * static_cast<uint64_t>(variantHeight);
			if (gap < bestAboveGap || (gap == bestAboveGap && area < bestAboveArea)) {
				bestAbove = &variant;
				bestAboveGap = gap;
				bestAboveArea = area;
			}
			continue;
		}

		if (variantWidth <= requestedWidth && variantHeight <= requestedHeight) {
			const uint32_t widthGap = requestedWidth - variantWidth;
			const uint32_t heightGap = requestedHeight - variantHeight;
			if (widthGap > maxSizeGap || heightGap > maxSizeGap) {
				continue;
			}

			const uint64_t gap =
				static_cast<uint64_t>(widthGap) +
				static_cast<uint64_t>(heightGap);
			const uint64_t area = static_cast<uint64_t>(variantWidth) * static_cast<uint64_t>(variantHeight);
			if (gap < bestLowerGap || (gap == bestLowerGap && area > bestLowerArea)) {
				bestLower = &variant;
				bestLowerGap = gap;
				bestLowerArea = area;
			}
		}
	}

	if (bestAbove) {
		return bestAbove;
	}
	if (bestLower) {
		return bestLower;
	}

	return nullptr;
}

IconManager::VariantEntry& IconManager::ensureVariantForRequest(
	std::string_view nameKey,
	uint32_t requestedWidth,
	uint32_t requestedHeight,
	bool exactSize) {
	const VariantKey requestKey = makeVariantKey(nameKey, requestedWidth, requestedHeight);
	if (exactSize) {
		const auto cached = controller_->variantsByKeyAndSize.find(requestKey);
		if (cached != controller_->variantsByKeyAndSize.end()) return cached->second;
	} else {
		if (VariantEntry* cached = findBestCachedVariant(nameKey, requestKey.requestedWidth, requestKey.requestedHeight))
			return *cached;
	}

	TransientRasterResult raster = rasterizeForAtlas(nameKey, requestKey.requestedWidth, requestKey.requestedHeight);
	decltype(controller_->variantsByKeyAndSize) preparedVariants;
	preparedVariants.emplace(requestKey, VariantEntry{.key = requestKey});
	auto preparedVariant = preparedVariants.extract(requestKey);
	controller_->variantsByKeyAndSize.reserve(controller_->variantsByKeyAndSize.size() + 1);
	storage_->system().noteManagerMutation(InvalidWindowId);
	AtlasAllocation allocation{};
	if (!tryAllocateAtlasRegion(raster.image.width, raster.image.height, allocation)) {
		throw FlowUiException(makeError(ErrorCode::IconAtlasCapacityExceeded, ErrorSite::IconPublishAtlas));
	}
	if (allocation.pageIndex >= controller_->atlasPages.size()) {
		detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::IconPublishAtlas));
	}

	AtlasPage& page = controller_->atlasPages[allocation.pageIndex];
	VariantEntry& entry = preparedVariant.mapped();
	entry.pageIndex = allocation.pageIndex;
	entry.paddedRect = allocation.paddedRect;
	entry.contentRect = allocation.contentRect;
	entry.sourceWidth = raster.image.width;
	entry.sourceHeight = raster.image.height;
	entry.lastUsedFrame = controller_->frameCounter;
	entry.referencedThisFrame = true;
	recalcAtlasUvs(entry, page);
	TextureHandle preparedTexture{};
	try {
		uploadRasterToAtlasPage(page, raster, allocation.contentRect);
		preparedTexture = storage_->system().createAnonymousTexture(storage::TextureViewDesc{
			.imageView = page.view,
			.sampler = controller_->atlasSampler,
			.uv0x = entry.uv0x,
			.uv0y = entry.uv0y,
			.uv1x = entry.uv1x,
			.uv1y = entry.uv1y,
			.sourceWidth = static_cast<int32_t>(entry.sourceWidth),
			.sourceHeight = static_cast<int32_t>(entry.sourceHeight),
		});
		entry.texture = preparedTexture;
		auto insertion = controller_->variantsByKeyAndSize.insert(std::move(preparedVariant));
		if (!insertion.inserted) {
			storage_->system().releaseAnonymousTexture(preparedTexture);
			releasePageRegion(page, allocation.paddedRect);
			return insertion.position->second;
		}
		page.lastUsedFrame = controller_->frameCounter;
		return insertion.position->second;
	} catch (...) {
		if (preparedTexture)
			try {
				storage_->system().releaseAnonymousTexture(preparedTexture);
			} catch (...) {}
		releasePageRegion(page, allocation.paddedRect);
		throw;
	}
}

void IconManager::prepareFrameTextures(
	Clay_RenderCommandArray& renderCommands,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY)
{
	if (!storage_ || !controller_) {
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconPublishAtlas));
	}

	if (!std::isfinite(uiToFramebufferScaleX) || !std::isfinite(uiToFramebufferScaleY)) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
	}
	const float clampedScaleX = std::max(uiToFramebufferScaleX, 1.0e-6f);
	const float clampedScaleY = std::max(uiToFramebufferScaleY, 1.0e-6f);

	for (int32_t i = 0; i < renderCommands.length; ++i) {
		Clay_RenderCommand& command = renderCommands.internalArray[i];
		if (command.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE) {
			continue;
		}

		auto* textureRef = reinterpret_cast<TextureRef*>(command.renderData.image.imageData);
		if (!textureRef || !textureRef->handle) {
			continue;
		}

		const std::string* requestedKey = findRequestedKeyByTextureHandle(textureRef->handle);
		if (!requestedKey) {
			continue;
		}

		const float scaledWidth = std::max(0.0f, command.boundingBox.width * clampedScaleX);
		const float scaledHeight = std::max(0.0f, command.boundingBox.height * clampedScaleY);
		if (!std::isfinite(command.boundingBox.width) ||
			!std::isfinite(command.boundingBox.height) || !std::isfinite(scaledWidth) ||
			!std::isfinite(scaledHeight) ||
			static_cast<double>(std::ceil(scaledWidth)) > std::numeric_limits<uint32_t>::max() ||
			static_cast<double>(std::ceil(scaledHeight)) > std::numeric_limits<uint32_t>::max()) {
			throw FlowUiException(
				makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
		}
		const uint32_t requestedWidth = std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(scaledWidth)));
		const uint32_t requestedHeight = std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(scaledHeight)));

		VariantEntry* variantPointer = nullptr;
		try {
			variantPointer = &ensureVariantForRequest(*requestedKey, requestedWidth, requestedHeight);
		} catch (const FlowUiException& error) {
			const ErrorCode code = error.error().code;
			if (generationPolicy_ == IconGenerationFailurePolicy::FailRequest ||
				(code != ErrorCode::IconNotFound &&
				 code != ErrorCode::IconRasterizationFailed &&
				 code != ErrorCode::IconRasterInvalid &&
				 code != ErrorCode::IconAtlasCapacityExceeded)) {
				throw;
			}
			textureRef->handle = {};
			textureRef->skipIfUnavailable =
				generationPolicy_ == IconGenerationFailurePolicy::SkipVisual;
			const storage::ResourceKey diagnosticKey =
				iconKey(storage_->system(), ResourceKey{.name = *requestedKey});
			if (storage_->system().markDiagnosticOnce(diagnosticKey, IconGenerationDiagnostic)) {
				detail::reportErrorEvent(ErrorEventView{
					.error = error.error(),
					.kind = ErrorEventKind::Resolved,
					.resolution = generationPolicy_ == IconGenerationFailurePolicy::SkipVisual
						? ErrorResolution::Skipped
						: ErrorResolution::UsedFallback,
				});
			}
			continue;
		}
		VariantEntry& variant = *variantPointer;
		touchVariant(variant, controller_->frameCounter);
		if (variant.pageIndex < controller_->atlasPages.size()) {
			controller_->atlasPages[variant.pageIndex].lastUsedFrame = controller_->frameCounter;
		}

		textureRef->handle = variant.texture;
		textureRef->uv0x = variant.uv0x;
		textureRef->uv0y = variant.uv0y;
		textureRef->uv1x = variant.uv1x;
		textureRef->uv1y = variant.uv1y;
		textureRef->sourceWidth = static_cast<int32_t>(variant.sourceWidth);
		textureRef->sourceHeight = static_cast<int32_t>(variant.sourceHeight);
	}
}

void IconManager::beginAppTick() {
	if (!storage_ || !controller_) return;
	for (auto it = controller_->retiredRegions.begin(); it != controller_->retiredRegions.end();) {
		if (!storage_->system().textureRetirementComplete(it->texture)) {
			++it;
			continue;
		}
		if (it->pageIndex < controller_->atlasPages.size()) {
			AtlasPage& page = controller_->atlasPages[it->pageIndex];
			releasePageRegion(page, it->paddedRect);
			mergeFreeRects(page);
		}
		it = controller_->retiredRegions.erase(it);
	}
	advanceFrameCounter();
	resetVariantFrameMarks();
}

void IconManager::init(storage::StorageBinding& storageBinding, const IconManagerConfig& config,
					   IconGenerationFailurePolicy generationPolicy,
					   CacheCapacityPolicy capacityPolicy) {
	auto& storageSystem = storageBinding.system();
	destroy();
	generationPolicy_ = generationPolicy;
	capacityPolicy_ = capacityPolicy;
	const storage::StringId name = storageSystem.intern("flowui.icon.cache");
	const storage::ResourceKey key{storage::ResourceDomain::Icon, name, InvalidWindowId};
	const storage::ManagerRecordHandle handle = manager_storage::createState<manager_storage::IconCacheController>(
		storageSystem, key, storage::ResourceKind::IconVariant, name,
		std::ref(storageSystem), std::cref(config));
	storage_ = &storageBinding;
	controllerHandle_ = handle.packed();
	controller_ = manager_storage::state<manager_storage::IconCacheController>(
		storage_, handle, storage::ResourceKind::IconVariant);
	if (!controller_) {
		destroy();
		throw FlowUiException(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::IconManagerInitialize));
	}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	controller_->capacity_demand.bind(storageSystem.memory_demand_control());
#endif
	detail::reserve_initial_capacity(controller_->documentsByKey,
									 storageSystem.initial_memory_capacity(
										 FlowUi::detail::InitialCapacityTarget::icons_documents));
	detail::reserve_initial_capacity(controller_->requestTextureByKey,
									 storageSystem.initial_memory_capacity(
										 FlowUi::detail::InitialCapacityTarget::icons_requests));
	detail::reserve_initial_capacity(
		controller_->requestedKeyByTexture,
		storageSystem.initial_memory_capacity(
			FlowUi::detail::InitialCapacityTarget::icons_reverse_lookup));
	detail::reserve_initial_capacity(controller_->variantsByKeyAndSize,
									 storageSystem.initial_memory_capacity(
										 FlowUi::detail::InitialCapacityTarget::icons_variants));
	detail::reserve_initial_capacity(
		controller_->atlasPages,
		storageSystem.initial_memory_capacity(FlowUi::detail::InitialCapacityTarget::icons_pages));
	detail::reserve_initial_capacity(
		controller_->retiredRegions,
		storageSystem.initial_memory_capacity(
			FlowUi::detail::InitialCapacityTarget::icons_retired_regions));
	try {
		controller_->atlasPages.push_back(createAtlasPage(0u));
	} catch (...) {
		destroy();
		throw;
	}
}

Result<bool> IconManager::registerSvg(std::string_view key, std::string_view svgSource) noexcept try {
	return registerSvg(ResourceKey{.name = key}, svgSource);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconRegisterSource));
}

Result<bool> IconManager::registerSvg(ResourceKey key, std::string_view svgSource) noexcept try {
	if (!storage_ || !controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconRegisterSource));
	storage::ResourceKey normalized{};
	try {
		normalized = iconKey(storage_->system(), key);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}

	const std::string keyString(key.name);
	if (controller_->documentsByKey.find(keyString) != controller_->documentsByKey.end()) {
		return false;
	}

	if (svgSource.empty()) {
		return unexpectedError(makeError(ErrorCode::IconSourceInvalid, ErrorSite::IconRegisterSource));
	}
	if (svgSource.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
		return unexpectedError(
			makeError(ErrorCode::IconSourceInvalid, ErrorSite::IconRegisterSource));
	}

	auto parsed =
		controller_->provider.parse(std::as_bytes(std::span(svgSource.data(), svgSource.size())));
	if (!parsed)
		return unexpectedError(parsed.error());
	const auto dimensions = controller_->provider.intrinsicSize(*parsed);
	if (!std::isfinite(dimensions.width) || !std::isfinite(dimensions.height) ||
		dimensions.width < 0.0f || dimensions.height < 0.0f ||
		static_cast<double>(dimensions.width) > std::numeric_limits<int32_t>::max() ||
		static_cast<double>(dimensions.height) > std::numeric_limits<int32_t>::max()) {
		return unexpectedError(
			makeError(ErrorCode::IconSourceInvalid, ErrorSite::IconRegisterSource));
	}
	storage_->system().noteManagerMutation(InvalidWindowId);
	IconSourceBlobGuard sourceOwner{
		&storage_->system(),
		storage_->system().createBlob(std::as_bytes(std::span(svgSource.data(), svgSource.size())),
									  normalized.name)};
	DocumentRecord record{
		.document = std::move(*parsed),
		.source = sourceOwner.source,
		.intrinsicWidth = dimensions.width,
		.intrinsicHeight = dimensions.height,
	};
	const auto insertion = controller_->documentsByKey.try_emplace(keyString, std::move(record));
	if (!insertion.second)
		return false;
	sourceOwner.source = {};
	return true;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconRegisterSource));
}

Result<bool> IconManager::registerFromFile(std::string_view key, std::string_view filePath) noexcept try {
	return registerFromFile(ResourceKey{.name = key}, filePath);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconRegisterSource));
}

Result<bool> IconManager::registerFromFile(ResourceKey key, std::string_view filePath) noexcept try {
	return register_file(key, detail::requireApiValue(path_from_utf8(filePath)));
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconRegisterSource));
}

Result<bool> IconManager::register_file(ResourceKey key, const std::filesystem::path& path) noexcept try {
	if (!storage_ || !controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconRegisterSource));
	try {
		(void)iconKey(storage_->system(), key);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	if (path.empty()) return unexpectedError(makeError(ErrorCode::AssetPathEmpty, ErrorSite::IconRegisterSource));
	std::error_code pathError;
	if (!std::filesystem::is_regular_file(path, pathError)) {
		return unexpectedError(makeError(
			pathError ? ErrorCode::AssetOpenFailed : ErrorCode::AssetNotFound,
			ErrorSite::IconRegisterSource));
	}

	std::ifstream stream(path, std::ios::binary);
	if (!stream) return unexpectedError(makeError(ErrorCode::AssetOpenFailed, ErrorSite::IconRegisterSource));
	const std::string source((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (!stream.eof() && stream.fail()) return unexpectedError(makeError(ErrorCode::AssetReadFailed, ErrorSite::IconRegisterSource));
	return registerSvg(key, source);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconRegisterSource));
}

Result<bool> IconManager::remove(std::string_view key) noexcept try { return remove(ResourceKey{.name = key}); } catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconRemove));
}

Result<bool> IconManager::remove(ResourceKey key) noexcept try {
	if (!storage_ || !controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconRemove));
	storage::ResourceKey normalized{};
	try {
		normalized = iconKey(storage_->system(), key);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	const std::string keyString(key.name);
	auto it = controller_->documentsByKey.find(keyString);
	if (it == controller_->documentsByKey.end()) {
		return false;
	}

	controller_->retiredRegions.reserve(
		controller_->retiredRegions.size() + controller_->variantsByKeyAndSize.size());
	storage_->system().noteManagerMutation(InvalidWindowId);
	const auto requestIdIt = controller_->requestTextureByKey.find(keyString);
	if (requestIdIt != controller_->requestTextureByKey.end()) {
		(void)storage_->system().removeTexture(normalized);
		FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
									FlowUi::detail::InitialCapacityTarget::icons_reverse_lookup,
									controller_->requestedKeyByTexture.size());
		controller_->requestedKeyByTexture.erase(requestIdIt->second.packed());
		FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
									FlowUi::detail::InitialCapacityTarget::icons_requests,
									controller_->requestTextureByKey.size());
		controller_->requestTextureByKey.erase(requestIdIt);
	}

	for (auto variantIt = controller_->variantsByKeyAndSize.begin(); variantIt != controller_->variantsByKeyAndSize.end();) {
		VariantEntry& variant = variantIt->second;
		if (variant.key.nameKey != keyString) {
			++variantIt;
			continue;
		}

		storage_->system().releaseAnonymousTexture(variant.texture);
		controller_->retiredRegions.push_back({variant.texture, variant.pageIndex, variant.paddedRect});
		variantIt = controller_->variantsByKeyAndSize.erase(variantIt);
	}

	const storage::BlobHandle source = it->second.source;
	FLOWUI_MEMORY_DEMAND_RECORD(controller_->capacity_demand,
								FlowUi::detail::InitialCapacityTarget::icons_documents,
								controller_->documentsByKey.size());
	controller_->documentsByKey.erase(it);
	storage_->system().releaseBlob(source);
	return true;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconRemove));
}

Result<bool> IconManager::contains(std::string_view key) const noexcept try {
	return contains(ResourceKey{.name = key});
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconLookup));
}

Result<bool> IconManager::contains(ResourceKey key) const noexcept try {
	if (!storage_ || !controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconLookup));
	(void)iconKey(storage_->system(), key);
	return controller_->documentsByKey.find(std::string(key.name)) != controller_->documentsByKey.end();
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconLookup));
}

Result<TextureRef> IconManager::textureRef(std::string_view key) noexcept try { return textureRef(ResourceKey{.name = key}); } catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconLookup));
}

Result<TextureRef> IconManager::textureRef(ResourceKey key) noexcept try {
	if (!storage_ || !controller_) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconLookup));
	const storage::ResourceKey normalized = iconKey(storage_->system(), key);
	const std::string_view stableKey = storage_->system().string(normalized.name);
	if (controller_->atlasPages.empty() || !controller_->atlasPages.front().view || !controller_->atlasSampler) {
		detail::terminateForFatalError(makeError(ErrorCode::RendererNativeResourceInvalid, ErrorSite::IconLookup));
	}

	const std::string keyString(key.name);
	const auto documentIt = controller_->documentsByKey.find(keyString);
	if (documentIt == controller_->documentsByKey.end()) {
		if (generationPolicy_ == IconGenerationFailurePolicy::FailRequest) {
			throw FlowUiException(makeError(ErrorCode::IconNotFound, ErrorSite::IconLookup));
		}
		if (storage_->system().markDiagnosticOnce(normalized, MissingIconDiagnostic)) {
			detail::reportErrorEvent(ErrorEventView{
				.error = makeError(
					ErrorCode::IconNotFound, ErrorSite::IconLookup,
					normalized.name),
				.kind = ErrorEventKind::Resolved,
				.resolution = generationPolicy_ == IconGenerationFailurePolicy::SkipVisual
					? ErrorResolution::Skipped
					: ErrorResolution::UsedFallback,
			});
		}
		return TextureRef{
			.sourceKey = stableKey,
			.sourceDomain = ResourceDomain::Icon,
			.skipIfUnavailable = generationPolicy_ == IconGenerationFailurePolicy::SkipVisual,};
	}

	auto requestIdIt = controller_->requestTextureByKey.find(keyString);
	if (requestIdIt == controller_->requestTextureByKey.end()) {
		// Prepare both index nodes and bucket capacity before publishing a texture.
		// Re-keying/inserting the prepared reverse node then requires no allocation.
		std::unordered_map<uint64_t, std::string> pendingReverseIndex;
		pendingReverseIndex.emplace(0, keyString);
		auto reverseNode = pendingReverseIndex.extract(0);
		controller_->requestedKeyByTexture.reserve(controller_->requestedKeyByTexture.size() + 1);
		storage_->system().noteManagerMutation(InvalidWindowId);
		auto pendingRequest =
			controller_->requestTextureByKey.try_emplace(keyString, TextureHandle{}).first;
		bool inserted = false;
		const AtlasPage& page = controller_->atlasPages.front();
		TextureHandle requestTexture{};
		try {
			requestTexture = storage_->system().publishTexture(
				normalized,
				storage::TextureViewDesc{
					.imageView = page.view,
					.sampler = controller_->atlasSampler,
					.sourceWidth = static_cast<int32_t>(
						std::max(1.0f, std::round(documentIt->second.intrinsicWidth))),
					.sourceHeight = static_cast<int32_t>(
						std::max(1.0f, std::round(documentIt->second.intrinsicHeight))),
				},
				&inserted);
		} catch (...) {
			controller_->requestTextureByKey.erase(pendingRequest);
			throw;
		}
		if (!inserted) {
			detail::terminateForFatalError(makeError(ErrorCode::IconKeyCollision, ErrorSite::IconLookup));
		}
		pendingRequest->second = requestTexture;
		requestIdIt = pendingRequest;
		reverseNode.key() = requestTexture.packed();
		const auto reverseInsertion = controller_->requestedKeyByTexture.insert(std::move(reverseNode));
		if (!reverseInsertion.inserted) {
			detail::terminateForFatalError(makeError(ErrorCode::IconKeyCollision, ErrorSite::IconLookup));
		}
	}

	TextureRef texture{};
	texture.sourceDomain = ResourceDomain::Icon;
	texture.sourceKey = stableKey;
	texture.handle = requestIdIt->second;
	texture.sourceWidth = static_cast<int32_t>(std::max(1.0f, std::round(documentIt->second.intrinsicWidth)));
	texture.sourceHeight = static_cast<int32_t>(std::max(1.0f, std::round(documentIt->second.intrinsicHeight)));
	return texture;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::IconLookup));
}

IconManager::TransientRasterResult IconManager::rasterizeForAtlas(std::string_view key,
																  uint32_t requestedWidth,
																  uint32_t requestedHeight) const {
	if (!storage_ || !controller_) {
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconRasterize));
	}
	if (key.empty()) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
	}
	const auto record = controller_->documentsByKey.find(std::string(key));
	if (record == controller_->documentsByKey.end()) {
		throw FlowUiException(makeError(ErrorCode::IconNotFound, ErrorSite::IconRasterize));
	}
	const uint32_t targetWidth = std::max(1u, requestedWidth);
	const uint32_t targetHeight = std::max(1u, requestedHeight);
	const auto intrinsic = controller_->provider.intrinsicSize(record->second.document);
	const auto content = intrinsic.width > 0.0f && intrinsic.height > 0.0f
							 ? detail::icon::IconDimensions{}
							 : controller_->provider.contentSize(record->second.document);
	const auto fitted =
		detail::icon::fitIconRasterExtent(targetWidth, targetHeight, intrinsic, content);
	if (!fitted.valid) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
	}
	const uint32_t rasterWidth = fitted.width;
	const uint32_t rasterHeight = fitted.height;
	const auto padded =
		detail::icon::paddedIconExtent(rasterWidth, rasterHeight, controller_->atlasPadding);
	if (!padded.valid || padded.width > controller_->atlasSize ||
		padded.height > controller_->atlasSize) {
		throw FlowUiException(
			makeError(ErrorCode::IconAtlasCapacityExceeded, ErrorSite::IconPublishAtlas));
	}

	auto rendered =
		controller_->provider.rasterize(record->second.document, {rasterWidth, rasterHeight});
	if (!rendered)
		throw FlowUiException(rendered.error());
	const auto imageBytes = detail::rgba8ByteSize(rasterWidth, rasterHeight);
	if (!imageBytes.valid || rendered->width != rasterWidth || rendered->height != rasterHeight ||
		rendered->pixels.size() != imageBytes.value) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
	}
	return TransientRasterResult{.image = std::move(*rendered)};
}

void IconManager::destroy() noexcept {
	if (storage_) {
		try {
			const storage::StringId name = storage_->system().intern("flowui.icon.cache");
			(void)storage_->system().removeManagerRecord(
				storage::ResourceKey{storage::ResourceDomain::Icon, name, InvalidWindowId},
				storage::ResourceKind::IconVariant);
		} catch (...) {}
	}
	controller_ = nullptr;
	controllerHandle_ = 0;
	storage_ = nullptr;
	generationPolicy_ = IconGenerationFailurePolicy::UseFallbackTexture;
	capacityPolicy_ = CacheCapacityPolicy::RejectOperation;
}

} // namespace FlowUi

#endif
