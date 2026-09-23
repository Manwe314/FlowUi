#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;

int main() {
	const std::array descriptors{timing_zones::kWindowFrameTotal,
								 timing_zones::kWindowFrameUserBuild, timing_zones::kElementInvoke};
	std::vector<TimingAppTickReport> reports(2);
	for (size_t report_index = 0; report_index < reports.size(); ++report_index) {
		auto& report = reports[report_index];
		report.appTick = report_index + 1;
		report.windows.resize(1);
		auto& window = report.windows.front();
		window.window = 1;
		window.frames.resize(1);
		auto& frame = window.frames.front();
		frame.key = {1, report_index + 1};
		const auto start = 1'000'000'000ULL + report_index * 40'000'000ULL;
		const auto duration = report_index ? 35'000'000ULL : 16'000'000ULL;
		frame.cpuZones = {{.startNs = start,
						   .durationNs = duration,
						   .directChildNs = 4'000'000,
						   .invocationId = 1,
						   .typeId = descriptors[0].typeId,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .track = 7},
						  {.startNs = start + 1'000'000,
						   .durationNs = 4'000'000,
						   .directChildNs = 100'000,
						   .invocationId = 2,
						   .parentInvocationId = 1,
						   .typeId = descriptors[1].typeId,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .track = 7},
						  {.startNs = start + 1'100'000,
						   .durationNs = 25'000,
						   .invocationId = 3,
						   .parentInvocationId = 2,
						   .typeId = descriptors[2].typeId,
						   .frame = frame.key,
						   .appTick = report.appTick,
						   .track = 7},
						  {.startNs = start + 1'125'000,
						   .durationNs = 25'000,
						   .invocationId = 4,
						   .parentInvocationId = 2,
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
	}
	DevPerformanceSelection selection;
	selection.selected_scope = {1, DevPerformanceScopeKind::Window};
	auto snapshot = extract_timeline(reports, descriptors, selection);
	assert(snapshot.frames.size() == 2 && snapshot.blocks.size() == 16);
	assert(snapshot.uncalibrated_gpu_count == 2);
	assert(snapshot.blocks[1].parent == 0 && snapshot.blocks[9].parent == 8);
	assert(snapshot.blocks[5].parent == 4 && snapshot.blocks[7].parent == 6);
	assert(snapshot.blocks[6].exclusive_ns == 2'000'000);
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
	assert(snapshot.blocks.size() == 16 &&
		   snapshot.frame_metrics == std::vector<uint64_t>({4'000'000, 4'000'000}));
	assert(snapshot.blocks[1].selected && !snapshot.blocks[0].selected);
	selection.selected_scope = {88, DevPerformanceScopeKind::Thread};
	snapshot = extract_timeline(reports, descriptors, selection);
	assert(snapshot.frames.size() == 2 &&
		   snapshot.blocks.size() == 8); // frame anchors + GPU are independent of CPU thread
	selection.selected_scope = {};
	snapshot = extract_timeline(reports, descriptors, selection);
	assert(snapshot.frames[0].label == "AppTick #1");
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
	assert(state.paused && state.selected_frame == 1);
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
}
