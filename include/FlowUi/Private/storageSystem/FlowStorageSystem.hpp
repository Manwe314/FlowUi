#pragma once

#include <memory>

#include "FlowUi/Private/storageSystem/StorageContractMetadata.hpp"
#include "FlowUi/Private/storageSystem/StorageTypes.hpp"

struct VulkanContext;

namespace FlowUi::detail::storage {

/** Default concrete storage implementation for the Vulkan renderer. */
class FlowStorageSystem final {
public:
	static constexpr uint32_t contractRevision = storageContractRevision;
	static constexpr uint32_t interfaceVersionValue = currentStorageInterfaceVersion;
	static constexpr uint64_t supportedCapabilities = flowStorageSupportedCapabilities;
	static constexpr uint32_t CurrentInterfaceVersion = interfaceVersionValue;

	explicit FlowStorageSystem(VulkanContext& vulkanContext);
	~FlowStorageSystem() noexcept;

	FlowStorageSystem(const FlowStorageSystem&) = delete;
	FlowStorageSystem& operator=(const FlowStorageSystem&) = delete;
	FlowStorageSystem(FlowStorageSystem&&) = delete;
	FlowStorageSystem& operator=(FlowStorageSystem&&) = delete;

	void initialize(const StorageConfig& config);
	void shutdown() noexcept;
	/* Effective startup hint, resolved before storage initialization. */
	[[nodiscard]] uint64_t
	initial_memory_capacity(FlowUi::detail::InitialCapacityTarget target) const noexcept;
	[[nodiscard]] uint32_t interfaceVersion() const noexcept;
	[[nodiscard]] uint64_t capabilities() const noexcept;

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	[[nodiscard]] const ::FlowUi::detail::MemoryDemandControl*
	memory_demand_control() const noexcept;
#endif
	void registerWindow(WindowId id, const WindowStorageDesc& desc);
	void unregisterWindow(WindowId id, SubmissionSerial lastUse);
	[[nodiscard]] FrameToken beginFrame(WindowId id, const FrameStorageDesc& desc);
	[[nodiscard]] FrameReadLease sealFrame(const FrameToken& frame);
	void cancelFrame(const FrameToken& frame) noexcept;

	[[nodiscard]] MemoryBlock allocatePersistent(size_t bytes, size_t alignment,
												 MemoryClass memoryClass, StringId debugName);
	[[nodiscard]] MemoryBlock allocatePersistent(size_t bytes, size_t alignment,
												 const AllocationTag& tag);
	void releasePersistent(MemoryBlock block) noexcept;
	[[nodiscard]] ArenaView frameArena(const FrameToken& frame, MemoryClass memoryClass);
	[[nodiscard]] ArenaView workerArena(const FrameToken& frame, uint32_t workerIndex);
	[[nodiscard]] BufferWriteView beginBufferWrite(const FrameToken& frame, BufferHandle buffer,
												   uint64_t destinationOffset, uint64_t bytes,
												   BufferWriteMode mode = BufferWriteMode::Default);
	void commitBufferWrite(const FrameToken& frame, const BufferWriteView& write,
						   uint64_t bytesWritten);
	void writeBuffer(const FrameToken& frame, BufferHandle buffer, uint64_t destinationOffset,
					 std::span<const std::byte> bytes);

	[[nodiscard]] StringId intern(std::string_view value);
	[[nodiscard]] std::string_view string(StringId id) const noexcept;
	bool markDiagnosticOnce(ResourceKey key, uint32_t diagnosticCode);
	void clearDiagnosticMark(ResourceKey key, uint32_t diagnosticCode);
	[[nodiscard]] ManagerRecordHandle createManagerRecord(const ManagerRecordDesc& desc);
	[[nodiscard]] ManagerRecordHandle findManagerRecord(ResourceKey key,
														ResourceKind kind) const noexcept;
	[[nodiscard]] void* managerRecordData(ManagerRecordHandle handle, ResourceKind kind) noexcept;
	[[nodiscard]] const void* managerRecordData(ManagerRecordHandle handle,
												ResourceKind kind) const noexcept;
	bool removeManagerRecord(ResourceKey key, ResourceKind kind);
	void releaseWindowManagerRecords(WindowId window) noexcept;
	[[nodiscard]] PersistentRecordHandle createPersistentRecord(const PersistentRecordDesc& desc);
	[[nodiscard]] PersistentRecordView persistentRecord(PersistentRecordHandle handle,
														ResourceKind kind) noexcept;
	[[nodiscard]] ConstPersistentRecordView persistentRecord(PersistentRecordHandle handle,
															 ResourceKind kind) const noexcept;
	bool removePersistentRecord(PersistentRecordHandle handle, ResourceKind kind) noexcept;
	void releaseWindowPersistentRecords(WindowId window) noexcept;
	void noteManagerMutation(WindowId window);
	[[nodiscard]] ManagerFrameView managerFrameView(const FrameToken& frame) const noexcept;
	[[nodiscard]] uint64_t managerSharedRevision() const noexcept;
	[[nodiscard]] uint64_t managerWindowRevision(WindowId window) const noexcept;
	void setRecordFailureCountdown(uint32_t checkpoints) noexcept;
	/** Compatibility alias for deterministic record failure injection. */
	void setManagerFailureCountdown(uint32_t checkpoints) noexcept {
		setRecordFailureCountdown(checkpoints);
	}
	[[nodiscard]] BlobHandle createBlob(std::span<const std::byte> bytes, StringId debugName);
	[[nodiscard]] std::span<const std::byte> readBlob(BlobHandle handle) const noexcept;
	void releaseBlob(BlobHandle handle, SubmissionSerial lastUse = 0);

	[[nodiscard]] BufferHandle createBuffer(const BufferDesc& desc);
	[[nodiscard]] ImageHandle createImage(const ImageDesc& desc);
	[[nodiscard]] ImageViewHandle createImageView(ImageHandle image, const ImageViewDesc& desc);
	[[nodiscard]] SamplerHandle acquireSampler(const SamplerDesc& desc);
	void releaseBuffer(BufferHandle buffer, SubmissionSerial lastUse = 0);
	void releaseImage(ImageHandle image, SubmissionSerial lastUse = 0);
	void releaseImageView(ImageViewHandle view, SubmissionSerial lastUse = 0);
	void releaseSampler(SamplerHandle sampler, SubmissionSerial lastUse = 0);

	[[nodiscard]] TextureHandle publishTexture(ResourceKey key, const TextureViewDesc& desc,
											   bool* inserted = nullptr);
	[[nodiscard]] TextureHandle createAnonymousTexture(const TextureViewDesc& desc);
	void releaseAnonymousTexture(TextureHandle texture, SubmissionSerial lastUse = 0);
	[[nodiscard]] TextureHandle replaceTexture(ResourceKey key, const TextureViewDesc& desc);
	bool removeTexture(ResourceKey key, SubmissionSerial lastUse = 0);
	[[nodiscard]] TextureHandle findTexture(ResourceKey key) const noexcept;
	[[nodiscard]] TextureMetadata textureMetadata(TextureHandle texture) const noexcept;
	[[nodiscard]] bool textureRetirementComplete(TextureHandle texture) const noexcept;
	void setFallbackTexture(TextureHandle texture);
#if FLOW_UI_DEV_MODE
	[[nodiscard]] DevTextureMetadata devTextureMetadata(TextureHandle texture) const noexcept;
	[[nodiscard]] bool retainTextureForDevInspection(TextureHandle texture) noexcept;
	void releaseTextureFromDevInspection(TextureHandle texture);
#endif

	[[nodiscard]] PreparedTextureBindings
	prepareTextureBindings(const FrameToken& frame, std::span<const TextureHandle> textures);
	void acknowledgeTextureBindings(const FrameToken& frame,
									std::span<const DescriptorWriteRecord> appliedBindings);
	void resetTextureBindings(WindowId id, uint32_t frameSlot);
	[[nodiscard]] ResolvedTextureBinding resolveTexture(const FrameToken& frame,
														TextureHandle texture);
	void trackUse(const FrameToken& frame, BufferHandle buffer);
	void trackUse(const FrameToken& frame, ImageHandle image);
	void trackUses(const FrameToken& frame, std::span<const ResourceUse> resources);
	void invalidateWindowBindings(WindowId id, TextureHandle texture);
	[[nodiscard]] StorageReadView readView(const FrameReadLease& lease) const;
	[[nodiscard]] WindowBindingView windowBindingView(const FrameReadLease& lease) const;
	[[nodiscard]] WindowStorageSnapshot windowSnapshot(WindowId id) const;

	[[nodiscard]] UploadTicket enqueueUpload(const UploadRequest& request);
	[[nodiscard]] ResourceState uploadState(UploadTicket ticket) const noexcept;
	void flushUploads();
	[[nodiscard]] SubmissionToken noteSubmission(const FrameReadLease& lease);
	void noteCompleted(SubmissionToken submission);
	[[nodiscard]] SubmissionSerial completedSerial() const noexcept;

	void retire(const RetirementRequest& request);
	void collect();
	void trim(uint64_t targetBytes);

	[[nodiscard]] StorageStats stats() const;
	[[nodiscard]] ResourceStats resourceStats(ResourceKind kind) const;
#if FLOW_UI_DEV_MODE
	void appendMemorySnapshot(const StorageMemorySnapshotRequest& request,
							  StorageMemorySnapshot& destination) const;
#endif
	[[nodiscard]] bool validateHandle(ResourceKind kind, uint32_t index,
									  uint32_t generation) const noexcept;
	void setBudget(uint64_t cpuBytes, uint64_t gpuBytes);
	[[nodiscard]] NativePublishResult<RendererLayoutHandle>
	publishRendererLayout(const RendererLayoutKey& key, const NativeRendererLayout& native,
						  StringId debugName = 0);
	[[nodiscard]] RendererLayoutHandle acquireRendererLayout(const RendererLayoutKey& key);
	[[nodiscard]] NativePublishResult<RendererPipelineBundleHandle>
	publishRendererPipelineBundle(const RendererPipelineKey& key,
								  const NativeRendererPipelineBundle& native,
								  StringId debugName = 0);
	[[nodiscard]] RendererPipelineBundleHandle
	acquireRendererPipelineBundle(const RendererPipelineKey& key);
	[[nodiscard]] WindowDescriptorBundleHandle
	adoptWindowDescriptorBundle(const WindowDescriptorBundleDesc& desc,
								const NativeWindowDescriptorBundle& native);
	[[nodiscard]] NativeRendererLayout
	nativeRendererLayout(RendererLayoutHandle layout) const noexcept;
	[[nodiscard]] NativeRendererPipelineBundle
	nativeRendererPipelineBundle(RendererPipelineBundleHandle bundle) const noexcept;
	[[nodiscard]] NativeWindowDescriptorView
	nativeWindowDescriptorBundle(WindowDescriptorBundleHandle bundle) const noexcept;
	void releaseRendererLayout(RendererLayoutHandle layout, SubmissionSerial lastUse = 0);
	void releaseRendererPipelineBundle(RendererPipelineBundleHandle bundle,
									   SubmissionSerial lastUse = 0);
	void releaseWindowDescriptorBundle(WindowDescriptorBundleHandle bundle,
									   SubmissionSerial lastUse = 0);

	[[nodiscard]] NativeBufferView nativeBuffer(BufferHandle buffer) const noexcept;
	[[nodiscard]] NativeImageView nativeImage(ImageHandle image) const noexcept;
	[[nodiscard]] NativeImageViewInfo nativeImageView(ImageViewHandle view) const noexcept;
	[[nodiscard]] NativeSamplerInfo nativeSampler(SamplerHandle sampler) const noexcept;

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
