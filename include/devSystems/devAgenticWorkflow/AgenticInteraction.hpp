#pragma once

/** @file
 * @brief Public AgenticInteraction declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"

#include <optional>
#include <span>

namespace FlowUi::devSystems::agentic {

/** Window-logical input state. Text and scroll are transient payloads of one selected frame. */
struct AgenticInputSnapshot {
	/**
	 * @brief Owned Unicode scalar input payload; text is delivered once on the first selected
	 * frame of a span.
	 */
	std::vector<char32_t> text;
	/** @brief Sorted unique GLFW-compatible key slots held during this input frame. */
	std::vector<std::uint16_t> keys_down;
	/** @brief Sorted unique GLFW-compatible mouse button slots held during this frame. */
	std::vector<std::uint8_t> buttons_down;
	/** @brief Horizontal pointer coordinate in window logical units. */
	double pointer_x = 0.0;
	/** @brief Vertical pointer coordinate in window logical units. */
	double pointer_y = 0.0;
	/** @brief Horizontal scroll delta delivered once on the first selected span frame. */
	double scroll_x = 0.0;
	/** @brief Vertical scroll delta delivered once on the first selected span frame. */
	double scroll_y = 0.0;
	/** @brief Whether the pointer is inside the target window. */
	bool pointer_inside = false;
	/**
	 * @brief Whether the consumer window is focused in the selected or observed input state.
	 */
	bool window_focused = true;
};

/**
 * @brief Point in the coordinate system declared by the containing selector or resolved target.
 */
struct AgenticInteractionPoint {
	/**
	 * @brief Horizontal coordinate in the containing selector or resolved target coordinate
	 * system.
	 */
	double x = 0.0;
	/**
	 * @brief Vertical coordinate in the containing selector or resolved target coordinate system.
	 */
	double y = 0.0;
};
/** @brief Axis-aligned target rectangle in layout coordinates. */
struct AgenticInteractionRegion {
	/** @brief Left edge in layout coordinates. */
	double x = 0.0;
	/** @brief Top edge in layout coordinates. */
	double y = 0.0;
	/** @brief Region width in layout coordinates. */
	double width = 0.0;
	/** @brief Region height in layout coordinates. */
	double height = 0.0;
};
/**
 * @brief How a target selector identifies explicit coordinates, a completed Clay/Flow node or a
 * custom region.
 */
enum class AgenticTargetKind : std::uint8_t {
	coordinates, /**< Use an explicit window logical point. */
	clay_id, /**< Resolve a Clay element ID. */
	clay_name, /**< Resolve an exact Clay element name. */
	flow_instance, /**< Resolve a 64-bit Flow element instance ID. */
	flow_debug, /**< Resolve an exact Flow debug name. */
	custom_region /**< Resolve a revisioned application-declared region. */
};
/** Identity/region selector independent of widget types. Missing revisions mean latest layout. */
struct AgenticInteractionTarget {
	/** @brief Clay element name used by a name-based target selector. */
	std::string clay_name;
	/** @brief Flow debug name used by a debug-name target selector. */
	std::string flow_debug_name;
	/** @brief Application-declared custom target identity used by this selector. */
	std::string custom_id;
	/** @brief Expected or observed UI tree generation, independent of layout revision. */
	std::optional<std::uint64_t> tree_generation;
	/**
	 * @brief Expected or observed completed-layout revision; absent selectors use current layout.
	 */
	std::optional<std::uint64_t> layout_revision;
	/**
	 * @brief Expected or observed custom target revision; absent selectors use the current
	 * revision.
	 */
	std::optional<std::uint64_t> custom_revision;
	/**
	 * @brief Stable application window identity associated with this request or evidence.
	 */
	std::uint64_t window_id = 0;
	/**
	 * @brief Resolved Clay element identity; distinct from the 64-bit Flow instance identity.
	 */
	std::uint64_t clay_id = 0;
	/** @brief 64-bit Flow element instance identity, separate from Clay identity. */
	std::uint64_t flow_instance_id = 0;
	/** @brief Explicit target location for coordinate-based selectors. */
	AgenticInteractionPoint point;
	/** @brief Horizontal target anchor as a fraction of the resolved region width. */
	double anchor_x = 0.5;
	/** @brief Vertical target anchor as a fraction of the resolved region height. */
	double anchor_y = 0.5;
	/** @brief Selector domain determining which identity fields are interpreted. */
	AgenticTargetKind kind = AgenticTargetKind::coordinates;
};
/** Owned result of revisioned geometry resolution; membership does not prove exclusive routing. */
struct AgenticResolvedInteractionTarget {
	/** @brief Application-supplied target label; persistence follows the content policy. */
	std::string label;
	/** @brief Owned source selector whose geometry was resolved. */
	AgenticInteractionTarget selector;
	/** @brief Resolved or declared target rectangle in layout coordinates. */
	AgenticInteractionRegion bounds;
	/** @brief Resolved target bounds after known clipping. */
	AgenticInteractionRegion visible_region;
	/** @brief Resolved pointer location in layout coordinates. */
	AgenticInteractionPoint layout_point;
	/** @brief Resolved pointer location in window logical coordinates. */
	AgenticInteractionPoint window_point;
	/** @brief Owned Clay IDs matching this resolved selector. */
	std::vector<std::uint64_t> matching_clay_ids;
	/** @brief Expected or observed UI tree generation, independent of layout revision. */
	std::uint64_t tree_generation = 0;
	/** @brief Completed-layout revision used when resolving this target. */
	std::uint64_t layout_revision = 0;
	/**
	 * @brief Resolved Clay element identity; distinct from the 64-bit Flow instance identity.
	 */
	std::uint64_t clay_id = 0;
	/**
	 * @brief Producer-supplied application tick identity; independent of window frame and
	 * resource slot.
	 */
	std::uint64_t app_tick = 0;
	/**
	 * @brief Producer-supplied frame identity within one window; distinct from reusable slots.
	 */
	std::uint64_t window_frame = 0;
	/** @brief 64-bit Flow element instance identity, separate from Clay identity. */
	std::uint64_t flow_instance_id = 0;
	/** @brief Observed application-declared target revision; zero when not applicable. */
	std::uint64_t custom_revision = 0;
	/** @brief Completed-layout paint ordering used to distinguish overlapping targets. */
	std::uint32_t paint_order = 0;
	/** @brief Completed-layout stacking index used to distinguish overlapping targets. */
	std::int32_t z_index = 0;
	/**
	 * @brief Horizontal multiplier from layout coordinates to window logical coordinates.
	 */
	double layout_to_window_x = 1.0;
	/** @brief Vertical multiplier from layout coordinates to window logical coordinates. */
	double layout_to_window_y = 1.0;
	/** @brief Whether this target supports completed-layout hit-test evidence. */
	bool hit_test_supported = false;
	/** @brief Whether the resolved point passed the supported hit-test check. */
	bool hit_test_verified = false;
	/** @brief Whether the resolved target belongs to a floating layout surface. */
	bool floating = false;
};

/**
 * @brief Whether a plan contains explicit input spans or mechanical operations compiled into
 * spans.
 */
enum class AgenticInteractionMode : std::uint8_t {
	raw_frames, /**< Consume explicit complete-state input spans. */
	common_controls /**< Compile mechanical steps into explicit input spans. */
};
/**
 * @brief Whether offsets count selected input frames or exact producer window-frame identities.
 */
enum class AgenticInteractionClock : std::uint8_t {
	window_input_frames, /**< Count frames whose assessed input is selected. */
	exact_window_frames /**< Bind offsets to exact producer window-frame identities. */
};
/** @brief Neutral input policy applied before the first plan frame acquires control. */
enum class AgenticInteractionBaseline : std::uint8_t {
	neutral_before_start, /**< Deliver neutral input before workload execution. */
	require_neutral /**< Require an already neutral baseline before execution. */
};
/**
 * @brief Whether interaction text is redacted, retained for replay or kept only in the live plan.
 */
enum class AgenticInteractionPersistence : std::uint8_t {
	redact_text, /**< Omit text from retained execution evidence. */
	retain_text, /**< Retain text explicitly for reproducible replay. */
	volatile_plan /**< Keep source plan content only in live memory. */
};
/** @brief Supported mechanical input or observation assertion operation. */
enum class AgenticInteractionOperation : std::uint8_t {
	pointer_move, /**< Move the pointer to the resolved point. */
	pointer_down, /**< Press the selected mouse button. */
	pointer_up, /**< Release the selected mouse button. */
	pointer_hold, /**< Maintain the selected mouse button state. */
	click, /**< Compile released/down/hold/up click frames. */
	drag, /**< Move the pointer while holding the selected button. */
	scroll, /**< Deliver a transient scroll delta. */
	key_down, /**< Press the declared key slots. */
	key_up, /**< Release the declared key slots. */
	key_chord, /**< Compile a finite key chord press and release. */
	text_enter, /**< Deliver Unicode scalar text. */
	wait, /**< Advance input frames without a new action. */
	assert_observation /**< Evaluate required facts from genuine consumer observations. */
};
/** @brief Bitmask of genuine consumer facts required by an interaction assertion. */
enum class AgenticInteractionExpectation : std::uint32_t {
	none = 0, /**< Require no observation flags. */
	target_hovered = 1u, /**< Target appears in actual hovered membership. */
	target_pressed = 2u, /**< Target appears in actual pressed membership. */
	target_held = 4u, /**< Target appears in actual held membership. */
	target_released = 8u, /**< Target appears in actual released membership. */
	layout_completed = 16u, /**< A completed consumer layout was observed. */
	frame_presented = 32u, /**< The selected frame was reported presented. */
	window_focused = 64u, /**< The selected window was observed focused. */
	text_focused = 128u /**< Text input focus was genuinely observed. */
};
/** @brief Combine independent flags without modifying either input. */
[[nodiscard]] constexpr AgenticInteractionExpectation
operator|(AgenticInteractionExpectation first, AgenticInteractionExpectation second) noexcept {
	return static_cast<AgenticInteractionExpectation>(static_cast<std::uint32_t>(first) |
													  static_cast<std::uint32_t>(second));
}
/** @brief Return whether value contains at least one flag from expected. */
[[nodiscard]] constexpr bool has_expectation(AgenticInteractionExpectation value,
											 AgenticInteractionExpectation expected) noexcept {
	return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(expected)) != 0;
}

/** Finite mechanical click recipe; down and release always occupy distinct input frames. */
struct AgenticClickRecipe {
	/** @brief Released input frames emitted before the first click press. */
	std::uint64_t prepare_up_frames = 1;
	/** @brief Input frames with the selected button down before optional holding. */
	std::uint64_t down_frames = 1;
	/** @brief Additional selected input frames holding the button down. */
	std::uint64_t hold_frames = 0;
	/** @brief Released input frames emitted after each click press/hold sequence. */
	std::uint64_t up_frames = 1;
	/** @brief Number of clicks generated by the finite click recipe. */
	std::uint64_t repeat = 1;
	/** @brief Released input frames separating repeated clicks. */
	std::uint64_t inter_click_gap_frames = 1;
	/** @brief GLFW-compatible mouse button slot used by the operation. */
	std::uint8_t button = 0;
};
/** Mechanical operation, optional geometry targets and independently checked observations. */
struct AgenticInteractionStep {
	/**
	 * @brief Owned drag path points in window logical coordinates, following the resolved source
	 * target.
	 */
	std::vector<AgenticInteractionPoint> path;
	/** @brief GLFW-compatible key slots participating in the mechanical operation. */
	std::vector<std::uint16_t> keys;
	/**
	 * @brief Owned Unicode scalar input payload; text is delivered once on the first selected
	 * frame of a span.
	 */
	std::vector<char32_t> text;
	/** @brief Application-declared semantic step identity. */
	std::string step_id;
	/** @brief Optional source geometry target resolved against completed layout. */
	std::optional<AgenticInteractionTarget> target;
	/** @brief Optional second geometry target, such as the endpoint of a drag. */
	std::optional<AgenticInteractionTarget> destination;
	/** @brief Finite released/down/hold/up sequence used to compile click operations. */
	AgenticClickRecipe recipe;
	/** @brief Selected input frames occupied by this mechanical step. */
	std::uint64_t frames = 1;
	/**
	 * @brief Minimum elapsed step duration in nanoseconds before proceeding; nonzero values are
	 * supported only for wait and pointer_hold.
	 */
	std::uint64_t minimum_duration_ns = 0;
	/** @brief Relative operation or scenario deadline duration, in nanoseconds. */
	std::uint64_t deadline_ns = 2'000'000'000ull;
	/** @brief Horizontal scroll delta delivered once on the first selected span frame. */
	double scroll_x = 0.0;
	/** @brief Vertical scroll delta delivered once on the first selected span frame. */
	double scroll_y = 0.0;
	/** @brief Observation flags the step requires from actual consumer evidence. */
	AgenticInteractionExpectation expect = AgenticInteractionExpectation::none;
	/** @brief Mechanical input action or consumer-observation assertion to compile. */
	AgenticInteractionOperation operation = AgenticInteractionOperation::pointer_move;
	/** @brief GLFW-compatible mouse button slot used by the operation. */
	std::uint8_t button = 0;
	/** @brief Whether pressing requires a genuine completed hover observation first. */
	bool require_hover_before_down = true;
};
/** Explicit bounded repeated state. Transients apply only to the first frame of a span. */
struct AgenticInteractionSpan {
	/**
	 * @brief Complete held input state; transient text and scroll apply only to the first span
	 * frame.
	 */
	AgenticInputSnapshot input;
	/**
	 * @brief Relative selected input-frame offset; zero begins after baseline synchronization.
	 */
	std::uint64_t offset = 0;
	/** @brief Number of consecutive frames represented by this input span. */
	std::uint64_t count = 1;
	/**
	 * @brief Minimum elapsed step duration in nanoseconds before proceeding; nonzero values are
	 * supported only for wait and pointer_hold.
	 */
	std::uint64_t minimum_duration_ns = 0;
	/**
	 * @brief Zero-based source step index associated with this compiled span or evidence record.
	 */
	std::uint32_t step_index = 0;
	/** @brief Interpolation fraction along the compiled pointer path. */
	double pointer_progress = 0.0;
	/** @brief Whether this span resolves the pointer against the source target selector. */
	bool target_pointer = false;
	/** @brief Whether this span resolves the pointer against the destination selector. */
	bool destination_pointer = false;
	/** @brief Whether target geometry is resolved before this compiled span begins. */
	bool resolve_before = false;
	/**
	 * @brief Whether this compiled span requires observed target hover before proceeding.
	 */
	bool require_hover_before = false;
	/** @brief Whether this span keeps the prior effective pointer position. */
	bool preserve_pointer = false;
};
/**
 * @brief Per-plan interaction limits for decoded and compiled storage, separate from transport bounds.
 * Counts bound frames, spans, steps and observations; artifact sizes are bytes and times nanoseconds.
 */
struct AgenticInteractionLimits {
	/** @brief Maximum selected input frames admitted by a plan. */
	std::uint64_t maximum_frames = 4096;
	/** @brief Maximum compiled input spans admitted by a plan. */
	std::uint64_t maximum_spans = 4096;
	/** @brief Maximum mechanical steps admitted by a plan. */
	std::uint64_t maximum_steps = 256;
	/** @brief Maximum matching geometry candidates considered during target resolution. */
	std::uint64_t maximum_target_candidates = 128;
	/** @brief Maximum Unicode scalar values admitted as interaction text. */
	std::uint64_t maximum_text_codepoints = 16384;
	/** @brief Maximum retained interaction observations. */
	std::uint64_t maximum_observations = 8192;
	/** @brief Maximum admitted artifact payload size, in bytes. */
	std::uint64_t maximum_artifact_bytes = 16u * 1024u * 1024u;
	/** @brief Maximum compiled workload duration, in nanoseconds. */
	std::uint64_t maximum_duration_ns = 60'000'000'000ull;
};
/** Immutable source request. Raw spans must cover [0,frame_count) without gaps or overlaps. */
struct AgenticInteractionPlan {
	/** @brief Owned bounded complete-state input spans. */
	std::vector<AgenticInteractionSpan> spans;
	/** @brief Owned mechanical source steps used in common_controls mode. */
	std::vector<AgenticInteractionStep> steps;
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/** @brief Application-declared fixture identity used to reproduce the workload. */
	std::string fixture_id;
	/** @brief Content identity of the fixture used to reproduce the workload. */
	std::string fixture_digest;
	/**
	 * @brief Caller retry identity used to avoid duplicate admission; excluded from workload
	 * identity.
	 */
	std::string idempotency_key;
	/**
	 * @brief Hard admission and retention bounds; exceeding them returns an explicit failure.
	 */
	AgenticInteractionLimits limits;
	/**
	 * @brief Stable application window identity associated with this request or evidence.
	 */
	std::uint64_t window_id = 0;
	/**
	 * @brief Requested first exact window-frame address, when using the exact-frame clock.
	 */
	std::uint64_t first_window_frame = 0;
	/** @brief Deterministic application-supplied workload seed. */
	std::uint64_t seed = 0;
	/** @brief Relative operation or scenario deadline duration, in nanoseconds. */
	std::uint64_t deadline_ns = 30'000'000'000ull;
	/** @brief Whether source spans are explicit or compiled from mechanical steps. */
	AgenticInteractionMode mode = AgenticInteractionMode::raw_frames;
	/** @brief Clock defining how plan offsets bind to selected window frames. */
	AgenticInteractionClock clock = AgenticInteractionClock::window_input_frames;
	/**
	 * @brief Neutral-input synchronization policy required before selecting the first workload
	 * frame.
	 */
	AgenticInteractionBaseline baseline = AgenticInteractionBaseline::neutral_before_start;
	/** @brief Policy controlling retained interaction text and replay availability. */
	AgenticInteractionPersistence persistence = AgenticInteractionPersistence::redact_text;
	/** @brief Whether the request explicitly selected an interaction persistence policy. */
	bool persistence_declared = false;
	/**
	 * @brief Whether receipts retain a copied native-input snapshot alongside effective input.
	 */
	bool include_native_input = false;
	/** @brief Whether genuine consumer observations are retained during execution. */
	bool record_observations = true;
};
/** Advertised mechanics and observations are independent capabilities. */
struct AgenticInteractionCapabilities {
	/**
	 * @brief Hard admission and retention bounds; exceeding them returns an explicit failure.
	 */
	AgenticInteractionLimits limits;
	/** @brief Whether explicit complete-state frame spans are supported. */
	bool raw_frames = true;
	/**
	 * @brief Whether high-level mechanical operations such as click and drag are supported.
	 */
	bool common_controls = true;
	/** @brief Whether completed layout geometry can resolve interaction selectors. */
	bool geometry_targets = true;
	/** @brief Whether application-declared custom region targets are supported. */
	bool custom_targets = false;
	/** @brief Whether primary pointer interaction membership is observable. */
	bool primary_observations = true;
	/** @brief Whether genuine focused-text-field observations are supported. */
	bool text_focus_observations = false;
	/** @brief Whether completed frame presentation evidence is supported. */
	bool presented_observations = true;
	/** @brief Whether interaction execution evidence recording is supported. */
	bool recording = true;
	/** @brief Whether exact window-frame addressing is supported. */
	bool exact_addresses = true;
};
/** Deterministic compiled workload. Relative offset zero is the first selected frame after
 * baseline. */
struct AgenticCompiledInteraction {
	/** @brief Owned source plan from which this compiled workload was derived. */
	AgenticInteractionPlan plan;
	/** @brief Owned bounded complete-state input spans. */
	std::vector<AgenticInteractionSpan> spans;
	/** @brief Explicit unsupported, missing or degraded evidence conditions. */
	std::vector<std::string> limitations;
	/**
	 * @brief Content digest used to bind immutable evidence or compiled workload identity.
	 */
	std::string digest;
	/** @brief Total selected input frames in the compiled workload. */
	std::uint64_t frame_count = 0;
	/** @brief Estimated owned compiled-plan storage, in bytes. */
	std::uint64_t estimated_bytes = 0;
	/** @brief Number of Unicode scalar values in the compiled interaction text. */
	std::uint64_t text_codepoints = 0;
	/** @brief Whether execution must resolve completed layout geometry. */
	bool requires_geometry = false;
	/** @brief Whether execution requires genuine consumer observations to proceed. */
	bool requires_observations = false;
	/**
	 * @brief Whether execution needs a neutral release sequence after the compiled workload.
	 */
	bool cleanup_required = false;
	/** @brief Whether retained workload content suffices to replay this compiled plan. */
	bool replayable_when_retained = true;
};

/**
 * @brief Execution lifecycle; completion is separate from assertion and evidence outcomes.
 */
enum class AgenticInteractionPhase : std::uint8_t {
	queued, /**< Waiting for execution admission. */
	validating, /**< Checking finite input and capability bounds. */
	acquiring_control, /**< Acquiring the exclusive execution lease. */
	synchronizing, /**< Establishing the declared neutral baseline. */
	executing, /**< Selecting compiled input frames. */
	draining_observations, /**< Waiting for bounded completed consumer evidence. */
	releasing_control, /**< Delivering neutral release and native-input handoff. */
	completed, /**< Mechanical execution completed. */
	canceled, /**< Execution was canceled. */
	failed /**< Execution failed with an explicit reason. */
};
/**
 * @brief Assertion outcome; unavailable represents missing evidence rather than an observed
 * mismatch.
 */
enum class AgenticInteractionVerdict : std::uint8_t {
	pending, /**< Required assertions have not completed. */
	passed, /**< All required observed facts matched. */
	failed, /**< A definite observed mismatch occurred. */
	unavailable /**< Required facts could not be observed. */
};
/** One actual delivery, separate from layout observation and presentation. */
struct AgenticInteractionReceipt {
	/** @brief Owned complete input state actually selected for the consumer frame. */
	AgenticInputSnapshot effective_input;
	/** @brief Optional owned copy of native input before executor replacement. */
	std::optional<AgenticInputSnapshot> native_input;
	/** @brief Exact consumer frame identity, when a frame was actually observed. */
	std::optional<EvidenceFrameKey> frame;
	/**
	 * @brief Semantic compiled workload digest, independent of retries and relative run binding.
	 */
	std::string plan_digest;
	/** @brief Stable queued operation identity recoverable after client disconnect. */
	std::string job_id;
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/**
	 * @brief Explicit machine-readable or diagnostic explanation of an outcome or limitation.
	 */
	std::string reason;
	/** @brief Monotonic evidence record sequence assigned by the executor. */
	std::uint64_t sequence = 0;
	/**
	 * @brief Relative selected input-frame offset; zero begins after baseline synchronization.
	 */
	std::uint64_t offset = 0;
	/** @brief Monotonic input selection timestamp, in nanoseconds. */
	std::uint64_t selected_ns = 0;
	/**
	 * @brief Zero-based source step index associated with this compiled span or evidence record.
	 */
	std::uint32_t step_index = 0;
	/**
	 * @brief Whether this receipt represents input actually selected for a consumer frame.
	 */
	bool selected = false;
	/** @brief Whether execution supplied synthetic focus in the effective input state. */
	bool synthetic_focus = false;
	/** @brief Whether this delivery missed its required selected input frame. */
	bool missed = false;
	/**
	 * @brief Whether this receipt represents a neutral-input release delivery during executor
	 * cleanup.
	 */
	bool cleanup = false;
	/**
	 * @brief Whether this delivery belongs to neutral input synchronization before execution.
	 */
	bool baseline = false;
};
/** Genuine completed consumer facts; never populated from requested expectations. */
struct AgenticInteractionObservation {
	/** @brief Optional genuine application-provided target observation. */
	std::optional<Json> target_observation;
	/** @brief Clay IDs actually observed hovered by the primary pointer interaction. */
	std::vector<std::uint64_t> hovered;
	/** @brief Clay IDs actually observed pressed by the primary pointer interaction. */
	std::vector<std::uint64_t> pressed;
	/** @brief Clay IDs actually observed held by the primary pointer interaction. */
	std::vector<std::uint64_t> held;
	/** @brief Clay IDs actually observed released by the primary pointer interaction. */
	std::vector<std::uint64_t> released;
	/** @brief Exact consumer frame identity, when a frame was actually observed. */
	std::optional<EvidenceFrameKey> frame;
	/** @brief Optional owned target geometry resolved for this observation. */
	std::optional<AgenticResolvedInteractionTarget> target;
	/**
	 * @brief Semantic compiled workload digest, independent of retries and relative run binding.
	 */
	std::string plan_digest;
	/** @brief Stable queued operation identity recoverable after client disconnect. */
	std::string job_id;
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/**
	 * @brief Explicit machine-readable or diagnostic explanation of an outcome or limitation.
	 */
	std::string reason;
	/** @brief Monotonic evidence record sequence assigned by the executor. */
	std::uint64_t sequence = 0;
	/**
	 * @brief Relative selected input-frame offset; zero begins after baseline synchronization.
	 */
	std::uint64_t offset = 0;
	/** @brief Monotonic consumer observation timestamp, in nanoseconds. */
	std::uint64_t observed_ns = 0;
	/**
	 * @brief Zero-based source step index associated with this compiled span or evidence record.
	 */
	std::uint32_t step_index = 0;
	/** @brief Whether a genuine completed layout was observed for the selected frame. */
	bool layout_completed = false;
	/** @brief Whether presentation was actually reported for the observed frame. */
	bool presented = false;
	/** @brief Whether the consumer frame was canceled before the expected outcome. */
	bool canceled = false;
	/** @brief Whether ordinary input routing suppressed this observed consumer frame. */
	bool input_suppressed = false;
	/**
	 * @brief Whether the consumer window is focused in the selected or observed input state.
	 */
	bool window_focused = false;
	/** @brief Whether this consumer provides a genuine text-focus observation. */
	bool text_focus_supported = false;
	/** @brief Whether the completed consumer observation reports text input focus. */
	bool text_focused = false;
};
/**
 * @brief Independently checked assertion result over one half-open relative frame interval.
 */
struct AgenticInteractionStepResult {
	/** @brief Application-declared semantic step identity. */
	std::string step_id;
	/**
	 * @brief Explicit machine-readable or diagnostic explanation of an outcome or limitation.
	 */
	std::string reason;
	/** @brief Observation flags expected by the step. */
	AgenticInteractionExpectation expected = AgenticInteractionExpectation::none;
	/** @brief Consumer facts actually observed for this step. */
	AgenticInteractionExpectation observed = AgenticInteractionExpectation::none;
	/** @brief Inclusive lower bound of this step's relative frame offsets. */
	std::uint64_t first_offset = 0;
	/** @brief Exclusive upper bound of this step's relative frame offsets. */
	std::uint64_t end_offset_exclusive = 0;
	/** @brief Step assertion outcome, independent of transport or dispatch success. */
	AgenticInteractionVerdict verdict = AgenticInteractionVerdict::pending;
};
/**
 * @brief Independent delivery, observation, assertion and retention counters for execution.
 */
struct AgenticInteractionCoverage {
	/** @brief Number of input frames scheduled by the compiled workload. */
	std::uint64_t scheduled = 0;
	/**
	 * @brief Number of scheduled interaction frames actually selected as effective input.
	 */
	std::uint64_t consumed = 0;
	/** @brief Scheduled frames not selected at their required input address. */
	std::uint64_t missed = 0;
	/** @brief Scheduled frames canceled before delivery. */
	std::uint64_t canceled = 0;
	/** @brief Number of genuine completed consumer observations. */
	std::uint64_t observed = 0;
	/**
	 * @brief Number of interaction frames whose observations were checked against assertions.
	 */
	std::uint64_t asserted = 0;
	/** @brief Number of input receipts retained as execution evidence. */
	std::uint64_t retained_inputs = 0;
	/** @brief Number of completed consumer observations retained as evidence. */
	std::uint64_t retained_observations = 0;
	/** @brief Count of interaction evidence records lost to configured retention bounds. */
	std::uint64_t lost = 0;
};
/**
 * @brief Owned execution outcome; mechanical completion, assertions and cleanup are independent
 * facts.
 */
struct AgenticInteractionResult {
	/** @brief Owned independent assertion results for the source plan steps. */
	std::vector<AgenticInteractionStepResult> steps;
	/** @brief Explicit unsupported, missing or degraded evidence conditions. */
	std::vector<std::string> limitations;
	/** @brief Exact frame on which the interaction acquired input control, when observed. */
	std::optional<EvidenceFrameKey> activation_frame;
	/** @brief First relative input offset where execution diverged, when known. */
	std::optional<std::uint64_t> first_divergence_offset;
	/**
	 * @brief Semantic compiled workload digest, independent of retries and relative run binding.
	 */
	std::string plan_digest;
	/** @brief Stable queued operation identity recoverable after client disconnect. */
	std::string job_id;
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/**
	 * @brief Explicit machine-readable or diagnostic explanation of an outcome or limitation.
	 */
	std::string reason;
	/** @brief Owned coverage and loss evidence associated with this result or capture. */
	AgenticInteractionCoverage coverage;
	/**
	 * @brief Stable application window identity associated with this request or evidence.
	 */
	std::uint64_t window_id = 0;
	/**
	 * @brief Current job or interaction lifecycle phase; separate from assertion verdict.
	 */
	AgenticInteractionPhase phase = AgenticInteractionPhase::queued;
	/**
	 * @brief Aggregate assertion outcome, evaluated independently of execution completion.
	 */
	AgenticInteractionVerdict assertion_verdict = AgenticInteractionVerdict::pending;
	/**
	 * @brief Whether mechanical execution completed successfully; assertions have a separate
	 * verdict.
	 */
	bool execution_succeeded = false;
	/** @brief Whether required execution evidence was retained without reported loss. */
	bool evidence_complete = true;
	/** @brief Whether the executor completed its input release and cleanup contract. */
	bool cleanup_complete = false;
	/** @brief Whether synthetic input control has returned to ordinary native input. */
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
/**
 * @brief Return the static wire-format name of value; the returned view has static storage
 * lifetime.
 */
[[nodiscard]] std::string_view name(AgenticInteractionPhase value) noexcept;
/**
 * @brief Return the static wire-format name of value; the returned view has static storage
 * lifetime.
 */
[[nodiscard]] std::string_view name(AgenticInteractionVerdict value) noexcept;
/**
 * @brief Return the static wire-format name of value; the returned view has static storage
 * lifetime.
 */
[[nodiscard]] std::string_view name(AgenticInteractionOperation value) noexcept;
/**
 * @brief Encode owned versioned wire data, preserving exact integer identities. include_text
 * controls disclosure of copied interaction text.
 */
[[nodiscard]] Json to_json(const AgenticInputSnapshot& value, bool include_text = true);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticInteractionTarget& value);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticResolvedInteractionTarget& value);
/**
 * @brief Encode owned versioned wire data, preserving exact integer identities. include_text
 * controls disclosure of copied interaction text.
 */
[[nodiscard]] Json to_json(const AgenticInteractionPlan& value, bool include_text = true);
/**
 * @brief Encode owned versioned wire data, preserving exact integer identities. include_text
 * controls disclosure of copied interaction text.
 */
[[nodiscard]] Json to_json(const AgenticCompiledInteraction& value, bool include_text = true);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticInteractionCapabilities& value);
/**
 * @brief Encode owned versioned wire data, preserving exact integer identities. include_text
 * controls disclosure of copied interaction text.
 */
[[nodiscard]] Json to_json(const AgenticInteractionReceipt& value, bool include_text = false);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticInteractionObservation& value);
/** @brief Encode owned versioned wire data, preserving exact integer identities. */
[[nodiscard]] Json to_json(const AgenticInteractionResult& value);

} // namespace FlowUi::devSystems::agentic
#endif
