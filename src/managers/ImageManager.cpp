#include "managers/ImageManager.hpp"

#include <cstdio>
#include <filesystem>
#include <limits>
#include <stdexcept>

#include <stb_image.h>

#include "internal/ManagerStorage/ResourceKeyNormalization.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"

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

void ImageManager::init(storage::IStorageSystem& storageSystem) {
	storage_ = &storageSystem;
}

void ImageManager::destroy() noexcept {
	storage_ = nullptr;
}

bool ImageManager::registerImage(ResourceKey key, std::string_view filePath) {
	if (!storage_) throw std::runtime_error("ImageManager is not initialized.");
	const storage::ResourceKey normalized = imageKey(*storage_, key);
	if (filePath.empty()) throw std::invalid_argument("Image file path must not be empty.");

	const std::filesystem::path path(filePath);
	if (!std::filesystem::is_regular_file(path)) {
		throw std::runtime_error("Image file does not exist: " + path.string());
	}

	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* decoded = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (!decoded || width <= 0 || height <= 0) {
		const std::string reason = stbi_failure_reason() ? stbi_failure_reason() : "unknown decode error";
		if (decoded) stbi_image_free(decoded);
		throw std::runtime_error("Failed to decode image: " + path.string() + " (" + reason + ")");
	}

	const uint64_t pixelCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
	if (pixelCount > std::numeric_limits<size_t>::max() / 4u) {
		stbi_image_free(decoded);
		throw std::overflow_error("Decoded image byte size exceeds the host address space.");
	}
	const size_t byteCount = static_cast<size_t>(pixelCount * 4u);
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
	} catch (...) {
		if (decoded) stbi_image_free(decoded);
		throw;
	}
}

bool ImageManager::removeImage(ResourceKey key) {
	if (!storage_) throw std::runtime_error("ImageManager is not initialized.");
	return storage_->removeTexture(imageKey(*storage_, key));
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
		if (storage_->markDiagnosticOnce(normalized, MissingImageDiagnostic)) {
			std::fprintf(
				stderr,
				"[FlowUi] Warning: texture key '%.*s' was not found, using the fallback texture.\n",
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
