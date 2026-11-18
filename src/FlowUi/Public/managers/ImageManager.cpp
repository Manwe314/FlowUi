#include "FlowUi/Public/managers/ImageManager.hpp"
#include "FlowUi/Private/storageSystem/StorageBinding.hpp"
#include "FlowUi/Public/Resources.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

#include <stb_image.h>

#include "FlowUi/Private/storageSystem/ResourceKeyNormalization.hpp"
#include "FlowUi/Private/storageSystem/StorageSystemSelection.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

namespace FlowUi {
namespace storage = detail::storage;
namespace managerStorage = detail::managerStorage;

namespace {

constexpr uint32_t MissingImageDiagnostic = 1u;

storage::ResourceKey imageKey(storage::ActiveStorageSystem& storageSystem, ResourceKey key) {
	return managerStorage::normalizeResourceKey(
		storageSystem,
		key,
		ResourceDomain::Image,
		managerStorage::ResourceScope::AppShared);
}

struct CandidateImage {
	storage::ActiveStorageSystem* storageSystem = nullptr;
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

void ImageManager::init(storage::StorageBinding& storageBinding,
						MissingVisualPolicy missingPolicy) {
	storage_ = &storageBinding;
	missingPolicy_ = missingPolicy;
}

void ImageManager::destroy() noexcept {
#if FLOW_UI_DEV_MODE
	devImages_.clear();
	++devRevision_;
#endif
	storage_ = nullptr;
	missingPolicy_ = MissingVisualPolicy::UseFallbackTexture;
}

Result<bool> ImageManager::registerImage(ResourceKey key, std::string_view filePath) noexcept try {
	return register_image_file(key, detail::requireApiValue(path_from_utf8(filePath)));
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ImageRegister));
}

Result<bool> ImageManager::register_image_file(ResourceKey key, const std::filesystem::path& path) noexcept try {
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ImageRegister));
	storage::ResourceKey normalized{};
	try {
		normalized = imageKey(storage_->system(), key);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	if (path.empty()) return unexpectedError(makeError(ErrorCode::AssetPathEmpty, ErrorSite::ImageLoad));
	std::error_code pathError;
	if (!std::filesystem::is_regular_file(path, pathError)) {
		return unexpectedError(makeError(
			pathError ? ErrorCode::AssetOpenFailed : ErrorCode::AssetNotFound,
			ErrorSite::ImageLoad));
	}

	int width = 0;
	int height = 0;
	int channels = 0;
	const auto encoded = read_file_bytes(path);
	if (!encoded) return unexpectedError(encoded.error());
	if (encoded->size() > static_cast<size_t>(std::numeric_limits<int>::max()))
		return unexpectedError(makeError(ErrorCode::AssetReadFailed, ErrorSite::ImageLoad));
	std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decodedOwner(
		stbi_load_from_memory(encoded->data(), static_cast<int>(encoded->size()), &width, &height, &channels, STBI_rgb_alpha),
		&stbi_image_free);
	stbi_uc* decoded = decodedOwner.get();
	if (!decoded || width <= 0 || height <= 0) {
		return unexpectedError(makeError(ErrorCode::ImageDecodeFailed, ErrorSite::ImageDecode));
	}

	const uint64_t pixelCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
	if (pixelCount > std::numeric_limits<size_t>::max() / 4u) {
		return unexpectedError(makeError(ErrorCode::ImageSizeOverflow, ErrorSite::ImageDecode));
	}
	const size_t byteCount = static_cast<size_t>(pixelCount * 4u);
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevExternalMemoryScope decodeMemory(
		devMemoryRecorder_, devSystems::memory_sources::kImageDecode.id, byteCount);
#endif
	const bool inserted = !static_cast<bool>(storage_->system().findTexture(normalized));

	CandidateImage candidate{};
	candidate.storageSystem = &storage_->system();
	try {
		const storage::StringId debugName = storage_->system().intern(key.name);
		candidate.pixels = storage_->system().createBlob(
			std::span<const std::byte>(reinterpret_cast<const std::byte*>(decoded), byteCount),
			debugName);
		decodedOwner.reset();
		decoded = nullptr;

		candidate.image = storage_->system().createImage(storage::ImageDesc{
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height),
			.format = storage::PixelFormat::Rgba8Srgb,
			.usage = storage::ImageUsage::Sampled | storage::ImageUsage::TransferDestination,
			.memory = storage::MemoryPreference::DeviceLocal,
			.sharing = storage::ResourceSharing::AppShared,
			.access = storage::AccessMode::ReadOnly,
			.debugName = debugName,
		});
		candidate.view = storage_->system().createImageView(
			candidate.image, storage::ImageViewDesc{
								 .type = storage::ImageType::Image2D,
								 .format = storage::PixelFormat::Rgba8Srgb,
								 .debugName = debugName,
							 });
		candidate.sampler =
			storage_->system().acquireSampler(storage::SamplerDesc{.debugName = debugName});
		(void)storage_->system().enqueueUpload(storage::UploadRequest{
			.byteCount = byteCount,
			.imageRegion =
				storage::ImageRegion{
					.width = static_cast<uint32_t>(width),
					.height = static_cast<uint32_t>(height),
				},
			.source = candidate.pixels,
			.destinationImage = candidate.image,
			.destination = storage::UploadDestination::Image,
			.finalState = storage::ResourceState::Ready,
			.releaseSourceWhenComplete = true,
		});
		candidate.uploadQueued = true;
		storage_->system().flushUploads();
		candidate.uploadFlushed = true;

		const TextureHandle texture =
			storage_->system().publishTexture(normalized, storage::TextureViewDesc{
															  .imageView = candidate.view,
															  .sampler = candidate.sampler,
															  .sourceWidth = width,
															  .sourceHeight = height,
														  });
#if FLOW_UI_DEV_MODE
		try {
			auto existing = std::find_if(devImages_.begin(), devImages_.end(),
				[key](const DevImageRecord& record) { return record.key == key.name; });
			DevImageRecord record{
				.key = std::string(key.name),
				.sourcePath = detail::requireApiValue(path_to_utf8(path)),
				.texture = texture,
			};
			if (existing == devImages_.end()) devImages_.push_back(std::move(record));
			else *existing = std::move(record);
		} catch (...) {}
		++devRevision_;
#endif
		storage_->system().clearDiagnosticMark(normalized, MissingImageDiagnostic);
		return inserted;
	} catch (const std::bad_alloc&) {
		throw;
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::ImagePublicationFailed, ErrorSite::ImagePublish));
	}
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ImageRegister));
}

Result<bool> ImageManager::removeImage(ResourceKey key) noexcept try {
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ImageRemove));
	try {
		const bool removed = storage_->system().removeTexture(imageKey(storage_->system(), key));
#if FLOW_UI_DEV_MODE
		if (removed) {
			std::erase_if(devImages_, [key](const DevImageRecord& record) {
				return record.key == key.name;
			});
			++devRevision_;
		}
#endif
		return removed;
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ImageRemove));
}

#if FLOW_UI_DEV_MODE
Result<bool> ImageManager::visitDevImages(void* userData, DevImageVisitor visitor) const noexcept try {
	if (!visitor) return false;
	for (const DevImageRecord& record : devImages_) {
		if (!visitor(userData, DevImageView{
			.key = record.key,
			.sourcePath = record.sourcePath,
			.texture = record.texture,
		})) return false;
	}
	return true;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ImageLookup));
}
#endif

Result<bool> ImageManager::contains(ResourceKey key) const noexcept try {
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ImageLookup));
	return static_cast<bool>(storage_->system().findTexture(imageKey(storage_->system(), key)));
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ImageLookup));
}

Result<TextureRef> ImageManager::getTexture(ResourceKey key) const noexcept try {
	TextureRef result{};
	if (!storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ImageLookup));
	const storage::ResourceKey normalized = imageKey(storage_->system(), key);
	result.sourceDomain = ResourceDomain::Image;
	result.sourceKey = storage_->system().string(normalized.name);
	result.handle = storage_->system().findTexture(normalized);
	if (!result.handle) {
		result.skipIfUnavailable = missingPolicy_ == MissingVisualPolicy::SkipVisual;
		if (storage_->system().markDiagnosticOnce(normalized, MissingImageDiagnostic)) {
			detail::reportErrorEvent(ErrorEventView{
				.error = makeError(
					ErrorCode::ResourceNotFound, ErrorSite::ImageLookup,
					normalized.name),
				.kind = ErrorEventKind::Resolved,
				.resolution = missingPolicy_ == MissingVisualPolicy::SkipVisual
					? ErrorResolution::Skipped
					: ErrorResolution::UsedFallback,
			});
		}
		return result;
	}
	const storage::TextureMetadata metadata = storage_->system().textureMetadata(result.handle);
	result.sourceWidth = metadata.sourceWidth;
	result.sourceHeight = metadata.sourceHeight;
	return result;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ImageLookup));
}

} // namespace FlowUi
