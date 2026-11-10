#pragma once

/** @file
 * @brief Public AgenticProvenance declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticSnapshot.hpp"

namespace FlowUi {
class App;
struct ResourceConfig;
namespace devSystems {
class DevMonitoringAndReporting;
class DevTooling;
} // namespace devSystems
} // namespace FlowUi
namespace FlowUi::devSystems::agentic {
/** Gather bounded provenance before measurement admission. Reads executable and resource files
 * using the application's exact resource policy, owns reflected values, and releases every lease
 * before returning. Unavailable facts remain unknown with explicit scope/limitations. */
[[nodiscard]] Status complete_provenance(AgenticCaptureSnapshot& snapshot, App& application,
										 DevMonitoringAndReporting& monitoring, DevTooling& tooling,
										 const ResourceConfig& resources) noexcept;
} // namespace FlowUi::devSystems::agentic
#endif
