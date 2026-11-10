#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 1

#include <memory>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"

struct VulkanContext;

namespace FlowUi::devSystems {

/** Platform and Vulkan context probes. Sampling never waits for the GPU. */
class DevEnvironmentMemoryProbe {
public:
	/** Create detached platform/GPU probes. */
	DevEnvironmentMemoryProbe();
	/** Destroy probe state without owning the attached Vulkan context. */
	~DevEnvironmentMemoryProbe();
	DevEnvironmentMemoryProbe(const DevEnvironmentMemoryProbe&) = delete;
	DevEnvironmentMemoryProbe& operator=(const DevEnvironmentMemoryProbe&) = delete;

	/** Attach borrowed Vulkan/VMA resources; detach them before context destruction. */
	void initializeVulkan(const VulkanContext& context) noexcept;
	/** Detach borrowed Vulkan resources and disable GPU sampling. */
	void detachVulkan() noexcept;
	/** Apply process/GPU sampling intervals and detail controls. */
	void setConfig(const DevMemoryConfig& config) noexcept;
	/** Update VMA's frame index used for budget statistics. */
	void advanceVmaFrameIndex(uint32_t frameIndex) noexcept;
	/**
	 * Return an owned environment sample at monotonic nowNs. Cached process/GPU values obey configured
	 * intervals unless force is true; attributable bytes qualify the residual estimate.
	 */
	[[nodiscard]] MemoryEnvironmentSnapshot sample(
		uint64_t nowNs,
		uint64_t safelyAttributableFlowUiCpuBytes,
		bool force = false) noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};

} // namespace FlowUi::devSystems

#endif
