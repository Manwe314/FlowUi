#include "managers/IconManager.hpp"
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

#include "internal/ManagerStorage/IconCacheController.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/ManagerStorage/ResourceKeyNormalization.hpp"
#include <plutosvg.h>

namespace FlowUi {
#if FLOW_UI_DEV_MODE
std::uint64_t IconManager::devRevision() const noexcept {
	return storage_ ? storage_->managerSharedRevision() : 0;
}

std::size_t IconManager::devIconCount() const noexcept {
	return controller_ ? controller_->documentsByKey.size() + controller_->variantsByKeyAndSize.size() : 0;
}

std::size_t IconManager::devAtlasCount() const noexcept {
	return controller_ ? controller_->atlasPages.size() : 0;
}

bool IconManager::visitDevIcons(void* userData, DevIconVisitor visitor) const {
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
}

bool IconManager::visitDevAtlases(void* userData, DevAtlasVisitor visitor) const {
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
}
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void IconManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (!controller_) return;
	try {
		devSystems::DevContainerMemoryAccumulator memory{};
		memory.addNodeContainer(controller_->documentsByKey);
		memory.addNodeContainer(controller_->requestTextureByKey);
		memory.addNodeContainer(controller_->requestedKeyByTexture);
		memory.addNodeContainer(controller_->variantsByKeyAndSize);
		memory.add(controller_->atlasPages);
		memory.add(controller_->retiredRegions);
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

storage::ResourceKey iconKey(storage::IStorageSystem& storageSystem, ResourceKey key) {
	return key_storage::normalizeResourceKey(
		storageSystem, key, ResourceDomain::Icon,
		key_storage::ResourceScope::AppShared);
}

void convertArgbPremultipliedToRgbaStraight(
	uint8_t* data,
	uint32_t width,
	uint32_t height,
	uint32_t strideBytes) {
	if (!data || width == 0u || height == 0u || strideBytes < width * 4u) {
		return;
	}

	for (uint32_t y = 0u; y < height; ++y) {
		uint8_t* row = data + static_cast<size_t>(y) * static_cast<size_t>(strideBytes);
		for (uint32_t x = 0u; x < width; ++x) {
			const size_t index = static_cast<size_t>(x) * 4u;
			const uint32_t b = row[index + 0u];
			const uint32_t g = row[index + 1u];
			const uint32_t r = row[index + 2u];
			const uint32_t a = row[index + 3u];
			if (a == 0u) {
				row[index + 0u] = 0u;
				row[index + 1u] = 0u;
				row[index + 2u] = 0u;
				row[index + 3u] = 0u;
				continue;
			}

			uint32_t rr = r;
			uint32_t gg = g;
			uint32_t bb = b;
			if (a != 255u) {
				rr = (rr * 255u) / a;
				gg = (gg * 255u) / a;
				bb = (bb * 255u) / a;
			}

			row[index + 0u] = static_cast<uint8_t>(rr);
			row[index + 1u] = static_cast<uint8_t>(gg);
			row[index + 2u] = static_cast<uint8_t>(bb);
			row[index + 3u] = static_cast<uint8_t>(a);
		}
	}
}

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
	page.width = std::max<uint32_t>(1u, controller_->atlasSize);
	page.height = std::max<uint32_t>(1u, controller_->atlasSize);
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
		const storage::StringId name = storage_->intern("flowui.icon.atlas.page");
		page.image = storage_->createImage(storage::ImageDesc{
			.width = page.width, .height = page.height,
			.format = storage::PixelFormat::Rgba8Srgb,
			.usage = storage::ImageUsage::Sampled | storage::ImageUsage::TransferDestination,
			.sharing = storage::ResourceSharing::AppShared, .debugName = name,
		});
		page.view = storage_->createImageView(page.image, storage::ImageViewDesc{
			.format = storage::PixelFormat::Rgba8Srgb, .debugName = name,
		});
		std::vector<std::byte> zeroes(static_cast<size_t>(page.width) * page.height * 4u, std::byte{0});
		zeroBlob = storage_->createBlob(zeroes, name);
		(void)storage_->enqueueUpload(storage::UploadRequest{
			.destination = storage::UploadDestination::Image, .source = zeroBlob,
			.byteCount = zeroes.size(), .destinationImage = page.image,
			.imageRegion = storage::ImageRegion{.width = page.width, .height = page.height},
			.releaseSourceWhenComplete = true,
		});
		storage_->flushUploads();
		zeroBlob = {};
	} catch (...) {
		if (zeroBlob) storage_->releaseBlob(zeroBlob);
		if (page.view) storage_->releaseImageView(page.view);
		if (page.image) storage_->releaseImage(page.image);
		throw;
	}

	return page;
}

void IconManager::destroyAtlasPage(AtlasPage& page) {
	if (storage_ && page.view) storage_->releaseImageView(page.view);
	if (storage_ && page.image) storage_->releaseImage(page.image);
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
	const uint32_t neededWidth = contentWidth + pad * 2u;
	const uint32_t neededHeight = contentHeight + pad * 2u;

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
	if (!raster.rgbaPixels || raster.width == 0u || raster.height == 0u || raster.strideBytes < raster.width * 4u) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconPublishAtlas));
	}
	if ((raster.strideBytes % 4u) != 0u) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconPublishAtlas));
	}
	if (contentRect.x + raster.width > page.width || contentRect.y + raster.height > page.height) {
		detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::IconPublishAtlas));
	}

	const size_t tightRowBytes = static_cast<size_t>(raster.width) * 4u;
	std::vector<std::byte> tight(tightRowBytes * raster.height);
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevExternalMemoryScope tightMemory(
		devMemoryRecorder_, devSystems::memory_sources::kIconTightUpload.id, tight.size());
#endif
	for (uint32_t row = 0; row < raster.height; ++row) {
		std::memcpy(tight.data() + row * tightRowBytes,
			raster.rgbaPixels + static_cast<size_t>(row) * raster.strideBytes, tightRowBytes);
	}
	storage::BlobHandle blob{};
	try {
		const storage::StringId name = storage_->intern("flowui.icon.atlas.region");
		blob = storage_->createBlob(tight, name);
		(void)storage_->enqueueUpload(storage::UploadRequest{
			.destination = storage::UploadDestination::Image, .source = blob,
			.byteCount = tight.size(), .destinationImage = page.image,
			.imageRegion = storage::ImageRegion{
				.x = contentRect.x, .y = contentRect.y,
				.width = raster.width, .height = raster.height,
			},
			.releaseSourceWhenComplete = true,
		});
		storage_->flushUploads();
		blob = {};
	} catch (...) {
		if (blob) storage_->releaseBlob(blob);
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
	storage_->releaseAnonymousTexture(victim.texture);
	controller_->retiredRegions.push_back({victim.texture, victim.pageIndex, victim.paddedRect});
	controller_->variantsByKeyAndSize.erase(victimIt);
	storage_->noteManagerMutation(InvalidWindowId);
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
	uint32_t requestedHeight) {
	const VariantKey requestKey = makeVariantKey(nameKey, requestedWidth, requestedHeight);
	if (VariantEntry* cached = findBestCachedVariant(nameKey, requestKey.requestedWidth, requestKey.requestedHeight)) {
		return *cached;
	}

	TransientRasterResult raster = rasterizeForAtlas(nameKey, requestKey.requestedWidth, requestKey.requestedHeight);
	AtlasAllocation allocation{};
	if (!tryAllocateAtlasRegion(raster.width, raster.height, allocation)) {
		throw FlowUiException(makeError(ErrorCode::IconAtlasCapacityExceeded, ErrorSite::IconPublishAtlas));
	}
	if (allocation.pageIndex >= controller_->atlasPages.size()) {
		detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::IconPublishAtlas));
	}

	AtlasPage& page = controller_->atlasPages[allocation.pageIndex];
	uploadRasterToAtlasPage(page, raster, allocation.contentRect);

	VariantEntry entry{};
	entry.key = requestKey;
	entry.pageIndex = allocation.pageIndex;
	entry.paddedRect = allocation.paddedRect;
	entry.contentRect = allocation.contentRect;
	entry.sourceWidth = raster.width;
	entry.sourceHeight = raster.height;
	entry.lastUsedFrame = controller_->frameCounter;
	entry.referencedThisFrame = true;
	recalcAtlasUvs(entry, page);
	try {
		entry.texture = storage_->createAnonymousTexture(storage::TextureViewDesc{
			.imageView = page.view, .sampler = controller_->atlasSampler,
			.uv0x = entry.uv0x, .uv0y = entry.uv0y, .uv1x = entry.uv1x, .uv1y = entry.uv1y,
			.sourceWidth = static_cast<int32_t>(entry.sourceWidth),
			.sourceHeight = static_cast<int32_t>(entry.sourceHeight),
		});
	} catch (...) {
		releasePageRegion(page, allocation.paddedRect);
		throw;
	}

	auto [it, inserted] = controller_->variantsByKeyAndSize.emplace(entry.key, std::move(entry));
	if (!inserted) {
		storage_->releaseAnonymousTexture(entry.texture);
		releasePageRegion(page, allocation.paddedRect);
		return it->second;
	}

	page.lastUsedFrame = controller_->frameCounter;
	storage_->noteManagerMutation(InvalidWindowId);
	return it->second;
}

void IconManager::prepareFrameTextures(
	Clay_RenderCommandArray& renderCommands,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY)
{
	if (!storage_ || !controller_) {
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconPublishAtlas));
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
			const storage::ResourceKey diagnosticKey = iconKey(
				*storage_, ResourceKey{.name = *requestedKey});
			if (storage_->markDiagnosticOnce(diagnosticKey, IconGenerationDiagnostic)) {
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
		if (!storage_->textureRetirementComplete(it->texture)) {
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

void IconManager::init(
	storage::IStorageSystem& storageSystem,
	const IconManagerConfig& config,
	IconGenerationFailurePolicy generationPolicy,
	CacheCapacityPolicy capacityPolicy) {
	destroy();
	generationPolicy_ = generationPolicy;
	capacityPolicy_ = capacityPolicy;
	const storage::StringId name = storageSystem.intern("flowui.icon.cache");
	const storage::ResourceKey key{storage::ResourceDomain::Icon, name, InvalidWindowId};
	const storage::ManagerRecordHandle handle = manager_storage::createState<manager_storage::IconCacheController>(
		storageSystem, key, storage::ResourceKind::IconVariant, name,
		std::ref(storageSystem), std::cref(config));
	storage_ = &storageSystem;
	controllerHandle_ = handle.packed();
	controller_ = manager_storage::state<manager_storage::IconCacheController>(
		storage_, handle, storage::ResourceKind::IconVariant);
	if (!controller_) {
		destroy();
		throw FlowUiException(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::IconManagerInitialize));
	}
	try {
		controller_->atlasPages.push_back(createAtlasPage(0u));
	} catch (...) {
		destroy();
		throw;
	}
}

Result<bool> IconManager::registerSvg(std::string_view key, std::string_view svgSource) {
	return registerSvg(ResourceKey{.name = key}, svgSource);
}

Result<bool> IconManager::registerSvg(ResourceKey key, std::string_view svgSource) {
	if (!storage_ || !controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconRegisterSource));
	storage::ResourceKey normalized{};
	try {
		normalized = iconKey(*storage_, key);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	if (svgSource.empty()) {
		return unexpectedError(makeError(ErrorCode::IconSourceInvalid, ErrorSite::IconRegisterSource));
	}
	if (svgSource.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
		return unexpectedError(makeError(ErrorCode::IconSourceInvalid, ErrorSite::IconRegisterSource));
	}

	const std::string keyString(key.name);
	if (controller_->documentsByKey.find(keyString) != controller_->documentsByKey.end()) {
		return false;
	}

	char* ownedSource = static_cast<char*>(std::malloc(svgSource.size() + 1u));
	if (!ownedSource) {
		throw std::bad_alloc{};
	}

	std::memcpy(ownedSource, svgSource.data(), svgSource.size());
	ownedSource[svgSource.size()] = '\0';

	plutosvg_document_t* document = plutosvg_document_load_from_data(
		ownedSource,
		static_cast<int>(svgSource.size()),
		-1.0f,
		-1.0f,
		std::free,
		ownedSource);
	if (!document) {
		std::free(ownedSource);
		return unexpectedError(makeError(ErrorCode::IconSourceInvalid, ErrorSite::IconRegisterSource));
	}

	storage::BlobHandle source{};
	try {
		source = storage_->createBlob(
			std::as_bytes(std::span(svgSource.data(), svgSource.size())), normalized.name);
	} catch (const std::bad_alloc&) {
		plutosvg_document_destroy(document);
		throw;
	} catch (const FlowUiException& exception) {
		plutosvg_document_destroy(document);
		return unexpectedError(exception.error());
	} catch (...) {
		plutosvg_document_destroy(document);
		return unexpectedError(makeError(ErrorCode::ResourceCreationFailed, ErrorSite::IconRegisterSource));
	}

	DocumentRecord record{};
	record.source = source;
	record.document = document;
	record.intrinsicWidth = std::max(0.0f, plutosvg_document_get_width(document));
	record.intrinsicHeight = std::max(0.0f, plutosvg_document_get_height(document));

	bool inserted = false;
	try {
		inserted = controller_->documentsByKey.emplace(keyString, record).second;
	} catch (const std::bad_alloc&) {
		plutosvg_document_destroy(document);
		storage_->releaseBlob(source);
		throw;
	} catch (...) {
		plutosvg_document_destroy(document);
		storage_->releaseBlob(source);
		return unexpectedError(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::IconRegisterSource));
	}
	if (!inserted) {
		plutosvg_document_destroy(document);
		storage_->releaseBlob(source);
		return false;
	}
	storage_->noteManagerMutation(InvalidWindowId);
	return true;
}

Result<bool> IconManager::registerFromFile(std::string_view key, std::string_view filePath) {
	return registerFromFile(ResourceKey{.name = key}, filePath);
}

Result<bool> IconManager::registerFromFile(ResourceKey key, std::string_view filePath) {
	if (!storage_ || !controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconRegisterSource));
	try {
		(void)iconKey(*storage_, key);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	if (filePath.empty()) return unexpectedError(makeError(ErrorCode::AssetPathEmpty, ErrorSite::IconRegisterSource));

	const std::filesystem::path path(filePath);
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
}

Result<bool> IconManager::remove(std::string_view key) { return remove(ResourceKey{.name = key}); }

Result<bool> IconManager::remove(ResourceKey key) {
	if (!storage_ || !controller_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconRemove));
	storage::ResourceKey normalized{};
	try {
		normalized = iconKey(*storage_, key);
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
	const auto requestIdIt = controller_->requestTextureByKey.find(keyString);
	if (requestIdIt != controller_->requestTextureByKey.end()) {
		(void)storage_->removeTexture(normalized);
		controller_->requestedKeyByTexture.erase(requestIdIt->second.packed());
		controller_->requestTextureByKey.erase(requestIdIt);
	}

	for (auto variantIt = controller_->variantsByKeyAndSize.begin(); variantIt != controller_->variantsByKeyAndSize.end();) {
		VariantEntry& variant = variantIt->second;
		if (variant.key.nameKey != keyString) {
			++variantIt;
			continue;
		}

		storage_->releaseAnonymousTexture(variant.texture);
		controller_->retiredRegions.push_back({variant.texture, variant.pageIndex, variant.paddedRect});
		variantIt = controller_->variantsByKeyAndSize.erase(variantIt);
	}

	plutosvg_document_destroy(it->second.document);
	storage_->releaseBlob(it->second.source);
	controller_->documentsByKey.erase(it);
	storage_->noteManagerMutation(InvalidWindowId);
	return true;
}

bool IconManager::contains(std::string_view key) const {
	return contains(ResourceKey{.name = key});
}

bool IconManager::contains(ResourceKey key) const {
	if (!storage_ || !controller_) return false;
	(void)iconKey(*storage_, key);
	return controller_->documentsByKey.find(std::string(key.name)) != controller_->documentsByKey.end();
}

TextureRef IconManager::textureRef(std::string_view key) { return textureRef(ResourceKey{.name = key}); }

TextureRef IconManager::textureRef(ResourceKey key) {
	if (!storage_ || !controller_) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconLookup));
	const storage::ResourceKey normalized = iconKey(*storage_, key);
	const std::string_view stableKey = storage_->string(normalized.name);
	if (controller_->atlasPages.empty() || !controller_->atlasPages.front().view || !controller_->atlasSampler) {
		detail::terminateForFatalError(makeError(ErrorCode::RendererNativeResourceInvalid, ErrorSite::IconLookup));
	}

	const std::string keyString(key.name);
	const auto documentIt = controller_->documentsByKey.find(keyString);
	if (documentIt == controller_->documentsByKey.end()) {
		if (generationPolicy_ == IconGenerationFailurePolicy::FailRequest) {
			throw FlowUiException(makeError(ErrorCode::IconNotFound, ErrorSite::IconLookup));
		}
		if (storage_->markDiagnosticOnce(normalized, MissingIconDiagnostic)) {
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
			.sourceDomain = ResourceDomain::Icon,
			.sourceKey = stableKey,
			.skipIfUnavailable = generationPolicy_ == IconGenerationFailurePolicy::SkipVisual,
		};
	}

	auto requestIdIt = controller_->requestTextureByKey.find(keyString);
	if (requestIdIt == controller_->requestTextureByKey.end()) {
		bool inserted = false;
		const AtlasPage& page = controller_->atlasPages.front();
		const TextureHandle requestTexture = storage_->publishTexture(normalized, storage::TextureViewDesc{
			.imageView = page.view, .sampler = controller_->atlasSampler,
			.sourceWidth = static_cast<int32_t>(std::max(1.0f, std::round(documentIt->second.intrinsicWidth))),
			.sourceHeight = static_cast<int32_t>(std::max(1.0f, std::round(documentIt->second.intrinsicHeight))),
		}, &inserted);
		if (!inserted) {
			detail::terminateForFatalError(makeError(ErrorCode::IconKeyCollision, ErrorSite::IconLookup));
		}
		requestIdIt = controller_->requestTextureByKey.emplace(keyString, requestTexture).first;
		controller_->requestedKeyByTexture.emplace(requestTexture.packed(), keyString);
		storage_->noteManagerMutation(InvalidWindowId);
	}

	TextureRef texture{};
	texture.sourceDomain = ResourceDomain::Icon;
	texture.sourceKey = stableKey;
	texture.handle = requestIdIt->second;
	texture.sourceWidth = static_cast<int32_t>(std::max(1.0f, std::round(documentIt->second.intrinsicWidth)));
	texture.sourceHeight = static_cast<int32_t>(std::max(1.0f, std::round(documentIt->second.intrinsicHeight)));
	return texture;
}

IconManager::TransientRasterResult IconManager::rasterizeForAtlas(std::string_view key, uint32_t requestedWidth, uint32_t requestedHeight) const
{
	if (!storage_ || !controller_) {
		throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::IconRasterize));
	}
	if (key.empty()) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
	}

	const auto recordIt = controller_->documentsByKey.find(std::string(key));
	if (recordIt == controller_->documentsByKey.end() || !recordIt->second.document) {
		throw FlowUiException(makeError(ErrorCode::IconNotFound, ErrorSite::IconRasterize));
	}

	const uint32_t targetWidth = std::max<uint32_t>(1u, requestedWidth);
	const uint32_t targetHeight = std::max<uint32_t>(1u, requestedHeight);

	double intrinsicWidth = static_cast<double>(recordIt->second.intrinsicWidth);
	double intrinsicHeight = static_cast<double>(recordIt->second.intrinsicHeight);
	if (intrinsicWidth <= 0.0 || intrinsicHeight <= 0.0) {
		plutovg_rect_t extents{};
		if (plutosvg_document_extents(recordIt->second.document, nullptr, &extents) && extents.w > 0.0f && extents.h > 0.0f) {
			intrinsicWidth = static_cast<double>(extents.w);
			intrinsicHeight = static_cast<double>(extents.h);
		}
	}
	if (intrinsicWidth <= 0.0 || intrinsicHeight <= 0.0) {
		intrinsicWidth = static_cast<double>(targetWidth);
		intrinsicHeight = static_cast<double>(targetHeight);
	}

	const double scaleX = static_cast<double>(targetWidth) / intrinsicWidth;
	const double scaleY = static_cast<double>(targetHeight) / intrinsicHeight;
	const double scale = std::max(0.0, std::min(scaleX, scaleY));
	if (!(scale > 0.0)) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
	}

	uint32_t rasterWidth = static_cast<uint32_t>(std::llround(intrinsicWidth * scale));
	uint32_t rasterHeight = static_cast<uint32_t>(std::llround(intrinsicHeight * scale));
	rasterWidth = std::max<uint32_t>(1u, std::min<uint32_t>(targetWidth, rasterWidth));
	rasterHeight = std::max<uint32_t>(1u, std::min<uint32_t>(targetHeight, rasterHeight));

	SurfaceOwner owner(plutosvg_document_render_to_surface(
		recordIt->second.document,
		nullptr,
		static_cast<int>(rasterWidth),
		static_cast<int>(rasterHeight),
		nullptr,
		nullptr,
		nullptr));
	if (!owner.surface) {
		throw FlowUiException(makeError(ErrorCode::IconRasterizationFailed, ErrorSite::IconRasterize));
	}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevExternalMemoryScope rasterMemory(
		devMemoryRecorder_, devSystems::memory_sources::kIconRaster.id,
		static_cast<uint64_t>(rasterWidth) * rasterHeight * 4u);
#endif

	const int surfaceWidth = plutovg_surface_get_width(owner.surface);
	const int surfaceHeight = plutovg_surface_get_height(owner.surface);
	const int surfaceStride = plutovg_surface_get_stride(owner.surface);
	unsigned char* surfaceData = plutovg_surface_get_data(owner.surface);
	if (!surfaceData || surfaceWidth <= 0 || surfaceHeight <= 0 || surfaceStride <= 0) {
		throw FlowUiException(makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
	}

	convertArgbPremultipliedToRgbaStraight(
		surfaceData,
		static_cast<uint32_t>(surfaceWidth),
		static_cast<uint32_t>(surfaceHeight),
		static_cast<uint32_t>(surfaceStride));

	TransientRasterResult result{};
	result.owner = std::move(owner);
	result.rgbaPixels = surfaceData;
	result.width = static_cast<uint32_t>(surfaceWidth);
	result.height = static_cast<uint32_t>(surfaceHeight);
	result.strideBytes = static_cast<uint32_t>(surfaceStride);
	result.requestedWidth = targetWidth;
	result.requestedHeight = targetHeight;
	return result;
}

void IconManager::destroy() noexcept {
	if (storage_) {
		try {
			const storage::StringId name = storage_->intern("flowui.icon.cache");
			(void)storage_->removeManagerRecord(
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
