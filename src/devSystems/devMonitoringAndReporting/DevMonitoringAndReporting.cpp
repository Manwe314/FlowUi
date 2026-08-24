#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"

namespace FlowUi::devSystems {

DevMonitoringAndReporting::DevMonitoringAndReporting()
	: timing_(std::make_unique<DevTiming>()),
	  gpuTiming_(std::make_unique<DevGpuTiming>(*timing_)),
	  timingReporting_(std::make_unique<DevTimingReporting>(*timing_, *gpuTiming_)),
	  memory_(std::make_unique<DevMemory>()),
	  memoryReporting_(std::make_unique<DevMemoryReporting>(*memory_)),
	  errors_(std::make_unique<DevErrorMonitoring>()),
	  errorReporting_(std::make_unique<DevErrorReporting>(
		  *errors_, DevErrorReportingConfig{}, timingReporting_.get(), memoryReporting_.get())) {}

DevMonitoringAndReporting::~DevMonitoringAndReporting() = default;

DevTiming& DevMonitoringAndReporting::timing() noexcept { return *timing_; }
const DevTiming& DevMonitoringAndReporting::timing() const noexcept { return *timing_; }
DevGpuTiming& DevMonitoringAndReporting::gpuTiming() noexcept { return *gpuTiming_; }
const DevGpuTiming& DevMonitoringAndReporting::gpuTiming() const noexcept { return *gpuTiming_; }
DevTimingReporting& DevMonitoringAndReporting::timingReporting() noexcept { return *timingReporting_; }
const DevTimingReporting& DevMonitoringAndReporting::timingReporting() const noexcept { return *timingReporting_; }
DevMemory& DevMonitoringAndReporting::memory() noexcept { return *memory_; }
const DevMemory& DevMonitoringAndReporting::memory() const noexcept { return *memory_; }
DevMemoryReporting& DevMonitoringAndReporting::memoryReporting() noexcept { return *memoryReporting_; }
const DevMemoryReporting& DevMonitoringAndReporting::memoryReporting() const noexcept { return *memoryReporting_; }
DevErrorMonitoring& DevMonitoringAndReporting::errors() noexcept { return *errors_; }
const DevErrorMonitoring& DevMonitoringAndReporting::errors() const noexcept { return *errors_; }
DevErrorReporting& DevMonitoringAndReporting::errorReporting() noexcept { return *errorReporting_; }
const DevErrorReporting& DevMonitoringAndReporting::errorReporting() const noexcept { return *errorReporting_; }

} // namespace FlowUi::devSystems

#endif
