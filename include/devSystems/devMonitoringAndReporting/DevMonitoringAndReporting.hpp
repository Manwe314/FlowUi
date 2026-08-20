#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>

namespace FlowUi::devSystems {

class DevTiming;

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

private:
	std::unique_ptr<DevTiming> timing_{};
};

} // namespace FlowUi::devSystems

#endif
