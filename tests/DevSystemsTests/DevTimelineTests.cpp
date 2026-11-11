#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;

namespace {
void check_cpu_detail_projection() {
	const std::array descriptors{
		timing_zones::kWindowFrameTotal, timing_zones::kElementInvoke,
		makeBuiltinTimingDescriptor(1001, TimingCategory::User, TimingZoneRole::Work,
									CpuTimingLevel::Summary, "test.summary"),
		makeBuiltinTimingDescriptor(1002, TimingCategory::User, TimingZoneRole::Work,
									CpuTimingLevel::Balanced, "test.balanced"),
		makeBuiltinTimingDescriptor(1003, TimingCategory::User, TimingZoneRole::Work,
									CpuTimingLevel::Deep, "test.deep")};
	TimingAppTickReport report{};
	report.occupied = true;
	report.appTick = 1;
	report.captureConfig.cpuLevel = CpuTimingLevel::Deep;
	report.captureConfig.balancedElementRetentionThresholdNs = 1000;
	report.captureConfig.selectedElementInstance = FlowElementID{123};
	report.captureConfig.selectedElementDefinition = FlowDefinitionID{456};
	report.windows.resize(1);
	auto& window = report.windows.front();
	window.occupied = true;
	window.window = 1;
	window.frames.resize(1);
	auto& frame = window.frames.front();
	frame.occupied = true;
	frame.key = {1, 1};
	frame.cpuZones.reserve(9);
	const auto append_cpu = [&](uint64_t invocation, uint64_t parent, uint64_t type,
								uint64_t duration, uint64_t instance = 0, uint64_t definition = 0) {
		frame.cpuZones.emplace_back(CpuTimingRecord{
			.startNs = 100,
			.durationNs = duration,
			.invocationId = invocation,
			.parentInvocationId = parent,
			.typeId = type,
			.frame = frame.key,
			.appTick = 1,
			.primaryEntityId = instance,
			.secondaryEntityId = definition,
			.track = 7,
			.entityKind = instance ? TimingEntityKind::ElementInstance : TimingEntityKind::None});
	};
	append_cpu(1, 0, timing_zones::kWindowFrameTotal.typeId, 10000);
	append_cpu(2, 1, 1001, 9000);
	append_cpu(3, 2, 1003, 8000);
	append_cpu(4, 3, 1001, 100);
	append_cpu(5, 2, 1002, 500);
	append_cpu(6, 2, timing_zones::kElementInvoke.typeId, 200, 99, 77);
	append_cpu(7, 2, timing_zones::kElementInvoke.typeId, 1000, 99, 77);
	append_cpu(8, 2, timing_zones::kElementInvoke.typeId, 20, 123, 77);
	append_cpu(9, 2, timing_zones::kElementInvoke.typeId, 20, 99, 456);
	frame.gpuZones.emplace_back(
		GpuTimingRecord{.durationNs = 500,
						.cpuAlignedStartNs = 100,
						.typeId = 2000,
						.frame = frame.key,
						.appTick = 1,
						.flags = gpuTimingRecordFlags(GpuTimingRecordFlag::Completed)});
	report.applicationCpuZones = {
		{.startNs = 100, .durationNs = 1000, .invocationId = 100, .typeId = 1003, .track = 9},
		{.startNs = 100,
		 .durationNs = 100,
		 .invocationId = 101,
		 .parentInvocationId = 100,
		 .typeId = 1001,
		 .track = 9}};
	const std::span reports{&report, 1};
	DevPerformanceSelection selection{};
	const auto deep = extract_timeline(reports, descriptors, selection);
	assert(deep.blocks.size() == 13);
	selection.cpu_detail_level = CpuTimingLevel::Summary;
	const auto summary = extract_timeline(reports, descriptors, selection);
	assert(summary.blocks.size() == 6);
	assert(summary.minimum_recorded_cpu_level == CpuTimingLevel::Deep &&
		   summary.maximum_recorded_cpu_level == CpuTimingLevel::Deep);
	assert(summary.frames.size() == 1 && summary.frames.front().duration_ns == 10000);
	const auto projected_child =
		std::ranges::find(summary.blocks, uint64_t{4}, &TimelineBlockSlice::invocation_id);
	assert(projected_child != summary.blocks.end() && projected_child->filtered_parent);
	assert(summary.blocks[projected_child->parent].invocation_id == 2);
	selection.selected_scope = {9, DevPerformanceScopeKind::Thread};
	const auto scoped_summary = extract_timeline(reports, descriptors, selection);
	const auto summary_lanes = timeline_major_lanes(scoped_summary, selection);
	assert(summary_lanes.size() == 1 && summary_lanes.front().blocks.size() == 1);
	assert(scoped_summary.blocks[summary_lanes.front().blocks.front()].invocation_id == 101);
	selection.selected_scope = {};
	selection.cpu_detail_level = CpuTimingLevel::Balanced;
	const auto balanced = extract_timeline(reports, descriptors, selection);
	assert(balanced.blocks.size() == 10);
	assert(std::ranges::none_of(balanced.blocks, [](const auto& block) {
		return block.invocation_id == 3 || block.invocation_id == 6 || block.invocation_id == 100;
	}));
	for (const auto invocation : {7u, 8u, 9u})
		assert(std::ranges::any_of(
			balanced.blocks, [&](const auto& block) { return block.invocation_id == invocation; }));
	selection.cpu_detail_level = CpuTimingLevel::OnlyFrameTime;
	const auto frame_only = extract_timeline(reports, descriptors, selection);
	assert(frame_only.blocks.size() == 3 && frame_only.frames.size() == 1);
	assert(std::ranges::any_of(frame_only.blocks, [](const auto& block) {
		return block.domain == TimingSampleDomain::Gpu;
	}));
	selection.cpu_detail_level = CpuTimingLevel::Deep;
	const auto restored = extract_timeline(reports, descriptors, selection);
	assert(restored.blocks.size() == deep.blocks.size());
	assert(frame.cpuZones.size() == 9 && report.applicationCpuZones.size() == 2);
	assert(frame.cpuZones[3].parentInvocationId == 3);
}
} // namespace

int main() {
	check_cpu_detail_projection();
	const std::array descriptors{timing_zones::kWindowFrameTotal,
								 timing_zones::kWindowFrameUserBuild, timing_zones::kElementInvoke};
	std::vector<TimingAppTickReport> reports(2);
	for (size_t report_index = 0; report_index < reports.size(); ++report_index) {
		auto& report = reports[report_index];
		report.appTick = report_index + 1;
		report.occupied = true;
		report.captureConfig.cpuLevel = report_index == 0
			? CpuTimingLevel::Summary : CpuTimingLevel::Balanced;
		report.windows.resize(1);
		auto& window = report.windows.front();
		window.window = 1;
		window.occupied = true;
		window.frames.resize(1);
		auto& frame = window.frames.front();
		frame.key = {1, report_index + 1};
		frame.occupied = true;
		const auto start = 1'000'000'000ULL + report_index * 40'000'000ULL;
		const auto duration = report_index ? 35'000'000ULL : 16'000'000ULL;
		frame.cpuZones = {{.startNs = start,
						   .durationNs = duration,
						   .directChildNs = 4'000'000,
						   .invocationId = report_index * 10 + 1,
						   .typeId = descriptors[0].typeId,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .track = 7},
						  {.startNs = start + 1'000'000,
						   .durationNs = 4'000'000,
						   .directChildNs = 100'000,
						   .invocationId = report_index * 10 + 2,
						   .parentInvocationId = report_index * 10 + 1,
						   .typeId = descriptors[1].typeId,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .track = 7},
						  {.startNs = start + 1'100'000,
						   .durationNs = 25'000,
						   .invocationId = report_index * 10 + 3,
						   .parentInvocationId = report_index * 10 + 2,
						   .typeId = descriptors[2].typeId,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .track = 7},
						  {.startNs = start + 1'125'000,
						   .durationNs = 25'000,
						   .invocationId = report_index * 10 + 4,
						   .parentInvocationId = report_index * 10 + 2,
						   .typeId = descriptors[2].typeId,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .track = 7}};
		frame.gpuZones = {{.durationNs = 2'000'000,
						   .cpuAlignedStartNs = start + 5'000'000,
						   .submissionSerial = 10,
						   .typeId = 99,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .queueFamilyIndex = 2,
						   .flags = 1},
						  {.durationNs = 1'000'000,
						   .cpuAlignedStartNs = start + 5'100'000,
						   .submissionSerial = 10,
						   .typeId = 100,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .parentZoneIndex = 0,
						   .queueFamilyIndex = 2,
						   .flags = 1},
						  {.durationNs = 1'000'000,
						   .submissionSerial = 11,
						   .typeId = 99,
						   .frame = frame.key,
						   .appTick = report.appTick},
						  {.durationNs = 3'000'000,
						   .cpuAlignedStartNs = start + 8'000'000,
						   .submissionSerial = 12,
						   .typeId = 99,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .queueFamilyIndex = 2,
						   .flags = 1},
						  {.durationNs = 1'000'000,
						   .cpuAlignedStartNs = start + 8'100'000,
						   .submissionSerial = 12,
						   .typeId = 100,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .parentZoneIndex = 0,
						   .queueFamilyIndex = 2,
						   .flags = 1}};
		frame.gpuZones[0].zone_index = 4;
		frame.gpuZones[0].device_identity = 101;
		frame.gpuZones[0].queue_identity = 501;
		frame.gpuZones[1].zone_index = 9;
		frame.gpuZones[1].parentZoneIndex = 4;
		frame.gpuZones[1].device_identity = 101;
		frame.gpuZones[1].queue_identity = 501;
		frame.gpuZones[3].zone_index = 6;
		frame.gpuZones[3].submissionSerial = 10;
		frame.gpuZones[3].device_identity = 101;
		frame.gpuZones[3].queue_identity = 502;
		frame.gpuZones[4].zone_index = 8;
		frame.gpuZones[4].parentZoneIndex = 6;
		frame.gpuZones[4].submissionSerial = 10;
		frame.gpuZones[4].device_identity = 101;
		frame.gpuZones[4].queue_identity = 502;
	}
	DevPerformanceSelection selection;
	selection.selected_scope = {1, DevPerformanceScopeKind::Window};
	auto snapshot = extract_timeline(reports, descriptors, selection);
	assert(snapshot.has_recording_policy);
	assert(snapshot.minimum_recorded_cpu_level == CpuTimingLevel::Summary);
	assert(snapshot.maximum_recorded_cpu_level == CpuTimingLevel::Balanced);
	const TimelineSnapshot policy_copy = snapshot;
	assert(policy_copy.minimum_recorded_cpu_level == CpuTimingLevel::Summary);
	assert(policy_copy.maximum_recorded_cpu_level == CpuTimingLevel::Balanced);
	assert(snapshot.frames.size() == 2 && snapshot.blocks.size() == 20);
	assert(snapshot.uncalibrated_gpu_count == 2);
	assert(snapshot.blocks[1].parent == 0 && snapshot.blocks[9].parent == 8);
	assert(snapshot.blocks[5].parent == 4 && snapshot.blocks[7].parent == 6);
	assert(snapshot.blocks[4].zone_index == 4 && snapshot.blocks[6].queue_identity == 502);
	assert(snapshot.blocks[6].recorded_exclusive_ns() == 0);
	auto lanes = timeline_lanes(snapshot, 1);
	assert(lanes.size() == 2);
	assert(lanes[0].blocks == std::vector<size_t>({1, 9})); // window root is replaced by milestones
	const std::array<size_t, 1> roots{1};
	auto children = timeline_lanes(snapshot, 1, roots);
	assert(children.size() == 1 && children[0].blocks == std::vector<size_t>({2, 3}));
	auto clusters = cluster_timeline(snapshot, children[0].blocks, snapshot.blocks[1].start_ns,
									 4'000'000, 1000);
	assert(clusters.size() == 1 && clusters[0].members.size() == 2 &&
		   clusters[0].duration_ns == 50'000);
	snapshot.blocks[3].selected = true;
	assert(
		cluster_timeline(snapshot, children[0].blocks, snapshot.blocks[1].start_ns, 4'000'000, 1000)
			.size() == 2);
	snapshot.blocks[3].selected = false;
	snapshot.blocks[3].start_ns += 100'000;
	assert(
		cluster_timeline(snapshot, children[0].blocks, snapshot.blocks[1].start_ns, 4'000'000, 1000)
			.size() == 2);
	assert(cluster_timeline(snapshot, children[0].blocks, 0, 1, 1000).empty());
	selection.selector_mode = 1;
	selection.selected_zone = descriptors[1].typeId;
	snapshot = extract_timeline(reports, descriptors, selection);
	assert(snapshot.blocks.size() == 20 &&
		   snapshot.frame_metrics == std::vector<uint64_t>({4'000'000, 4'000'000}));
	assert(snapshot.blocks[1].selected && !snapshot.blocks[0].selected);
	selection.selected_scope = {88, DevPerformanceScopeKind::Thread};
	snapshot = extract_timeline(reports, descriptors, selection);
	assert(snapshot.frames.empty() &&
		   snapshot.blocks.size() == 20); // no frame totals exist on this thread
	selection.selected_scope = {};
	snapshot = extract_timeline(reports, descriptors, selection);
	assert(snapshot.frames[0].label == "Window 1 · Frame #1");
	selection.selector_mode = 0;
	DevTimelineState state;
	state.snapshot = extract_timeline(reports, descriptors, selection);
	state.zoom = 5;
	clamp_timeline_view(state);
	assert(state.visible_duration_ns == (state.snapshot.end_ns - state.snapshot.start_ns) / 5);
	center_timeline(state, 0);
	assert(state.visible_start_ns == state.snapshot.start_ns);
	center_timeline(state, UINT64_MAX);
	assert(timeline_end(state.visible_start_ns, state.visible_duration_ns) ==
		   state.snapshot.end_ns);
	state.pending.action = TimelineAction::Spike;
	apply_timeline_command(state, selection);
	assert(state.selected_frame == 1);
	state.pending.action = TimelineAction::Next;
	apply_timeline_command(state, selection);
	assert(state.selected_frame == 1);
	state.pending.action = TimelineAction::Previous;
	apply_timeline_command(state, selection);
	assert(state.selected_frame == 0);
	state.pending = {{1}, TimelineAction::Open, 0};
	apply_timeline_command(state, selection);
	state.pending = {{2}, TimelineAction::Open, 1};
	apply_timeline_command(state, selection);
	assert(state.cards.size() == 2);
	state.cards[1].active_depth = 1;
	state.pending = {{}, TimelineAction::Depth, 1, 1};
	apply_timeline_command(state, selection);
	assert(state.cards[1].active_depth == 2);
	state.pending = {{}, TimelineAction::Close, 0};
	apply_timeline_command(state, selection);
	assert(state.cards.empty());
	state.pending = {{1000}, TimelineAction::Open};
	apply_timeline_command(state, selection);
	assert(state.cards.empty());
	state.zoom = NAN;
	clamp_timeline_view(state);
	assert(state.zoom == 1);
	assert(timeline_end(UINT64_MAX - 2, 4) == UINT64_MAX);
	DevTimelineState empty;
	apply_timeline_command(empty, selection);
	assert(empty.visible_duration_ns == 1);
	snapshot.blocks[0].parent = 1;
	snapshot.blocks[1].parent = 0;
	const auto cyclic = timeline_lanes(snapshot, 64); // malformed ancestry must terminate
	assert(cyclic.size() <= 64);
	// Capture-wide parent lookup survives different attribution buckets.
	reports[1].windows[0].frames[0].cpuZones[1].parentInvocationId = 1;
	reports[1].windows[0].frames[0].cpuZones[1].startNs =
		reports[0].windows[0].frames[0].cpuZones[0].startNs + 100;
	reports[1].windows[0].frames[0].cpuZones[1].durationNs = 1000;
	selection.selected_scope = {1, DevPerformanceScopeKind::Window};
	auto cross_tick = extract_timeline(reports, descriptors, selection);
	assert(cross_tick.blocks[9].parent == 0);
	// Flat major roots omit descendants and separate overlapping roots.
	selection.selector_mode = 0;
	auto major = timeline_major_lanes(cross_tick, selection);
	assert(std::ranges::none_of(major, [](const auto& lane) {
		return std::ranges::find(lane.blocks, size_t{1}) != lane.blocks.end();
	}));
	state.pending = {{2}, TimelineAction::Inspect, 0, 0, state.snapshot_revision};
	const auto history_size = state.cards.size();
	apply_timeline_command(state, selection);
	assert(state.inspected_sample == 2 && state.cards.size() == history_size);
	reports[0].boundary_start_ns = 100;
	reports[0].boundary_end_ns = 200;
	selection.selected_scope = {};
	auto cadence = extract_timeline(reports, descriptors, selection);
	assert(cadence.tick_cadence && cadence.ticks.front().start_ns == 100 &&
		   cadence.ticks.front().duration_ns == 100);

	// All Windows uses application cadence as the CPU major track.
	const auto cadence_lanes = timeline_major_lanes(cadence, selection);
	assert(std::ranges::any_of(cadence_lanes, [&](const auto& lane) {
		return lane.domain == TimingSampleDomain::Cpu && !lane.blocks.empty();
	}));
	for (const auto& lane : cadence_lanes)
		if (lane.domain == TimingSampleDomain::Cpu)
			for (const auto block_index : lane.blocks)
				assert(cadence.blocks[block_index].synthetic_tick);
	assert(DevTimelineState{}.minimap_zoom == 1 && DevTimelineState{}.zoom == 4);
	// Reporting boundary snapshots remain owned and consistent after eviction.
	DevTiming timing;
	DevGpuTiming gpu_timing(timing);
	DevTimingReporting reporting(timing, gpu_timing);
	auto reporting_config = reporting.config();
	reporting_config.retainedAppTickCapacity = 2;
	reporting_config.minimumFramesInFlightMultiplier = 1;
	reporting.setConfig(reporting_config);
	assert(reporting.begin_capture(1) == 1);
	reporting.admit_tick(1, 100);
	reporting.note_tick_boundary(1, 100);
	reporting.note_tick_boundary(2, 200);
	reporting.admit_tick(2, 200);
	const auto frozen_capture = reporting.capture_snapshot();
	assert(frozen_capture.reports.size() == 2 && frozen_capture.reports[0].boundary_end_ns == 200);
	assert(!frozen_capture.reports[0].boundary_open && frozen_capture.reports[1].boundary_open);
	reporting.note_tick_boundary(3, 300);
	reporting.admit_tick(3, 300);
	assert(!reporting.appTickReport(1) && frozen_capture.reports[0].boundary_start_ns == 100);
	// Uncalibrated GPU records stay selectable without contaminating the CPU clock range.
	assert(!cross_tick.blocks[16].cpu_clock_aligned &&
		   cross_tick.blocks[16].duration_ns == 1'000'000);
	state.snapshot = cross_tick;
	state.pending = {{16}, TimelineAction::Open, 0, 0, state.snapshot_revision};
	apply_timeline_command(state, selection);
	assert(state.cards.back().roots.front() == 16);
	// Track preferences are view operations keyed by semantic identity, not capture indices.
	state.selection = selection;
	state.selection.selected_scope = {1, DevPerformanceScopeKind::Window};
	state.selection.selector_mode = 0;
	state.pending = {{0}, TimelineAction::TrackHide, 0, 0, state.snapshot_revision};
	apply_timeline_command(state, selection);
	auto preferred_lanes =
		timeline_major_lanes(state.snapshot, state.selection, state.track_preferences);
	assert(std::ranges::none_of(
		preferred_lanes, [](const auto& lane) { return lane.domain == TimingSampleDomain::Cpu; }));
	state.inspected_sample = 0;
	state.pending = {{}, TimelineAction::FitSelected, 0, 0, state.snapshot_revision};
	apply_timeline_command(state, selection);
	preferred_lanes =
		timeline_major_lanes(state.snapshot, state.selection, state.track_preferences);
	assert(std::ranges::any_of(
		preferred_lanes, [](const auto& lane) { return lane.domain == TimingSampleDomain::Cpu; }));
	for (const auto sample_index : {size_t{4}, size_t{6}}) {
		state.pending = {{sample_index}, TimelineAction::TrackPin, 0, 0, state.snapshot_revision};
		apply_timeline_command(state, selection);
	}
	state.pending = {{6, 4}, TimelineAction::TrackMove, 0, 0, state.snapshot_revision};
	apply_timeline_command(state, selection);
	preferred_lanes =
		timeline_major_lanes(state.snapshot, state.selection, state.track_preferences);
	assert(preferred_lanes.front().track == 502 &&
		   preferred_lanes.front().domain == TimingSampleDomain::Gpu);
	state.pending = {{state.snapshot.tick_samples.front()},
					 TimelineAction::TrackHide,
					 0,
					 0,
					 state.snapshot_revision};
	apply_timeline_command(state, selection);
	assert(std::ranges::any_of(state.track_preferences, [](const auto& preference) {
		return preference.key.contextual && preference.hidden;
	}));
	assert(state.snapshot.blocks.size() == 20 && state.cards.back().roots.front() == 16);
}
