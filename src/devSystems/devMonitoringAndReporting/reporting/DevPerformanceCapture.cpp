#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include <limits>
#include <utility>
namespace FlowUi::devSystems {
DevPerformanceCapture::DevPerformanceCapture(DevTimingReporting& reporting) noexcept
	: reporting_(&reporting) {}
Status DevPerformanceCapture::request_capture(const PerformanceCaptureSettings& settings) noexcept {
	const auto phase = status_.phase;
	if ((phase != PerformanceCapturePhase::Idle && phase != PerformanceCapturePhase::Sealed) ||
		settings.duration_ns == 0 || settings.budget_ns == 0 ||
		settings.start_mode > PerformanceCaptureStartMode::Shortcut ||
		settings.end_mode > PerformanceCaptureEndMode::Duration || settings.start_chord.key < 32 ||
		settings.start_chord.key >= 349 ||
		settings.start_chord.trigger != DevShortcutTrigger::Press ||
		settings.duration_ns > uint64_t{86'400'000'000'000} ||
		settings.post_event_ns > uint64_t{86'400'000'000'000})
		return unexpectedError(makeError(ErrorCode::ShortcutInvalid, ErrorSite::ShortcutRegister));
	startup_capture_ = startup_open_requested_ = false;
	status_.settings = settings;
	status_.phase = PerformanceCapturePhase::ClosingInterface;
	reopen_requested_ = false;
	return {};
}
void DevPerformanceCapture::request_startup_capture() noexcept {
	if (status_.phase != PerformanceCapturePhase::Idle)
		return;
	startup_capture_ = true;
	startup_open_requested_ = reopen_requested_ = false;
	status_.settings.end_mode = PerformanceCaptureEndMode::Manual;
	earliest_start_tick_ = 1;
	status_.phase = PerformanceCapturePhase::Armed;
}
void DevPerformanceCapture::interface_closed(uint64_t app_tick) noexcept {
	if (status_.phase != PerformanceCapturePhase::ClosingInterface)
		return;
	earliest_start_tick_ = app_tick + 1;
	status_.phase = PerformanceCapturePhase::Armed;
}
void DevPerformanceCapture::request_interface_open(uint64_t app_tick,
												   uint64_t timestamp_ns) noexcept {
	if (startup_capture_)
		startup_open_requested_ = true;
	if (status_.phase == PerformanceCapturePhase::Armed ||
		status_.phase == PerformanceCapturePhase::ClosingInterface) {
		status_.phase =
			status_.generation ? PerformanceCapturePhase::Sealed : PerformanceCapturePhase::Idle;
	} else if (status_.phase == PerformanceCapturePhase::Recording ||
			   status_.phase == PerformanceCapturePhase::PostEvent) {
		stop(app_tick, timestamp_ns, PerformanceCaptureStopReason::ManualOpen);
	}
}
void DevPerformanceCapture::stop(uint64_t app_tick, uint64_t timestamp_ns,
								 PerformanceCaptureStopReason reason) noexcept {
	status_.end_app_tick_exclusive = app_tick;
	status_.end_ns = timestamp_ns;
	status_.stop_reason = reason;
	status_.phase = PerformanceCapturePhase::Finalizing;
	finalization_start_ns_ = timestamp_ns;
	reporting_->stop_capture(app_tick);
}
void DevPerformanceCapture::advance_tick(uint64_t app_tick, uint64_t timestamp_ns,
										 bool interface_visible, bool shortcut_pressed,
										 uint64_t pending_measurements) noexcept {
	if (status_.phase == PerformanceCapturePhase::Armed && !interface_visible &&
		app_tick >= earliest_start_tick_ &&
		(status_.settings.start_mode == PerformanceCaptureStartMode::Immediate ||
		 shortcut_pressed)) {
		const auto settings = status_.settings;
		const auto generation = reporting_->begin_capture(
			app_tick, status_.settings.budget_window,
			status_.settings.end_mode == PerformanceCaptureEndMode::BudgetEvent
				? status_.settings.budget_ns
				: UINT64_MAX);
		status_ = {.settings = settings,
				   .generation = generation,
				   .start_ns = timestamp_ns,
				   .first_app_tick = app_tick,
				   .phase = PerformanceCapturePhase::Recording};
	}
	if (status_.phase == PerformanceCapturePhase::Recording ||
		status_.phase == PerformanceCapturePhase::PostEvent) {
		if (interface_visible) {
			stop(app_tick, timestamp_ns, PerformanceCaptureStopReason::ManualOpen);
		} else {
			if (startup_capture_) {
				const auto reporting_status = reporting_->status();
				if (reporting_status.capture_admitted_ticks >= reporting_status.effectiveCapacity)
					stop(app_tick, timestamp_ns, PerformanceCaptureStopReason::CapacityReached);
			}
			if (status_.phase == PerformanceCapturePhase::Recording &&
				status_.settings.end_mode == PerformanceCaptureEndMode::BudgetEvent) {
				const auto event = reporting_->budget_event(status_.settings.budget_window,
															status_.settings.budget_ns);
				if (event) {
					status_.event_end_ns = event->end_ns;
					status_.event_duration_ns = event->duration_ns;
					status_.event_app_tick = event->app_tick;
					status_.phase = PerformanceCapturePhase::PostEvent;
				}
			}
			if (status_.phase == PerformanceCapturePhase::PostEvent &&
				timestamp_ns >= status_.event_end_ns &&
				timestamp_ns - status_.event_end_ns >= status_.settings.post_event_ns)
				stop(app_tick, timestamp_ns, PerformanceCaptureStopReason::BudgetEvent);
			else if (status_.settings.end_mode == PerformanceCaptureEndMode::Duration &&
					 timestamp_ns >= status_.start_ns &&
					 timestamp_ns - status_.start_ns >= status_.settings.duration_ns)
				stop(app_tick, timestamp_ns, PerformanceCaptureStopReason::DurationElapsed);
		}
	}
	if (status_.phase == PerformanceCapturePhase::Finalizing &&
		timestamp_ns > finalization_start_ns_) {
		status_.pending_measurements = pending_measurements;
		if (pending_measurements == 0 || (timestamp_ns >= finalization_start_ns_ &&
										  timestamp_ns - finalization_start_ns_ >= 2'000'000'000)) {
			status_.incomplete = pending_measurements != 0;
			reporting_->seal_capture();
			status_.phase = PerformanceCapturePhase::Sealed;
			reopen_requested_ = !startup_capture_ || startup_open_requested_;
		}
	}
	if (status_.phase == PerformanceCapturePhase::Recording ||
		status_.phase == PerformanceCapturePhase::PostEvent)
		reporting_->admit_tick(app_tick, timestamp_ns);
}
void DevPerformanceCapture::retry_reopen() noexcept {
	if (status_.phase == PerformanceCapturePhase::Sealed)
		reopen_requested_ = true;
}
void DevPerformanceCapture::fail_capture(uint64_t app_tick, uint64_t timestamp_ns) noexcept {
	if (status_.phase == PerformanceCapturePhase::Recording ||
		status_.phase == PerformanceCapturePhase::PostEvent)
		stop(app_tick, timestamp_ns, PerformanceCaptureStopReason::Failure);
}
bool DevPerformanceCapture::take_reopen_request() noexcept {
	return std::exchange(reopen_requested_, false);
}
void DevPerformanceCapture::shutdown(uint64_t app_tick, uint64_t timestamp_ns) noexcept {
	request_interface_open(app_tick, timestamp_ns);
	if (status_.phase == PerformanceCapturePhase::Finalizing) {
		reporting_->seal_capture();
		status_.phase = PerformanceCapturePhase::Sealed;
		status_.stop_reason = PerformanceCaptureStopReason::Shutdown;
		status_.incomplete = true;
	}
	reopen_requested_ = false;
}
} // namespace FlowUi::devSystems
#endif
