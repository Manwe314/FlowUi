#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Error.hpp"
#include "FlowUi/PublicStructs.hpp"
namespace FlowUi::devSystems {
class DevTimingReporting;
enum class PerformanceCaptureStartMode : uint8_t { Immediate, Shortcut };
enum class PerformanceCaptureEndMode : uint8_t { Manual, BudgetEvent, Duration };
enum class PerformanceCapturePhase : uint8_t {
	Idle,
	ClosingInterface,
	Armed,
	Recording,
	PostEvent,
	Finalizing,
	Sealed
};
enum class PerformanceCaptureStopReason : uint8_t {
	ManualOpen,
	DurationElapsed,
	BudgetEvent,
	Shutdown,
	Failure,
	CapacityReached
};
/** Immutable recording policy, independent of investigation filters. */
struct PerformanceCaptureSettings {
	DevShortcutChord start_chord{.key = 297, .ctrl = true, .shift = true};
	uint64_t duration_ns = 5'000'000'000;
	uint64_t budget_ns = 16'600'000;
	uint64_t post_event_ns = 500'000'000;
	WindowId budget_window = InvalidWindowId;
	PerformanceCaptureStartMode start_mode = PerformanceCaptureStartMode::Immediate;
	PerformanceCaptureEndMode end_mode = PerformanceCaptureEndMode::Duration;
};
/** Application-owned capture progress and immutable result identity. */
struct PerformanceCaptureStatus {
	PerformanceCaptureSettings settings{};
	uint64_t generation = 0, start_ns = 0, end_ns = 0;
	uint64_t event_end_ns = 0, event_duration_ns = 0, event_app_tick = 0;
	uint64_t first_app_tick = 0, end_app_tick_exclusive = 0;
	uint64_t pending_measurements = 0;
	PerformanceCapturePhase phase = PerformanceCapturePhase::Idle;
	PerformanceCaptureStopReason stop_reason = PerformanceCaptureStopReason::ManualOpen;
	bool incomplete = false;
};
/** Nonblocking controller advanced once at the application polling safe point. */
class DevPerformanceCapture {
public:
	explicit DevPerformanceCapture(DevTimingReporting& reporting) noexcept;
	/** Validate and queue capture without performing window operations. */
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
	[[nodiscard]] const PerformanceCaptureStatus& status() const noexcept { return status_; }
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
