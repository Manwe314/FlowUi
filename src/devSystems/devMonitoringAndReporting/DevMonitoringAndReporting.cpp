#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"

namespace FlowUi::devSystems {

DevMonitoringAndReporting::DevMonitoringAndReporting()
	: timing_(std::make_unique<DevTiming>()) {}

DevMonitoringAndReporting::~DevMonitoringAndReporting() = default;

DevTiming& DevMonitoringAndReporting::timing() noexcept { return *timing_; }
const DevTiming& DevMonitoringAndReporting::timing() const noexcept { return *timing_; }

} // namespace FlowUi::devSystems

#endif
