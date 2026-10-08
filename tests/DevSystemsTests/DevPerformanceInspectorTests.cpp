#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Performance/Inspector/DevPerformanceAnalysis.hpp"
#include "devSystems/devInterface/Performance/Inspector/DevPerformanceCapturePolicy.hpp"
#include <cassert>
#include <limits>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
int main() {
	PerformanceRefreshGate gate;
	assert(gate.advance(false) && gate.frames_remaining == 240);
	for (int frame_count = 1; frame_count < 240; ++frame_count)
		assert(!gate.advance(false));
	assert(gate.advance(false) && gate.frames_remaining == 240);
	assert(gate.advance(true) && gate.frames_remaining == 240);
	const std::array<uint64_t, 4> durations{10, 20, 20, 40};
	assert(performance_percentile(durations, .5) == 20);
	assert(performance_percentile(durations, .95) == 40);
	assert(performance_percentile(durations, 0) == 10);
	assert(performance_percentile({}, .5) == 0);
	assert(performance_duration(25000) == "25.000 us");
	assert(performance_duration(4000000) == "4.000 ms");
	std::array descriptors{timing_zones::kWindowFrameTotal, timing_zones::kWindowFrameUserBuild,
						   timing_zones::kElementInvoke};
	std::array<TimingAppTickReport, 3> reports;
	for (size_t tick_position = 0; tick_position < reports.size(); ++tick_position) {
		auto& tick = reports[tick_position];
		tick.appTick = tick_position + 1;
		tick.occupied = true;
		tick.boundary_start_ns = 10000000 * tick.appTick;
		tick.boundary_end_ns = tick.boundary_start_ns + 4000000 + tick_position * 1000000;
		tick.windows.resize(1);
		auto& window = tick.windows.front();
		window.window = 1;
		window.occupied = true;
		window.frames.resize(1);
		auto& frame = window.frames.front();
		frame.occupied = true;
		frame.key = {1, tick.appTick};
		frame.cpuZones.reserve(8);
		frame.cpuZones.emplace_back(CpuTimingRecord{.startNs = tick.boundary_start_ns,
													.durationNs = 4000000,
													.directChildNs = 1000000,
													.invocationId = tick.appTick * 10 + 1,
													.typeId = descriptors[0].typeId,
													.frame = frame.key,
													.appTick = tick.appTick,
													.track = 1});
		frame.cpuZones.emplace_back(CpuTimingRecord{.startNs = tick.boundary_start_ns + 1000,
													.durationNs = 1000000,
													.directChildNs = 60000,
													.invocationId = tick.appTick * 10 + 2,
													.parentInvocationId = tick.appTick * 10 + 1,
													.typeId = descriptors[1].typeId,
													.frame = frame.key,
													.appTick = tick.appTick,
													.track = 1});
		for (uint64_t call_position = 0; call_position < 3; ++call_position)
			frame.cpuZones.emplace_back(
				CpuTimingRecord{.startNs = tick.boundary_start_ns + 2000 + call_position * 25000,
								.durationNs = 20000,
								.invocationId = tick.appTick * 10 + 3 + call_position,
								.parentInvocationId = tick.appTick * 10 + 2,
								.typeId = descriptors[2].typeId,
								.frame = frame.key,
								.appTick = tick.appTick,
								.track = 1});
		frame.cpuZones.emplace_back(
			CpuTimingRecord{.startNs = tick.boundary_start_ns,
							.durationNs = 999000000,
							.invocationId = tick.appTick * 10 + 9,
							.typeId = descriptors[2].typeId,
							.frame = frame.key,
							.appTick = tick.appTick,
							.track = 2,
							.flags = timingRecordFlags(TimingRecordFlag::Canceled)});
		for (uint64_t queue = 1; queue <= 2; ++queue)
			frame.gpuZones.emplace_back(GpuTimingRecord{.durationNs = queue * 100000,
														.submissionSerial = tick.appTick,
														.device_identity = 7,
														.queue_identity = queue,
														.typeId = 77,
														.frame = frame.key,
														.appTick = tick.appTick,
														.zone_index = 0,
														.queueFamilyIndex = 0});
	}
	reports.back().boundary_open = true;
	TimingCaptureReadView view;
	view.first = reports;
	view.descriptors = descriptors;
	view.generation = 7;
	const auto analysis = analyze_performance(view, 0, 0);
	assert(analysis.complete_ticks == 2 && analysis.excluded_intervals == 1 &&
		   analysis.excluded_samples == 3);
	assert(analysis.sorted_cadence == std::vector<uint64_t>({4000000, 5000000}));
	assert(analysis.zones.size() == 5); // Three CPU descriptors and two distinct GPU queues.
	const auto small_calls = std::ranges::find_if(analysis.zones, [&](const auto& zone) {
		return zone.largest.type_id == descriptors[2].typeId;
	});
	assert(small_calls != analysis.zones.end() && small_calls->durations.size() == 9 &&
		   small_calls->self_ns == 180000);
	const auto frames = std::ranges::find_if(analysis.zones, [&](const auto& zone) {
		return zone.largest.type_id == descriptors[0].typeId;
	});
	assert(frames != analysis.zones.end() && frames->total_ns == 12000000 &&
		   frames->self_ns == 9000000);
	const auto last_tick = analyze_performance(view, 1, 0);
	assert(last_tick.first_tick == 3 && last_tick.complete_ticks == 0 &&
		   last_tick.sorted_cadence.empty());
	const auto window_frames = analyze_performance(view, 0, 1);
	assert(window_frames.sorted_cadence == std::vector<uint64_t>({4000000, 4000000, 4000000}));
	DevTimelineState timeline;
	timeline.snapshot = extract_timeline(view, {});
	timeline.inspected_sample = 0;
	timeline.pending = {{1}, TimelineAction::FitSelected, 0, 0, timeline.snapshot_revision};
	DevPerformanceSelection selection;
	apply_timeline_command(timeline, selection);
	assert(timeline.inspected_sample == 1); // Reveal addresses the invocation named by its command.
	DevInterfaceState session;
	assert(performance_capture_policy_error(session, nullptr).empty());
	session.capture_duration_seconds = std::numeric_limits<float>::quiet_NaN();
	assert(!performance_capture_policy_error(session, nullptr).empty());
}
