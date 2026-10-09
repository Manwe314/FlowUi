#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Error.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include <filesystem>
#include <ostream>
namespace FlowUi::devSystems {
/** Write every retained timing field as a versioned CSV with one row per record.
 * Missing fields remain empty; integer timings and identities retain their exact values.
 */
[[nodiscard]] Status
write_performance_capture_csv(std::ostream& output, const TimingCaptureSnapshot& snapshot,
							  const TimingReportingStatus& reporting_status,
							  const PerformanceCaptureStatus& capture_status) noexcept;
/** Export the specified sealed generation to a timestamped CSV beside the executable.
 * An explicit directory is available for embedding and validation. Disk I/O holds no ring lease.
 */
[[nodiscard]] Result<std::filesystem::path>
export_performance_capture_csv(DevTimingReporting& reporting, uint64_t expected_generation,
							   const PerformanceCaptureStatus& capture_status,
							   const std::filesystem::path& directory = {}) noexcept;
} // namespace FlowUi::devSystems
#endif
