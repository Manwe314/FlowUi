#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp"
#include <cassert>
#include "internal/InputQueue.hpp"
#include <iostream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
namespace {
struct Fixture {
	DevTiming timing{};
	DevGpuTiming gpu{timing};
	DevTimingReporting reporting{timing, gpu};
	DevPerformanceCapture capture{reporting};
	Fixture() {
		auto config = reporting.config();
		config.retainedAppTickCapacity = 2;
		config.minimumFramesInFlightMultiplier = 1;
		config.rollingSampleCapacity = 2;
		reporting.setConfig(config);
	}
	void boundary(uint64_t tick, uint64_t timestamp, bool visible = false, bool pressed = false,
				  uint64_t pending = 0) {
		reporting.consumeThrough(tick - 1);
		reporting.note_tick_boundary(tick, timestamp);
		capture.advance_tick(tick, timestamp, visible, pressed, pending);
	}
};
void retention_and_statistics() {
	Fixture fixture;
	auto attachment = fixture.timing.attachCurrentThread("capture.test");
	auto& recorder = attachment.recorder();
	recorder.setFrameContext({}, 10);
	auto token = recorder.tryBegin(timing_zones::kWindowFrameTotal);
	assert(token);
	assert(fixture.timing.pending_scope_count(10, 11) == 1);
	recorder.end(token);
	assert(fixture.timing.pending_scope_count(10, 11) == 0);
	fixture.reporting.consumeThrough(10);
	assert(!fixture.reporting.status().hasRetainedTicks);
	assert(fixture.reporting.rollingStatistics().front().sampleCount == 1);
	assert(fixture.reporting.begin_capture(1'000'000) == 1);
	for (uint64_t tick = 1'000'000; tick < 1'000'003; ++tick) {
		fixture.reporting.note_tick_boundary(tick, tick * 100);
		fixture.reporting.admit_tick(tick, tick * 100);
		recorder.setFrameContext({1, tick}, tick);
		token = recorder.tryBegin(timing_zones::kWindowFrameTotal);
		assert(token);
		recorder.end(token);
		fixture.reporting.consumeThrough(tick);
	}
	fixture.reporting.note_tick_boundary(1'000'003, 100'000'300);
	fixture.reporting.stop_capture(1'000'003);
	fixture.reporting.seal_capture();
	const auto status = fixture.reporting.status();
	assert(status.retainedTickCount == 2 && status.capture_overwritten_ticks == 1 &&
		   status.capture_admitted_ticks == 3);
	const CpuTimingRecord* reusable_address = nullptr;
	const auto statistics = fixture.reporting.rollingStatistics().front();
	{
		auto view = fixture.reporting.read_capture();
		assert(view.generation == 1 && view.first.size() + view.second.size() == 2);
		assert(view.first.front().appTick == 1'000'001);
		const auto& newest = view.second.empty() ? view.first.back() : view.second.back();
		assert(newest.appTick == 1'000'002 && !newest.boundary_open);
		reusable_address = view.first.front().windows.front().frames.front().cpuZones.data();
		const auto first_duration =
			view.first.front().windows.front().frames.front().cpuZones.front().durationNs;
		const auto last_duration =
			newest.windows.front().frames.front().cpuZones.front().durationNs;
		assert(statistics.minimumNs == std::min(first_duration, last_duration));
		assert(statistics.maximumNs == std::max(first_duration, last_duration));
		assert(statistics.averageNs == (double(first_duration) + double(last_duration)) / 2);
		auto index = interface_elements::extract_timeline(view, {});
		assert(!index.blocks.empty());
		assert(index.blocks.front().cpu_sample.front().appTick == 1'000'001);
		assert(index.blocks.front().details().exclusive_ns ==
			   index.blocks.front().cpu_sample.front().exclusiveNs());
	}
	recorder.setFrameContext({}, 2'000'000);
	token = recorder.tryBegin(timing_zones::kWindowFrameTotal);
	recorder.end(token);
	fixture.reporting.note_tick_boundary(2'000'000, 200'000'000);
	fixture.reporting.consumeThrough(2'000'000);
	assert(fixture.reporting.status().mutationSequence == status.mutationSequence);
	assert(fixture.reporting.status().late_after_seal == 0);
	assert(fixture.reporting.status().not_retained_by_policy > 0);
	assert(fixture.reporting.rollingStatistics().front().sampleCount == 2);
	assert(fixture.reporting.begin_capture(3'000'000) == 2);
	fixture.reporting.admit_tick(3'000'000, 300'000'000);
	recorder.setFrameContext({1, 3'000'000}, 3'000'000);
	token = recorder.tryBegin(timing_zones::kWindowFrameTotal);
	recorder.end(token);
	// Late old-generation data affects statistics, never this generation's slots.
	recorder.setFrameContext({}, 1'000'002);
	token = recorder.tryBegin(timing_zones::kWindowFrameTotal);
	recorder.end(token);
	fixture.reporting.consumeThrough(3'000'000);
	assert(fixture.reporting.status().retainedTickCount == 1);
	fixture.reporting.note_tick_boundary(3'000'001, 300'000'100);
	fixture.reporting.stop_capture(3'000'001);
	fixture.reporting.seal_capture();
	auto view = fixture.reporting.read_capture();
	assert(view.generation == 2 && view.first.size() + view.second.size() == 1);
	const auto& report = view.first.front();
	assert(report.appTick == 3'000'000 && report.applicationCpuZones.empty());
	// At least one previously allocated slot remains reused across capture replacement.
	assert(report.windows.front().frames.front().cpuZones.data() == reusable_address);
}
void duration_and_manual_exclusion() {
	Fixture fixture;
	PerformanceCaptureSettings settings;
	settings.duration_ns = 200;
	assert(fixture.capture.request_capture(settings));
	fixture.capture.interface_closed(10);
	fixture.boundary(10, 1000);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Armed);
	fixture.boundary(11, 1100);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Recording);
	fixture.boundary(12, 1200);
	fixture.boundary(13, 1300);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Finalizing);
	fixture.boundary(14, 1400);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Sealed);
	assert(fixture.capture.take_reopen_request() && !fixture.capture.take_reopen_request());
	assert(fixture.reporting.status().newestRetainedAppTick == 12);
	settings.end_mode = PerformanceCaptureEndMode::Manual;
	assert(fixture.capture.request_capture(settings));
	fixture.capture.interface_closed(15);
	fixture.boundary(16, 1600);
	fixture.reporting.note_tick_boundary(17, 1700);
	fixture.capture.request_interface_open(17, 1700);
	fixture.capture.advance_tick(17, 1700, false, false, 0);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Finalizing);
	fixture.boundary(18, 1800, false, false, 1);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Finalizing);
	fixture.boundary(19, 1700 + 2'000'000'000, false, false, 1);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Sealed &&
		   fixture.capture.status().incomplete);
	assert(fixture.reporting.status().newestRetainedAppTick == 16);
}
void shortcut_and_event_tail() {
	Fixture fixture;
	PerformanceCaptureSettings settings;
	settings.start_mode = PerformanceCaptureStartMode::Shortcut;
	settings.end_mode = PerformanceCaptureEndMode::BudgetEvent;
	settings.budget_ns = 100;
	settings.post_event_ns = 100;
	assert(fixture.capture.request_capture(settings));
	fixture.capture.interface_closed(1);
	fixture.boundary(2, 1000);
	assert(!fixture.reporting.status().hasRetainedTicks);
	fixture.capture.request_interface_open(2, 1000);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Idle);
	assert(fixture.capture.request_capture(settings));
	fixture.capture.interface_closed(3);
	fixture.boundary(4, 2000, false, true);
	fixture.boundary(5, 2100); // equality is not an event
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Recording);
	fixture.boundary(6, 2300);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::PostEvent);
	assert(fixture.capture.status().event_end_ns == 2300 &&
		   fixture.capture.status().event_duration_ns == 200);
	fixture.boundary(7, 2400);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Finalizing);
	fixture.boundary(8, 2500);
	assert(fixture.capture.status().stop_reason == PerformanceCaptureStopReason::BudgetEvent);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Sealed);
	const auto generation = fixture.capture.status().generation;
	assert(fixture.capture.request_capture(settings));
	fixture.capture.interface_closed(9);
	fixture.capture.request_interface_open(10, 2700);
	assert(fixture.capture.status().generation == generation &&
		   fixture.reporting.status().capture_sealed);
	settings.duration_ns = 0;
	assert(!fixture.capture.request_capture(settings));
}
void startup_retention_and_first_open() {
	Fixture fixture;
	fixture.capture.request_startup_capture();
	assert(fixture.capture.startup_capture());
	fixture.boundary(1, 100);
	fixture.boundary(2, 200);
	fixture.boundary(3, 300);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Finalizing);
	assert(fixture.capture.status().stop_reason == PerformanceCaptureStopReason::CapacityReached);
	fixture.boundary(4, 400);
	assert(fixture.capture.status().phase == PerformanceCapturePhase::Sealed);
	assert(!fixture.capture.take_reopen_request());
	assert(fixture.reporting.status().capture_overwritten_ticks == 0);
	assert(fixture.reporting.status().capture_admitted_ticks == 2);
	{
		auto view = fixture.reporting.read_capture();
		assert(view.generation == 1 && view.first.front().appTick == 1);
	}
	Fixture early_open;
	early_open.capture.request_startup_capture();
	early_open.boundary(1, 100);
	early_open.capture.request_interface_open(2, 200);
	early_open.boundary(2, 200);
	early_open.boundary(3, 300);
	assert(early_open.capture.status().phase == PerformanceCapturePhase::Sealed);
	assert(early_open.capture.status().stop_reason == PerformanceCaptureStopReason::ManualOpen);
	assert(early_open.capture.take_reopen_request());
	assert(early_open.reporting.status().capture_admitted_ticks == 1);
	PerformanceCaptureSettings normal_capture;
	assert(early_open.capture.request_capture(normal_capture));
	assert(!early_open.capture.startup_capture());
}
} // namespace
int main() {
	FlowUi::detail::InputQueue input;
	input.pushKey(297, true);
	input.note_dev_shortcut_press(297, 3);
	input.pushKey(297, false);
	assert(!input.queuedKeysDown_[297] && input.dev_shortcut_presses[297] == (1u << 3));
	input.dev_shortcut_presses.fill(0);
	assert(input.dev_shortcut_presses[297] == 0);
	startup_retention_and_first_open();
	retention_and_statistics();
	duration_and_manual_exclusion();
	shortcut_and_event_tail();
	std::cout << "Performance capture and retention checks passed\n";
}
