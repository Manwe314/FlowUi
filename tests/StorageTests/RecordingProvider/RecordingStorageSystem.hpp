#pragma once

#include "FlowUi/Private/storageSystem/FlowStorageSystem.hpp"
#include <atomic>
#include <stdexcept>

namespace FlowUi::test {
using namespace FlowUi::detail::storage;
/** Recording lifecycle hooks implemented in the provider's separate static dependency. */
void noteRecordingStorageConstruction() noexcept;
[[nodiscard]] uint64_t recordingStorageConstructionCount() noexcept;

/** Full nonvirtual recording replacement which delegates the existing storage behavior. */
class RecordingStorageSystem {
public:
	static constexpr uint32_t contractRevision = storageContractRevision;
	static constexpr uint32_t interfaceVersionValue = currentStorageInterfaceVersion;
	static constexpr uint64_t supportedCapabilities = flowStorageSupportedCapabilities;
	struct Calls {
		std::atomic_uint64_t initialize{0};
		std::atomic_uint64_t shutdown{0};
		std::atomic_uint64_t initial_memory_capacity{0};
		std::atomic_uint64_t interfaceVersion{0};
		std::atomic_uint64_t capabilities{0};
		std::atomic_uint64_t memory_demand_control{0};
		std::atomic_uint64_t registerWindow{0};
		std::atomic_uint64_t unregisterWindow{0};
		std::atomic_uint64_t beginFrame{0};
		std::atomic_uint64_t sealFrame{0};
		std::atomic_uint64_t cancelFrame{0};
		std::atomic_uint64_t allocatePersistent{0};
		std::atomic_uint64_t releasePersistent{0};
		std::atomic_uint64_t frameArena{0};
		std::atomic_uint64_t workerArena{0};
		std::atomic_uint64_t beginBufferWrite{0};
		std::atomic_uint64_t commitBufferWrite{0};
		std::atomic_uint64_t writeBuffer{0};
		std::atomic_uint64_t intern{0};
		std::atomic_uint64_t string{0};
		std::atomic_uint64_t markDiagnosticOnce{0};
		std::atomic_uint64_t clearDiagnosticMark{0};
		std::atomic_uint64_t createManagerRecord{0};
		std::atomic_uint64_t findManagerRecord{0};
		std::atomic_uint64_t managerRecordData{0};
		std::atomic_uint64_t removeManagerRecord{0};
		std::atomic_uint64_t releaseWindowManagerRecords{0};
		std::atomic_uint64_t createPersistentRecord{0};
		std::atomic_uint64_t persistentRecord{0};
		std::atomic_uint64_t removePersistentRecord{0};
		std::atomic_uint64_t releaseWindowPersistentRecords{0};
		std::atomic_uint64_t noteManagerMutation{0};
		std::atomic_uint64_t managerFrameView{0};
		std::atomic_uint64_t managerSharedRevision{0};
		std::atomic_uint64_t managerWindowRevision{0};
		std::atomic_uint64_t setRecordFailureCountdown{0};
		std::atomic_uint64_t createBlob{0};
		std::atomic_uint64_t readBlob{0};
		std::atomic_uint64_t releaseBlob{0};
		std::atomic_uint64_t createBuffer{0};
		std::atomic_uint64_t createImage{0};
		std::atomic_uint64_t createImageView{0};
		std::atomic_uint64_t acquireSampler{0};
		std::atomic_uint64_t releaseBuffer{0};
		std::atomic_uint64_t releaseImage{0};
		std::atomic_uint64_t releaseImageView{0};
		std::atomic_uint64_t releaseSampler{0};
		std::atomic_uint64_t publishTexture{0};
		std::atomic_uint64_t createAnonymousTexture{0};
		std::atomic_uint64_t releaseAnonymousTexture{0};
		std::atomic_uint64_t replaceTexture{0};
		std::atomic_uint64_t removeTexture{0};
		std::atomic_uint64_t findTexture{0};
		std::atomic_uint64_t textureMetadata{0};
		std::atomic_uint64_t textureRetirementComplete{0};
		std::atomic_uint64_t setFallbackTexture{0};
		std::atomic_uint64_t devTextureMetadata{0};
		std::atomic_uint64_t retainTextureForDevInspection{0};
		std::atomic_uint64_t releaseTextureFromDevInspection{0};
		std::atomic_uint64_t prepareTextureBindings{0};
		std::atomic_uint64_t acknowledgeTextureBindings{0};
		std::atomic_uint64_t resetTextureBindings{0};
		std::atomic_uint64_t resolveTexture{0};
		std::atomic_uint64_t trackUse{0};
		std::atomic_uint64_t trackUses{0};
		std::atomic_uint64_t invalidateWindowBindings{0};
		std::atomic_uint64_t readView{0};
		std::atomic_uint64_t windowBindingView{0};
		std::atomic_uint64_t windowSnapshot{0};
		std::atomic_uint64_t enqueueUpload{0};
		std::atomic_uint64_t uploadState{0};
		std::atomic_uint64_t flushUploads{0};
		std::atomic_uint64_t noteSubmission{0};
		std::atomic_uint64_t noteCompleted{0};
		std::atomic_uint64_t completedSerial{0};
		std::atomic_uint64_t retire{0};
		std::atomic_uint64_t collect{0};
		std::atomic_uint64_t trim{0};
		std::atomic_uint64_t stats{0};
		std::atomic_uint64_t resourceStats{0};
		std::atomic_uint64_t appendMemorySnapshot{0};
		std::atomic_uint64_t validateHandle{0};
		std::atomic_uint64_t setBudget{0};
		std::atomic_uint64_t publishRendererLayout{0};
		std::atomic_uint64_t acquireRendererLayout{0};
		std::atomic_uint64_t publishRendererPipelineBundle{0};
		std::atomic_uint64_t acquireRendererPipelineBundle{0};
		std::atomic_uint64_t adoptWindowDescriptorBundle{0};
		std::atomic_uint64_t nativeRendererLayout{0};
		std::atomic_uint64_t nativeRendererPipelineBundle{0};
		std::atomic_uint64_t nativeWindowDescriptorBundle{0};
		std::atomic_uint64_t releaseRendererLayout{0};
		std::atomic_uint64_t releaseRendererPipelineBundle{0};
		std::atomic_uint64_t releaseWindowDescriptorBundle{0};
		std::atomic_uint64_t nativeBuffer{0};
		std::atomic_uint64_t nativeImage{0};
		std::atomic_uint64_t nativeImageView{0};
		std::atomic_uint64_t nativeSampler{0};
		std::atomic_uint64_t constructions{0};
		std::atomic_uint64_t destructions{0};
	};
	static Calls calls;
	explicit RecordingStorageSystem(VulkanContext& context) : storage_(context) {
		++calls.constructions;
		noteRecordingStorageConstruction();
	}
	~RecordingStorageSystem() noexcept { ++calls.destructions; }
	RecordingStorageSystem(const RecordingStorageSystem&) = delete;
	RecordingStorageSystem& operator=(const RecordingStorageSystem&) = delete;
	RecordingStorageSystem(RecordingStorageSystem&&) = delete;
	RecordingStorageSystem& operator=(RecordingStorageSystem&&) = delete;
	void initialize(const StorageConfig& config) {
		++calls.initialize;
		if (storage_.interfaceVersion() != interfaceVersionValue ||
			storage_.capabilities() != supportedCapabilities) {
			throw std::logic_error("Storage static and runtime metadata disagree");
		}
		storage_.initialize(config);
	}
	void shutdown() noexcept {
		++calls.shutdown;
		storage_.shutdown();
	}
	[[nodiscard]] uint64_t
	initial_memory_capacity(FlowUi::detail::InitialCapacityTarget target) const noexcept {
		++calls.initial_memory_capacity;
		return storage_.initial_memory_capacity(target);
	}
	[[nodiscard]] uint32_t interfaceVersion() const noexcept {
		++calls.interfaceVersion;
		return storage_.interfaceVersion();
	}
	[[nodiscard]] uint64_t capabilities() const noexcept {
		++calls.capabilities;
		return storage_.capabilities();
	}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	[[nodiscard]] const ::FlowUi::detail::MemoryDemandControl*
	memory_demand_control() const noexcept {
		++calls.memory_demand_control;
		return storage_.memory_demand_control();
	}
#endif
	void registerWindow(WindowId id, const WindowStorageDesc& desc) {
		++calls.registerWindow;
		storage_.registerWindow(id, desc);
	}
	void unregisterWindow(WindowId id, SubmissionSerial lastUse) {
		++calls.unregisterWindow;
		storage_.unregisterWindow(id, lastUse);
	}
	[[nodiscard]] FrameToken beginFrame(WindowId id, const FrameStorageDesc& desc) {
		++calls.beginFrame;
		return storage_.beginFrame(id, desc);
	}
	[[nodiscard]] FrameReadLease sealFrame(const FrameToken& frame) {
		++calls.sealFrame;
		return storage_.sealFrame(frame);
	}
	void cancelFrame(const FrameToken& frame) noexcept {
		++calls.cancelFrame;
		storage_.cancelFrame(frame);
	}
	[[nodiscard]] MemoryBlock allocatePersistent(size_t bytes, size_t alignment,
												 MemoryClass memoryClass, StringId debugName) {
		++calls.allocatePersistent;
		return storage_.allocatePersistent(bytes, alignment, memoryClass, debugName);
	}
	[[nodiscard]] MemoryBlock allocatePersistent(size_t bytes, size_t alignment,
												 const AllocationTag& tag) {
		++calls.allocatePersistent;
		return storage_.allocatePersistent(bytes, alignment, tag);
	}
	void releasePersistent(MemoryBlock block) noexcept {
		++calls.releasePersistent;
		storage_.releasePersistent(block);
	}
	[[nodiscard]] ArenaView frameArena(const FrameToken& frame, MemoryClass memoryClass) {
		++calls.frameArena;
		return storage_.frameArena(frame, memoryClass);
	}
	[[nodiscard]] ArenaView workerArena(const FrameToken& frame, uint32_t workerIndex) {
		++calls.workerArena;
		return storage_.workerArena(frame, workerIndex);
	}
	[[nodiscard]] BufferWriteView
	beginBufferWrite(const FrameToken& frame, BufferHandle buffer, uint64_t destinationOffset,
					 uint64_t bytes, BufferWriteMode mode = BufferWriteMode::Default) {
		++calls.beginBufferWrite;
		return storage_.beginBufferWrite(frame, buffer, destinationOffset, bytes, mode);
	}
	void commitBufferWrite(const FrameToken& frame, const BufferWriteView& write,
						   uint64_t bytesWritten) {
		++calls.commitBufferWrite;
		storage_.commitBufferWrite(frame, write, bytesWritten);
	}
	void writeBuffer(const FrameToken& frame, BufferHandle buffer, uint64_t destinationOffset,
					 std::span<const std::byte> bytes) {
		++calls.writeBuffer;
		storage_.writeBuffer(frame, buffer, destinationOffset, bytes);
	}
	[[nodiscard]] StringId intern(std::string_view value) {
		++calls.intern;
		return storage_.intern(value);
	}
	[[nodiscard]] std::string_view string(StringId id) const noexcept {
		++calls.string;
		return storage_.string(id);
	}
	bool markDiagnosticOnce(FlowUi::detail::storage::ResourceKey key, uint32_t diagnosticCode) {
		++calls.markDiagnosticOnce;
		return storage_.markDiagnosticOnce(key, diagnosticCode);
	}
	void clearDiagnosticMark(FlowUi::detail::storage::ResourceKey key, uint32_t diagnosticCode) {
		++calls.clearDiagnosticMark;
		storage_.clearDiagnosticMark(key, diagnosticCode);
	}
	[[nodiscard]] ManagerRecordHandle createManagerRecord(const ManagerRecordDesc& desc) {
		++calls.createManagerRecord;
		return storage_.createManagerRecord(desc);
	}
	[[nodiscard]] ManagerRecordHandle findManagerRecord(FlowUi::detail::storage::ResourceKey key,
														ResourceKind kind) const noexcept {
		++calls.findManagerRecord;
		return storage_.findManagerRecord(key, kind);
	}
	[[nodiscard]] void* managerRecordData(ManagerRecordHandle handle, ResourceKind kind) noexcept {
		++calls.managerRecordData;
		return storage_.managerRecordData(handle, kind);
	}
	[[nodiscard]] const void* managerRecordData(ManagerRecordHandle handle,
												ResourceKind kind) const noexcept {
		++calls.managerRecordData;
		return storage_.managerRecordData(handle, kind);
	}
	bool removeManagerRecord(FlowUi::detail::storage::ResourceKey key, ResourceKind kind) {
		++calls.removeManagerRecord;
		return storage_.removeManagerRecord(key, kind);
	}
	void releaseWindowManagerRecords(WindowId window) noexcept {
		++calls.releaseWindowManagerRecords;
		storage_.releaseWindowManagerRecords(window);
	}
	[[nodiscard]] PersistentRecordHandle createPersistentRecord(const PersistentRecordDesc& desc) {
		++calls.createPersistentRecord;
		return storage_.createPersistentRecord(desc);
	}
	[[nodiscard]] PersistentRecordView persistentRecord(PersistentRecordHandle handle,
														ResourceKind kind) noexcept {
		++calls.persistentRecord;
		return storage_.persistentRecord(handle, kind);
	}
	[[nodiscard]] ConstPersistentRecordView persistentRecord(PersistentRecordHandle handle,
															 ResourceKind kind) const noexcept {
		++calls.persistentRecord;
		return storage_.persistentRecord(handle, kind);
	}
	bool removePersistentRecord(PersistentRecordHandle handle, ResourceKind kind) noexcept {
		++calls.removePersistentRecord;
		return storage_.removePersistentRecord(handle, kind);
	}
	void releaseWindowPersistentRecords(WindowId window) noexcept {
		++calls.releaseWindowPersistentRecords;
		storage_.releaseWindowPersistentRecords(window);
	}
	void noteManagerMutation(WindowId window) {
		++calls.noteManagerMutation;
		storage_.noteManagerMutation(window);
	}
	[[nodiscard]] ManagerFrameView managerFrameView(const FrameToken& frame) const noexcept {
		++calls.managerFrameView;
		return storage_.managerFrameView(frame);
	}
	[[nodiscard]] uint64_t managerSharedRevision() const noexcept {
		++calls.managerSharedRevision;
		return storage_.managerSharedRevision();
	}
	[[nodiscard]] uint64_t managerWindowRevision(WindowId window) const noexcept {
		++calls.managerWindowRevision;
		return storage_.managerWindowRevision(window);
	}
	void setRecordFailureCountdown(uint32_t checkpoints) noexcept {
		++calls.setRecordFailureCountdown;
		storage_.setRecordFailureCountdown(checkpoints);
	}
	[[nodiscard]] BlobHandle createBlob(std::span<const std::byte> bytes, StringId debugName) {
		++calls.createBlob;
		return storage_.createBlob(bytes, debugName);
	}
	[[nodiscard]] std::span<const std::byte> readBlob(BlobHandle handle) const noexcept {
		++calls.readBlob;
		return storage_.readBlob(handle);
	}
	void releaseBlob(BlobHandle handle, SubmissionSerial lastUse = 0) {
		++calls.releaseBlob;
		storage_.releaseBlob(handle, lastUse);
	}
	[[nodiscard]] BufferHandle createBuffer(const BufferDesc& desc) {
		++calls.createBuffer;
		return storage_.createBuffer(desc);
	}
	[[nodiscard]] ImageHandle createImage(const ImageDesc& desc) {
		++calls.createImage;
		return storage_.createImage(desc);
	}
	[[nodiscard]] ImageViewHandle createImageView(ImageHandle image, const ImageViewDesc& desc) {
		++calls.createImageView;
		return storage_.createImageView(image, desc);
	}
	[[nodiscard]] SamplerHandle acquireSampler(const SamplerDesc& desc) {
		++calls.acquireSampler;
		return storage_.acquireSampler(desc);
	}
	void releaseBuffer(BufferHandle buffer, SubmissionSerial lastUse = 0) {
		++calls.releaseBuffer;
		storage_.releaseBuffer(buffer, lastUse);
	}
	void releaseImage(ImageHandle image, SubmissionSerial lastUse = 0) {
		++calls.releaseImage;
		storage_.releaseImage(image, lastUse);
	}
	void releaseImageView(ImageViewHandle view, SubmissionSerial lastUse = 0) {
		++calls.releaseImageView;
		storage_.releaseImageView(view, lastUse);
	}
	void releaseSampler(SamplerHandle sampler, SubmissionSerial lastUse = 0) {
		++calls.releaseSampler;
		storage_.releaseSampler(sampler, lastUse);
	}
	[[nodiscard]] TextureHandle publishTexture(FlowUi::detail::storage::ResourceKey key,
											   const TextureViewDesc& desc,
											   bool* inserted = nullptr) {
		++calls.publishTexture;
		return storage_.publishTexture(key, desc, inserted);
	}
	[[nodiscard]] TextureHandle createAnonymousTexture(const TextureViewDesc& desc) {
		++calls.createAnonymousTexture;
		return storage_.createAnonymousTexture(desc);
	}
	void releaseAnonymousTexture(TextureHandle texture, SubmissionSerial lastUse = 0) {
		++calls.releaseAnonymousTexture;
		storage_.releaseAnonymousTexture(texture, lastUse);
	}
	[[nodiscard]] TextureHandle replaceTexture(FlowUi::detail::storage::ResourceKey key,
											   const TextureViewDesc& desc) {
		++calls.replaceTexture;
		return storage_.replaceTexture(key, desc);
	}
	bool removeTexture(FlowUi::detail::storage::ResourceKey key, SubmissionSerial lastUse = 0) {
		++calls.removeTexture;
		return storage_.removeTexture(key, lastUse);
	}
	[[nodiscard]] TextureHandle
	findTexture(FlowUi::detail::storage::ResourceKey key) const noexcept {
		++calls.findTexture;
		return storage_.findTexture(key);
	}
	[[nodiscard]] TextureMetadata textureMetadata(TextureHandle texture) const noexcept {
		++calls.textureMetadata;
		return storage_.textureMetadata(texture);
	}
	[[nodiscard]] bool textureRetirementComplete(TextureHandle texture) const noexcept {
		++calls.textureRetirementComplete;
		return storage_.textureRetirementComplete(texture);
	}
	void setFallbackTexture(TextureHandle texture) {
		++calls.setFallbackTexture;
		storage_.setFallbackTexture(texture);
	}
#if FLOW_UI_DEV_MODE
	[[nodiscard]] DevTextureMetadata devTextureMetadata(TextureHandle texture) const noexcept {
		++calls.devTextureMetadata;
		return storage_.devTextureMetadata(texture);
	}
	[[nodiscard]] bool retainTextureForDevInspection(TextureHandle texture) noexcept {
		++calls.retainTextureForDevInspection;
		return storage_.retainTextureForDevInspection(texture);
	}
	void releaseTextureFromDevInspection(TextureHandle texture) {
		++calls.releaseTextureFromDevInspection;
		storage_.releaseTextureFromDevInspection(texture);
	}
#endif
	[[nodiscard]] PreparedTextureBindings
	prepareTextureBindings(const FrameToken& frame, std::span<const TextureHandle> textures) {
		++calls.prepareTextureBindings;
		return storage_.prepareTextureBindings(frame, textures);
	}
	void acknowledgeTextureBindings(const FrameToken& frame,
									std::span<const DescriptorWriteRecord> appliedBindings) {
		++calls.acknowledgeTextureBindings;
		storage_.acknowledgeTextureBindings(frame, appliedBindings);
	}
	void resetTextureBindings(WindowId id, uint32_t frameSlot) {
		++calls.resetTextureBindings;
		storage_.resetTextureBindings(id, frameSlot);
	}
	[[nodiscard]] ResolvedTextureBinding resolveTexture(const FrameToken& frame,
														TextureHandle texture) {
		++calls.resolveTexture;
		return storage_.resolveTexture(frame, texture);
	}
	void trackUse(const FrameToken& frame, BufferHandle buffer) {
		++calls.trackUse;
		storage_.trackUse(frame, buffer);
	}
	void trackUse(const FrameToken& frame, ImageHandle image) {
		++calls.trackUse;
		storage_.trackUse(frame, image);
	}
	void trackUses(const FrameToken& frame, std::span<const ResourceUse> resources) {
		++calls.trackUses;
		storage_.trackUses(frame, resources);
	}
	void invalidateWindowBindings(WindowId id, TextureHandle texture) {
		++calls.invalidateWindowBindings;
		storage_.invalidateWindowBindings(id, texture);
	}
	[[nodiscard]] StorageReadView readView(const FrameReadLease& lease) const {
		++calls.readView;
		return storage_.readView(lease);
	}
	[[nodiscard]] WindowBindingView windowBindingView(const FrameReadLease& lease) const {
		++calls.windowBindingView;
		return storage_.windowBindingView(lease);
	}
	[[nodiscard]] WindowStorageSnapshot windowSnapshot(WindowId id) const {
		++calls.windowSnapshot;
		return storage_.windowSnapshot(id);
	}
	[[nodiscard]] UploadTicket enqueueUpload(const UploadRequest& request) {
		++calls.enqueueUpload;
		return storage_.enqueueUpload(request);
	}
	[[nodiscard]] ResourceState uploadState(UploadTicket ticket) const noexcept {
		++calls.uploadState;
		return storage_.uploadState(ticket);
	}
	void flushUploads() {
		++calls.flushUploads;
		storage_.flushUploads();
	}
	[[nodiscard]] SubmissionToken noteSubmission(const FrameReadLease& lease) {
		++calls.noteSubmission;
		return storage_.noteSubmission(lease);
	}
	void noteCompleted(SubmissionToken submission) {
		++calls.noteCompleted;
		storage_.noteCompleted(submission);
	}
	[[nodiscard]] SubmissionSerial completedSerial() const noexcept {
		++calls.completedSerial;
		return storage_.completedSerial();
	}
	void retire(const RetirementRequest& request) {
		++calls.retire;
		storage_.retire(request);
	}
	void collect() {
		++calls.collect;
		storage_.collect();
	}
	void trim(uint64_t targetBytes) {
		++calls.trim;
		storage_.trim(targetBytes);
	}
	[[nodiscard]] StorageStats stats() const {
		++calls.stats;
		return storage_.stats();
	}
	[[nodiscard]] ResourceStats resourceStats(ResourceKind kind) const {
		++calls.resourceStats;
		return storage_.resourceStats(kind);
	}
#if FLOW_UI_DEV_MODE
	void appendMemorySnapshot(const StorageMemorySnapshotRequest& request,
							  StorageMemorySnapshot& destination) const {
		++calls.appendMemorySnapshot;
		storage_.appendMemorySnapshot(request, destination);
	}
#endif
	[[nodiscard]] bool validateHandle(ResourceKind kind, uint32_t index,
									  uint32_t generation) const noexcept {
		++calls.validateHandle;
		return storage_.validateHandle(kind, index, generation);
	}
	void setBudget(uint64_t cpuBytes, uint64_t gpuBytes) {
		++calls.setBudget;
		storage_.setBudget(cpuBytes, gpuBytes);
	}
	[[nodiscard]] NativePublishResult<RendererLayoutHandle>
	publishRendererLayout(const RendererLayoutKey& key, const NativeRendererLayout& native,
						  StringId debugName = 0) {
		++calls.publishRendererLayout;
		return storage_.publishRendererLayout(key, native, debugName);
	}
	[[nodiscard]] RendererLayoutHandle acquireRendererLayout(const RendererLayoutKey& key) {
		++calls.acquireRendererLayout;
		return storage_.acquireRendererLayout(key);
	}
	[[nodiscard]] NativePublishResult<RendererPipelineBundleHandle>
	publishRendererPipelineBundle(const RendererPipelineKey& key,
								  const NativeRendererPipelineBundle& native,
								  StringId debugName = 0) {
		++calls.publishRendererPipelineBundle;
		return storage_.publishRendererPipelineBundle(key, native, debugName);
	}
	[[nodiscard]] RendererPipelineBundleHandle
	acquireRendererPipelineBundle(const RendererPipelineKey& key) {
		++calls.acquireRendererPipelineBundle;
		return storage_.acquireRendererPipelineBundle(key);
	}
	[[nodiscard]] WindowDescriptorBundleHandle
	adoptWindowDescriptorBundle(const WindowDescriptorBundleDesc& desc,
								const NativeWindowDescriptorBundle& native) {
		++calls.adoptWindowDescriptorBundle;
		return storage_.adoptWindowDescriptorBundle(desc, native);
	}
	[[nodiscard]] NativeRendererLayout
	nativeRendererLayout(RendererLayoutHandle layout) const noexcept {
		++calls.nativeRendererLayout;
		return storage_.nativeRendererLayout(layout);
	}
	[[nodiscard]] NativeRendererPipelineBundle
	nativeRendererPipelineBundle(RendererPipelineBundleHandle bundle) const noexcept {
		++calls.nativeRendererPipelineBundle;
		return storage_.nativeRendererPipelineBundle(bundle);
	}
	[[nodiscard]] NativeWindowDescriptorView
	nativeWindowDescriptorBundle(WindowDescriptorBundleHandle bundle) const noexcept {
		++calls.nativeWindowDescriptorBundle;
		return storage_.nativeWindowDescriptorBundle(bundle);
	}
	void releaseRendererLayout(RendererLayoutHandle layout, SubmissionSerial lastUse = 0) {
		++calls.releaseRendererLayout;
		storage_.releaseRendererLayout(layout, lastUse);
	}
	void releaseRendererPipelineBundle(RendererPipelineBundleHandle bundle,
									   SubmissionSerial lastUse = 0) {
		++calls.releaseRendererPipelineBundle;
		storage_.releaseRendererPipelineBundle(bundle, lastUse);
	}
	void releaseWindowDescriptorBundle(WindowDescriptorBundleHandle bundle,
									   SubmissionSerial lastUse = 0) {
		++calls.releaseWindowDescriptorBundle;
		storage_.releaseWindowDescriptorBundle(bundle, lastUse);
	}
	[[nodiscard]] NativeBufferView nativeBuffer(BufferHandle buffer) const noexcept {
		++calls.nativeBuffer;
		return storage_.nativeBuffer(buffer);
	}
	[[nodiscard]] NativeImageView nativeImage(ImageHandle image) const noexcept {
		++calls.nativeImage;
		return storage_.nativeImage(image);
	}
	[[nodiscard]] NativeImageViewInfo nativeImageView(ImageViewHandle view) const noexcept {
		++calls.nativeImageView;
		return storage_.nativeImageView(view);
	}
	[[nodiscard]] NativeSamplerInfo nativeSampler(SamplerHandle sampler) const noexcept {
		++calls.nativeSampler;
		return storage_.nativeSampler(sampler);
	}

private:
	FlowStorageSystem storage_;
};
inline RecordingStorageSystem::Calls RecordingStorageSystem::calls{};
} // namespace FlowUi::test
