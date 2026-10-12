#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Memory/Analysis/DevMemorySession.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchData.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
int main() {
	MemoryTuningTargetDescriptor target{};
	target.id = 1;
	target.configKey = "storage.initialDecodeScratchBytes";
	target.alignment = 1;
	target.minimum = 1;
	target.productionDefault = 4;
	const std::array evidence = {
		MemoryDemandObservation{2, 1, 1, 1, 0}, MemoryDemandObservation{4, 1, 2, 1, 0},
		MemoryDemandObservation{5, 1, 3, 1, 0}, MemoryDemandObservation{9, 1, 4, 1, 0}};
	auto replay = replay_memory_plan(evidence, target, 4, 1.5);
	assert(replay && replay->growth_count == 2 && replay->final_capacity == 9);
	replay = replay_memory_plan(evidence, target, 4, 2.25);
	assert(replay && replay->growth_count == 1 && replay->final_capacity == 9);
	assert((replay->ladder == std::array<uint64_t, 4>{4, 9, 21, 48}));
	MemoryPlannerSettings settings;
	settings.preset = 1;
	settings.initial_mode = 1;
	settings.factor_mode = 1;
	settings.percentile = 50;
	auto calculated = calculate_memory_plan(evidence, target, settings);
	assert(calculated && calculated->initial == 4 && calculated->maximum_instance_growths <= 1);
	assert(calculated->factor > 1.5 && calculated->initial_coverage == .5);
	settings.preset = 2;
	calculated = calculate_memory_plan(evidence, target, settings);
	assert(calculated && calculated->maximum_instance_growths <= 2);
	settings.preset = 0;
	settings.initial_mode = 2;
	settings.factor_mode = 0;
	calculated = calculate_memory_plan(evidence, target, settings);
	assert(calculated && calculated->initial == 9 && !calculated->growth_count);
	settings.final_budget = 8;
	assert(!calculate_memory_plan(evidence, target, settings));
	settings.final_budget = 0;
	settings.initial_mode = 1;
	settings.maximum_only = true;
	assert(!calculate_memory_plan(evidence, target, settings));
	settings.maximum_only = false;
	settings.percentile = 101;
	assert(!calculate_memory_plan(evidence, target, settings));
	assert(!replay_memory_plan(evidence, target, 4, NAN));
	assert(!replay_memory_plan(evidence, target, 4, 1));
	assert(!replay_memory_plan({}, target, 4, 1.5));
	target.alignment = 4;
	target.maximum = 10;
	assert(!replay_memory_plan(evidence, target, 9, 1.5));
	target.maximum = UINT64_MAX;
	assert(!replay_memory_plan(evidence, target, UINT64_MAX, 1.5));
	target.alignment = 1;
	std::vector<MemoryDemandObservation> instances(evidence.begin(), evidence.end());
	for (auto observation : evidence) {
		observation.instance = 2;
		instances.emplace_back(observation);
	}
	replay = replay_memory_plan(instances, target, 4, 2.25);
	assert(replay && replay->growth_count == 2 && replay->maximum_instance_growths == 1 &&
		   replay->instance_count == 2);
	instances.emplace_back(
		MemoryDemandObservation{2, 1, 5, 1, 0}); // logical reset does not release capacity
	replay = replay_memory_plan(instances, target, 4, 2.25);
	assert(replay && replay->growth_count == 2);
	instances.emplace_back(MemoryDemandObservation{9, 1, 6, 1, 1}); // recreated allocator
	replay = replay_memory_plan(instances, target, 4, 2.25);
	assert(replay && replay->growth_count == 3 && replay->instance_count == 3);
	MemoryAnalysisState state;
	state.evidence.assign(evidence.begin(), evidence.end());
	state.plan = *replay_memory_plan(evidence, target, 4, 2.25);
	state.planner.preset = 1;
	assert(stage_memory_setting(state, target));
	assert(state.staged.size() == 1 && state.shared_factor == 2.25);
	assert(!stage_memory_setting(state, target));
	assert(stage_memory_setting(state, target, true, true));
	auto other = target;
	other.id = 2;
	state.plan = *replay_memory_plan(evidence, other, 4, 1.5);
	state.planner.preset = 2;
	assert(!stage_memory_setting(state, other));
	assert(!stage_memory_setting(
		state, other, true, true)); // shared factor violates first target's one-growth objective
	assert(state.staged.size() == 1 && state.shared_factor == 2.25);
	state.plan = *replay_memory_plan(evidence, other, 9, 2.25);
	assert(stage_memory_setting(state, other, true, true));
	assert(state.staged.size() == 2);
	assert(memory_public_config_key("managers.inputFieldsReserve") ==
		   "AppConfig.memoryCapacityProfile.managers.inputFields");
	assert(memory_target_value(5, MemoryCapacityUnit::Entries, false) == "5 entries");
	DevMemoryWorkbenchState chart;
	chart.retained_begin_ns = 100;
	chart.retained_end_ns = 1100;
	chart.view_begin_ns = 500;
	chart.view_end_ns = 900;
	chart.duration_ns = 400;
	zoom_memory_view(chart, .5, .25);
	assert(chart.view_begin_ns == 550 && chart.view_end_ns == 750);
	for (int step = 0; step < 30; ++step)
		pan_memory_view(chart, -1);
	assert(chart.view_begin_ns == 100);
	for (int step = 0; step < 30; ++step)
		pan_memory_view(chart, 1);
	assert(chart.view_end_ns == 1100);
	std::cout << "Capacity percentile, joint tuning, replay lifetimes, projected ladder, shared "
				 "drafts and chart navigation verified\n";
}
