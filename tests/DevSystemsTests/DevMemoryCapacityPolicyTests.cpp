#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Memory/Analysis/DevMemoryCapacityTool.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace {
[[nodiscard]] const FlowUi::detail::InitialCapacityDefinition&
definition(FlowUi::detail::InitialCapacityTarget target) noexcept {
	return FlowUi::detail::initial_capacity_definitions[static_cast<size_t>(target)];
}
[[nodiscard]] const MemorySettingDraft& prepared(const MemoryAnalysisState& state,
												 FlowUi::detail::InitialCapacityTarget target) {
	const auto found = std::ranges::find(state.staged, definition(target).id(),
										 [](const auto& draft) { return draft.target.id; });
	assert(found != state.staged.end());
	return *found;
}
MemoryLiveRead measured() {
	MemoryLiveRead live;
	live.status.last_consumed_ns = 10;
	for (const auto target : {FlowUi::detail::InitialCapacityTarget::fonts_family_lookup,
							  FlowUi::detail::InitialCapacityTarget::icons_requests}) {
		auto& peak = live.capacity_high_water[static_cast<size_t>(target)];
		peak.active_entries = 80;
		peak.active_bytes = 640;
		peak.valid_fields =
			uint16_t(MemoryValueField::Objects) | uint16_t(MemoryValueField::LogicalLive);
		peak.first_measured_ns = peak.peak_ns = 1;
		peak.revision = 1;
	}
	return live;
}
void add_recording(MemoryAnalysisState& state) {
	MemoryDataset recording;
	recording.identity = 1;
	recording.name = "Normal use";
	recording.begin_ns = 1;
	recording.end_ns = 11;
	recording.complete = true;
	recording.segments.reserve(8);
	for (const auto target : {FlowUi::detail::InitialCapacityTarget::fonts_family_lookup,
							  FlowUi::detail::InitialCapacityTarget::icons_requests}) {
		for (uint64_t index = 0; index < 4; ++index) {
			MemorySampleSegment segment;
			segment.beginNs = 1 + index * 2;
			segment.endNs = segment.beginNs + 2;
			segment.beginTick = index;
			segment.endTickExclusive = index + 1;
			segment.value.source = definition(target).id();
			segment.value.objectCount = (index + 1) * 20;
			segment.value.value_fields = uint16_t(MemoryValueField::Objects);
			recording.segments.emplace_back(segment);
		}
	}
	state.datasets.emplace_back(std::move(recording));
}
void check_inheritance_and_locks() {
	auto live = measured();
	MemoryAnalysisState state;
	add_recording(state);
	select_memory_capacity_scope(state, uint64_t(DevMemoryNodeId::Fonts));
	state.capacity_tool.editor.planner.margin_percent = 0;
	preview_memory_capacities(state, live);
	assert(prepare_memory_capacities(state));
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup).plan.initial ==
		80);
	assert(lock_memory_capacity_rule(state, 0));
	select_memory_capacity_scope(state, uint64_t(DevMemoryNodeId::Managers));
	auto& tool = state.capacity_tool;
	tool.editor.evidence_mode = 0;
	tool.editor.dataset = 1;
	tool.editor.planner.initial_mode = 1;
	tool.editor.planner.percentile = 70;
	tool.editor.planner.margin_percent = 0;
	preview_memory_capacities(state, live);
	assert(prepare_memory_capacities(state));
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup).plan.initial ==
		80);
	assert(prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.initial ==
		   60);
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.steps.empty());
	assert(!revert_memory_capacity_rule(state, 0, live));
	assert(lock_memory_capacity_rule(state, 0)); // unlocking preserves values
	tool.editor.planner.percentile = 50;
	preview_memory_capacities(state, live);
	assert(prepare_memory_capacities(state));
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup).plan.initial ==
		80); // specificity, even unlocked
	assert(prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.initial ==
		   40);
	undo_memory_capacity_edit(state);
	assert(prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.initial ==
		   60);
	const auto font_rule =
		std::ranges::find(tool.rules, uint64_t(DevMemoryNodeId::Fonts), &MemoryCapacityRule::scope);
	assert(font_rule != tool.rules.end());
	assert(revert_memory_capacity_rule(state, size_t(font_rule - tool.rules.begin()), live));
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup).plan.initial ==
		60);
	undo_memory_capacity_edit(state);
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup).plan.initial ==
		80);
	const auto restored_font =
		std::ranges::find(tool.rules, uint64_t(DevMemoryNodeId::Fonts), &MemoryCapacityRule::scope);
	assert(lock_memory_capacity_rule(state, size_t(restored_font - tool.rules.begin())));
	live.capacity_high_water[static_cast<size_t>(
								 FlowUi::detail::InitialCapacityTarget::fonts_family_lookup)]
		.active_entries = 120;
	select_memory_capacity_scope(state, uint64_t(DevMemoryNodeId::Fonts));
	preview_memory_capacities(state, live);
	assert(std::ranges::any_of(tool.proposals, [](const auto& value) { return value.exceeded; }));
	assert(!prepare_memory_capacities(state));
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup).plan.initial ==
		80);
	const auto manifest = export_staged_memory_capacities(state, true);
	assert(manifest && manifest->targets.size() == 2);
	assert(memory_capacity_scope(uint64_t(DevMemoryNodeId::SharedManagers)) ==
		   uint64_t(DevMemoryNodeId::Managers));
	assert(memory_capacity_contains(uint64_t(DevMemoryNodeId::Managers),
									uint64_t(DevMemoryNodeId::Fonts)));
	state.datasets.clear();
	preview_memory_capacities(state, live);
	assert(prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.initial ==
		   60); // retained prepared evidence survives deletion
}
void check_parent_first() {
	MemoryAnalysisState state;
	const auto live = measured();
	add_recording(state);
	select_memory_capacity_scope(state, uint64_t(DevMemoryNodeId::Managers));
	auto& recipe = state.capacity_tool.editor;
	recipe.evidence_mode = 0;
	recipe.dataset = 1;
	recipe.planner.initial_mode = 1;
	recipe.planner.percentile = 70;
	recipe.planner.margin_percent = 0;
	preview_memory_capacities(state, live);
	assert(prepare_memory_capacities(state));
	select_memory_capacity_scope(state, uint64_t(DevMemoryNodeId::Fonts));
	state.capacity_tool.editor.evidence_mode = 1;
	state.capacity_tool.editor.planner.initial_mode = 2;
	state.capacity_tool.editor.planner.margin_percent = 0;
	preview_memory_capacities(state, live);
	assert(prepare_memory_capacities(state));
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup).plan.initial ==
		80);
	assert(prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.initial ==
		   60);
}
void check_peak_and_percentile_gates() {
	auto live = measured();
	MemoryAnalysisState state;
	select_memory_capacity_scope(state, uint64_t(DevMemoryNodeId::Icons));
	preview_memory_capacities(state, live);
	assert(prepare_memory_capacities(state));
	assert(prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.initial ==
		   84);
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.steps.empty());
	state.capacity_tool.editor.planner.initial_mode = 1;
	state.capacity_tool.editor.planner.percentile = 70;
	preview_memory_capacities(state, live);
	assert(!prepare_memory_capacities(state));
	assert(prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.initial ==
		   84);
	state.capacity_tool.editor.planner.initial_mode = 2;
	state.capacity_tool.editor.planner.margin_percent = 1e100;
	preview_memory_capacities(state, live);
	assert(!prepare_memory_capacities(state));
	live.capacity_high_water[static_cast<size_t>(
								 FlowUi::detail::InitialCapacityTarget::icons_requests)]
		.active_entries = 0;
	state.capacity_tool.editor.planner.margin_percent = 0;
	preview_memory_capacities(state, live);
	assert(prepare_memory_capacities(state));
	assert(prepared(state, FlowUi::detail::InitialCapacityTarget::icons_requests).plan.initial ==
		   1);
	live.capacity_high_water[static_cast<size_t>(
								 FlowUi::detail::InitialCapacityTarget::icons_requests)]
		.valid_fields = 0;
	state.capacity_tool.editor.planner.margin_percent = 0;
	preview_memory_capacities(state, live);
	assert(!prepare_memory_capacities(state));
}
void check_shared_storage_factor() {
	MemoryAnalysisState state;
	MemoryDataset recording;
	recording.identity = 1;
	recording.name = "Storage workload";
	recording.begin_ns = 1;
	recording.end_ns = 10;
	recording.complete = true;
	recording.segments.reserve(8);
	for (const auto& [source, demands] :
		 std::array{std::pair{memory_sources::kStoragePersistent.id,
							  std::array<uint64_t, 4>{64, 64, 128, 256}},
					std::pair{memory_sources::kStorageStringPool.id,
							  std::array<uint64_t, 4>{128, 128, 192, 384}}}) {
		for (size_t index = 0; index < demands.size(); ++index) {
			MemorySampleSegment segment;
			segment.beginNs = 1 + index * 2;
			segment.endNs = segment.beginNs + 2;
			segment.value.source = source;
			segment.value.logicalLiveBytes = demands[index];
			segment.value.value_fields = uint16_t(MemoryValueField::LogicalLive);
			recording.segments.emplace_back(segment);
		}
	}
	state.datasets.emplace_back(std::move(recording));
	select_memory_capacity_scope(state, 0);
	auto& recipe = state.capacity_tool.editor;
	recipe.evidence_mode = 0;
	recipe.dataset = 1;
	recipe.planner.initial_mode = 1;
	recipe.planner.percentile = 50;
	recipe.planner.margin_percent = 0;
	recipe.planner.factor_mode = 1;
	recipe.planner.maximum_growths = 1;
	MemoryLiveRead live;
	preview_memory_capacities(state, live);
	assert(prepare_memory_capacities(state));
	const auto& persistent =
		prepared(state, FlowUi::detail::InitialCapacityTarget::storage_persistent_bytes);
	const auto& strings =
		prepared(state, FlowUi::detail::InitialCapacityTarget::storage_strings_bytes);
	assert(persistent.plan.factor == strings.plan.factor);
	assert(persistent.plan.maximum_instance_growths <= 1 &&
		   strings.plan.maximum_instance_growths <= 1);
	assert(lock_memory_capacity_rule(state, 0));
	recipe.planner.percentile = 95;
	preview_memory_capacities(state, live);
	assert(!prepare_memory_capacities(state));
}
void check_pending_edits_and_default_reset() {
	MemoryAnalysisState state;
	auto live = measured();
	select_memory_capacity_scope(state, uint64_t(DevMemoryNodeId::Fonts));
	state.capacity_tool.editor.planner.margin_percent = 12;
	select_memory_capacity_scope(state, uint64_t(DevMemoryNodeId::Managers));
	select_memory_capacity_scope(state, uint64_t(DevMemoryNodeId::Fonts));
	assert(state.capacity_tool.editor.planner.margin_percent == 12);
	assert(state.staged.empty()); // Navigation preserves a preview without committing it.
	state.capacity_tool.editor.planner.initial_mode = 4;
	preview_memory_capacities(state, live);
	assert(prepare_memory_capacities(state));
	const auto& font = definition(FlowUi::detail::InitialCapacityTarget::fonts_family_lookup);
	assert(
		prepared(state, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup).plan.initial ==
		font.default_value);
	const auto manifest = export_staged_memory_capacities(state, true);
	assert(manifest && manifest->targets.size() == 1 && manifest->targets.front().value == 0);
	MemoryAnalysisState target_state;
	select_memory_capacity_scope(target_state, 0);
	target_state.capacity_tool.editor_target = font.id();
	target_state.capacity_tool.editor.planner.margin_percent = 0;
	preview_memory_capacities(target_state, live);
	assert(prepare_memory_capacities(target_state));
	select_memory_capacity_scope(target_state, uint64_t(DevMemoryNodeId::Fonts));
	target_state.capacity_tool.editor_target = font.id();
	target_state.capacity_tool.editor.planner.margin_percent = 10;
	preview_memory_capacities(target_state, live);
	assert(prepare_memory_capacities(target_state));
	assert(target_state.capacity_tool.rules.size() == 1 && target_state.staged.size() == 1);
	assert(prepared(target_state, FlowUi::detail::InitialCapacityTarget::fonts_family_lookup)
			   .plan.initial == 88);
}
struct PeakProbe {
	uint64_t active = 100, backing = 4096;
	uint16_t fields = uint16_t(MemoryValueField::LogicalLive) | uint16_t(MemoryValueField::Objects);
};
void sample_peak(const void* owner, MemoryProbeContext& context) noexcept {
	const auto& probe = *static_cast<const PeakProbe*>(owner);
	MemoryValueSample sample;
	sample.source = definition(FlowUi::detail::InitialCapacityTarget::fonts_family_lookup).id();
	sample.window = 99;
	sample.objectCount = probe.active;
	sample.logicalLiveBytes = probe.active * 8;
	sample.backingAllocatedBytes = probe.backing;
	sample.value_fields = probe.fields;
	const auto appended = context.sink.append(sample);
	assert(appended);
}
void check_high_water_lifetime() {
	DevMemory memory(DevMemoryConfig{.level = MemoryMonitoringLevel::SubsystemCapacity,
									 .gpuMemory = false,
									 .processMemory = false});
	PeakProbe owner;
	assert(memory.registerProbe({memory_sources::kFonts.id, &owner, sample_peak}));
	DevMemoryReporting reporting(
		memory, MemoryReportingConfig{.segmentCapacity = 2, .managerSampleEveryTicks = 1});
	MemoryCapacityProfile caller;
	caller.growthFactor = 1.01f;
	caller.storage.initialStringBytes = 1024;
	caller.allowRuntimeGrowth = false;
	const auto resolved = FlowUi::detail::resolve_memory_capacity_profile(caller);
	assert(resolved);
	reporting.set_capacity_baseline(*resolved);
	FlowUi::detail::storage::StorageConfig storage;
	const auto applied = FlowUi::detail::storage::applyMemoryCapacityProfile(storage, *resolved);
	assert(applied != 0);
	const auto baseline = reporting.read_live();
	assert(baseline.storage_growth_factor == storage.growthFactor);
	assert(baseline.allow_runtime_growth == storage.allowRuntimeGrowth);
	assert(baseline.initial_capacities.get(
			   FlowUi::detail::InitialCapacityTarget::storage_strings_bytes) ==
		   storage.initialStringBytes);
	const auto peak_index =
		static_cast<size_t>(FlowUi::detail::InitialCapacityTarget::fonts_family_lookup);
	reporting.consume(1);
	auto live = reporting.read_live();
	assert(live.capacity_high_water[peak_index].active_entries == 100);
	assert(live.capacity_high_water[peak_index].active_bytes == 800);
	const auto revision = live.capacity_high_water[peak_index].revision;
	owner.active = 10;
	owner.backing = 100000;
	for (uint64_t tick = 2; tick < 8; ++tick)
		reporting.consume(tick);
	assert(reporting.read_live().capacity_high_water[peak_index].active_bytes == 800);
	auto freeze = reporting.freeze_history();
	owner.active = 120;
	reporting.consume(8);
	assert(reporting.read_live().capacity_high_water[peak_index].active_entries == 120);
	freeze.reset();
	auto config = reporting.config();
	config.segmentCapacity = 1;
	assert(reporting.setConfig(config));
	owner.fields = 0;
	owner.active = 9999;
	reporting.consume(9);
	assert(reporting.read_live().capacity_high_water[peak_index].active_entries == 120);
	owner.fields = uint16_t(MemoryValueField::Objects) | uint16_t(MemoryValueField::LogicalLive);
	owner.active = 10;
	reporting.consume(10);
	assert(reporting.read_live().capacity_high_water[peak_index].active_entries == 120);
	memory.unregisterProbe(memory_sources::kFonts.id, &owner);
	reporting.consume(11);
	live = reporting.read_live();
	assert(live.capacity_high_water[peak_index].active_entries == 120);
	assert(live.capacity_high_water[peak_index].revision > revision);
}
} // namespace
int main() {
	check_inheritance_and_locks();
	check_parent_first();
	check_peak_and_percentile_gates();
	check_high_water_lifetime();
	check_shared_storage_factor();
	check_pending_edits_and_default_reset();
	std::cout << "Hierarchical policies, locked exact values, recording percentiles and "
				 "independent high-water verified\n";
}
