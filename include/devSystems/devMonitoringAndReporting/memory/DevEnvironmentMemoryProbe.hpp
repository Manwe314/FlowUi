#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 1

#include <memory>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"

struct VulkanContext;

namespace FlowUi::devSystems {

/** Platform and Vulkan context probes. Sampling never waits for the GPU. */
class DevEnvironmentMemoryProbe {
public:
	DevEnvironmentMemoryProbe();
	~DevEnvironmentMemoryProbe();
	DevEnvironmentMemoryProbe(const DevEnvironmentMemoryProbe&) = delete;
	DevEnvironmentMemoryProbe& operator=(const DevEnvironmentMemoryProbe&) = delete;

	void initializeVulkan(const VulkanContext& context) noexcept;
	void detachVulkan() noexcept;
	void setConfig(const DevMemoryConfig& config) noexcept;
	void advanceVmaFrameIndex(uint32_t frameIndex) noexcept;
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
