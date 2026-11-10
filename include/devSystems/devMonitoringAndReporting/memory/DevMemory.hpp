#pragma once
#include "FlowUi/Private/storageSystem/capacity/MemoryDemandPeaks.hpp"

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <vector>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryRecorder.hpp"
#include "FlowUi/Private/storageSystem/StorageTypes.hpp"

namespace FlowUi::detail::storage { class IStorageSystem; }
struct VulkanContext;

namespace FlowUi::devSystems {

/**
 * Owns memory source/probe registration, the bounded operation recorder, and environment sampling.
 * Registered probes borrow their owners and must be removed before owner destruction.
 */
class DevMemory {
public:
	/** Create the monitoring service and its fixed-capacity producer recorder. */
	explicit DevMemory(DevMemoryConfig config = {});
	/** Destroy the recorder and registrations; detach borrowed source owners before their destruction. */
	~DevMemory();

	DevMemory(const DevMemory&) = delete;
	DevMemory& operator=(const DevMemory&) = delete;

	/**
	 * Apply normalized runtime levels and sampling controls; producer event capacity remains fixed at
	 * construction.
	 */
	void setConfig(const DevMemoryConfig& config) noexcept;
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** Stable gate whose lifetime matches this monitoring service. */
	[[nodiscard]] const ::FlowUi::detail::MemoryDemandControl* demand_control() const noexcept;
#endif
	/** Copy normalized runtime configuration. */
	[[nodiscard]] DevMemoryConfig config() const noexcept;
	/** Return the maximum memory instrumentation level present in this build. */
	[[nodiscard]] MemoryMonitoringLevel compiledLevel() const noexcept;

	/**
	 * Copy source metadata; return false for invalid identity/name or a conflicting registration.
	 * Identical registration is accepted.
	 */
	[[nodiscard]] bool registerSource(const StaticMemorySourceDescriptor& descriptor);
	/** Copy a capacity target; return false for invalid bounds/identity or incompatible existing metadata. */
	[[nodiscard]] bool registerTuningTarget(const MemoryTuningTargetDescriptor& descriptor);
	/** Register a borrowed owner/callback; return false for invalid or conflicting registration. */
	[[nodiscard]] bool registerProbe(const RegisteredMemoryProbe& probe);
	/** Remove the matching source/owner probe before destroying its borrowed owner. */
	void unregisterProbe(MemorySourceId source, const void* owner) noexcept;

	/** Return an owned copy of registered source metadata. */
	[[nodiscard]] std::vector<MemorySourceDescriptor> descriptorSnapshot() const;
	/** Return an owned copy of registered capacity target metadata. */
	[[nodiscard]] std::vector<MemoryTuningTargetDescriptor> tuningTargetSnapshot() const;
	/** Copy probe registrations; copied owner pointers remain borrowed and do not extend owner lifetimes. */
	[[nodiscard]] std::vector<RegisteredMemoryProbe> probeSnapshot() const;
	/** Borrow the producer recorder for this service lifetime. */
	[[nodiscard]] DevMemoryRecorder& recorder() noexcept;
	/** Borrow the producer recorder as const for this service lifetime. */
	[[nodiscard]] const DevMemoryRecorder& recorder() const noexcept;
	/** Copy cumulative recorder and registration quality counters. */
	[[nodiscard]] MemoryQualitySnapshot qualitySnapshot() const noexcept;
	/** Attach borrowed runtime storage for sampling, or null to detach; storage must outlive attached use. */
	void setStorageSystem(::FlowUi::detail::storage::IStorageSystem* storage) noexcept;
	/**
	 * Append a storage checkpoint to destination; return false when storage is detached or snapshot
	 * acquisition fails.
	 */
	[[nodiscard]] bool appendStorageSnapshot(
		const ::FlowUi::detail::storage::StorageMemorySnapshotRequest& request,
		::FlowUi::detail::storage::StorageMemorySnapshot& destination) const noexcept;
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
	/** Attach borrowed Vulkan/VMA resources for GPU environment sampling. */
	void initializeEnvironmentProbes(const ::VulkanContext& context) noexcept;
	/** Detach GPU environment resources before Vulkan shutdown. */
	void detachEnvironmentProbes() noexcept;
	/** Forward the active frame index to GPU allocation-budget sampling. */
	void advanceGpuFrameIndex(uint32_t frameIndex) noexcept;
	/**
	 * Return an owned process/GPU sample using monotonic nowNs and safely attributable CPU bytes.
	 * force bypasses configured sampling intervals.
	 */
	[[nodiscard]] MemoryEnvironmentSnapshot sampleEnvironment(
		uint64_t nowNs,
		uint64_t safelyAttributableFlowUiCpuBytes,
		bool force = false) noexcept;
#endif

private:
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};

} // namespace FlowUi::devSystems

#endif
