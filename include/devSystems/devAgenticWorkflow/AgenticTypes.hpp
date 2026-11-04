#pragma once
#include "FlowUi/BuildConfig.hpp"
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

inline constexpr std::string_view agentic_schema_version = "1.0";
/** Local errors are carried by the canonical FlowUi result model. */
enum class AgenticError : std::uint8_t {
	invalid_request,
	unavailable,
	busy,
	stale_generation,
	invalid_scope,
	budget_exhausted,
	incomplete,
	publication_failed,
	incompatible,
	canceled,
	timeout,
	integrity_failed
};
[[nodiscard]] FlowUiError agentic_error(AgenticError error) noexcept;
/** Opaque, validated store/run/job/capture labels, never path fragments supplied unchecked. */
[[nodiscard]] bool valid_identifier(std::string_view identifier) noexcept;
[[nodiscard]] Result<std::string> make_identifier(std::string_view prefix) noexcept;
[[nodiscard]] std::uint64_t monotonic_nanoseconds() noexcept;
[[nodiscard]] std::string utc_timestamp();

/** Exact producer-supplied evidence key; slots are independent resource indices. */
struct EvidenceFrameKey {
	std::string run_id;
	std::uint64_t app_tick = 0;
	std::uint64_t window_frame = 0;
	std::uint64_t window_id = 0;
};

enum class ExecutionMode : std::uint8_t {
	offline,
	cpu_scenario,
	layout_scenario,
	offscreen_vulkan,
	windowed
};
enum class CaptureProfile : std::uint8_t {
	performance_diagnose,
	performance_verify,
	diagnostics_reproduce,
	visual_inspect,
	memory_diagnose,
	startup_diagnose
};
enum class SectionStatus : std::uint8_t {
	complete,
	partial,
	not_requested,
	disabled,
	unsupported,
	failed
};
enum class SectionRequirement : std::uint8_t { omit, optional, required };
enum class JobPhase : std::uint8_t {
	queued,
	preparing,
	warming_up,
	recording,
	finalizing,
	materializing,
	publishing,
	sealed,
	canceling,
	canceled,
	failed
};
enum class ObserverActivity : std::uint32_t {
	none = 0,
	developer_interface = 1u,
	picking_overlay = 2u,
	tree_copy = 4u,
	image_readback = 8u,
	snapshot_copy = 16u,
	artifact_write = 32u,
	validation = 64u,
	deep_instrumentation = 128u
};
[[nodiscard]] constexpr ObserverActivity operator|(ObserverActivity first,
												   ObserverActivity second) noexcept {
	return static_cast<ObserverActivity>(static_cast<std::uint32_t>(first) |
										 static_cast<std::uint32_t>(second));
}
[[nodiscard]] constexpr bool any(ObserverActivity activity) noexcept {
	return activity != ObserverActivity::none;
}
[[nodiscard]] std::string_view name(ExecutionMode value) noexcept;
[[nodiscard]] std::string_view name(CaptureProfile value) noexcept;
[[nodiscard]] std::string_view name(SectionStatus value) noexcept;
[[nodiscard]] std::string_view name(SectionRequirement value) noexcept;
[[nodiscard]] std::string_view name(JobPhase value) noexcept;

/** Half-open retained interval with independently reported uncertainty and loss. */
struct TickRange {
	std::uint64_t first_app_tick = 0;
	std::uint64_t end_app_tick_exclusive = 0;
};
struct SectionCoverage {
	std::vector<TickRange> covered_ranges;
	std::vector<TickRange> gaps;
	std::vector<std::string> reasons;
	std::uint64_t producer_records = 0;
	std::uint64_t retained_records = 0;
	std::uint64_t dropped_records = 0;
	std::uint64_t evicted_records = 0;
	std::uint64_t pending_records = 0;
	std::uint64_t generation = 0;
	std::uint64_t finalization_watermark = 0;
	SectionStatus status = SectionStatus::not_requested;
	bool uncertain = false;
};
/** Hard bounds reserved before admitting collection. */
struct AgenticLimits {
	std::uint64_t maximum_snapshot_bytes = 128u * 1024u * 1024u;
	std::uint64_t maximum_artifact_bytes = 256u * 1024u * 1024u;
	std::uint64_t maximum_disk_bytes = 2ull * 1024u * 1024u * 1024u;
	std::uint64_t maximum_ticks = 4096;
	std::uint64_t maximum_records = 100000;
	std::uint64_t maximum_images = 8;
	std::uint64_t maximum_image_bytes = 64u * 1024u * 1024u;
	std::uint64_t finalization_timeout_ns = 2000000000ull;
	std::uint32_t maximum_queued_jobs = 8;
};
/** Only explicitly enabled services create directories, threads or endpoints. */
struct AgenticWorkflowConfig {
	std::filesystem::path store_path;
	std::string application_id;
	std::string application_revision;
	std::string run_id;
	Json provenance = Json::make_object();
	AgenticLimits limits;
	/** Unset accepts launch handoff; false explicitly keeps the service dormant. */
	std::optional<bool> enabled;
	/** Unset accepts launch handoff; false explicitly disables the endpoint. */
	std::optional<bool> local_attachment_enabled;
	bool visual_capture_enabled = false;
	/** Permit frame-bound input replacement; collection alone never grants input control. */
	bool input_control_enabled = false;
	/** Permit invocation of application-declared exported actions. */
	bool action_control_enabled = false;
};
struct AgenticCapabilities {
	std::string run_id;
	std::string application_id;
	std::vector<ExecutionMode> modes;
	std::vector<CaptureProfile> profiles;
	std::vector<std::string> limitations;
	Json details = Json::make_object();
	AgenticLimits limits;
	std::uint32_t timing_level = 0;
	std::uint32_t memory_level = 0;
	std::uint32_t error_level = 0;
	bool enabled = false;
	bool runtime_diagnostics = false;
	bool memory = false;
	bool visual = false;
	bool trees = false;
	bool themes = false;
	bool local_attachment = false;
};
/** Job identities remain recoverable after client disconnect. */
using AgenticJobId = std::string;
struct AgenticJobStatus {
	std::string job_id;
	std::string capture_id;
	std::filesystem::path manifest_path;
	std::string reason;
	std::uint64_t measured_ticks = 0;
	std::uint64_t required_ticks = 0;
	std::uint64_t measured_intervals = 0;
	std::uint64_t required_intervals = 0;
	JobPhase phase = JobPhase::queued;
	bool cancellation_requested = false;
};
/** Query metadata and owned records; pagination is bound to manifest and query digests. */
struct AgenticPage {
	Json records = Json::make_array();
	std::string cursor;
	std::string manifest_digest;
	bool truncated = false;
};
[[nodiscard]] Json to_json(const SectionCoverage& coverage);
[[nodiscard]] Json to_json(const AgenticCapabilities& capabilities);
[[nodiscard]] Json to_json(const AgenticJobStatus& status);
[[nodiscard]] Json to_json(const AgenticLimits& limits);

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
