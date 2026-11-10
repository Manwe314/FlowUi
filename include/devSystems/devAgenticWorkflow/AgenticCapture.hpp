#pragma once

/** @file
 * @brief Public AgenticCapture declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"

namespace FlowUi::devSystems::agentic {

/** Objective verdict is independent of transport/execution success. */
struct AgenticObjective {
	/** @brief Named cadence metric evaluated by the objective. */
	std::string metric = "window_present_call_cadence_fps";
	/** @brief Objective comparison operator token, such as gt. */
	std::string comparison = "gt";
	/** @brief Numeric acceptance threshold in the selected objective metric's units. */
	double threshold = 40.0;
	/** @brief Minimum valid continuous cadence intervals required by the objective. */
	std::uint64_t minimum_valid_intervals = 120;
	/**
	 * @brief Minimum measured capture duration in nanoseconds required before evaluating the
	 * objective.
	 */
	std::uint64_t minimum_duration_ns = 10000000000ull;
	/**
	 * @brief Minimum independent successful measurement passes required by the objective.
	 */
	std::uint32_t minimum_passes = 5;
	/**
	 * @brief Optional upper bound for the 95th-percentile cadence interval, in nanoseconds.
	 */
	std::optional<double> maximum_p95_interval_ns;
};
/** Explicit data selection; applied to owned evidence before asynchronous publication. */
struct AgenticContentPolicy {
	/** @brief Whether copied UI tree text may be persisted. */
	bool include_tree_text = false;
	/** @brief Whether copied native error text may be persisted. */
	bool include_native_messages = true;
	/** @brief Whether copied stack evidence may be persisted. */
	bool include_stacks = true;
	/** @brief Whether source path evidence may be persisted. */
	bool include_source_paths = true;
	/** Persist copied live target labels only when the caller explicitly requests them. */
	bool include_interaction_labels = false;
	/** Persist application-supplied target outcomes and action payloads only on explicit request.
	 */
	bool include_interaction_outcomes = false;
};
/** Fully owned request, copied and capability-checked before queue admission. */
struct AgenticCaptureRequest {
	/** @brief Explicit application windows included in this capture. */
	std::vector<std::uint64_t> window_ids;
	/** @brief Application-declared semantic workload identity. */
	std::string scenario_id;
	/** @brief Expected application-declared workload revision. */
	std::string scenario_revision;
	/** @brief Expected scenario content digest used to reject changed workloads. */
	std::string scenario_digest;
	/** @brief Application-declared fixture identity used to reproduce the workload. */
	std::string fixture_id;
	/**
	 * @brief Caller retry identity used to avoid duplicate admission; excluded from workload
	 * identity.
	 */
	std::string idempotency_key;
	/**
	 * @brief Optional acceptance criterion evaluated independently of capture publication
	 * success.
	 */
	std::optional<AgenticObjective> objective;
	/**
	 * @brief Hard admission and retention bounds; exceeding them returns an explicit failure.
	 */
	AgenticLimits limits;
	/**
	 * @brief Policy controlling which copied payloads may be persisted in capture artifacts.
	 */
	AgenticContentPolicy content;
	/** @brief Requested measurement duration, in nanoseconds. */
	std::uint64_t duration_ns = 1000000000ull;
	/** @brief Application ticks executed before the measured capture interval. */
	std::uint64_t warmup_ticks = 0;
	/**
	 * @brief Requested measured application-tick count; zero leaves duration-based selection.
	 */
	std::uint64_t measurement_ticks = 0;
	/** @brief Deterministic application-supplied workload seed. */
	std::uint64_t seed = 0;
	/**
	 * @brief Expected upper application-tick rate used to reserve capture capacity, in ticks per
	 * second.
	 */
	std::uint32_t maximum_expected_tick_cadence = 240;
	/** @brief Requested execution backend, checked against effective capabilities. */
	ExecutionMode mode = ExecutionMode::windowed;
	/** @brief Named capture policy whose defaults are resolved before admission. */
	CaptureProfile profile = CaptureProfile::performance_diagnose;
	/** @brief Requirement for retained performance evidence. */
	SectionRequirement performance = SectionRequirement::required;
	/** @brief Admission requirement for retained runtime diagnostic evidence. */
	SectionRequirement runtime_diagnostics = SectionRequirement::required;
	/** @brief Admission requirement for retained memory evidence. */
	SectionRequirement memory = SectionRequirement::omit;
	/** @brief Admission requirement for retained image evidence. */
	SectionRequirement visual = SectionRequirement::omit;
	/**
	 * @brief Whether admission may proceed with explicitly reported missing optional
	 * capabilities.
	 */
	bool allow_degraded = false;
	/** @brief Whether capture requests include the native window image. */
	bool capture_window = true;
	/** @brief Whether capture requests include viewport image sources. */
	bool capture_viewports = false;
};
/**
 * @brief Normalized capture admission result with reservations and explicit rejection reasons.
 */
struct AgenticCapturePlan {
	/** @brief Owned normalized capture request used to reserve and publish this snapshot. */
	AgenticCaptureRequest request;
	/** @brief Explicit unsupported, missing or degraded evidence conditions. */
	std::vector<std::string> limitations;
	/** @brief Reasons admission rejected the requested capture plan. */
	std::vector<std::string> rejected_reasons;
	/** @brief Estimated owned snapshot storage required for admission, in bytes. */
	std::uint64_t estimated_snapshot_bytes = 0;
	/** @brief Application ticks reserved by capture planning. */
	std::uint64_t reserved_ticks = 0;
	/** @brief Whether the capture request passes capability and reservation checks. */
	bool feasible = false;
};
/** Validate and estimate before starting a live operation. */
[[nodiscard]] Result<AgenticCapturePlan>
plan_capture(const AgenticCaptureRequest& request,
			 const AgenticCapabilities& capabilities) noexcept;
/**
 * @brief Decode and validate a bounded owned capture request; return a structured error for
 * invalid wire data.
 */
[[nodiscard]] Result<AgenticCaptureRequest> capture_request_from_json(const Json& input) noexcept;
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticCaptureRequest& request);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticCapturePlan& plan);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticObjective& objective);

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
