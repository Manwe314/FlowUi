#include "managers/ImageManager.hpp"

#include <cstdio>
#include <filesystem>
#include <limits>
#include <new>
#include <stdexcept>

#include <stb_image.h>

#include "internal/ManagerStorage/ResourceKeyNormalization.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

namespace FlowUi {
namespace storage = detail::storage;
namespace managerStorage = detail::managerStorage;

namespace {

constexpr uint32_t MissingImageDiagnostic = 1u;

storage::ResourceKey imageKey(storage::IStorageSystem& storageSystem, ResourceKey key) {
	return managerStorage::normalizeResourceKey(
		storageSystem,
		key,
		ResourceDomain::Image,
		managerStorage::ResourceScope::AppShared);
}

struct CandidateImage {
	storage::IStorageSystem* storageSystem = nullptr;
	storage::BlobHandle pixels{};
	storage::ImageHandle image{};
	storage::ImageViewHandle view{};
	storage::SamplerHandle sampler{};
	bool uploadQueued = false;
	bool uploadFlushed = false;

	CandidateImage() = default;
	CandidateImage(const CandidateImage&) = delete;
	CandidateImage& operator=(const CandidateImage&) = delete;

	~CandidateImage() {
		if (!storageSystem) return;
		if (view) storageSystem->releaseImageView(view);
		if (sampler) storageSystem->releaseSampler(sampler);
		if (image) storageSystem->releaseImage(image);
		// A successful upload with releaseSourceWhenComplete consumes both the
		// caller and upload references. Failed/unflushed candidates retain the
		// caller reference and release it here.
		if (pixels && (!uploadFlushed || !uploadQueued)) storageSystem->releaseBlob(pixels);
	}
};

} // namespace

void ImageManager::init(storage::IStorageSystem& storageSystem, MissingVisualPolicy missingPolicy) {
	storage_ = &storageSystem;
	missingPolicy_ = missingPolicy;
}

void ImageManager::destroy() noexcept {
	storage_ = nullptr;
	missingPolicy_ = MissingVisualPolicy::UseFallbackTexture;
}

Result<bool> ImageManager::registerImage(ResourceKey key, std::string_view filePath) {
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized));
	storage::ResourceKey normalized{};
	try {
		normalized = imageKey(*storage_, key);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	if (filePath.empty()) return unexpectedError(makeError(ErrorCode::AssetPathEmpty));

	const std::filesystem::path path(filePath);
	std::error_code pathError;
	if (!std::filesystem::is_regular_file(path, pathError)) {
		return unexpectedError(makeError(
			pathError ? ErrorCode::AssetOpenFailed : ErrorCode::AssetNotFound));
	}

	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* decoded = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (!decoded || width <= 0 || height <= 0) {
		if (decoded) stbi_image_free(decoded);
		return unexpectedError(makeError(ErrorCode::ImageDecodeFailed));
	}

	const uint64_t pixelCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
	if (pixelCount > std::numeric_limits<size_t>::max() / 4u) {
		stbi_image_free(decoded);
		return unexpectedError(makeError(ErrorCode::ImageSizeOverflow));
	}
	const size_t byteCount = static_cast<size_t>(pixelCount * 4u);
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevExternalMemoryScope decodeMemory(
		devMemoryRecorder_, devSystems::memory_sources::kImageDecode.id, byteCount);
#endif
	const bool inserted = !static_cast<bool>(storage_->findTexture(normalized));

	CandidateImage candidate{};
	candidate.storageSystem = storage_;
	try {
		const storage::StringId debugName = storage_->intern(key.name);
		candidate.pixels = storage_->createBlob(
			std::span<const std::byte>(reinterpret_cast<const std::byte*>(decoded), byteCount),
			debugName);
		stbi_image_free(decoded);
		decoded = nullptr;

		candidate.image = storage_->createImage(storage::ImageDesc{
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height),
			.format = storage::PixelFormat::Rgba8Srgb,
			.usage = storage::ImageUsage::Sampled | storage::ImageUsage::TransferDestination,
			.memory = storage::MemoryPreference::DeviceLocal,
			.sharing = storage::ResourceSharing::AppShared,
			.access = storage::AccessMode::ReadOnly,
			.debugName = debugName,
		});
		candidate.view = storage_->createImageView(candidate.image, storage::ImageViewDesc{
			.type = storage::ImageType::Image2D,
			.format = storage::PixelFormat::Rgba8Srgb,
			.debugName = debugName,
		});
		candidate.sampler = storage_->acquireSampler(storage::SamplerDesc{.debugName = debugName});
		(void)storage_->enqueueUpload(storage::UploadRequest{
			.destination = storage::UploadDestination::Image,
			.source = candidate.pixels,
			.byteCount = byteCount,
			.destinationImage = candidate.image,
			.imageRegion = storage::ImageRegion{
				.width = static_cast<uint32_t>(width),
				.height = static_cast<uint32_t>(height),
			},
			.finalState = storage::ResourceState::Ready,
			.releaseSourceWhenComplete = true,
		});
		candidate.uploadQueued = true;
		storage_->flushUploads();
		candidate.uploadFlushed = true;

		(void)storage_->publishTexture(normalized, storage::TextureViewDesc{
			.imageView = candidate.view,
			.sampler = candidate.sampler,
			.sourceWidth = width,
			.sourceHeight = height,
		});
		storage_->clearDiagnosticMark(normalized, MissingImageDiagnostic);
		return inserted;
	} catch (const std::bad_alloc&) {
		if (decoded) stbi_image_free(decoded);
		throw;
	} catch (const FlowUiException& exception) {
		if (decoded) stbi_image_free(decoded);
		return unexpectedError(exception.error());
	} catch (...) {
		if (decoded) stbi_image_free(decoded);
		return unexpectedError(makeError(ErrorCode::ImagePublicationFailed));
	}
}

Result<bool> ImageManager::removeImage(ResourceKey key) {
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized));
	try {
		return storage_->removeTexture(imageKey(*storage_, key));
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
}

bool ImageManager::contains(ResourceKey key) const {
	return storage_ && static_cast<bool>(storage_->findTexture(imageKey(*storage_, key)));
}

TextureRef ImageManager::getTexture(ResourceKey key) const {
	TextureRef result{};
	if (!storage_) return result;
	const storage::ResourceKey normalized = imageKey(*storage_, key);
	result.handle = storage_->findTexture(normalized);
	if (!result.handle) {
		result.skipIfUnavailable = missingPolicy_ == MissingVisualPolicy::SkipVisual;
		if (storage_->markDiagnosticOnce(normalized, MissingImageDiagnostic)) {
			std::fprintf(
				stderr,
				missingPolicy_ == MissingVisualPolicy::SkipVisual
					? "[FlowUi] Warning: texture key '%.*s' was not found, skipping the visual.\n"
					: "[FlowUi] Warning: texture key '%.*s' was not found, using the fallback texture.\n",
				static_cast<int>(key.name.size()),
				key.name.data());
		}
		return result;
	}
	const storage::TextureMetadata metadata = storage_->textureMetadata(result.handle);
	result.sourceWidth = metadata.sourceWidth;
	result.sourceHeight = metadata.sourceHeight;
	return result;
}

} // namespace FlowUi
