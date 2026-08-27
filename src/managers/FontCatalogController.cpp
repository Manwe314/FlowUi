#include "internal/ManagerStorage/FontCatalogController.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "managers/FontManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

namespace FlowUi::detail::manager_storage {

namespace storage = detail::storage;

namespace {

uint32_t nextLayerCapacity(uint32_t current, uint32_t required) {
	uint32_t capacity = std::max(current, FontManager::kInitialAtlasLayerCapacity);
	while (capacity < required) {
		if (capacity > std::numeric_limits<uint32_t>::max() - FontManager::kAtlasLayerGrowthStep) {
			throw FlowUiException(makeError(ErrorCode::FontAtlasCapacityExceeded, ErrorSite::FontPublishAtlas));
		}
		capacity += FontManager::kAtlasLayerGrowthStep;
	}
	return capacity;
}

} // namespace

FontCatalogController::FontCatalogController(storage::IStorageSystem& storageSystem, uint32_t atlasSize)
	: storage(&storageSystem), atlasSizeHint(atlasSize) {
	if (atlasSize == 0) throw FlowUiException(makeError(ErrorCode::RendererConfigurationInvalid, ErrorSite::FontManagerInitialize));
	const storage::StringId name = storageSystem.intern("flowui.font.atlas.sampler");
	atlasSampler = storageSystem.acquireSampler(storage::SamplerDesc{
		.minFilter = storage::FilterMode::Linear,
		.magFilter = storage::FilterMode::Linear,
		.addressU = storage::AddressMode::ClampToEdge,
		.addressV = storage::AddressMode::ClampToEdge,
		.addressW = storage::AddressMode::ClampToEdge,
		.debugName = name,
	});
	refreshBorrowedAtlas();
}

FontCatalogController::~FontCatalogController() noexcept {
	if (!storage) return;
#if FLOW_UI_DEV_MODE
	try { if (devAtlasTexture) storage->releaseAnonymousTexture(devAtlasTexture); } catch (...) {}
#endif
	try { if (atlasView) storage->releaseImageView(atlasView); } catch (...) {}
	try { if (atlasImage) storage->releaseImage(atlasImage); } catch (...) {}
	try { if (atlasSampler) storage->releaseSampler(atlasSampler); } catch (...) {}
}

void FontCatalogController::refreshBorrowedAtlas() {
	borrowedAtlas = {};
	borrowedAtlas.width = atlasSizeHint;
	borrowedAtlas.height = atlasSizeHint;
	borrowedAtlas.layersUsed = static_cast<uint32_t>(atlasLayerPixels.size());
	if (atlasImage) {
		const storage::NativeImageView native = storage->nativeImage(atlasImage);
		borrowedAtlas.image = reinterpret_cast<VkImage>(static_cast<uintptr_t>(native.nativeImage));
		borrowedAtlas.view = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(
			storage->nativeImageView(atlasView).nativeImageView));
	}
	if (atlasSampler) {
		borrowedAtlas.sampler = reinterpret_cast<VkSampler>(static_cast<uintptr_t>(
			storage->nativeSampler(atlasSampler).nativeSampler));
	}
}

void FontCatalogController::uploadLayerTransactional(
	uint32_t layer,
	const std::vector<uint8_t>& rgbaPixels) {
	const uint64_t layerBytes64 = static_cast<uint64_t>(atlasSizeHint) * atlasSizeHint * 4u;
	if (layerBytes64 > std::numeric_limits<size_t>::max() || rgbaPixels.size() != layerBytes64) {
		throw FlowUiException(makeError(ErrorCode::AssetPayloadInvalid, ErrorSite::FontPublishAtlas));
	}
	if (layer != atlasLayerPixels.size()) {
		detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::FontPublishAtlas));
	}
	const uint32_t requiredLayers = layer + 1u;
	const uint32_t currentCapacity = borrowedAtlas.layersCapacity;
	if (atlasImage && requiredLayers <= currentCapacity) {
		const storage::StringId name = storage->intern("flowui.font.atlas.layer");
		storage::BlobHandle blob = storage->createBlob(
			std::as_bytes(std::span(rgbaPixels)), name);
		try {
			(void)storage->enqueueUpload(storage::UploadRequest{
				.destination = storage::UploadDestination::Image,
				.source = blob,
				.byteCount = rgbaPixels.size(),
				.destinationImage = atlasImage,
				.imageRegion = storage::ImageRegion{
					.width = atlasSizeHint,
					.height = atlasSizeHint,
					.depth = 1,
					.baseArrayLayer = layer,
					.layerCount = 1,
				},
				.releaseSourceWhenComplete = true,
			});
			storage->flushUploads();
			blob = {};
		} catch (...) {
			if (blob) storage->releaseBlob(blob);
			throw;
		}
		atlasLayerPixels.push_back(rgbaPixels);
		borrowedAtlas.layersUsed = requiredLayers;
		return;
	}

	const uint32_t candidateCapacity = nextLayerCapacity(currentCapacity, requiredLayers);
	if (layerBytes64 > std::numeric_limits<uint64_t>::max() / candidateCapacity) {
		throw FlowUiException(makeError(ErrorCode::ArithmeticOverflow, ErrorSite::FontPublishAtlas));
	}
	const size_t candidateBytes = static_cast<size_t>(layerBytes64 * candidateCapacity);
	std::vector<std::byte> combined(candidateBytes, std::byte{0});
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevExternalMemoryScope combinedMemory(
		devMemoryRecorder, devSystems::memory_sources::kFontAtlasCombine.id, combined.size());
#endif
	for (size_t existing = 0; existing < atlasLayerPixels.size(); ++existing) {
		std::memcpy(combined.data() + existing * static_cast<size_t>(layerBytes64),
			atlasLayerPixels[existing].data(), static_cast<size_t>(layerBytes64));
	}
	std::memcpy(combined.data() + static_cast<size_t>(layer) * static_cast<size_t>(layerBytes64),
		rgbaPixels.data(), static_cast<size_t>(layerBytes64));

	storage::ImageHandle candidateImage{};
	storage::ImageViewHandle candidateView{};
	storage::BlobHandle candidateBlob{};
#if FLOW_UI_DEV_MODE
	TextureHandle candidateDevTexture{};
#endif
	const storage::StringId name = storage->intern("flowui.font.atlas");
	try {
		candidateImage = storage->createImage(storage::ImageDesc{
			.width = atlasSizeHint,
			.height = atlasSizeHint,
			.layers = candidateCapacity,
			.format = storage::PixelFormat::Rgba8Unorm,
			.type = storage::ImageType::Image2DArray,
			.usage = storage::ImageUsage::Sampled | storage::ImageUsage::TransferDestination,
			.sharing = storage::ResourceSharing::AppShared,
			.debugName = name,
		});
		candidateView = storage->createImageView(candidateImage, storage::ImageViewDesc{
			.type = storage::ImageType::Image2DArray,
			.format = storage::PixelFormat::Rgba8Unorm,
			.arrayLayerCount = candidateCapacity,
			.debugName = name,
		});
		candidateBlob = storage->createBlob(combined, name);
		(void)storage->enqueueUpload(storage::UploadRequest{
			.destination = storage::UploadDestination::Image,
			.source = candidateBlob,
			.byteCount = combined.size(),
			.destinationImage = candidateImage,
			.imageRegion = storage::ImageRegion{
				.width = atlasSizeHint,
				.height = atlasSizeHint,
				.depth = 1,
				.layerCount = candidateCapacity,
			},
			.releaseSourceWhenComplete = true,
		});
		storage->flushUploads();
		candidateBlob = {};
#if FLOW_UI_DEV_MODE
		candidateDevTexture = storage->createAnonymousTexture(storage::TextureViewDesc{
			.imageView = candidateView,
			.sampler = atlasSampler,
			.sourceWidth = static_cast<std::int32_t>(atlasSizeHint),
			.sourceHeight = static_cast<std::int32_t>(atlasSizeHint),
		});
#endif
	} catch (...) {
#if FLOW_UI_DEV_MODE
		if (candidateDevTexture) storage->releaseAnonymousTexture(candidateDevTexture);
#endif
		if (candidateBlob) storage->releaseBlob(candidateBlob);
		if (candidateView) storage->releaseImageView(candidateView);
		if (candidateImage) storage->releaseImage(candidateImage);
		throw;
	}

	const storage::ImageViewHandle oldView = atlasView;
	const storage::ImageHandle oldImage = atlasImage;
#if FLOW_UI_DEV_MODE
	const TextureHandle oldDevTexture = devAtlasTexture;
	devAtlasTexture = candidateDevTexture;
#endif
	atlasView = candidateView;
	atlasImage = candidateImage;
	atlasLayerPixels.push_back(rgbaPixels);
	const uint32_t nextRevision = borrowedAtlas.bindingRevision + 1u;
	refreshBorrowedAtlas();
	borrowedAtlas.layersCapacity = candidateCapacity;
	borrowedAtlas.bindingRevision = nextRevision;
#if FLOW_UI_DEV_MODE
	if (oldDevTexture) storage->releaseAnonymousTexture(oldDevTexture);
#endif
	if (oldView) storage->releaseImageView(oldView);
	if (oldImage) storage->releaseImage(oldImage);
}

} // namespace FlowUi::detail::manager_storage
