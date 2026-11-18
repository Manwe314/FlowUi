#pragma once

#include <cstdint>

#include "FlowUi/Private/storageSystem/StorageTypes.hpp"
#include "FlowUi/Public/BuildConfig.hpp"

namespace FlowUi::detail::storage {

inline constexpr uint32_t storageContractRevision = 1u;
inline constexpr uint32_t currentStorageInterfaceVersion = 8u;

[[nodiscard]] constexpr uint64_t storageCapabilityMask(StorageCapability capability) noexcept {
	return static_cast<uint64_t>(capability);
}

/** Test every requested bit; an empty request succeeds. */
[[nodiscard]] constexpr bool supportsStorageCapabilities(uint64_t supported,
														 StorageCapability requested) noexcept {
	const uint64_t requestedMask = storageCapabilityMask(requested);
	return (supported & requestedMask) == requestedMask;
}

inline constexpr uint64_t requiredStorageCapabilities =
	storageCapabilityMask(StorageCapability::WindowScopes) |
	storageCapabilityMask(StorageCapability::WorkerArenas) |
	storageCapabilityMask(StorageCapability::GenerationalHandles) |
	storageCapabilityMask(StorageCapability::BatchedBindingResolution) |
	storageCapabilityMask(StorageCapability::SynchronousUploads) |
	storageCapabilityMask(StorageCapability::DeferredRetirement) |
	storageCapabilityMask(StorageCapability::VulkanInterop) |
	storageCapabilityMask(StorageCapability::DirectMappedWrites) |
	storageCapabilityMask(StorageCapability::HostScratchBufferWrites) |
	storageCapabilityMask(StorageCapability::BindingWriteBatches) |
	storageCapabilityMask(StorageCapability::FrameReadLeases) |
	storageCapabilityMask(StorageCapability::RendererResourceBundles) |
	storageCapabilityMask(StorageCapability::ManagerRecords) |
	storageCapabilityMask(StorageCapability::ManagerFrameSnapshots) |
	storageCapabilityMask(StorageCapability::PersistentRecords)
#if FLOW_UI_DEV_MODE
	| storageCapabilityMask(StorageCapability::DevelopmentTelemetry)
#endif
	;

// Exactly the current default mask, including optional runtime growth support.
inline constexpr uint64_t flowStorageSupportedCapabilities =
	requiredStorageCapabilities | storageCapabilityMask(StorageCapability::RuntimeGrowth);

} // namespace FlowUi::detail::storage
