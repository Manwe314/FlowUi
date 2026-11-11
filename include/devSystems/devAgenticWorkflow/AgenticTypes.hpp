#pragma once

/** @file
 * @brief Public AgenticTypes declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "devSystems/devAgenticWorkflow/AgenticJson.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace FlowUi::devSystems::agentic {

/** @brief Supported version of the serialized agentic evidence schema. */
inline constexpr std::string_view agentic_schema_version = "1.0";
/** Local errors are carried by the canonical FlowUi result model. */
enum class AgenticError : std::uint8_t {
	invalid_request, /**< Request validation failed. */
	unavailable, /**< Required capability or evidence is unavailable. */
	busy, /**< An exclusive operation already owns the required control. */
	stale_generation, /**< Expected revision no longer matches. */
	invalid_scope, /**< The selected evidence or control scope is invalid. */
	budget_exhausted, /**< A declared admission or retention bound was exceeded. */
	incomplete, /**< Required evidence or completion is incomplete. */
	publication_failed, /**< Immutable evidence publication failed. */
	incompatible, /**< Workload, schema or evidence identities are incompatible. */
	canceled, /**< Cancellation was acknowledged. */
	timeout, /**< The declared deadline elapsed. */
	integrity_failed /**< Content, reference or checksum validation failed. */
};
/** @brief Map an agentic failure category into the canonical FlowUi error model. */
[[nodiscard]] FlowUiError agentic_error(AgenticError error) noexcept;
/**
 * @brief Accept 1 to 128 ASCII letters, digits, underscores, hyphens or dots, except single and
 * double dots.
 */
[[nodiscard]] bool valid_identifier(std::string_view identifier) noexcept;
/**
 * @brief Return prefix, an underscore, and 32 random hexadecimal characters; prefix must be a
 * valid identifier of at most 64 characters. Failures return a structured error.
 */
[[nodiscard]] Result<std::string> make_identifier(std::string_view prefix) noexcept;
/**
 * @brief Return the steady monotonic clock in nanoseconds for deadlines and evidence timestamps.
 */
[[nodiscard]] std::uint64_t monotonic_nanoseconds() noexcept;
/** @brief Return the current UTC time as a serialized provenance timestamp. */
[[nodiscard]] std::string utc_timestamp();

/** Exact producer-supplied evidence key; slots are independent resource indices. */
struct EvidenceFrameKey {
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/**
	 * @brief Producer-supplied application tick identity; independent of window frame and
	 * resource slot.
	 */
	std::uint64_t app_tick = 0;
	/**
	 * @brief Producer-supplied frame identity within one window; distinct from reusable slots.
	 */
	std::uint64_t window_frame = 0;
	/**
	 * @brief Stable application window identity associated with this request or evidence.
	 */
	std::uint64_t window_id = 0;
};

/** @brief Execution backend used to reproduce an application-declared workload. */
enum class ExecutionMode : std::uint8_t {
	offline, /**< Read immutable evidence without an application or GPU. */
	cpu_scenario, /**< Execute an application-declared CPU workload. */
	layout_scenario, /**< Execute a layout workload. */
	offscreen_vulkan, /**< Execute using offscreen Vulkan rendering. */
	windowed /**< Execute ordinary native window frames. */
};
/**
 * @brief Named capture policy resolved to explicit section requirements before admission.
 */
enum class CaptureProfile : std::uint8_t {
	performance_diagnose, /**< Collect evidence for performance investigation. */
	performance_verify, /**< Collect evidence for repeated performance objective checks. */
	diagnostics_reproduce, /**< Collect evidence for reproducing reported errors. */
	visual_inspect, /**< Collect image and related UI evidence. */
	memory_diagnose, /**< Collect evidence for memory investigation. */
	startup_diagnose /**< Collect startup behavior evidence. */
};
/**
 * @brief Completion or availability state of one independently retained evidence section.
 */
enum class SectionStatus : std::uint8_t {
	complete, /**< All requested evidence is available. */
	partial, /**< Only some requested evidence is available. */
	not_requested, /**< This section was omitted by the request. */
	disabled, /**< Collection is disabled by effective configuration. */
	unsupported, /**< The producer does not support this evidence. */
	failed /**< Collection or materialization failed. */
};
/**
 * @brief Whether a requested evidence section is omitted, optional or required for admission.
 */
enum class SectionRequirement : std::uint8_t {
	omit, /**< Do not request this evidence section. */
	optional, /**< Collect when supported; report unavailability explicitly. */
	required /**< Reject admission when the required capability is unavailable. */
};
/**
 * @brief Capture lifecycle from queue admission through immutable publication or cancellation.
 */
enum class JobPhase : std::uint8_t {
	queued, /**< Admitted and waiting for execution. */
	preparing, /**< Preparing collection and scenario readiness. */
	warming_up, /**< Executing unmeasured warmup ticks. */
	recording, /**< Measuring the admitted workload. */
	finalizing, /**< Waiting for bounded producer evidence to finalize. */
	materializing, /**< Copying producer evidence into owned storage. */
	publishing, /**< Writing and validating immutable artifacts. */
	sealed, /**< Immutable capture publication completed. */
	canceling, /**< Cancellation and bounded cleanup are in progress. */
	canceled, /**< The operation was canceled. */
	failed /**< The operation failed with an explicit reason. */
};
/** @brief Bitmask of observer work that may affect the measured frame workload. */
enum class ObserverActivity : std::uint32_t {
	none = 0, /**< No observer activity reported. */
	developer_interface = 1u, /**< Built-in developer interface drawing. */
	picking_overlay = 2u, /**< Element picking overlay work. */
	tree_copy = 4u, /**< UI tree copying. */
	image_readback = 8u, /**< GPU image readback. */
	snapshot_copy = 16u, /**< Producer snapshot copying. */
	artifact_write = 32u, /**< Artifact serialization or writing. */
	validation = 64u, /**< Evidence validation. */
	deep_instrumentation = 128u /**< Detailed instrumentation work. */
};
/** @brief Combine independent flags without modifying either input. */
[[nodiscard]] constexpr ObserverActivity operator|(ObserverActivity first,
												   ObserverActivity second) noexcept {
	return static_cast<ObserverActivity>(static_cast<std::uint32_t>(first) |
										 static_cast<std::uint32_t>(second));
}
/** @brief Return whether at least one observer activity flag is present. */
[[nodiscard]] constexpr bool any(ObserverActivity activity) noexcept {
	return activity != ObserverActivity::none;
}
/**
 * @brief Return the static wire-format name of value; the returned view has static storage
 * lifetime.
 */
[[nodiscard]] std::string_view name(ExecutionMode value) noexcept;
/**
 * @brief Return the static wire-format name of value; the returned view has static storage
 * lifetime.
 */
[[nodiscard]] std::string_view name(CaptureProfile value) noexcept;
/**
 * @brief Return the static wire-format name of value; the returned view has static storage
 * lifetime.
 */
[[nodiscard]] std::string_view name(SectionStatus value) noexcept;
/**
 * @brief Return the static wire-format name of value; the returned view has static storage
 * lifetime.
 */
[[nodiscard]] std::string_view name(SectionRequirement value) noexcept;
/**
 * @brief Return the static wire-format name of value; the returned view has static storage
 * lifetime.
 */
[[nodiscard]] std::string_view name(JobPhase value) noexcept;

/** Half-open retained interval with independently reported uncertainty and loss. */
struct TickRange {
	/** @brief Inclusive lower bound of the application-tick interval. */
	std::uint64_t first_app_tick = 0;
	/** @brief Exclusive upper bound of the application-tick interval. */
	std::uint64_t end_app_tick_exclusive = 0;
};
/**
 * @brief Owned retained coverage intervals and explicit producer loss/finalization evidence.
 */
struct SectionCoverage {
	/** @brief Half-open application-tick intervals for which evidence is retained. */
	std::vector<TickRange> covered_ranges;
	/** @brief Half-open intervals for which requested evidence is missing. */
	std::vector<TickRange> gaps;
	/** @brief Owned explanations of missing or degraded section coverage. */
	std::vector<std::string> reasons;
	/** @brief Records produced for the requested interval before retention losses. */
	std::uint64_t producer_records = 0;
	/** @brief Producer records retained for this section at the cutoff. */
	std::uint64_t retained_records = 0;
	/**
	 * @brief Producer records lost before retention; never treated as measured zero activity.
	 */
	std::uint64_t dropped_records = 0;
	/** @brief Records removed from retained history before the requested cutoff. */
	std::uint64_t evicted_records = 0;
	/** @brief Producer records that have not finalized at the capture cutoff. */
	std::uint64_t pending_records = 0;
	/** @brief Producer generation used to distinguish resets or reconfiguration. */
	std::uint64_t generation = 0;
	/** @brief Producer watermark describing finalized evidence at the cutoff. */
	std::uint64_t finalization_watermark = 0;
	/** @brief Explicit section completion or availability state. */
	SectionStatus status = SectionStatus::not_requested;
	/** @brief Whether the producer explicitly reports uncertainty in section coverage. */
	bool uncertain = false;
};
/**
 * @brief Hard capture admission and evidence-store bounds; sizes are bytes and times nanoseconds.
 * Requests exceeding a bound fail explicitly before collection is admitted.
 */
struct AgenticLimits {
	/** @brief Maximum owned capture snapshot storage, in bytes. */
	std::uint64_t maximum_snapshot_bytes = 128u * 1024u * 1024u;
	/** @brief Maximum admitted artifact payload size, in bytes. */
	std::uint64_t maximum_artifact_bytes = 256u * 1024u * 1024u;
	/** @brief Maximum store disk reservation, in bytes. */
	std::uint64_t maximum_disk_bytes = 2ull * 1024u * 1024u * 1024u;
	/** @brief Maximum application ticks admitted by a capture reservation. */
	std::uint64_t maximum_ticks = 4096;
	/** @brief Maximum producer records admitted by a capture reservation. */
	std::uint64_t maximum_records = 100000;
	/** @brief Maximum image count admitted within the configured capture bounds. */
	std::uint64_t maximum_images = 8;
	/** @brief Maximum retained image payload bytes admitted by capture. */
	std::uint64_t maximum_image_bytes = 64u * 1024u * 1024u;
	/** @brief Maximum wait for producer finalization after measurement, in nanoseconds. */
	std::uint64_t finalization_timeout_ns = 2000000000ull;
	/** @brief Maximum pending jobs admitted by the service. */
	std::uint32_t maximum_queued_jobs = 8;
};
/**
 * @brief Agentic service startup policy, available with FLOW_UI_AGENTIC_WORKFLOW.
 * Unset enablement accepts launch handoff; explicit false keeps the service dormant.
 * Directories, worker threads and endpoints are created only when the service is enabled.
 */
struct AgenticWorkflowConfig {
	/**
	 * @brief Evidence-store root; use .flowui/agentic for repository-local runtime outputs.
	 * Empty accepts an enabled launch handoff path; enabled services require a resolved path.
	 */
	std::filesystem::path store_path;
	/** @brief Application identity recorded in run provenance. */
	std::string application_id;
	/** @brief Application build or source revision recorded in run provenance. */
	std::string application_revision;
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/** @brief Owned allowlisted build, workload and environment evidence. */
	Json provenance = Json::make_object();
	/**
	 * @brief Hard admission and retention bounds; exceeding them returns an explicit failure.
	 */
	AgenticLimits limits;
	/** @brief Unset accepts launch handoff; false explicitly keeps the service dormant. */
	std::optional<bool> enabled;
	/** @brief Unset accepts launch handoff; false explicitly disables the local endpoint. */
	std::optional<bool> local_attachment_enabled;
	/** @brief Whether image capture resources may be enabled before window creation. */
	bool visual_capture_enabled = false;
	/** @brief Permit frame-bound input replacement; evidence collection alone grants no control. */
	bool input_control_enabled = false;
	/** @brief Permit invocation of application-declared exported actions. */
	bool action_control_enabled = false;
};
/**
 * @brief Owned effective service capabilities; unsupported evidence remains explicit.
 */
struct AgenticCapabilities {
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/** @brief Application identity recorded in run provenance. */
	std::string application_id;
	/** @brief Execution backends currently supported by the service. */
	std::vector<ExecutionMode> modes;
	/** @brief Named capture policies currently supported by the service. */
	std::vector<CaptureProfile> profiles;
	/** @brief Explicit unsupported, missing or degraded evidence conditions. */
	std::vector<std::string> limitations;
	/** @brief Owned structured capability metadata. */
	Json details = Json::make_object();
	/**
	 * @brief Hard admission and retention bounds; exceeding them returns an explicit failure.
	 */
	AgenticLimits limits;
	/** @brief Compiled or effective timing collection detail advertised by the service. */
	std::uint32_t timing_level = 0;
	/** @brief Compiled or effective memory collection detail advertised by the service. */
	std::uint32_t memory_level = 0;
	/**
	 * @brief Compiled or effective error evidence collection level advertised by the service.
	 */
	std::uint32_t error_level = 0;
	/**
	 * @brief Whether this capability or service is enabled by the effective configuration.
	 */
	bool enabled = false;
	/** @brief Whether runtime diagnostic evidence is supported and available. */
	bool runtime_diagnostics = false;
	/** @brief Whether memory evidence is supported and available. */
	bool memory = false;
	/** @brief Whether captured image evidence is supported and available. */
	bool visual = false;
	/** @brief Whether copied UI tree evidence is supported. */
	bool trees = false;
	/** @brief Whether immutable theme evidence is supported and available. */
	bool themes = false;
	/** @brief Whether an authenticated local endpoint is available for this run. */
	bool local_attachment = false;
};
/** Job identities remain recoverable after client disconnect. */
using AgenticJobId = std::string;
/** @brief Owned current capture lifecycle and measurement progress snapshot. */
struct AgenticJobStatus {
	/** @brief Stable queued operation identity recoverable after client disconnect. */
	std::string job_id;
	/** @brief Immutable capture identity within its run. */
	std::string capture_id;
	/** @brief Filesystem location of the authoritative sealed manifest. */
	std::filesystem::path manifest_path;
	/**
	 * @brief Explicit machine-readable or diagnostic explanation of an outcome or limitation.
	 */
	std::string reason;
	/** @brief Number of application ticks measured so far. */
	std::uint64_t measured_ticks = 0;
	/** @brief Application ticks required by the admitted capture. */
	std::uint64_t required_ticks = 0;
	/** @brief Number of valid cadence intervals measured so far. */
	std::uint64_t measured_intervals = 0;
	/** @brief Valid cadence intervals required before the objective can be assessed. */
	std::uint64_t required_intervals = 0;
	/**
	 * @brief Current job or interaction lifecycle phase; separate from assertion verdict.
	 */
	JobPhase phase = JobPhase::queued;
	/**
	 * @brief Whether cancellation has been requested, without implying cleanup is complete.
	 */
	bool cancellation_requested = false;
};
/** Query metadata and owned records; pagination is bound to manifest and query digests. */
struct AgenticPage {
	/** @brief Owned bounded page records; continuation is identified by cursor. */
	Json records = Json::make_array();
	/** @brief Opaque pagination cursor bound to the manifest and query identities. */
	std::string cursor;
	/** @brief Digest of the immutable manifest to which this page is bound. */
	std::string manifest_digest;
	/** @brief Whether more matching records remain beyond this bounded page. */
	bool truncated = false;
};
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const SectionCoverage& coverage);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticCapabilities& capabilities);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticJobStatus& status);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticLimits& limits);

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
