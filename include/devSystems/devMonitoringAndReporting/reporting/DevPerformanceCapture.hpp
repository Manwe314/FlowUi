#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Public/errorManagement/Error.hpp"
#include "FlowUi/Public/configurationStructs/DevToolsConfig.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
namespace FlowUi::devSystems {
class DevTimingReporting;
/** Trigger that starts an armed performance capture. */
enum class PerformanceCaptureStartMode : uint8_t {
	Immediate, /**< Begin on the next eligible clean tick after interface suspension. */
	Shortcut /**< Wait for the configured keyboard chord after arming. */
};
/** Condition that stops admission of new performance intervals. */
enum class PerformanceCaptureEndMode : uint8_t {
	Manual, /**< Stop on explicit interface reopening or shutdown. */
	BudgetEvent, /**< Stop after a budget event and the configured post-event interval. */
	Duration /**< Stop after the configured nanosecond duration. */
};
/** State of the nonblocking performance capture lifecycle. */
enum class PerformanceCapturePhase : uint8_t {
	/** No performance capture is active. */
	Idle,
	/** Waiting for the developer interface to be suspended. */
	ClosingInterface,
	/** Capture is armed and awaiting its start condition or warmup. */
	Armed,
	/** Clean application intervals are being admitted. */
	Recording,
	/** Recording the configured interval after a budget event. */
	PostEvent,
	/** Admission stopped; waiting for pending measurements or timeout. */
	Finalizing,
	/** Generation is immutable and available for investigation or export. */
	Sealed
};
/** Reason that the current performance capture stopped admitting intervals. */
enum class PerformanceCaptureStopReason : uint8_t {
	/** Capture stopped when the developer interface was requested. */
	ManualOpen,
	/** Configured recording duration elapsed. */
	DurationElapsed,
	/** A frame exceeded the configured duration budget. */
	BudgetEvent,
	/** Application teardown stopped the capture. */
	Shutdown,
	/** A required capture resource or window became unavailable. */
	Failure,
	/** Startup capture reached its effective ring capacity. */
	CapacityReached
};
/**
 * @brief Per-operation performance capture policy copied when capture is armed.
 * Requires FLOW_UI_DEV_MODE. Durations are nanoseconds; warmup counts clean application ticks.
 * The selected start/end modes determine which trigger fields apply.
 */
struct PerformanceCaptureSettings {
	/** Keyboard chord used when start_mode selects shortcut triggering. */
	DevShortcutChord start_chord{.key = 297, .ctrl = true, .shift = true};
	/** Recording duration in nanoseconds for duration-based captures. */
	uint64_t duration_ns = 5'000'000'000;
	/** Window frame threshold in nanoseconds when end_mode is BudgetEvent. */
	uint64_t budget_ns = 16'600'000;
	/** Nanoseconds collected after the first budget event when end_mode is BudgetEvent. */
	uint64_t post_event_ns = 500'000'000;
	/** Maximum nanoseconds to wait for pending measurements before sealing incomplete. */
	uint64_t finalization_timeout_ns = 2'000'000'000;
	/** Clean application ticks skipped before capture admission starts. */
	uint64_t warmup_ticks = 0;
	/** Window used for budget evaluation; InvalidWindowId selects the configured default. */
	WindowId budget_window = InvalidWindowId;
	/** Condition that begins an armed capture. */
	PerformanceCaptureStartMode start_mode = PerformanceCaptureStartMode::Immediate;
	/** Condition that ends capture admission. */
	PerformanceCaptureEndMode end_mode = PerformanceCaptureEndMode::Duration;
	/** Request reopening the developer interface after capture finalization. */
	bool reopen_interface = true;
};
/** Application-owned capture progress and immutable result identity. */
struct PerformanceCaptureStatus {
	/** Immutable settings copied for the current recording. */
	PerformanceCaptureSettings settings{};
	/** @var PerformanceCaptureStatus::generation
	 * Identity of this capture generation.
	 */
	/** @var PerformanceCaptureStatus::start_ns
	 * Monotonic start timestamp in nanoseconds.
	 */
	/** @var PerformanceCaptureStatus::end_ns
	 * Monotonic end timestamp in nanoseconds.
	 */
	uint64_t generation = 0, start_ns = 0, end_ns = 0;
	/** @var PerformanceCaptureStatus::event_end_ns
	 * End timestamp of the triggering budget event, in nanoseconds.
	 */
	/** @var PerformanceCaptureStatus::event_duration_ns
	 * Duration of the triggering budget event, in nanoseconds.
	 */
	/** @var PerformanceCaptureStatus::event_app_tick
	 * Application tick containing the triggering budget event.
	 */
	uint64_t event_end_ns = 0, event_duration_ns = 0, event_app_tick = 0;
	/** @var PerformanceCaptureStatus::first_app_tick
	 * First tick admitted to the capture or occurrence.
	 */
	/** @var PerformanceCaptureStatus::end_app_tick_exclusive
	 * First application tick excluded from the capture interval.
	 */
	uint64_t first_app_tick = 0, end_app_tick_exclusive = 0;
	/** Number of admitted timing measurements still awaiting completion. */
	uint64_t pending_measurements = 0;
	/** Current application or capture lifecycle phase. */
	PerformanceCapturePhase phase = PerformanceCapturePhase::Idle;
	/** Reason the current capture stopped admitting intervals. */
	PerformanceCaptureStopReason stop_reason = PerformanceCaptureStopReason::ManualOpen;
	/** Whether some expected measurement or evidence failed to arrive. */
	bool incomplete = false;
};
/** Nonblocking controller advanced once at the application polling safe point. */
class DevPerformanceCapture {
public:
	/** Bind the capture controller to reporting, which must outlive this controller. */
	explicit DevPerformanceCapture(DevTimingReporting& reporting) noexcept;
	/**
	 * Validate and queue a capture without performing window operations.
	 * @param settings Recording policy copied for the capture generation.
	 * @return Success, or a recoverable configuration/lifecycle failure.
	 */
	[[nodiscard]] Status request_capture(const PerformanceCaptureSettings& settings) noexcept;
	/** Arm one startup recording; stop at effective ring capacity without automatic reopening. */
	void request_startup_capture() noexcept;
	/** Whether this recording was armed by startup policy. */
	[[nodiscard]] bool startup_capture() const noexcept { return startup_capture_; }
	/** Confirm suspension; immediate capture begins on the next clean tick. */
	void interface_closed(uint64_t app_tick) noexcept;
	/** Cancel arming or stop before the tick which opens the interface. */
	void request_interface_open(uint64_t app_tick, uint64_t timestamp_ns) noexcept;
	/** Consume complete-interval deadlines and admit one clean tick. */
	void advance_tick(uint64_t app_tick, uint64_t timestamp_ns, bool interface_visible,
					  bool shortcut_pressed, uint64_t pending_measurements) noexcept;
	/** Stop when the configured budget window becomes unavailable. */
	void fail_capture(uint64_t app_tick, uint64_t timestamp_ns) noexcept;
	/** Keep automatic reopening retryable after a window operation failure. */
	void retry_reopen() noexcept;
	/** Stop application teardown without scheduling interface reopening. */
	void shutdown(uint64_t app_tick, uint64_t timestamp_ns) noexcept;
	/** Suspend developer overlays and picking as well as the dedicated window. */
	[[nodiscard]] bool interface_work_suspended() const noexcept {
		return status_.phase != PerformanceCapturePhase::Idle &&
			   status_.phase != PerformanceCapturePhase::Sealed;
	}
	/**
	 * Return the current scalar progress, retention and quality state; borrowed reference overloads
	 * remain valid until the controller changes.
	 */
	[[nodiscard]] const PerformanceCaptureStatus& status() const noexcept { return status_; }
	/** Consume one pending automatic developer-interface reopening request. */
	[[nodiscard]] bool take_reopen_request() noexcept;

private:
	void stop(uint64_t app_tick, uint64_t timestamp_ns,
			  PerformanceCaptureStopReason reason) noexcept;
	DevTimingReporting* reporting_;
	PerformanceCaptureStatus status_{};
	uint64_t earliest_start_tick_ = 0, finalization_start_ns_ = 0;
	bool reopen_requested_ = false;
	bool startup_capture_ = false, startup_open_requested_ = false;
};
} // namespace FlowUi::devSystems
#endif
