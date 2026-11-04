#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"
#include "managers/structs/InputStructs.hpp"

#include <optional>
#include <span>

namespace FlowUi::devSystems::agentic {

/** Window-logical input state. Text and scroll are transient payloads of one selected frame. */
struct AgenticInputSnapshot {
	std::vector<char32_t> text;
	std::vector<std::uint16_t> keys_down;
	std::vector<std::uint8_t> buttons_down;
	double pointer_x = 0.0;
	double pointer_y = 0.0;
	double scroll_x = 0.0;
	double scroll_y = 0.0;
	bool pointer_inside = false;
	bool window_focused = true;
};

struct AgenticInteractionPoint {
	double x = 0.0;
	double y = 0.0;
};
struct AgenticInteractionRegion {
	double x = 0.0;
	double y = 0.0;
	double width = 0.0;
	double height = 0.0;
};
enum class AgenticTargetKind : std::uint8_t {
	coordinates,
	clay_id,
	clay_name,
	flow_instance,
	flow_debug,
	custom_region
};
/** Identity/region selector independent of widget types. Missing revisions mean latest layout. */
struct AgenticInteractionTarget {
	std::string clay_name;
	std::string flow_debug_name;
	std::string custom_id;
	std::optional<std::uint64_t> tree_generation;
	std::optional<std::uint64_t> layout_revision;
	std::optional<std::uint64_t> custom_revision;
	std::uint64_t window_id = 0;
	std::uint64_t clay_id = 0;
	std::uint64_t flow_instance_id = 0;
	AgenticInteractionPoint point;
	double anchor_x = 0.5;
	double anchor_y = 0.5;
	AgenticTargetKind kind = AgenticTargetKind::coordinates;
};
/** Owned result of revisioned geometry resolution; membership does not prove exclusive routing. */
struct AgenticResolvedInteractionTarget {
	std::string label;
	AgenticInteractionTarget selector;
	AgenticInteractionRegion bounds;
	AgenticInteractionRegion visible_region;
	AgenticInteractionPoint layout_point;
	AgenticInteractionPoint window_point;
	std::vector<std::uint64_t> matching_clay_ids;
	std::uint64_t tree_generation = 0;
	std::uint64_t layout_revision = 0;
	std::uint64_t clay_id = 0;
	std::uint64_t app_tick = 0;
	std::uint64_t window_frame = 0;
	std::uint64_t flow_instance_id = 0;
	std::uint64_t custom_revision = 0;
	std::uint32_t paint_order = 0;
	std::int32_t z_index = 0;
	double layout_to_window_x = 1.0;
	double layout_to_window_y = 1.0;
	bool hit_test_supported = false;
	bool hit_test_verified = false;
	bool floating = false;
};

enum class AgenticInteractionMode : std::uint8_t { raw_frames, common_controls };
enum class AgenticInteractionClock : std::uint8_t { window_input_frames, exact_window_frames };
enum class AgenticInteractionBaseline : std::uint8_t { neutral_before_start, require_neutral };
enum class AgenticInteractionPersistence : std::uint8_t { redact_text, retain_text, volatile_plan };
enum class AgenticInteractionOperation : std::uint8_t {
	pointer_move,
	pointer_down,
	pointer_up,
	pointer_hold,
	click,
	drag,
	scroll,
	key_down,
	key_up,
	key_chord,
	text_enter,
	wait,
	assert_observation
};
enum class AgenticInteractionExpectation : std::uint32_t {
	none = 0,
	target_hovered = 1u,
	target_pressed = 2u,
	target_held = 4u,
	target_released = 8u,
	layout_completed = 16u,
	frame_presented = 32u,
	window_focused = 64u,
	text_focused = 128u
};
[[nodiscard]] constexpr AgenticInteractionExpectation
operator|(AgenticInteractionExpectation first, AgenticInteractionExpectation second) noexcept {
	return static_cast<AgenticInteractionExpectation>(static_cast<std::uint32_t>(first) |
													  static_cast<std::uint32_t>(second));
}
[[nodiscard]] constexpr bool has_expectation(AgenticInteractionExpectation value,
											 AgenticInteractionExpectation expected) noexcept {
	return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(expected)) != 0;
}

/** Finite mechanical click recipe; down and release always occupy distinct input frames. */
struct AgenticClickRecipe {
	std::uint64_t prepare_up_frames = 1;
	std::uint64_t down_frames = 1;
	std::uint64_t hold_frames = 0;
	std::uint64_t up_frames = 1;
	std::uint64_t repeat = 1;
	std::uint64_t inter_click_gap_frames = 1;
	std::uint8_t button = 0;
};
/** Mechanical operation, optional geometry targets and independently checked observations. */
struct AgenticInteractionStep {
	std::vector<AgenticInteractionPoint> path;
	std::vector<std::uint16_t> keys;
	std::vector<char32_t> text;
	std::string step_id;
	std::optional<AgenticInteractionTarget> target;
	std::optional<AgenticInteractionTarget> destination;
	AgenticClickRecipe recipe;
	std::uint64_t frames = 1;
	std::uint64_t minimum_duration_ns = 0;
	std::uint64_t deadline_ns = 2'000'000'000ull;
	double scroll_x = 0.0;
	double scroll_y = 0.0;
	AgenticInteractionExpectation expect = AgenticInteractionExpectation::none;
	AgenticInteractionOperation operation = AgenticInteractionOperation::pointer_move;
	std::uint8_t button = 0;
	bool require_hover_before_down = true;
};
/** Explicit bounded repeated state. Transients apply only to the first frame of a span. */
struct AgenticInteractionSpan {
	AgenticInputSnapshot input;
	std::uint64_t offset = 0;
	std::uint64_t count = 1;
	std::uint64_t minimum_duration_ns = 0;
	std::uint32_t step_index = 0;
	double pointer_progress = 0.0;
	bool target_pointer = false;
	bool destination_pointer = false;
	bool resolve_before = false;
	bool require_hover_before = false;
	bool preserve_pointer = false;
};
/** Hard limits apply to decoded and compiled storage, independently of transport bounds. */
struct AgenticInteractionLimits {
	std::uint64_t maximum_frames = 4096;
	std::uint64_t maximum_spans = 4096;
	std::uint64_t maximum_steps = 256;
	std::uint64_t maximum_target_candidates = 128;
	std::uint64_t maximum_text_codepoints = 16384;
	std::uint64_t maximum_observations = 8192;
	std::uint64_t maximum_artifact_bytes = 16u * 1024u * 1024u;
	std::uint64_t maximum_duration_ns = 60'000'000'000ull;
};
/** Immutable source request. Raw spans must cover [0,frame_count) without gaps or overlaps. */
struct AgenticInteractionPlan {
	std::vector<AgenticInteractionSpan> spans;
	std::vector<AgenticInteractionStep> steps;
	std::string run_id;
	std::string fixture_id;
	std::string fixture_digest;
	std::string idempotency_key;
	AgenticInteractionLimits limits;
	std::uint64_t window_id = 0;
	std::uint64_t first_window_frame = 0;
	std::uint64_t seed = 0;
	std::uint64_t deadline_ns = 30'000'000'000ull;
	AgenticInteractionMode mode = AgenticInteractionMode::raw_frames;
	AgenticInteractionClock clock = AgenticInteractionClock::window_input_frames;
	AgenticInteractionBaseline baseline = AgenticInteractionBaseline::neutral_before_start;
	AgenticInteractionPersistence persistence = AgenticInteractionPersistence::redact_text;
	bool persistence_declared = false;
	bool include_native_input = false;
	bool record_observations = true;
};
/** Advertised mechanics and observations are independent capabilities. */
struct AgenticInteractionCapabilities {
	AgenticInteractionLimits limits;
	bool raw_frames = true;
	bool common_controls = true;
	bool geometry_targets = true;
	bool custom_targets = false;
	bool primary_observations = true;
	bool text_focus_observations = false;
	bool presented_observations = true;
	bool recording = true;
	bool exact_addresses = true;
};
/** Deterministic compiled workload. Relative offset zero is the first selected frame after
 * baseline. */
struct AgenticCompiledInteraction {
	AgenticInteractionPlan plan;
	std::vector<AgenticInteractionSpan> spans;
	std::vector<std::string> limitations;
	std::string digest;
	std::uint64_t frame_count = 0;
	std::uint64_t estimated_bytes = 0;
	std::uint64_t text_codepoints = 0;
	bool requires_geometry = false;
	bool requires_observations = false;
	bool cleanup_required = false;
	bool replayable_when_retained = true;
};

enum class AgenticInteractionPhase : std::uint8_t {
	queued,
	validating,
	acquiring_control,
	synchronizing,
	executing,
	draining_observations,
	releasing_control,
	completed,
	canceled,
	failed
};
enum class AgenticInteractionVerdict : std::uint8_t { pending, passed, failed, unavailable };
/** One actual delivery, separate from layout observation and presentation. */
struct AgenticInteractionReceipt {
	AgenticInputSnapshot effective_input;
	std::optional<AgenticInputSnapshot> native_input;
	std::optional<EvidenceFrameKey> frame;
	std::string plan_digest;
	std::string job_id;
	std::string run_id;
	std::string reason;
	std::uint64_t sequence = 0;
	std::uint64_t offset = 0;
	std::uint64_t selected_ns = 0;
	std::uint32_t step_index = 0;
	bool selected = false;
	bool synthetic_focus = false;
	bool missed = false;
	bool cleanup = false;
	bool baseline = false;
};
/** Genuine completed consumer facts; never populated from requested expectations. */
struct AgenticInteractionObservation {
	std::optional<Json> target_observation;
	std::vector<std::uint64_t> hovered;
	std::vector<std::uint64_t> pressed;
	std::vector<std::uint64_t> held;
	std::vector<std::uint64_t> released;
	std::optional<EvidenceFrameKey> frame;
	std::optional<AgenticResolvedInteractionTarget> target;
	std::string plan_digest;
	std::string job_id;
	std::string run_id;
	std::string reason;
	std::uint64_t sequence = 0;
	std::uint64_t offset = 0;
	std::uint64_t observed_ns = 0;
	std::uint32_t step_index = 0;
	bool layout_completed = false;
	bool presented = false;
	bool canceled = false;
	bool input_suppressed = false;
	bool window_focused = false;
	bool text_focus_supported = false;
	bool text_focused = false;
};
struct AgenticInteractionStepResult {
	std::string step_id;
	std::string reason;
	AgenticInteractionExpectation expected = AgenticInteractionExpectation::none;
	AgenticInteractionExpectation observed = AgenticInteractionExpectation::none;
	std::uint64_t first_offset = 0;
	std::uint64_t end_offset_exclusive = 0;
	AgenticInteractionVerdict verdict = AgenticInteractionVerdict::pending;
};
struct AgenticInteractionCoverage {
	std::uint64_t scheduled = 0;
	std::uint64_t consumed = 0;
	std::uint64_t missed = 0;
	std::uint64_t canceled = 0;
	std::uint64_t observed = 0;
	std::uint64_t asserted = 0;
	std::uint64_t retained_inputs = 0;
	std::uint64_t retained_observations = 0;
	std::uint64_t lost = 0;
};
struct AgenticInteractionResult {
	std::vector<AgenticInteractionStepResult> steps;
	std::vector<std::string> limitations;
	std::optional<EvidenceFrameKey> activation_frame;
	std::optional<std::uint64_t> first_divergence_offset;
	std::string plan_digest;
	std::string job_id;
	std::string run_id;
	std::string reason;
	AgenticInteractionCoverage coverage;
	std::uint64_t window_id = 0;
	AgenticInteractionPhase phase = AgenticInteractionPhase::queued;
	AgenticInteractionVerdict assertion_verdict = AgenticInteractionVerdict::pending;
	bool execution_succeeded = false;
	bool evidence_complete = true;
	bool cleanup_complete = false;
	bool handoff_complete = false;
};

/** Decode strict finite wire data; rejected unknown fields cannot silently change semantics. */
[[nodiscard]] Result<AgenticInteractionPlan> interaction_plan_from_json(const Json& input) noexcept;
/** Validate, estimate and deterministically compile mechanical operations without App access. */
[[nodiscard]] Result<AgenticCompiledInteraction>
compile_interaction_plan(const AgenticInteractionPlan& plan,
						 const AgenticInteractionCapabilities& capabilities = {}) noexcept;
/** Semantic workload identity includes original text; relative run bindings and retry keys are
 * excluded. */
[[nodiscard]] Result<std::string>
interaction_plan_digest(const AgenticInteractionPlan& plan) noexcept;
/** Return complete state at an offset; repeated spans clear first-frame text/scroll transients. */
[[nodiscard]] Result<AgenticInputSnapshot>
interaction_input_at(const AgenticCompiledInteraction& plan, std::uint64_t offset) noexcept;
/** Preserve real clock/loss counters and derive modifiers from GLFW-compatible effective key slots.
 */
[[nodiscard]] Result<FrameInput>
materialize_interaction_input(const AgenticInputSnapshot& snapshot,
							  const FrameInput& native_input) noexcept;
/** Own an assessed native snapshot before asynchronous recording. */
[[nodiscard]] Result<AgenticInputSnapshot>
snapshot_interaction_input(const FrameInput& input) noexcept;
/** Validate typed complete state, including sorted unique supported GLFW key slots and Unicode. */
[[nodiscard]] Status
validate_interaction_input(const AgenticInputSnapshot& input,
						   std::uint64_t maximum_text_codepoints = 16384) noexcept;
/** Evaluate only facts actually present in a completed observation. */
[[nodiscard]] AgenticInteractionExpectation
observed_interaction_facts(const AgenticInteractionObservation& observation,
						   std::uint64_t target_clay_id) noexcept;
[[nodiscard]] std::string_view name(AgenticInteractionPhase value) noexcept;
[[nodiscard]] std::string_view name(AgenticInteractionVerdict value) noexcept;
[[nodiscard]] std::string_view name(AgenticInteractionOperation value) noexcept;
[[nodiscard]] Json to_json(const AgenticInputSnapshot& value, bool include_text = true);
[[nodiscard]] Json to_json(const AgenticInteractionTarget& value);
[[nodiscard]] Json to_json(const AgenticResolvedInteractionTarget& value);
[[nodiscard]] Json to_json(const AgenticInteractionPlan& value, bool include_text = true);
[[nodiscard]] Json to_json(const AgenticCompiledInteraction& value, bool include_text = true);
[[nodiscard]] Json to_json(const AgenticInteractionCapabilities& value);
[[nodiscard]] Json to_json(const AgenticInteractionReceipt& value, bool include_text = false);
[[nodiscard]] Json to_json(const AgenticInteractionObservation& value);
[[nodiscard]] Json to_json(const AgenticInteractionResult& value);

} // namespace FlowUi::devSystems::agentic
#endif
