#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "internal/StorageSystem/StorageTypes.hpp"

namespace FlowUi::detail::storage {

class IStorageSystem {
public:
	static constexpr uint32_t CurrentInterfaceVersion = 1u;

	virtual ~IStorageSystem() = default;

	virtual void initialize(const StorageConfig& config) = 0;
	virtual void shutdown() noexcept = 0;
	[[nodiscard]] virtual uint32_t interfaceVersion() const noexcept = 0;
	[[nodiscard]] virtual uint64_t capabilities() const noexcept = 0;

	virtual void registerWindow(WindowId id, const WindowStorageDesc& desc) = 0;
	virtual void unregisterWindow(WindowId id, SubmissionSerial lastUse) = 0;
	[[nodiscard]] virtual FrameToken beginFrame(WindowId id, const FrameStorageDesc& desc) = 0;
	virtual void sealFrame(const FrameToken& frame) = 0;
	virtual void cancelFrame(const FrameToken& frame) noexcept = 0;

	[[nodiscard]] virtual MemoryBlock allocatePersistent(
		size_t bytes,
		size_t alignment,
		MemoryClass memoryClass,
		StringId debugName) = 0;
	virtual void releasePersistent(MemoryBlock block) noexcept = 0;
	[[nodiscard]] virtual ArenaView frameArena(const FrameToken& frame, MemoryClass memoryClass) = 0;
	[[nodiscard]] virtual ArenaView workerArena(const FrameToken& frame, uint32_t workerIndex) = 0;

	[[nodiscard]] virtual StringId intern(std::string_view value) = 0;
	[[nodiscard]] virtual std::string_view string(StringId id) const noexcept = 0;
	[[nodiscard]] virtual BlobHandle createBlob(std::span<const std::byte> bytes, StringId debugName) = 0;
	[[nodiscard]] virtual std::span<const std::byte> readBlob(BlobHandle handle) const noexcept = 0;
	virtual void releaseBlob(BlobHandle handle, SubmissionSerial lastUse = 0) = 0;

	[[nodiscard]] virtual BufferHandle createBuffer(const BufferDesc& desc) = 0;
	[[nodiscard]] virtual ImageHandle createImage(const ImageDesc& desc) = 0;
	[[nodiscard]] virtual ImageViewHandle createImageView(ImageHandle image, const ImageViewDesc& desc) = 0;
	[[nodiscard]] virtual SamplerHandle acquireSampler(const SamplerDesc& desc) = 0;
	virtual void releaseBuffer(BufferHandle buffer, SubmissionSerial lastUse = 0) = 0;
	virtual void releaseImage(ImageHandle image, SubmissionSerial lastUse = 0) = 0;
	virtual void releaseImageView(ImageViewHandle view, SubmissionSerial lastUse = 0) = 0;
	virtual void releaseSampler(SamplerHandle sampler, SubmissionSerial lastUse = 0) = 0;

	[[nodiscard]] virtual TextureHandle publishTexture(
		ResourceKey key,
		const TextureViewDesc& desc,
		bool* inserted = nullptr) = 0;
	[[nodiscard]] virtual TextureHandle replaceTexture(ResourceKey key, const TextureViewDesc& desc) = 0;
	virtual bool removeTexture(ResourceKey key, SubmissionSerial lastUse = 0) = 0;
	[[nodiscard]] virtual TextureHandle findTexture(ResourceKey key) const noexcept = 0;
	[[nodiscard]] virtual TextureMetadata textureMetadata(TextureHandle texture) const noexcept = 0;

	virtual void prepareTextureBindings(const FrameToken& frame, std::span<const TextureHandle> textures) = 0;
	[[nodiscard]] virtual ResolvedTextureBinding resolveTexture(
		const FrameToken& frame,
		TextureHandle texture) = 0;
	virtual void trackUse(const FrameToken& frame, BufferHandle buffer) = 0;
	virtual void trackUse(const FrameToken& frame, ImageHandle image) = 0;
	virtual void invalidateWindowBindings(WindowId id, TextureHandle texture) = 0;
	[[nodiscard]] virtual StorageReadView readView(const FrameToken& frame) const = 0;
	[[nodiscard]] virtual WindowBindingView windowBindingView(const FrameToken& frame) const = 0;
	[[nodiscard]] virtual WindowStorageSnapshot windowSnapshot(WindowId id) const = 0;

	[[nodiscard]] virtual UploadTicket enqueueUpload(const UploadRequest& request) = 0;
	[[nodiscard]] virtual ResourceState uploadState(UploadTicket ticket) const noexcept = 0;
	virtual void flushUploads() = 0;
	[[nodiscard]] virtual SubmissionToken noteSubmission(WindowId id, uint32_t frameSlot) = 0;
	virtual void noteCompleted(SubmissionToken submission) = 0;
	[[nodiscard]] virtual SubmissionSerial completedSerial() const noexcept = 0;

	virtual void retire(const RetirementRequest& request) = 0;
	virtual void collect() = 0;
	virtual void trim(uint64_t targetBytes) = 0;

	[[nodiscard]] virtual StorageStats stats() const = 0;
	[[nodiscard]] virtual ResourceStats resourceStats(ResourceKind kind) const = 0;
	[[nodiscard]] virtual bool validateHandle(
		ResourceKind kind,
		uint32_t index,
		uint32_t generation) const noexcept = 0;
	virtual void setBudget(uint64_t cpuBytes, uint64_t gpuBytes) = 0;

	[[nodiscard]] virtual NativeBufferView nativeBuffer(BufferHandle buffer) const noexcept = 0;
	[[nodiscard]] virtual NativeImageView nativeImage(ImageHandle image) const noexcept = 0;
};

} // namespace FlowUi::detail::storage
