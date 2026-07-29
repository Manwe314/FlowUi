#pragma once

#include <memory>

#include "internal/StorageSystem/IStorageSystem.hpp"

struct VulkanContext;

namespace FlowUi::detail::storage {

class FlowStorageSystem final : public IStorageSystem {
public:
	explicit FlowStorageSystem(VulkanContext& vulkanContext);
	~FlowStorageSystem() override;

	FlowStorageSystem(const FlowStorageSystem&) = delete;
	FlowStorageSystem& operator=(const FlowStorageSystem&) = delete;
	FlowStorageSystem(FlowStorageSystem&&) = delete;
	FlowStorageSystem& operator=(FlowStorageSystem&&) = delete;

	void initialize(const StorageConfig& config) override;
	void shutdown() noexcept override;
	[[nodiscard]] uint32_t interfaceVersion() const noexcept override;
	[[nodiscard]] uint64_t capabilities() const noexcept override;

	void registerWindow(WindowId id, const WindowStorageDesc& desc) override;
	void unregisterWindow(WindowId id, SubmissionSerial lastUse) override;
	[[nodiscard]] FrameToken beginFrame(WindowId id, const FrameStorageDesc& desc) override;
	void sealFrame(const FrameToken& frame) override;
	void cancelFrame(const FrameToken& frame) noexcept override;

	[[nodiscard]] MemoryBlock allocatePersistent(
		size_t bytes,
		size_t alignment,
		MemoryClass memoryClass,
		StringId debugName) override;
	void releasePersistent(MemoryBlock block) noexcept override;
	[[nodiscard]] ArenaView frameArena(const FrameToken& frame, MemoryClass memoryClass) override;
	[[nodiscard]] ArenaView workerArena(const FrameToken& frame, uint32_t workerIndex) override;

	[[nodiscard]] StringId intern(std::string_view value) override;
	[[nodiscard]] std::string_view string(StringId id) const noexcept override;
	[[nodiscard]] BlobHandle createBlob(std::span<const std::byte> bytes, StringId debugName) override;
	[[nodiscard]] std::span<const std::byte> readBlob(BlobHandle handle) const noexcept override;
	void releaseBlob(BlobHandle handle, SubmissionSerial lastUse) override;

	[[nodiscard]] BufferHandle createBuffer(const BufferDesc& desc) override;
	[[nodiscard]] ImageHandle createImage(const ImageDesc& desc) override;
	[[nodiscard]] ImageViewHandle createImageView(ImageHandle image, const ImageViewDesc& desc) override;
	[[nodiscard]] SamplerHandle acquireSampler(const SamplerDesc& desc) override;
	void releaseBuffer(BufferHandle buffer, SubmissionSerial lastUse) override;
	void releaseImage(ImageHandle image, SubmissionSerial lastUse) override;
	void releaseImageView(ImageViewHandle view, SubmissionSerial lastUse) override;
	void releaseSampler(SamplerHandle sampler, SubmissionSerial lastUse) override;

	[[nodiscard]] TextureHandle publishTexture(
		ResourceKey key,
		const TextureViewDesc& desc,
		bool* inserted) override;
	[[nodiscard]] TextureHandle replaceTexture(ResourceKey key, const TextureViewDesc& desc) override;
	bool removeTexture(ResourceKey key, SubmissionSerial lastUse) override;
	[[nodiscard]] TextureHandle findTexture(ResourceKey key) const noexcept override;
	[[nodiscard]] TextureMetadata textureMetadata(TextureHandle texture) const noexcept override;

	void prepareTextureBindings(const FrameToken& frame, std::span<const TextureHandle> textures) override;
	[[nodiscard]] ResolvedTextureBinding resolveTexture(const FrameToken& frame, TextureHandle texture) override;
	void trackUse(const FrameToken& frame, BufferHandle buffer) override;
	void trackUse(const FrameToken& frame, ImageHandle image) override;
	void invalidateWindowBindings(WindowId id, TextureHandle texture) override;
	[[nodiscard]] StorageReadView readView(const FrameToken& frame) const override;
	[[nodiscard]] WindowBindingView windowBindingView(const FrameToken& frame) const override;
	[[nodiscard]] WindowStorageSnapshot windowSnapshot(WindowId id) const override;

	[[nodiscard]] UploadTicket enqueueUpload(const UploadRequest& request) override;
	[[nodiscard]] ResourceState uploadState(UploadTicket ticket) const noexcept override;
	void flushUploads() override;
	[[nodiscard]] SubmissionToken noteSubmission(WindowId id, uint32_t frameSlot) override;
	void noteCompleted(SubmissionToken submission) override;
	[[nodiscard]] SubmissionSerial completedSerial() const noexcept override;

	void retire(const RetirementRequest& request) override;
	void collect() override;
	void trim(uint64_t targetBytes) override;

	[[nodiscard]] StorageStats stats() const override;
	[[nodiscard]] ResourceStats resourceStats(ResourceKind kind) const override;
	[[nodiscard]] bool validateHandle(ResourceKind kind, uint32_t index, uint32_t generation) const noexcept override;
	void setBudget(uint64_t cpuBytes, uint64_t gpuBytes) override;

	[[nodiscard]] NativeBufferView nativeBuffer(BufferHandle buffer) const noexcept override;
	[[nodiscard]] NativeImageView nativeImage(ImageHandle image) const noexcept override;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace FlowUi::detail::storage
