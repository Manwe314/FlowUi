#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>

namespace FlowUi::devSystems {

class DevTiming;
class DevGpuTiming;
class DevTimingReporting;
class DevMemory;
class DevMemoryReporting;

/** Development-only owner for monitoring collection and derived reporting. */
class DevMonitoringAndReporting {
public:
	DevMonitoringAndReporting();
	~DevMonitoringAndReporting();

	DevMonitoringAndReporting(const DevMonitoringAndReporting&) = delete;
	DevMonitoringAndReporting& operator=(const DevMonitoringAndReporting&) = delete;
	DevMonitoringAndReporting(DevMonitoringAndReporting&&) = delete;
	DevMonitoringAndReporting& operator=(DevMonitoringAndReporting&&) = delete;

	[[nodiscard]] DevTiming& timing() noexcept;
	[[nodiscard]] const DevTiming& timing() const noexcept;
	[[nodiscard]] DevGpuTiming& gpuTiming() noexcept;
	[[nodiscard]] const DevGpuTiming& gpuTiming() const noexcept;
	[[nodiscard]] DevTimingReporting& timingReporting() noexcept;
	[[nodiscard]] const DevTimingReporting& timingReporting() const noexcept;
	[[nodiscard]] DevMemory& memory() noexcept;
	[[nodiscard]] const DevMemory& memory() const noexcept;
	[[nodiscard]] DevMemoryReporting& memoryReporting() noexcept;
	[[nodiscard]] const DevMemoryReporting& memoryReporting() const noexcept;

private:
	std::unique_ptr<DevTiming> timing_{};
	std::unique_ptr<DevGpuTiming> gpuTiming_{};
	std::unique_ptr<DevTimingReporting> timingReporting_{};
	std::unique_ptr<DevMemory> memory_{};
	std::unique_ptr<DevMemoryReporting> memoryReporting_{};
};

} // namespace FlowUi::devSystems

#endif
