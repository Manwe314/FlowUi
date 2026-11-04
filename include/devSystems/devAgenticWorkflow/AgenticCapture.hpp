#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"

namespace FlowUi::devSystems::agentic {

/** Objective verdict is independent of transport/execution success. */
struct AgenticObjective {
	std::string metric = "window_present_call_cadence_fps";
	std::string comparison = "gt";
	double threshold = 40.0;
	std::uint64_t minimum_valid_intervals = 120;
	std::uint64_t minimum_duration_ns = 10000000000ull;
	std::uint32_t minimum_passes = 5;
	std::optional<double> maximum_p95_interval_ns;
};
/** Explicit data selection; applied to owned evidence before asynchronous publication. */
struct AgenticContentPolicy {
	bool include_tree_text = false;
	bool include_native_messages = true;
	bool include_stacks = true;
	bool include_source_paths = true;
	/** Persist copied live target labels only when the caller explicitly requests them. */
	bool include_interaction_labels = false;
	/** Persist application-supplied target outcomes and action payloads only on explicit request.
	 */
	bool include_interaction_outcomes = false;
};
/** Fully owned request, copied and capability-checked before queue admission. */
struct AgenticCaptureRequest {
	std::vector<std::uint64_t> window_ids;
	std::string scenario_id;
	std::string scenario_revision;
	std::string scenario_digest;
	std::string fixture_id;
	std::string idempotency_key;
	std::optional<AgenticObjective> objective;
	AgenticLimits limits;
	AgenticContentPolicy content;
	std::uint64_t duration_ns = 1000000000ull;
	std::uint64_t warmup_ticks = 0;
	std::uint64_t measurement_ticks = 0;
	std::uint64_t seed = 0;
	std::uint32_t maximum_expected_tick_cadence = 240;
	ExecutionMode mode = ExecutionMode::windowed;
	CaptureProfile profile = CaptureProfile::performance_diagnose;
	SectionRequirement performance = SectionRequirement::required;
	SectionRequirement runtime_diagnostics = SectionRequirement::required;
	SectionRequirement memory = SectionRequirement::omit;
	SectionRequirement visual = SectionRequirement::omit;
	bool allow_degraded = false;
	bool capture_window = true;
	bool capture_viewports = false;
};
struct AgenticCapturePlan {
	AgenticCaptureRequest request;
	std::vector<std::string> limitations;
	std::vector<std::string> rejected_reasons;
	std::uint64_t estimated_snapshot_bytes = 0;
	std::uint64_t reserved_ticks = 0;
	bool feasible = false;
};
/** Validate and estimate before starting a live operation. */
[[nodiscard]] Result<AgenticCapturePlan>
plan_capture(const AgenticCaptureRequest& request,
			 const AgenticCapabilities& capabilities) noexcept;
[[nodiscard]] Result<AgenticCaptureRequest> capture_request_from_json(const Json& input) noexcept;
[[nodiscard]] Json to_json(const AgenticCaptureRequest& request);
[[nodiscard]] Json to_json(const AgenticCapturePlan& plan);
[[nodiscard]] Json to_json(const AgenticObjective& objective);

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
