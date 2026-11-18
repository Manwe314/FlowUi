#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include "FlowUi/Private/storageSystem/StorageContractMetadata.hpp"
#include "FlowUi/Private/storageSystem/StorageTypes.hpp"
#include "FlowUi/Public/BuildConfig.hpp"

struct VulkanContext;

namespace FlowUi::detail::storage {

/** Complete private storage contract for the current Vulkan renderer. */
template <typename Storage>
concept StorageSystemConcept =
	std::is_constructible_v<Storage, VulkanContext&> && std::is_nothrow_destructible_v<Storage> &&
	requires(
		Storage& storage, const Storage& observed, const StorageConfig& config,
		FlowUi::detail::InitialCapacityTarget capacityTarget, WindowId window,
		const WindowStorageDesc& windowDesc, const FrameStorageDesc& frameDesc,
		const FrameToken& frame, const FrameReadLease& lease, SubmissionSerial serial, size_t bytes,
		size_t alignment, uint64_t offset, uint64_t byteCount, uint32_t count,
		MemoryClass memoryClass, StringId debugName, const AllocationTag& tag, MemoryBlock block,
		BufferHandle buffer, ImageHandle image, ImageViewHandle imageView, SamplerHandle sampler,
		TextureHandle texture, BlobHandle blob, BufferWriteMode writeMode,
		const BufferWriteView& write, std::string_view text, ResourceKey key, ResourceKind kind,
		ManagerRecordHandle managerHandle, const ManagerRecordDesc& managerDesc,
		PersistentRecordHandle persistentHandle, const PersistentRecordDesc& persistentDesc,
		std::span<const std::byte> byteSpan, const BufferDesc& bufferDesc,
		const ImageDesc& imageDesc, const ImageViewDesc& imageViewDesc,
		const SamplerDesc& samplerDesc, const TextureViewDesc& textureDesc, bool* inserted,
		std::span<const TextureHandle> textures,
		std::span<const DescriptorWriteRecord> descriptorWrites,
		std::span<const ResourceUse> resources, const UploadRequest& uploadRequest,
		UploadTicket uploadTicket, SubmissionToken submission, const RetirementRequest& retirement,
		const RendererLayoutKey& layoutKey, const NativeRendererLayout& nativeLayout,
		RendererLayoutHandle layout, const RendererPipelineKey& pipelineKey,
		const NativeRendererPipelineBundle& nativePipelines, RendererPipelineBundleHandle pipelines,
		const WindowDescriptorBundleDesc& descriptorDesc,
		const NativeWindowDescriptorBundle& nativeDescriptors,
		WindowDescriptorBundleHandle descriptors) {
		// Must be usable as a constant expression, not just a runtime value.
		typename std::integral_constant<uint32_t, Storage::contractRevision>;
		typename std::integral_constant<uint32_t, Storage::interfaceVersionValue>;
		typename std::integral_constant<uint64_t, Storage::supportedCapabilities>;
		requires std::same_as<decltype(Storage::contractRevision), const uint32_t>;
		requires std::same_as<decltype(Storage::interfaceVersionValue), const uint32_t>;
		requires std::same_as<decltype(Storage::supportedCapabilities), const uint64_t>;
		requires(Storage::contractRevision == storageContractRevision);
		requires(Storage::interfaceVersionValue == currentStorageInterfaceVersion);
		requires((Storage::supportedCapabilities & requiredStorageCapabilities) ==
				 requiredStorageCapabilities);

		{ storage.initialize(config) } -> std::same_as<void>;
		{ storage.shutdown() } noexcept -> std::same_as<void>;
		{ observed.interfaceVersion() } noexcept -> std::same_as<uint32_t>;
		{ observed.capabilities() } noexcept -> std::same_as<uint64_t>;
		{ observed.initial_memory_capacity(capacityTarget) } noexcept -> std::same_as<uint64_t>;
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		{
			observed.memory_demand_control()
		} noexcept -> std::same_as<const FlowUi::detail::MemoryDemandControl*>;
#endif
		{ storage.registerWindow(window, windowDesc) } -> std::same_as<void>;
		{ storage.unregisterWindow(window, serial) } -> std::same_as<void>;
		{ storage.beginFrame(window, frameDesc) } -> std::same_as<FrameToken>;
		{ storage.sealFrame(frame) } -> std::same_as<FrameReadLease>;
		{ storage.cancelFrame(frame) } noexcept -> std::same_as<void>;

		{
			storage.allocatePersistent(bytes, alignment, memoryClass, debugName)
		} -> std::same_as<MemoryBlock>;
		{ storage.allocatePersistent(bytes, alignment, tag) } -> std::same_as<MemoryBlock>;
		{ storage.releasePersistent(block) } noexcept -> std::same_as<void>;
		{ storage.frameArena(frame, memoryClass) } -> std::same_as<ArenaView>;
		{ storage.workerArena(frame, count) } -> std::same_as<ArenaView>;
		{
			storage.beginBufferWrite(frame, buffer, offset, byteCount, writeMode)
		} -> std::same_as<BufferWriteView>;
		{
			storage.beginBufferWrite(frame, buffer, offset, byteCount)
		} -> std::same_as<BufferWriteView>;
		{ storage.commitBufferWrite(frame, write, byteCount) } -> std::same_as<void>;
		{ storage.writeBuffer(frame, buffer, offset, byteSpan) } -> std::same_as<void>;

		{ storage.intern(text) } -> std::same_as<StringId>;
		{ observed.string(debugName) } noexcept -> std::same_as<std::string_view>;
		{ storage.markDiagnosticOnce(key, count) } -> std::same_as<bool>;
		{ storage.clearDiagnosticMark(key, count) } -> std::same_as<void>;
		{ storage.createManagerRecord(managerDesc) } -> std::same_as<ManagerRecordHandle>;
		{ observed.findManagerRecord(key, kind) } noexcept -> std::same_as<ManagerRecordHandle>;
		{ storage.managerRecordData(managerHandle, kind) } noexcept -> std::same_as<void*>;
		{ observed.managerRecordData(managerHandle, kind) } noexcept -> std::same_as<const void*>;
		{ storage.removeManagerRecord(key, kind) } -> std::same_as<bool>;
		{ storage.releaseWindowManagerRecords(window) } noexcept -> std::same_as<void>;
		{ storage.createPersistentRecord(persistentDesc) } -> std::same_as<PersistentRecordHandle>;
		{
			storage.persistentRecord(persistentHandle, kind)
		} noexcept -> std::same_as<PersistentRecordView>;
		{
			observed.persistentRecord(persistentHandle, kind)
		} noexcept -> std::same_as<ConstPersistentRecordView>;
		{ storage.removePersistentRecord(persistentHandle, kind) } noexcept -> std::same_as<bool>;
		{ storage.releaseWindowPersistentRecords(window) } noexcept -> std::same_as<void>;
		{ storage.noteManagerMutation(window) } -> std::same_as<void>;
		{ observed.managerFrameView(frame) } noexcept -> std::same_as<ManagerFrameView>;
		{ observed.managerSharedRevision() } noexcept -> std::same_as<uint64_t>;
		{ observed.managerWindowRevision(window) } noexcept -> std::same_as<uint64_t>;
		{ storage.setRecordFailureCountdown(count) } noexcept -> std::same_as<void>;
		{ storage.createBlob(byteSpan, debugName) } -> std::same_as<BlobHandle>;
		{ observed.readBlob(blob) } noexcept -> std::same_as<std::span<const std::byte>>;
		{ storage.releaseBlob(blob, serial) } -> std::same_as<void>;
		{ storage.releaseBlob(blob) } -> std::same_as<void>;

		{ storage.createBuffer(bufferDesc) } -> std::same_as<BufferHandle>;
		{ storage.createImage(imageDesc) } -> std::same_as<ImageHandle>;
		{ storage.createImageView(image, imageViewDesc) } -> std::same_as<ImageViewHandle>;
		{ storage.acquireSampler(samplerDesc) } -> std::same_as<SamplerHandle>;
		{ storage.releaseBuffer(buffer, serial) } -> std::same_as<void>;
		{ storage.releaseBuffer(buffer) } -> std::same_as<void>;
		{ storage.releaseImage(image, serial) } -> std::same_as<void>;
		{ storage.releaseImage(image) } -> std::same_as<void>;
		{ storage.releaseImageView(imageView, serial) } -> std::same_as<void>;
		{ storage.releaseImageView(imageView) } -> std::same_as<void>;
		{ storage.releaseSampler(sampler, serial) } -> std::same_as<void>;
		{ storage.releaseSampler(sampler) } -> std::same_as<void>;

		{ storage.publishTexture(key, textureDesc, inserted) } -> std::same_as<TextureHandle>;
		{ storage.publishTexture(key, textureDesc) } -> std::same_as<TextureHandle>;
		{ storage.createAnonymousTexture(textureDesc) } -> std::same_as<TextureHandle>;
		{ storage.releaseAnonymousTexture(texture, serial) } -> std::same_as<void>;
		{ storage.releaseAnonymousTexture(texture) } -> std::same_as<void>;
		{ storage.replaceTexture(key, textureDesc) } -> std::same_as<TextureHandle>;
		{ storage.removeTexture(key, serial) } -> std::same_as<bool>;
		{ storage.removeTexture(key) } -> std::same_as<bool>;
		{ observed.findTexture(key) } noexcept -> std::same_as<TextureHandle>;
		{ observed.textureMetadata(texture) } noexcept -> std::same_as<TextureMetadata>;
		{ observed.textureRetirementComplete(texture) } noexcept -> std::same_as<bool>;
		{ storage.setFallbackTexture(texture) } -> std::same_as<void>;
#if FLOW_UI_DEV_MODE
		{ observed.devTextureMetadata(texture) } noexcept -> std::same_as<DevTextureMetadata>;
		{ storage.retainTextureForDevInspection(texture) } noexcept -> std::same_as<bool>;
		{ storage.releaseTextureFromDevInspection(texture) } -> std::same_as<void>;
#endif
		{
			storage.prepareTextureBindings(frame, textures)
		} -> std::same_as<PreparedTextureBindings>;
		{ storage.acknowledgeTextureBindings(frame, descriptorWrites) } -> std::same_as<void>;
		{ storage.resetTextureBindings(window, count) } -> std::same_as<void>;
		{ storage.resolveTexture(frame, texture) } -> std::same_as<ResolvedTextureBinding>;
		{ storage.trackUse(frame, buffer) } -> std::same_as<void>;
		{ storage.trackUse(frame, image) } -> std::same_as<void>;
		{ storage.trackUses(frame, resources) } -> std::same_as<void>;
		{ storage.invalidateWindowBindings(window, texture) } -> std::same_as<void>;
		{ observed.readView(lease) } -> std::same_as<StorageReadView>;
		{ observed.windowBindingView(lease) } -> std::same_as<WindowBindingView>;
		{ observed.windowSnapshot(window) } -> std::same_as<WindowStorageSnapshot>;

		{ storage.enqueueUpload(uploadRequest) } -> std::same_as<UploadTicket>;
		{ observed.uploadState(uploadTicket) } noexcept -> std::same_as<ResourceState>;
		{ storage.flushUploads() } -> std::same_as<void>;
		{ storage.noteSubmission(lease) } -> std::same_as<SubmissionToken>;
		{ storage.noteCompleted(submission) } -> std::same_as<void>;
		{ observed.completedSerial() } noexcept -> std::same_as<SubmissionSerial>;
		{ storage.retire(retirement) } -> std::same_as<void>;
		{ storage.collect() } -> std::same_as<void>;
		{ storage.trim(byteCount) } -> std::same_as<void>;
		{ observed.stats() } -> std::same_as<StorageStats>;
		{ observed.resourceStats(kind) } -> std::same_as<ResourceStats>;
		{ observed.validateHandle(kind, count, count) } noexcept -> std::same_as<bool>;
		{ storage.setBudget(byteCount, byteCount) } -> std::same_as<void>;

		{
			storage.publishRendererLayout(layoutKey, nativeLayout, debugName)
		} -> std::same_as<NativePublishResult<RendererLayoutHandle>>;
		{
			storage.publishRendererLayout(layoutKey, nativeLayout)
		} -> std::same_as<NativePublishResult<RendererLayoutHandle>>;
		{ storage.acquireRendererLayout(layoutKey) } -> std::same_as<RendererLayoutHandle>;
		{
			storage.publishRendererPipelineBundle(pipelineKey, nativePipelines, debugName)
		} -> std::same_as<NativePublishResult<RendererPipelineBundleHandle>>;
		{
			storage.publishRendererPipelineBundle(pipelineKey, nativePipelines)
		} -> std::same_as<NativePublishResult<RendererPipelineBundleHandle>>;
		{
			storage.acquireRendererPipelineBundle(pipelineKey)
		} -> std::same_as<RendererPipelineBundleHandle>;
		{
			storage.adoptWindowDescriptorBundle(descriptorDesc, nativeDescriptors)
		} -> std::same_as<WindowDescriptorBundleHandle>;
		{ observed.nativeRendererLayout(layout) } noexcept -> std::same_as<NativeRendererLayout>;
		{
			observed.nativeRendererPipelineBundle(pipelines)
		} noexcept -> std::same_as<NativeRendererPipelineBundle>;
		{
			observed.nativeWindowDescriptorBundle(descriptors)
		} noexcept -> std::same_as<NativeWindowDescriptorView>;
		{ storage.releaseRendererLayout(layout, serial) } -> std::same_as<void>;
		{ storage.releaseRendererLayout(layout) } -> std::same_as<void>;
		{ storage.releaseRendererPipelineBundle(pipelines, serial) } -> std::same_as<void>;
		{ storage.releaseRendererPipelineBundle(pipelines) } -> std::same_as<void>;
		{ storage.releaseWindowDescriptorBundle(descriptors, serial) } -> std::same_as<void>;
		{ storage.releaseWindowDescriptorBundle(descriptors) } -> std::same_as<void>;
		{ observed.nativeBuffer(buffer) } noexcept -> std::same_as<NativeBufferView>;
		{ observed.nativeImage(image) } noexcept -> std::same_as<NativeImageView>;
		{ observed.nativeImageView(imageView) } noexcept -> std::same_as<NativeImageViewInfo>;
		{ observed.nativeSampler(sampler) } noexcept -> std::same_as<NativeSamplerInfo>;
	}
#if FLOW_UI_DEV_MODE
	&&
	requires(const Storage& observed, const StorageMemorySnapshotRequest& request,
			 StorageMemorySnapshot& destination) {
		{ observed.appendMemorySnapshot(request, destination) } -> std::same_as<void>;
	}
#endif
;

/** Query build-fixed provider support without constructing storage. */
template <StorageSystemConcept Storage>
[[nodiscard]] constexpr bool storageSupportsCapability(StorageCapability capability) noexcept {
	return supportsStorageCapabilities(Storage::supportedCapabilities, capability);
}

} // namespace FlowUi::detail::storage
