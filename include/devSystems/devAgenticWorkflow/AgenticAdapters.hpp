#pragma once

/** @file
 * @brief Public AgenticAdapters declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
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
	/** @brief Error-reporting baseline captured before the measurement interval. */
	DevErrorReportingStatus diagnostics;
	/** @brief Producer generation and loss baseline used to scope error evidence. */
	DevErrorProducerScope error_producers;
	/**
	 * @brief Memory reporting counters captured before measurement, from the same producer
	 * generation.
	 */
	MemoryReportingStatus memory;
};
/**
 * @brief Copy bounded timing, memory and diagnostic evidence relative to baseline into the owned
 * snapshot.
 */
[[nodiscard]] Status materialize_reporting(AgenticCaptureSnapshot& snapshot,
										   DevMonitoringAndReporting& monitoring,
										   const AgenticProducerBaseline& baseline) noexcept;
/**
 * @brief Copy registered tooling and theme evidence into an owned snapshot before asynchronous
 * publication.
 */
[[nodiscard]] Status materialize_tooling(AgenticCaptureSnapshot& snapshot, DevTooling& tooling,
										 ThemeManager& themes) noexcept;
/** Retain one exact selected-frame theme/schema revision before scenario cleanup mutates it. */
[[nodiscard]] Status materialize_theme_revision(AgenticCaptureSnapshot& snapshot,
												DevTooling& tooling, ThemeManager& themes) noexcept;
/**
 * @brief Encode an owned completed UI tree within maximum_bytes; include_text controls copied
 * text disclosure.
 */
[[nodiscard]] Result<Json> materialize_tree(const tooling::DevTreeSnapshot& tree,
											std::string_view run_id, bool include_text,
											std::uint64_t maximum_bytes) noexcept;
} // namespace FlowUi::devSystems::agentic
#endif
