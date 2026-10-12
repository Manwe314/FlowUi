#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchData.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace {
struct Probe {
	uint64_t first = 10, second = 30;
};
void sample(const void* pointer, MemoryProbeContext& context) noexcept {
	const auto& probe = *static_cast<const Probe*>(pointer);
	for (const auto window : {WindowId{1}, WindowId{2}}) {
		const auto bytes = window == 1 ? probe.first : probe.second;
		(void)context.sink.append(MemoryValueSample{.source = memory_sources::kInputFields.id,
													.window = window,
													.logicalLiveBytes = bytes,
													.reusableBytes = 64 - bytes,
													.backingAllocatedBytes = 64});
	}
}
} // namespace
int main() {
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	DevMemory memory(DevMemoryConfig{.level = MemoryMonitoringLevel::SubsystemCapacity,
									 .gpuMemory = false,
									 .processMemory = false});
	Probe probe;
	assert(memory.registerProbe({memory_sources::kInputFields.id, &probe, &sample}));
	DevMemoryReporting reporting(
		memory, MemoryReportingConfig{.segmentCapacity = 128, .managerSampleEveryTicks = 1});
	reporting.consume(1);
	reporting.consume(2);
	DevMemorySelection selection;
	selection.selected_node = uint64_t(DevMemoryNodeId::InputFields);
	DevMemoryWorkbenchState state;
	resolve_memory_tracks(state, selection, reporting.read_live());
	assert(state.tracks.front().value.logicalLiveBytes == 40);
	assert(state.tracks.front().value.backingAllocatedBytes == 128);
	assert(state.tracks.front().keys.size() == 2);
	assert(std::ranges::any_of(state.tracks, [](const auto& track) {
		return track.included && track.parent_identity == uint64_t(DevMemoryNodeId::InputFields);
	}));
	selection.window_scope = 2;
	resolve_memory_tracks(state, selection, reporting.read_live());
	assert(state.tracks.front().value.logicalLiveBytes == 30 &&
		   state.tracks.front().keys.size() == 1);
	selection.window_scope = 0;
	resolve_memory_tracks(state, selection, reporting.read_live());
	probe.first = 30;
	probe.second = 10;
	reporting.consume(3);
	reporting.consume(4);
	build_memory_history(state, reporting, 64);
	assert(state.all_lanes.size() == 2);
	assert(state.lanes.size() == 1);
	assert(state.lanes.front().columns.size() == 64);
	assert(std::ranges::any_of(state.lanes.front().columns, [](const auto& column) {
		return column.available && column.active_max == 30;
	}));
	// Pin reconstruction preserves per-instance identities rather than summing every window.
	const auto retained_segments = reporting.segmentSnapshot();
	const auto retained_source = std::ranges::find_if(retained_segments, [](const auto& segment) {
		return segment.value.source == memory_sources::kInputFields.id &&
			   segment.value.window == 2 && segment.endNs > segment.beginNs;
	});
	assert(retained_source != retained_segments.end());
	resolve_memory_history_tracks(state, reporting, retained_source->beginNs);
	const auto window_row = std::ranges::find_if(state.tracks, [](const auto& track) {
		return track.included && track.keys.size() == 1 && track.keys.front().window == 2;
	});
	assert(window_row != state.tracks.end() &&
		   window_row->value.logicalLiveBytes == retained_source->value.logicalLiveBytes);
	resolve_memory_tracks(state, selection, reporting.read_live());
	MemoryWorkbenchTrack unsupported;
	unsupported.identity = 999;
	unsupported.title = "Current-only detail";
	state.tracks.emplace_back(unsupported);
	state.focus = 999;
	build_memory_history(state, reporting, 64);
	assert(state.lanes.empty());
	state.focus = 0;
	resolve_memory_tracks(state, selection, reporting.read_live());
	assert(memory_axis_max(18) >= 20 && memory_axis_max(UINT64_MAX) == UINT64_MAX);
	assert(memory_bytes(1024, false) == "1.00 KiB" && memory_bytes(1000, true) == "1.00 kB");
	MemoryWorkbenchTrack capacity_only;
	capacity_only.available = true;
	capacity_only.value.value_fields = uint16_t(MemoryValueField::Backing);
	capacity_only.value.backingAllocatedBytes = 1024;
	assert(memory_track_reading(capacity_only, false).find("Active not reported") !=
		   std::string::npos);

	auto frozen = reporting.freeze_history();
	const auto retained = reporting.status().segments.totalPublished;
	probe.first = 50;
	reporting.consume(5);
	assert(reporting.status().segments.totalPublished == retained);
	frozen.reset();
	reporting.consume(6);
	reporting.consume(7);
	build_memory_history(state, reporting, 64);
	assert(std::ranges::any_of(state.lanes.front().columns, [](const auto& column) {
		return column.available && column.epoch == 2;
	}));

	assert(reporting.setConfig(
		MemoryReportingConfig{.segmentCapacity = 100'000, .managerSampleEveryTicks = 1}));
	for (uint64_t tick = 8; tick < 30'008; ++tick) {
		probe.first = tick % 48 + 1;
		probe.second = 49 - probe.first;
		reporting.consume(tick);
	}
	resolve_memory_tracks(state, selection, reporting.read_live());
	const auto scan_begin = memory_now_ns();
	build_memory_history(state, reporting, 900);
	std::cout << "Large-history projection: " << (memory_now_ns() - scan_begin) / 1e6
			  << " ms; longest locked slice " << reporting.status().history_max_lock_ns / 1e6
			  << " ms; retained " << reporting.status().segments.retainedCount << " records\n";
	memory.unregisterProbe(memory_sources::kInputFields.id, &probe);
	reporting.consume(30'008);
	const auto live = reporting.read_live();
	assert(std::ranges::none_of(live.sources, [](const auto& source) {
		return source.key.source == memory_sources::kInputFields.id;
	}));

	resolve_memory_tracks(state, selection, live);
	assert(!state.tracks.front().available);
	build_memory_history(state, reporting, 64);
	assert(state.all_lanes.size() == 2);
	assert(state.lanes.size() == 1);
	DevMemoryWorkbenchState counts;
	counts.history_metric = 2;
	MemoryWorkbenchTrack count_track;
	count_track.identity = uint64_t(DevMemoryNodeId::InputFields);
	count_track.history_sources = {memory_sources::kInputFields.id};
	counts.tracks.emplace_back(count_track);
	MemoryDataset count_data;
	count_data.identity = 1;
	count_data.live.status.last_consumed_ns = 20;
	MemorySampleSegment count_segment;
	count_segment.beginNs = 10;
	count_segment.endNs = 20;
	count_segment.value.source = memory_sources::kInputFields.id;
	count_segment.value.window = 1;
	count_segment.value.objectCount = 5;
	count_segment.value.capacityCount = 8;
	count_segment.value.value_fields =
		uint16_t(MemoryValueField::Objects) | uint16_t(MemoryValueField::Capacity);
	count_data.segments.emplace_back(count_segment);
	counts.analysis.datasets.emplace_back(std::move(count_data));
	counts.analysis.dataset = 1;
	build_memory_history(counts, reporting, 64);
	assert(counts.lanes.size() == 1 && counts.lanes.front().columns.front().active_max == 5);
	assert(counts.lanes.front().columns.front().backing_max == 8);
	counts.history_metric = 0;
	build_memory_history(counts, reporting, 64);
	assert(!(counts.lanes.front().columns.front().value_fields &
			 uint16_t(MemoryValueField::LogicalLive)));
	std::cout << "Footprint/tuning separation, source lanes, resume epochs and removed producer "
				 "verified\n";
#else
	DevMemory memory;
	assert(uint32_t(memory.compiledLevel()) < 2);
	assert(memory_bytes(1024, false) == "1.00 KiB");
#endif
}
