#include "FlowUi/Private/resourceService/FontCatalogController.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "FlowUi/Public/managers/FontManager.hpp"
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

#if FLOW_UI_DEV_MODE
void release_dev_atlas_layers(storage::IStorageSystem& storage_system,
							  std::span<const FontManager::DevAtlasLayerView> layers) noexcept {
	for (const auto& layer : layers) {
		try {
			if (layer.texture)
				storage_system.releaseAnonymousTexture(layer.texture);
		} catch (...) {}
		try {
			if (layer.image_view)
				storage_system.releaseImageView(layer.image_view);
		} catch (...) {}
	}
}

[[nodiscard]] Result<FontManager::DevAtlasLayerView>
create_dev_atlas_layer(storage::IStorageSystem& storage_system, storage::ImageHandle image,
					   storage::SamplerHandle sampler, uint32_t layer_index,
					   uint32_t atlas_size) noexcept {
	storage::ImageViewHandle image_view{};
	try {
		image_view =
			storage_system.createImageView(image, storage::ImageViewDesc{
													  .type = storage::ImageType::Image2D,
													  .format = storage::PixelFormat::Rgba8Unorm,
													  .baseArrayLayer = layer_index,
													  .arrayLayerCount = 1,
													  .opaque_alpha = true,
												  });
		const auto texture = storage_system.createAnonymousTexture(storage::TextureViewDesc{
			.imageView = image_view,
			.sampler = sampler,
			.sourceWidth = static_cast<int32_t>(atlas_size),
			.sourceHeight = static_cast<int32_t>(atlas_size),
		});
		return FontManager::DevAtlasLayerView{
			.texture = texture, .image_view = image_view, .layer_index = layer_index};
	} catch (const FlowUiException& exception) {
		try {
			if (image_view)
				storage_system.releaseImageView(image_view);
		} catch (...) {}
		return unexpectedError(exception.error());
	} catch (...) {
		try {
			if (image_view)
				storage_system.releaseImageView(image_view);
		} catch (...) {}
		return unexpectedError(
			makeError(ErrorCode::ResourceCreationFailed, ErrorSite::FontPublishAtlas));
	}
}
#endif

} // namespace

FontCatalogController::FontCatalogController(storage::IStorageSystem& storageSystem, uint32_t atlasSize)
	: storage(&storageSystem), atlasSizeHint(atlasSize) {
	if (atlasSize == 0) throw FlowUiException(makeError(ErrorCode::RendererConfigurationInvalid, ErrorSite::FontManagerInitialize));
	const storage::StringId name = storageSystem.intern("flowui.font.atlas.sampler");
	atlasSampler = storageSystem.acquireSampler(storage::SamplerDesc{
		.debugName = name,
		.minFilter = storage::FilterMode::Linear,
		.magFilter = storage::FilterMode::Linear,
		.addressU = storage::AddressMode::ClampToEdge,
		.addressV = storage::AddressMode::ClampToEdge,
		.addressW = storage::AddressMode::ClampToEdge,});
	refreshBorrowedAtlas();
}

FontCatalogController::~FontCatalogController() noexcept {
	if (!storage)
		return;
#if FLOW_UI_DEV_MODE
	release_dev_atlas_layers(*storage, dev_atlas_layers);
#endif
	try {
		if (atlasView)
			storage->releaseImageView(atlasView);
	} catch (...) {}
	try {
		if (atlasImage)
			storage->releaseImage(atlasImage);
	} catch (...) {}
	try {
		if (atlasSampler)
			storage->releaseSampler(atlasSampler);
	} catch (...) {}
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

void FontCatalogController::discard_atlas_layers(size_t layer_count) noexcept {
	if (layer_count >= atlasLayerPixels.size())
		return;
#if FLOW_UI_DEV_MODE
	if (layer_count < dev_atlas_layers.size()) {
		release_dev_atlas_layers(*storage, std::span(dev_atlas_layers).subspan(layer_count));
		dev_atlas_layers.resize(layer_count);
	}
#endif
	atlasLayerPixels.resize(layer_count);
	borrowedAtlas.layersUsed = static_cast<uint32_t>(layer_count);
	++catalogRevision;
}

void FontCatalogController::uploadLayerTransactional(uint32_t layer,
													 const std::vector<uint8_t>& rgbaPixels) {
	const uint64_t layerBytes64 = static_cast<uint64_t>(atlasSizeHint) * atlasSizeHint * 4u;
	if (layerBytes64 > std::numeric_limits<size_t>::max() || rgbaPixels.size() != layerBytes64) {
		throw FlowUiException(
			makeError(ErrorCode::AssetPayloadInvalid, ErrorSite::FontPublishAtlas));
	}
	if (layer != atlasLayerPixels.size()) {
		detail::terminateForFatalError(
			makeError(ErrorCode::InternalInvariantBroken, ErrorSite::FontPublishAtlas));
	}
	const uint32_t requiredLayers = layer + 1u;
	const uint32_t currentCapacity = borrowedAtlas.layersCapacity;
	const uint32_t candidateCapacity = nextLayerCapacity(currentCapacity, requiredLayers);
	std::vector<uint8_t> pending_pixels(rgbaPixels);
	atlasLayerPixels.reserve(candidateCapacity);
	if (atlasImage && requiredLayers <= currentCapacity) {
		const storage::StringId name = storage->intern("flowui.font.atlas.layer");
		storage::BlobHandle blob{};
#if FLOW_UI_DEV_MODE
		FontManager::DevAtlasLayerView preview{};
#endif
		try {
#if FLOW_UI_DEV_MODE
			auto created =
				create_dev_atlas_layer(*storage, atlasImage, atlasSampler, layer, atlasSizeHint);
			if (!created)
				throw FlowUiException(created.error());
			preview = *created;
#endif
			blob = storage->createBlob(std::as_bytes(std::span(rgbaPixels)), name);
			(void)storage->enqueueUpload(storage::UploadRequest{
				.byteCount = rgbaPixels.size(),
				.imageRegion =
					storage::ImageRegion{
						.width = atlasSizeHint,
						.height = atlasSizeHint,
						.depth = 1,
						.baseArrayLayer = layer,
						.layerCount = 1,
					},
				.source = blob,
				.destinationImage = atlasImage,
				.destination = storage::UploadDestination::Image,
				.releaseSourceWhenComplete = true,
			});
			storage->flushUploads();
			blob = {};
		} catch (...) {
#if FLOW_UI_DEV_MODE
			release_dev_atlas_layers(*storage, std::span(&preview, 1));
#endif
			if (blob)
				storage->releaseBlob(blob);
			throw;
		}
		atlasLayerPixels.emplace_back(std::move(pending_pixels));
#if FLOW_UI_DEV_MODE
		dev_atlas_layers.emplace_back(preview);
#endif
		borrowedAtlas.layersUsed = requiredLayers;
		return;
	}

	if (layerBytes64 > std::numeric_limits<uint64_t>::max() / candidateCapacity) {
		throw FlowUiException(
			makeError(ErrorCode::ArithmeticOverflow, ErrorSite::FontPublishAtlas));
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
	std::vector<FontManager::DevAtlasLayerView> candidate_preview_layers{};
	candidate_preview_layers.reserve(candidateCapacity);
#endif
	Font::AtlasArrayResource candidate_atlas{};
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
		candidateView =
			storage->createImageView(candidateImage, storage::ImageViewDesc{
														 .type = storage::ImageType::Image2DArray,
														 .format = storage::PixelFormat::Rgba8Unorm,
														 .arrayLayerCount = candidateCapacity,
														 .debugName = name,
													 });
		candidateBlob = storage->createBlob(combined, name);
		(void)storage->enqueueUpload(storage::UploadRequest{
			.byteCount = combined.size(),
			.imageRegion =
				storage::ImageRegion{
					.width = atlasSizeHint,
					.height = atlasSizeHint,
					.depth = 1,
					.layerCount = candidateCapacity,
				},
			.source = candidateBlob,
			.destinationImage = candidateImage,
			.destination = storage::UploadDestination::Image,
			.releaseSourceWhenComplete = true,
		});
		storage->flushUploads();
		candidateBlob = {};
#if FLOW_UI_DEV_MODE
		for (uint32_t layer_index = 0; layer_index < requiredLayers; ++layer_index) {
			auto created = create_dev_atlas_layer(*storage, candidateImage, atlasSampler,
												  layer_index, atlasSizeHint);
			if (!created)
				throw FlowUiException(created.error());
			candidate_preview_layers.emplace_back(*created);
		}
#endif
		candidate_atlas = Font::AtlasArrayResource{
			.image = reinterpret_cast<VkImage>(
				static_cast<uintptr_t>(storage->nativeImage(candidateImage).nativeImage)),
			.view = reinterpret_cast<VkImageView>(
				static_cast<uintptr_t>(storage->nativeImageView(candidateView).nativeImageView)),
			.sampler = borrowedAtlas.sampler,
			.width = atlasSizeHint,
			.height = atlasSizeHint,
			.layersUsed = requiredLayers,
			.layersCapacity = candidateCapacity,
			.bindingRevision = borrowedAtlas.bindingRevision + 1u,
		};
	} catch (...) {
#if FLOW_UI_DEV_MODE
		release_dev_atlas_layers(*storage, candidate_preview_layers);
#endif
		if (candidateBlob)
			storage->releaseBlob(candidateBlob);
		if (candidateView)
			storage->releaseImageView(candidateView);
		if (candidateImage)
			storage->releaseImage(candidateImage);
		throw;
	}

	const storage::ImageViewHandle oldView = atlasView;
	const storage::ImageHandle oldImage = atlasImage;
#if FLOW_UI_DEV_MODE
	const auto old_preview_layers = std::move(dev_atlas_layers);
	dev_atlas_layers = std::move(candidate_preview_layers);
#endif
	atlasView = candidateView;
	atlasImage = candidateImage;
	atlasLayerPixels.emplace_back(std::move(pending_pixels));
	borrowedAtlas = candidate_atlas;
#if FLOW_UI_DEV_MODE
	release_dev_atlas_layers(*storage, old_preview_layers);
#endif
	if (oldView)
		storage->releaseImageView(oldView);
	if (oldImage)
		storage->releaseImage(oldImage);
}

} // namespace FlowUi::detail::manager_storage
