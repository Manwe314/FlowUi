#ifdef NDEBUG
#undef NDEBUG
#endif

#include "FlowUi/Public/Flow.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsModel.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

using namespace FlowUi;
using namespace FlowUi::devSystems;

namespace {

constexpr MemorySourceId checkpoint_source = 0xd1a6'0001;
constexpr auto recorded_zone =
	makeTimingDescriptor(TimingCategory::User, TimingZoneRole::Work,
						 "diagnostics.navigation.captured_work", {}, CpuTimingLevel::OnlyFrameTime);

struct CheckpointProbe {
	uint64_t window_count = 1;
	static void sample(const void* owner, MemoryProbeContext& context) noexcept {
		const auto& probe = *static_cast<const CheckpointProbe*>(owner);
		for (uint64_t window_index = 0; window_index < probe.window_count; ++window_index)
			if (!context.sink.append(
					MemoryValueSample{.source = checkpoint_source,
									  .window = static_cast<WindowId>(MainWindowId + window_index),
									  .logicalLiveBytes = 1024,
									  .backingAllocatedBytes = 2048,
									  .objectCount = 2}))
				std::abort();
	}
};

void check_diagnostics_context(const DevDiagnosticsState& state) {
	assert(state.selected_id == 9001);
	assert(state.focus_kind == 1 && state.focus_sequence == 903);
	assert(state.selector_scroll == 17 && state.workbench_scroll == 29 &&
		   state.inspector_scroll == 41);
}

void timing_destinations(App& app, DevInterfaceState& interface_state) {
	auto& timing = app.devMonitoring().timing();
	auto& reporting = app.devMonitoring().timingReporting();
	const auto generation = reporting.begin_capture(100);
	assert(generation);
	reporting.admit_tick(100, timing.nowNs());
	auto attachment = timing.attachCurrentThread("diagnostics.navigation.timing");
	assert(attachment);
	attachment.recorder().setFrameContext({MainWindowId, 44}, 100);
	const TimingEntityRef entity{
		.kind = TimingEntityKind::ElementInstance, .primaryId = 12345, .secondaryId = 67890};
	const auto token = attachment.recorder().tryBegin(recorded_zone, entity);
	assert(token);
	attachment.recorder().end(token);
	reporting.consumeThrough(100);
	reporting.stop_capture(101);
	reporting.seal_capture();
	const auto report = reporting.appTickReport(100);
	assert(report);
	std::optional<CpuTimingRecord> captured;
	for (const auto& window : report->windows)
		for (const auto& frame : window.frames)
			for (const auto& record : frame.cpuZones)
				if (record.typeId == recorded_zone.typeId)
					captured = record;
	assert(captured);
	const DevErrorTimingCorrelation exact{.entity = entity,
										  .state = DevErrorCorrelationState::Available,
										  .confidence = DevErrorCorrelationConfidence::Explicit,
										  .appTick = captured->appTick,
										  .frame = captured->frame,
										  .containingInvocation = captured->invocationId,
										  .containingZone = captured->typeId,
										  .reportRevision = report->revision,
										  .capture_generation = generation,
										  .sealed = true};
	auto& state = interface_state.diagnostics;
	state.selected->timing = exact;
	interface_state.performance_selection.selector_mode = 1;
	interface_state.performance_selection.hardware_domain = 2;
	interface_state.performance_selection.category_mask = 0;
	interface_state.performance_selection.selected_scope = {999, DevPerformanceScopeKind::Thread};
	assert(navigate_diagnostics_performance(app, interface_state));
	assert(interface_state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Performance));
	assert(state.return_available);
	const auto& timeline = interface_state.performance_timeline;
	assert(timeline.capture_generation == generation);
	assert(timeline.inspected_sample < timeline.snapshot.blocks.size());
	const auto& selected = timeline.snapshot.blocks[timeline.inspected_sample];
	assert(selected.app_tick == captured->appTick && selected.frame == captured->frame);
	assert(selected.invocation_id == captured->invocationId &&
		   selected.type_id == captured->typeId);
	assert(selected.details().entity.kind == entity.kind);
	assert(selected.details().entity.primaryId == entity.primaryId);
	assert(selected.details().entity.secondaryId == entity.secondaryId);
	assert(interface_state.performance_selection.selected_scope ==
		   (DevPerformanceScope{MainWindowId, DevPerformanceScopeKind::Window}));
	assert(interface_state.performance_selection.selector_mode == 0);
	assert(interface_state.performance_selection.hardware_domain == 0);
	assert(interface_state.performance_selection.category_mask == UINT32_MAX);
	check_diagnostics_context(state);
	const auto snapshot_revision = timeline.snapshot_revision;
	const auto inspected_sample = timeline.inspected_sample;
	const auto scope = interface_state.performance_selection.selected_scope;
	for (uint64_t mismatch = 0; mismatch < 10; ++mismatch) {
		state.selected->timing = exact;
		auto& correlation = state.selected->timing;
		switch (mismatch) {
		case 0:
			++correlation.capture_generation;
			break;
		case 1:
			++correlation.appTick;
			break;
		case 2:
			++correlation.frame.window;
			break;
		case 3:
			++correlation.frame.frameNumber;
			break;
		case 4:
			++correlation.containingInvocation;
			break;
		case 5:
			++correlation.containingZone;
			break;
		case 6:
			correlation.entity.kind = TimingEntityKind::Window;
			break;
		case 7:
			++correlation.entity.primaryId;
			break;
		case 8:
			++correlation.entity.secondaryId;
			break;
		case 9:
			correlation.state = DevErrorCorrelationState::Evicted;
			break;
		}
		interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Diagnostics);
		assert(!navigate_diagnostics_performance(app, interface_state));
		assert(interface_state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Diagnostics));
		assert(timeline.snapshot_revision == snapshot_revision &&
			   timeline.inspected_sample == inspected_sample);
		assert(interface_state.performance_selection.selected_scope == scope);
		assert(timeline.capture_generation == generation);
		check_diagnostics_context(state);
	}
	state.selected->timing = exact;
	assert(reporting.begin_capture(200) != generation);
	assert(!navigate_diagnostics_performance(app, interface_state));
	assert(timeline.snapshot_revision == snapshot_revision &&
		   timeline.inspected_sample == inspected_sample);
	assert(interface_state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Diagnostics));
}

void memory_destinations(App& app, DevInterfaceState& interface_state) {
	auto& memory = app.devMonitoring().memory();
	auto& reporting = app.devMonitoring().memoryReporting();
	CheckpointProbe probe;
	assert(memory.registerSource({.name = "Diagnostics navigation manager",
								  .id = checkpoint_source,
								  .domain = MemoryDomain::ManagerCpu,
								  .kind = MemorySourceKind::Manager}));
	assert(memory.registerProbe(
		{.source = checkpoint_source, .owner = &probe, .sample = CheckpointProbe::sample}));
	const auto timestamp = app.devMonitoring().timing().nowNs();
	const MemoryOperationRecord operation{.source = checkpoint_source,
										  .lifetime = 811,
										  .operation = MemoryOperation::BackingGrow,
										  .detailLevel = MemoryMonitoringLevel::StorageSummary,
										  .appTick = 200,
										  .frame = {MainWindowId, 77},
										  .submissionSerial = 901,
										  .timestampNs = timestamp,
										  .bytesBefore = 1024,
										  .bytesAfter = 2048,
										  .bytesChanged = 1024,
										  .threadTrack = 5};
	assert(memory.recorder().tryRecord(operation));
	reporting.consume(200);
	reporting.consume(201);
	const auto events = reporting.eventSnapshot();
	const auto retained = std::ranges::find_if(events, [&](const auto& event) {
		return event.operation.source == checkpoint_source && event.operation.lifetime == 811;
	});
	assert(retained != events.end());
	const auto status = reporting.status();
	const DevErrorMemoryCorrelation exact{.eventSequence = retained->sequence,
										  .source = operation.source,
										  .checkpointTick = 200,
										  .storageMutationSequence = status.storageMutationSequence,
										  .history_generation = status.history_storage_generation,
										  .state = DevErrorCorrelationState::Available,
										  .confidence = DevErrorCorrelationConfidence::Explicit,
										  .operation = operation.operation,
										  .sealed = true};
	auto& state = interface_state.diagnostics;
	state.selected->memory = exact;
	interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Diagnostics);
	assert(navigate_diagnostics_memory(app, interface_state));
	assert(interface_state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Memory));
	assert(state.navigation_memory_event && !state.navigation_memory_checkpoint);
	assert(state.navigation_memory_event->sequence == retained->sequence);
	assert(state.navigation_memory_event->operation.source == operation.source);
	assert(state.navigation_memory_event->operation.lifetime == operation.lifetime);
	assert(state.navigation_memory_event->operation.submissionSerial == operation.submissionSerial);
	assert(state.navigation_memory_event->operation.frame == operation.frame);
	assert(interface_state.memory_workbench.selected_ns == timestamp);
	assert(interface_state.memory_selection.window_scope == MainWindowId);
	assert(!interface_state.memory_workbench.following);
	assert(interface_state.memory_workbench.scope ==
		   interface_state.memory_selection.selected_node);
	assert(interface_state.memory_workbench.window ==
		   interface_state.memory_selection.window_scope);
	check_diagnostics_context(state);
	const auto selected_timestamp = interface_state.memory_workbench.selected_ns;
	const auto view_begin = interface_state.memory_workbench.view_begin_ns;
	const auto view_end = interface_state.memory_workbench.view_end_ns;
	for (uint64_t mismatch = 0; mismatch < 5; ++mismatch) {
		state.selected->memory = exact;
		auto& correlation = state.selected->memory;
		switch (mismatch) {
		case 0:
			++correlation.history_generation;
			break;
		case 1:
			correlation.eventSequence = UINT64_MAX;
			break;
		case 2:
			++correlation.source;
			break;
		case 3:
			correlation.operation = MemoryOperation::BackingTrim;
			break;
		case 4:
			correlation.state = DevErrorCorrelationState::Evicted;
			break;
		}
		interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Diagnostics);
		assert(!navigate_diagnostics_memory(app, interface_state));
		assert(interface_state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Diagnostics));
		assert(state.navigation_memory_event->sequence == retained->sequence);
		assert(!state.navigation_memory_checkpoint);
		assert(interface_state.memory_workbench.selected_ns == selected_timestamp);
		assert(interface_state.memory_workbench.view_begin_ns == view_begin);
		assert(interface_state.memory_workbench.view_end_ns == view_end);
		assert(interface_state.memory_selection.window_scope == MainWindowId);
		check_diagnostics_context(state);
	}
	const auto segments = reporting.segmentSnapshot();
	const auto checkpoint = std::ranges::find_if(segments, [](const auto& segment) {
		return segment.value.source == checkpoint_source && segment.value.window == MainWindowId &&
			   segment.beginTick <= 200 && segment.endTickExclusive > 200;
	});
	assert(checkpoint != segments.end());
	state.selected->memory = exact;
	state.selected->memory.eventSequence = 0;
	const auto checkpoint_navigation = navigate_diagnostics_memory(app, interface_state);
	if (!checkpoint_navigation) {
		std::cerr << "Checkpoint navigation failed: code="
				  << static_cast<uint64_t>(checkpoint_navigation.error().code)
				  << " expected_generation=" << exact.history_generation
				  << " actual_generation=" << reporting.status().history_storage_generation
				  << " source=" << exact.source << " tick=" << exact.checkpointTick << '\n';
		reporting.visit_history([&](const auto& segment) {
			if (segment.value.source == exact.source && segment.beginTick <= exact.checkpointTick &&
				segment.endTickExclusive > exact.checkpointTick)
				std::cerr << "Checkpoint candidate: sequence=" << segment.sequence
						  << " window=" << segment.value.window << " ticks=" << segment.beginTick
						  << ".." << segment.endTickExclusive << " ns=" << segment.beginNs << ".."
						  << segment.endNs
						  << " flags=" << static_cast<uint64_t>(segment.value.flags) << '\n';
		});
	}
	assert(checkpoint_navigation);
	assert(interface_state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Memory));
	assert(!state.navigation_memory_event && state.navigation_memory_checkpoint);
	assert(state.navigation_memory_checkpoint->sequence == checkpoint->sequence);
	assert(state.navigation_memory_checkpoint->value.source == checkpoint_source);
	assert(state.navigation_memory_checkpoint->value.window == MainWindowId);
	assert(interface_state.memory_workbench.selected_ns == checkpoint->beginNs);
	assert(interface_state.memory_workbench.held_data);
	assert(std::ranges::any_of(
		interface_state.memory_workbench.held_data->segments,
		[&](const auto& segment) { return segment.sequence == checkpoint->sequence; }));
	check_diagnostics_context(state);
	probe.window_count = 2;
	reporting.consume(202);
	state.selected->memory.checkpointTick = 202;
	interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Diagnostics);
	const auto preserved_checkpoint = state.navigation_memory_checkpoint->sequence;
	const auto preserved_timestamp = interface_state.memory_workbench.selected_ns;
	assert(!navigate_diagnostics_memory(app, interface_state));
	assert(interface_state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Diagnostics));
	assert(state.navigation_memory_checkpoint->sequence == preserved_checkpoint);
	assert(interface_state.memory_workbench.selected_ns == preserved_timestamp);
	check_diagnostics_context(state);
	state.selected->memory = exact;
	auto config = reporting.config();
	config.eventByteCapacity *= 2;
	assert(reporting.setConfig(config));
	assert(reporting.status().history_storage_generation != exact.history_generation);
	assert(!navigate_diagnostics_memory(app, interface_state));
	assert(interface_state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Diagnostics));
	assert(state.navigation_memory_checkpoint->sequence == preserved_checkpoint);
	assert(interface_state.memory_workbench.selected_ns == preserved_timestamp);
	memory.unregisterProbe(checkpoint_source, &probe);
}

} // namespace

int main() {
	AppConfig config;
	config.window.title = "Diagnostics exact retained navigation verification";
	config.window.width = 640;
	config.window.height = 480;
	config.dev.monitoring.memory.gpuMemory = false;
	config.dev.monitoring.memory.processMemory = false;
	config.dev.monitoring.memoryReporting.retainLifetimeEvents = true;
	config.dev.monitoring.memoryReporting.managerSampleEveryTicks = 1;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState interface_state;
	interface_state.activeTab = static_cast<uint64_t>(DevInterfaceTab::Diagnostics);
	auto& state = interface_state.diagnostics;
	state.selected.emplace();
	state.selected->id = 9001;
	state.selected_id = 9001;
	state.focus_kind = 1;
	state.focus_sequence = 903;
	state.selector_scroll = 17;
	state.workbench_scroll = 29;
	state.inspector_scroll = 41;
	timing_destinations(app, interface_state);
	memory_destinations(app, interface_state);
	std::cout << "Diagnostics exact-generation timing/event/checkpoint navigation and refusal "
				 "checks passed\n";
}
