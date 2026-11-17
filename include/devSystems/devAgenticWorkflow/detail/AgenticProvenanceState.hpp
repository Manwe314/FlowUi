#pragma once

#include "devSystems/devAgenticWorkflow/AgenticProvenance.hpp"

#if FLOW_UI_AGENTIC_WORKFLOW
namespace FlowUi::devSystems::agentic::detail {
/** Run-owned identity of the loaded executable, never a cache of mutable resource files. */
struct AgenticBinaryIdentity {
	std::string digest;
	std::uint64_t bytes = 0;
};

[[nodiscard]] Result<std::string> processBinaryIdentity(AgenticBinaryIdentity& identity,
	std::uint64_t maximumBytes) noexcept;

[[nodiscard]] Status complete_provenance(AgenticCaptureSnapshot& snapshot, App& application,
	DevMonitoringAndReporting& monitoring, DevTooling& tooling, const ResourceConfig& resources,
	AgenticBinaryIdentity& binaryIdentity) noexcept;
} // namespace FlowUi::devSystems::agentic::detail
#endif
