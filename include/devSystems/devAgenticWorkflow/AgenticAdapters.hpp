#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticSnapshot.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
namespace FlowUi {
class ThemeManager;
}
namespace FlowUi::devSystems {
class DevMonitoringAndReporting;
class DevTooling;
} // namespace FlowUi::devSystems
namespace FlowUi::devSystems::tooling {
struct DevTreeSnapshot;
}
namespace FlowUi::devSystems::agentic {
/** Baseline counters from the same producer generations, captured before measurement. */
struct AgenticProducerBaseline {
	DevErrorReportingStatus diagnostics;
	DevErrorProducerScope error_producers;
	MemoryReportingStatus memory;
};
[[nodiscard]] Status materialize_reporting(AgenticCaptureSnapshot& snapshot,
										   DevMonitoringAndReporting& monitoring,
										   const AgenticProducerBaseline& baseline) noexcept;
[[nodiscard]] Status materialize_tooling(AgenticCaptureSnapshot& snapshot, DevTooling& tooling,
										 ThemeManager& themes) noexcept;
/** Retain one exact selected-frame theme/schema revision before scenario cleanup mutates it. */
[[nodiscard]] Status materialize_theme_revision(AgenticCaptureSnapshot& snapshot,
												DevTooling& tooling, ThemeManager& themes) noexcept;
[[nodiscard]] Result<Json> materialize_tree(const tooling::DevTreeSnapshot& tree,
											std::string_view run_id, bool include_text,
											std::uint64_t maximum_bytes) noexcept;
} // namespace FlowUi::devSystems::agentic
#endif
