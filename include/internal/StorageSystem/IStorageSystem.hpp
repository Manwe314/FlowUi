#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "internal/StorageSystem/StorageTypes.hpp"

namespace FlowUi::detail::storage {

class IStorageSystem {
public:
	static constexpr uint32_t CurrentInterfaceVersion = 5u;

	virtual ~IStorageSystem() = default;

	virtual void initialize(const StorageConfig& config) = 0;
	virtual void shutdown() noexcept = 0;
	[[nodiscard]] virtual uint32_t interfaceVersion() const noexcept = 0;
	[[nodiscard]] virtual uint64_t capabilities() const noexcept = 0;

	virtual void registerWindow(WindowId id, const WindowStorageDesc& desc) = 0;
	virtual void unregisterWindow(WindowId id, SubmissionSerial lastUse) = 0;
	[[nodiscard]] virtual FrameToken beginFrame(WindowId id, const FrameStorageDesc& desc) = 0;
	[[nodiscard]] virtual FrameReadLease sealFrame(const FrameToken& frame) = 0;
	virtual void cancelFrame(const FrameToken& frame) noexcept = 0;

	[[nodiscard]] virtual MemoryBlock allocatePersistent(
		size_t bytes,
		size_t alignment,
		MemoryClass memoryClass,
		StringId debugName) = 0;
	[[nodiscard]] virtual MemoryBlock allocatePersistent(
		size_t bytes,
		size_t alignment,
		const AllocationTag& tag) = 0;
	virtual void releasePersistent(MemoryBlock block) noexcept = 0;
	[[nodiscard]] virtual ArenaView frameArena(const FrameToken& frame, MemoryClass memoryClass) = 0;
	[[nodiscard]] virtual ArenaView workerArena(const FrameToken& frame, uint32_t workerIndex) = 0;

	[[nodiscard]] virtual BufferWriteView beginBufferWrite(
		const FrameToken& frame,
		BufferHandle buffer,
		uint64_t destinationOffset,
		uint64_t bytes,
		BufferWriteMode mode = BufferWriteMode::Default) = 0;
	virtual void commitBufferWrite(
		const FrameToken& frame,
		const BufferWriteView& write,
		uint64_t bytesWritten) = 0;
	virtual void writeBuffer(
		const FrameToken& frame,
		BufferHandle buffer,
		uint64_t destinationOffset,
		std::span<const std::byte> bytes) = 0;

	[[nodiscard]] virtual StringId intern(std::string_view value) = 0;
	[[nodiscard]] virtual std::string_view string(StringId id) const noexcept = 0;
	virtual bool markDiagnosticOnce(ResourceKey key, uint32_t diagnosticCode) = 0;
	virtual void clearDiagnosticMark(ResourceKey key, uint32_t diagnosticCode) = 0;

	[[nodiscard]] virtual ManagerRecordHandle createManagerRecord(const ManagerRecordDesc& desc) = 0;
	[[nodiscard]] virtual ManagerRecordHandle findManagerRecord(
		ResourceKey key, ResourceKind kind) const noexcept = 0;
	[[nodiscard]] virtual void* managerRecordData(
		ManagerRecordHandle handle, ResourceKind kind) noexcept = 0;
	[[nodiscard]] virtual const void* managerRecordData(
		ManagerRecordHandle handle, ResourceKind kind) const noexcept = 0;
	virtual bool removeManagerRecord(ResourceKey key, ResourceKind kind) = 0;
	virtual void releaseWindowManagerRecords(WindowId window) noexcept = 0;
	virtual void noteManagerMutation(WindowId window) = 0;
	[[nodiscard]] virtual ManagerFrameView managerFrameView(const FrameToken& frame) const noexcept = 0;
	[[nodiscard]] virtual uint64_t managerSharedRevision() const noexcept = 0;
	[[nodiscard]] virtual uint64_t managerWindowRevision(WindowId window) const noexcept = 0;
	virtual void setManagerFailureCountdown(uint32_t checkpoints) noexcept = 0;
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
	[[nodiscard]] virtual TextureHandle createAnonymousTexture(const TextureViewDesc& desc) = 0;
	virtual void releaseAnonymousTexture(TextureHandle texture, SubmissionSerial lastUse = 0) = 0;
	[[nodiscard]] virtual TextureHandle replaceTexture(ResourceKey key, const TextureViewDesc& desc) = 0;
	virtual bool removeTexture(ResourceKey key, SubmissionSerial lastUse = 0) = 0;
	[[nodiscard]] virtual TextureHandle findTexture(ResourceKey key) const noexcept = 0;
	[[nodiscard]] virtual TextureMetadata textureMetadata(TextureHandle texture) const noexcept = 0;
	[[nodiscard]] virtual bool textureRetirementComplete(TextureHandle texture) const noexcept = 0;
	virtual void setFallbackTexture(TextureHandle texture) = 0;

	[[nodiscard]] virtual PreparedTextureBindings prepareTextureBindings(
		const FrameToken& frame,
		std::span<const TextureHandle> textures) = 0;
	virtual void acknowledgeTextureBindings(
		const FrameToken& frame,
		std::span<const DescriptorWriteRecord> appliedBindings) = 0;
	virtual void resetTextureBindings(WindowId id, uint32_t frameSlot) = 0;
	[[nodiscard]] virtual ResolvedTextureBinding resolveTexture(
		const FrameToken& frame,
		TextureHandle texture) = 0;
	virtual void trackUse(const FrameToken& frame, BufferHandle buffer) = 0;
	virtual void trackUse(const FrameToken& frame, ImageHandle image) = 0;
	virtual void trackUses(const FrameToken& frame, std::span<const ResourceUse> resources) = 0;
	virtual void invalidateWindowBindings(WindowId id, TextureHandle texture) = 0;
	[[nodiscard]] virtual StorageReadView readView(const FrameReadLease& lease) const = 0;
	[[nodiscard]] virtual WindowBindingView windowBindingView(const FrameReadLease& lease) const = 0;
	[[nodiscard]] virtual WindowStorageSnapshot windowSnapshot(WindowId id) const = 0;

	[[nodiscard]] virtual UploadTicket enqueueUpload(const UploadRequest& request) = 0;
	[[nodiscard]] virtual ResourceState uploadState(UploadTicket ticket) const noexcept = 0;
	virtual void flushUploads() = 0;
	[[nodiscard]] virtual SubmissionToken noteSubmission(const FrameReadLease& lease) = 0;
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

	[[nodiscard]] virtual NativePublishResult<RendererLayoutHandle> publishRendererLayout(
		const RendererLayoutKey& key,
		const NativeRendererLayout& native,
		StringId debugName = 0) = 0;
	[[nodiscard]] virtual RendererLayoutHandle acquireRendererLayout(const RendererLayoutKey& key) = 0;
	[[nodiscard]] virtual NativePublishResult<RendererPipelineBundleHandle> publishRendererPipelineBundle(
		const RendererPipelineKey& key,
		const NativeRendererPipelineBundle& native,
		StringId debugName = 0) = 0;
	[[nodiscard]] virtual RendererPipelineBundleHandle acquireRendererPipelineBundle(
		const RendererPipelineKey& key) = 0;
	[[nodiscard]] virtual WindowDescriptorBundleHandle adoptWindowDescriptorBundle(
		const WindowDescriptorBundleDesc& desc,
		const NativeWindowDescriptorBundle& native) = 0;
	[[nodiscard]] virtual NativeRendererLayout nativeRendererLayout(RendererLayoutHandle layout) const noexcept = 0;
	[[nodiscard]] virtual NativeRendererPipelineBundle nativeRendererPipelineBundle(
		RendererPipelineBundleHandle bundle) const noexcept = 0;
	[[nodiscard]] virtual NativeWindowDescriptorView nativeWindowDescriptorBundle(
		WindowDescriptorBundleHandle bundle) const noexcept = 0;
	virtual void releaseRendererLayout(RendererLayoutHandle layout, SubmissionSerial lastUse = 0) = 0;
	virtual void releaseRendererPipelineBundle(
		RendererPipelineBundleHandle bundle,
		SubmissionSerial lastUse = 0) = 0;
	virtual void releaseWindowDescriptorBundle(
		WindowDescriptorBundleHandle bundle,
		SubmissionSerial lastUse = 0) = 0;

	[[nodiscard]] virtual NativeBufferView nativeBuffer(BufferHandle buffer) const noexcept = 0;
	[[nodiscard]] virtual NativeImageView nativeImage(ImageHandle image) const noexcept = 0;
	[[nodiscard]] virtual NativeImageViewInfo nativeImageView(ImageViewHandle view) const noexcept = 0;
	[[nodiscard]] virtual NativeSamplerInfo nativeSampler(SamplerHandle sampler) const noexcept = 0;
};

} // namespace FlowUi::detail::storage
