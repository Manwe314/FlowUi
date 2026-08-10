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
	[[nodiscard]] FrameReadLease sealFrame(const FrameToken& frame) override;
	void cancelFrame(const FrameToken& frame) noexcept override;

	[[nodiscard]] MemoryBlock allocatePersistent(
		size_t bytes,
		size_t alignment,
		MemoryClass memoryClass,
		StringId debugName) override;
	[[nodiscard]] MemoryBlock allocatePersistent(
		size_t bytes,
		size_t alignment,
		const AllocationTag& tag) override;
	void releasePersistent(MemoryBlock block) noexcept override;
	[[nodiscard]] ArenaView frameArena(const FrameToken& frame, MemoryClass memoryClass) override;
	[[nodiscard]] ArenaView workerArena(const FrameToken& frame, uint32_t workerIndex) override;
	[[nodiscard]] BufferWriteView beginBufferWrite(
		const FrameToken& frame,
		BufferHandle buffer,
		uint64_t destinationOffset,
		uint64_t bytes,
		BufferWriteMode mode = BufferWriteMode::Default) override;
	void commitBufferWrite(
		const FrameToken& frame,
		const BufferWriteView& write,
		uint64_t bytesWritten) override;
	void writeBuffer(
		const FrameToken& frame,
		BufferHandle buffer,
		uint64_t destinationOffset,
		std::span<const std::byte> bytes) override;

	[[nodiscard]] StringId intern(std::string_view value) override;
	[[nodiscard]] std::string_view string(StringId id) const noexcept override;
	bool markDiagnosticOnce(ResourceKey key, uint32_t diagnosticCode) override;
	void clearDiagnosticMark(ResourceKey key, uint32_t diagnosticCode) override;
	[[nodiscard]] ManagerRecordHandle createManagerRecord(const ManagerRecordDesc& desc) override;
	[[nodiscard]] ManagerRecordHandle findManagerRecord(
		ResourceKey key, ResourceKind kind) const noexcept override;
	[[nodiscard]] void* managerRecordData(
		ManagerRecordHandle handle, ResourceKind kind) noexcept override;
	[[nodiscard]] const void* managerRecordData(
		ManagerRecordHandle handle, ResourceKind kind) const noexcept override;
	bool removeManagerRecord(ResourceKey key, ResourceKind kind) override;
	void releaseWindowManagerRecords(WindowId window) noexcept override;
	[[nodiscard]] PersistentRecordHandle createPersistentRecord(
		const PersistentRecordDesc& desc) override;
	[[nodiscard]] PersistentRecordView persistentRecord(
		PersistentRecordHandle handle, ResourceKind kind) noexcept override;
	[[nodiscard]] ConstPersistentRecordView persistentRecord(
		PersistentRecordHandle handle, ResourceKind kind) const noexcept override;
	bool removePersistentRecord(PersistentRecordHandle handle, ResourceKind kind) noexcept override;
	void releaseWindowPersistentRecords(WindowId window) noexcept override;
	void noteManagerMutation(WindowId window) override;
	[[nodiscard]] ManagerFrameView managerFrameView(const FrameToken& frame) const noexcept override;
	[[nodiscard]] uint64_t managerSharedRevision() const noexcept override;
	[[nodiscard]] uint64_t managerWindowRevision(WindowId window) const noexcept override;
	void setRecordFailureCountdown(uint32_t checkpoints) noexcept override;
	[[nodiscard]] BlobHandle createBlob(std::span<const std::byte> bytes, StringId debugName) override;
	[[nodiscard]] std::span<const std::byte> readBlob(BlobHandle handle) const noexcept override;
	void releaseBlob(BlobHandle handle, SubmissionSerial lastUse = 0) override;

	[[nodiscard]] BufferHandle createBuffer(const BufferDesc& desc) override;
	[[nodiscard]] ImageHandle createImage(const ImageDesc& desc) override;
	[[nodiscard]] ImageViewHandle createImageView(ImageHandle image, const ImageViewDesc& desc) override;
	[[nodiscard]] SamplerHandle acquireSampler(const SamplerDesc& desc) override;
	void releaseBuffer(BufferHandle buffer, SubmissionSerial lastUse = 0) override;
	void releaseImage(ImageHandle image, SubmissionSerial lastUse = 0) override;
	void releaseImageView(ImageViewHandle view, SubmissionSerial lastUse = 0) override;
	void releaseSampler(SamplerHandle sampler, SubmissionSerial lastUse = 0) override;

	[[nodiscard]] TextureHandle publishTexture(
		ResourceKey key,
		const TextureViewDesc& desc,
		bool* inserted = nullptr) override;
	[[nodiscard]] TextureHandle createAnonymousTexture(const TextureViewDesc& desc) override;
	void releaseAnonymousTexture(TextureHandle texture, SubmissionSerial lastUse = 0) override;
	[[nodiscard]] TextureHandle replaceTexture(ResourceKey key, const TextureViewDesc& desc) override;
	bool removeTexture(ResourceKey key, SubmissionSerial lastUse = 0) override;
	[[nodiscard]] TextureHandle findTexture(ResourceKey key) const noexcept override;
	[[nodiscard]] TextureMetadata textureMetadata(TextureHandle texture) const noexcept override;
	[[nodiscard]] bool textureRetirementComplete(TextureHandle texture) const noexcept override;
	void setFallbackTexture(TextureHandle texture) override;

	[[nodiscard]] PreparedTextureBindings prepareTextureBindings(
		const FrameToken& frame,
		std::span<const TextureHandle> textures) override;
	void acknowledgeTextureBindings(
		const FrameToken& frame,
		std::span<const DescriptorWriteRecord> appliedBindings) override;
	void resetTextureBindings(WindowId id, uint32_t frameSlot) override;
	[[nodiscard]] ResolvedTextureBinding resolveTexture(const FrameToken& frame, TextureHandle texture) override;
	void trackUse(const FrameToken& frame, BufferHandle buffer) override;
	void trackUse(const FrameToken& frame, ImageHandle image) override;
	void trackUses(const FrameToken& frame, std::span<const ResourceUse> resources) override;
	void invalidateWindowBindings(WindowId id, TextureHandle texture) override;
	[[nodiscard]] StorageReadView readView(const FrameReadLease& lease) const override;
	[[nodiscard]] WindowBindingView windowBindingView(const FrameReadLease& lease) const override;
	[[nodiscard]] WindowStorageSnapshot windowSnapshot(WindowId id) const override;

	[[nodiscard]] UploadTicket enqueueUpload(const UploadRequest& request) override;
	[[nodiscard]] ResourceState uploadState(UploadTicket ticket) const noexcept override;
	void flushUploads() override;
	[[nodiscard]] SubmissionToken noteSubmission(const FrameReadLease& lease) override;
	void noteCompleted(SubmissionToken submission) override;
	[[nodiscard]] SubmissionSerial completedSerial() const noexcept override;

	void retire(const RetirementRequest& request) override;
	void collect() override;
	void trim(uint64_t targetBytes) override;

	[[nodiscard]] StorageStats stats() const override;
	[[nodiscard]] ResourceStats resourceStats(ResourceKind kind) const override;
	[[nodiscard]] bool validateHandle(ResourceKind kind, uint32_t index, uint32_t generation) const noexcept override;
	void setBudget(uint64_t cpuBytes, uint64_t gpuBytes) override;
	[[nodiscard]] NativePublishResult<RendererLayoutHandle> publishRendererLayout(
		const RendererLayoutKey& key,
		const NativeRendererLayout& native,
		StringId debugName = 0) override;
	[[nodiscard]] RendererLayoutHandle acquireRendererLayout(const RendererLayoutKey& key) override;
	[[nodiscard]] NativePublishResult<RendererPipelineBundleHandle> publishRendererPipelineBundle(
		const RendererPipelineKey& key,
		const NativeRendererPipelineBundle& native,
		StringId debugName = 0) override;
	[[nodiscard]] RendererPipelineBundleHandle acquireRendererPipelineBundle(
		const RendererPipelineKey& key) override;
	[[nodiscard]] WindowDescriptorBundleHandle adoptWindowDescriptorBundle(
		const WindowDescriptorBundleDesc& desc,
		const NativeWindowDescriptorBundle& native) override;
	[[nodiscard]] NativeRendererLayout nativeRendererLayout(RendererLayoutHandle layout) const noexcept override;
	[[nodiscard]] NativeRendererPipelineBundle nativeRendererPipelineBundle(
		RendererPipelineBundleHandle bundle) const noexcept override;
	[[nodiscard]] NativeWindowDescriptorView nativeWindowDescriptorBundle(
		WindowDescriptorBundleHandle bundle) const noexcept override;
	void releaseRendererLayout(RendererLayoutHandle layout, SubmissionSerial lastUse = 0) override;
	void releaseRendererPipelineBundle(
		RendererPipelineBundleHandle bundle,
		SubmissionSerial lastUse = 0) override;
	void releaseWindowDescriptorBundle(
		WindowDescriptorBundleHandle bundle,
		SubmissionSerial lastUse = 0) override;

	[[nodiscard]] NativeBufferView nativeBuffer(BufferHandle buffer) const noexcept override;
	[[nodiscard]] NativeImageView nativeImage(ImageHandle image) const noexcept override;
	[[nodiscard]] NativeImageViewInfo nativeImageView(ImageViewHandle view) const noexcept override;
	[[nodiscard]] NativeSamplerInfo nativeSampler(SamplerHandle sampler) const noexcept override;

private:
	void commitBufferWriteInternal(
		const FrameToken& frame,
		const BufferWriteView& write,
		uint64_t bytesWritten,
		const std::byte* sourceData);

	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace FlowUi::detail::storage
