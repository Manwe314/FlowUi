#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "internal/StorageSystem/StorageTypes.hpp"

namespace FlowUi::detail::storage {

class IStorageSystem {
public:
	static constexpr uint32_t CurrentInterfaceVersion = 4u;

	virtual ~IStorageSystem() = default;

	virtual void initialize(const StorageConfig& config) = 0;
	// All callers must be quiescent before shutdown; repeated shutdown is safe.
	virtual void shutdown() noexcept = 0;
	[[nodiscard]] virtual uint32_t interfaceVersion() const noexcept = 0;
	[[nodiscard]] virtual uint64_t capabilities() const noexcept = 0;

	virtual void registerWindow(WindowId id, const WindowStorageDesc& desc) = 0;
	virtual void unregisterWindow(WindowId id, SubmissionSerial lastUse) = 0;
	[[nodiscard]] virtual FrameToken beginFrame(WindowId id, const FrameStorageDesc& desc) = 0;
	// Sealing freezes arena allocation and binding mutation, automatically tracks
	// the window's active descriptor generation, and creates the lease consumed
	// exactly once by noteSubmission(), or invalidated by cancelFrame().
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
	// A write view is exclusively owned by one producer until commit returns.
	// Its frame must not be cancelled concurrently; shutdown requires all callers
	// to be quiescent. Different non-overlapping writes may be produced in parallel.
	// Commit copies a HostScratchThenCopy lease at most once, flushes only when
	// the allocation is non-coherent, and adds one generation-safe frame use.
	virtual void commitBufferWrite(
		const FrameToken& frame,
		const BufferWriteView& write,
		uint64_t bytesWritten) = 0;
	// A prebuilt span is copied directly into the mapped allocation exactly once;
	// defaultBufferWriteMode controls producer leases, not this convenience path.
	virtual void writeBuffer(
		const FrameToken& frame,
		BufferHandle buffer,
		uint64_t destinationOffset,
		std::span<const std::byte> bytes) = 0;

	[[nodiscard]] virtual StringId intern(std::string_view value) = 0;
	// Interned strings remain stable until shutdown.
	[[nodiscard]] virtual std::string_view string(StringId id) const noexcept = 0;
	[[nodiscard]] virtual BlobHandle createBlob(std::span<const std::byte> bytes, StringId debugName) = 0;
	// The returned blob span is borrowed while the caller's strong handle remains
	// alive; release/collect must not race readers of the span.
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
	//Transitional: borrowed native texture publication is removed after manager
	// resource stores migrate to storage-owned image/view/sampler handles.
	[[nodiscard]] virtual TextureHandle publishExternalTexture(
		ResourceKey key,
		const ExternalTextureDesc& desc,
		bool* inserted = nullptr) = 0;
	virtual bool removeTexture(ResourceKey key, SubmissionSerial lastUse = 0) = 0;
	[[nodiscard]] virtual TextureHandle findTexture(ResourceKey key) const noexcept = 0;
	[[nodiscard]] virtual TextureMetadata textureMetadata(TextureHandle texture) const noexcept = 0;
	// True only after a removed/replaced generation has passed serial retirement
	// and its table generation is no longer allocated.
	[[nodiscard]] virtual bool textureRetirementComplete(TextureHandle texture) const noexcept = 0;
	// The fallback is a root-shared strong reference and backs descriptor slot 0
	// for invalid, queued, and failed textures in every registered window.
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
	// All published Vulkan handles must have been created from this storage
	// instance's VkDevice using null allocation callbacks. acquire* returns a new
	// strong reference without constructing duplicate native objects. A new layout
	// publication transfers its three native handles. A new pipeline publication
	// transfers only pipelines[]; pipelineLayout is a borrowed identity check owned
	// by key.layout. No caller-owned handle transfers on a duplicate or exception.
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
	// Successful descriptor adoption always transfers pool ownership, copies the
	// set spans, and retires the window's previous active bundle. The returned
	// handle identifies that window-owned reference; release explicitly deactivates
	// it. An exception transfers nothing.
	// Native values are cold-path immutable views. Cache their values while the
	// corresponding strong handle is alive; do not query storage in draw loops.
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

	// Native values are borrowed identities, never ownership transfers. Cache them
	// only while the corresponding strong generation remains alive.
	[[nodiscard]] virtual NativeBufferView nativeBuffer(BufferHandle buffer) const noexcept = 0;
	[[nodiscard]] virtual NativeImageView nativeImage(ImageHandle image) const noexcept = 0;
	[[nodiscard]] virtual NativeImageViewInfo nativeImageView(ImageViewHandle view) const noexcept = 0;
	[[nodiscard]] virtual NativeSamplerInfo nativeSampler(SamplerHandle sampler) const noexcept = 0;
};

} // namespace FlowUi::detail::storage
