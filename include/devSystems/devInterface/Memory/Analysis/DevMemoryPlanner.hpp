#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Error.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include <array>
#include <span>
namespace FlowUi::devSystems::interface_elements {
/** One measured demand; independent instances/lifetimes never share capacity. */
struct MemoryDemandObservation {
	uint64_t demand = 0, weight = 1, timestamp_ns = 0, instance = 0, lifetime = 0;
};
/** Editable policy in native target units. Zero budget means unbounded. */
struct MemoryPlannerSettings {
	uint64_t preset = 0, initial_mode = 2, factor_mode = 0, weighting = 0;
	uint64_t manual_initial = 0, absolute_margin = 0, margin_mode = 0;
	uint64_t next_target = 0, final_budget = 0, maximum_growths = 1, preference = 1;
	double percentile = 95, margin_percent = 0, factor = 1.5, maximum_factor = 8;
	bool maximum_only = false;
	friend bool operator==(const MemoryPlannerSettings&, const MemoryPlannerSettings&) = default;
};
/** Capacity transition produced by replay, not an invented workload event. */
struct MemoryGrowthStep {
	uint64_t timestamp_ns = 0, before = 0, after = 0, required = 0, instance = 0;
};
/** Validated replay and projected minimal-overflow growth ladder. */
struct MemoryPlan {
	std::vector<MemoryGrowthStep> steps{};
	std::array<uint64_t, 4> ladder{};
	uint64_t initial = 0, final_capacity = 0, observed_maximum = 0, percentile_demand = 0;
	uint64_t growth_count = 0, maximum_instance_growths = 0, instance_count = 0;
	double factor = 1.5, initial_coverage = 0;
	bool ladder_overflow = false, uses_default = false;
};
/** Replay one proposal using rounded geometric grow-to-fit, with checked arithmetic. */
[[nodiscard]] Result<MemoryPlan>
replay_memory_plan(std::span<const MemoryDemandObservation> evidence,
				   const MemoryTuningTargetDescriptor& target, uint64_t initial, double factor);
/** Evaluate percentile/maximum/manual reserve and bounded joint factor candidates. */
[[nodiscard]] Result<MemoryPlan>
calculate_memory_plan(std::span<const MemoryDemandObservation> evidence,
					  const MemoryTuningTargetDescriptor& target,
					  const MemoryPlannerSettings& settings);
/** Formatting for bytes and count-based configuration targets. */
[[nodiscard]] std::string memory_target_value(uint64_t value, MemoryCapacityUnit unit,
											  bool decimal);
/** Map reporting descriptor keys to the corresponding public profile field. */
[[nodiscard]] std::string memory_public_config_key(std::string_view reporting_key);
} // namespace FlowUi::devSystems::interface_elements
#endif
