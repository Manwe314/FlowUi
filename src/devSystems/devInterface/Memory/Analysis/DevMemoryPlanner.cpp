#include "devSystems/devInterface/Memory/Analysis/DevMemoryPlanner.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchData.hpp"
#include <algorithm>
#include <cmath>
namespace FlowUi::devSystems::interface_elements {
namespace {
[[nodiscard]] auto failure(ErrorCode code = ErrorCode::ArithmeticOverflow) noexcept {
	return unexpectedError(makeError(code, ErrorSite::None));
}
[[nodiscard]] Result<uint64_t> aligned(uint64_t value,
									   const MemoryTuningTargetDescriptor& target) noexcept {
	const auto alignment = std::max<uint64_t>(1, target.alignment);
	value = std::max(value, target.minimum);
	const auto remainder = value % alignment;
	if (remainder) {
		const auto addition = alignment - remainder;
		if (value > UINT64_MAX - addition)
			return failure();
		value += addition;
	}
	if (value > target.maximum)
		return failure();
	return value;
}
[[nodiscard]] Result<uint64_t> grow(uint64_t capacity, uint64_t required, double factor,
									const MemoryTuningTargetDescriptor& target) noexcept {
	capacity = std::max<uint64_t>(1, capacity);
	while (capacity < required) {
		const auto candidate = std::ceil(static_cast<long double>(capacity) * factor);
		if (candidate > UINT64_MAX || !std::isfinite(candidate))
			return failure();
		const auto next = aligned(uint64_t(candidate), target);
		if (!next || *next <= capacity)
			return failure();
		capacity = *next;
	}
	return capacity;
}
} // namespace
Result<MemoryPlan> replay_memory_plan(std::span<const MemoryDemandObservation> evidence,
									  const MemoryTuningTargetDescriptor& target, uint64_t initial,
									  double factor) {
	if (evidence.empty() || !std::isfinite(factor) || factor < 1.1 || factor > 64)
		return failure(ErrorCode::UnsupportedBuildFeature);
	const auto reserve = aligned(initial, target);
	if (!reserve)
		return failure();
	MemoryPlan result;
	result.initial = *reserve;
	result.final_capacity = *reserve;
	result.factor = factor;
	result.steps.reserve(evidence.size());
	std::vector<MemoryDemandObservation> ordered(evidence.begin(), evidence.end());
	std::ranges::stable_sort(ordered, [](const auto& left, const auto& right) {
		if (left.instance != right.instance)
			return left.instance < right.instance;
		if (left.lifetime != right.lifetime)
			return left.lifetime < right.lifetime;
		return left.timestamp_ns < right.timestamp_ns;
	});
	uint64_t capacity = *reserve, growths = 0;
	long double covered = 0, total = 0;
	for (size_t index = 0; index < ordered.size(); ++index) {
		const auto& observation = ordered[index];
		if (!index || observation.instance != ordered[index - 1].instance ||
			observation.lifetime != ordered[index - 1].lifetime) {
			result.maximum_instance_growths = std::max(result.maximum_instance_growths, growths);
			capacity = *reserve;
			growths = 0;
			++result.instance_count;
		}
		result.observed_maximum = std::max(result.observed_maximum, observation.demand);
		total += observation.weight;
		if (observation.demand <= *reserve)
			covered += observation.weight;
		if (observation.demand > capacity) {
			const auto next = grow(capacity, observation.demand, factor, target);
			if (!next)
				return failure();
			result.steps.emplace_back(MemoryGrowthStep{observation.timestamp_ns, capacity, *next,
													   observation.demand, observation.instance});
			capacity = *next;
			++growths;
			++result.growth_count;
		}
		result.final_capacity = std::max(result.final_capacity, capacity);
	}
	result.maximum_instance_growths = std::max(result.maximum_instance_growths, growths);
	result.initial_coverage = total ? double(covered / total) : 0;
	result.ladder[0] = *reserve;
	for (size_t index = 1; index < result.ladder.size(); ++index) {
		if (result.ladder[index - 1] == UINT64_MAX) {
			result.ladder_overflow = true;
			break;
		}
		const auto next =
			grow(result.ladder[index - 1], result.ladder[index - 1] + 1, factor, target);
		if (!next) {
			result.ladder_overflow = true;
			break;
		}
		result.ladder[index] = *next;
	}
	return result;
}
Result<MemoryPlan> calculate_memory_plan(std::span<const MemoryDemandObservation> evidence,
										 const MemoryTuningTargetDescriptor& target,
										 const MemoryPlannerSettings& settings) {
	if ((settings.initial_mode == 0 && !target.productionDefault) || evidence.empty() ||
		!std::isfinite(settings.percentile) || settings.percentile < 1 ||
		settings.percentile > 100 || !std::isfinite(settings.margin_percent) ||
		settings.margin_percent < 0 || !std::isfinite(settings.maximum_factor) ||
		settings.maximum_factor < 1.1 || settings.maximum_factor > 64 ||
		(settings.maximum_only && settings.initial_mode == 1))
		return failure(ErrorCode::UnsupportedBuildFeature);
	std::vector<MemoryDemandObservation> distribution(evidence.begin(), evidence.end());
	std::ranges::sort(distribution, {}, &MemoryDemandObservation::demand);
	long double total = 0;
	for (const auto& value : distribution)
		total += value.weight;
	if (!total)
		return failure(ErrorCode::UnsupportedBuildFeature);
	const auto threshold = std::ceil(total * settings.percentile / 100);
	long double cumulative = 0;
	uint64_t percentile = 0;
	for (const auto& value : distribution) {
		cumulative += value.weight;
		if (cumulative >= threshold) {
			percentile = value.demand;
			break;
		}
	}
	uint64_t initial = settings.initial_mode == 0	? target.productionDefault
					   : settings.initial_mode == 1 ? percentile
					   : settings.initial_mode == 2 ? distribution.back().demand
													: settings.manual_initial;
	if (settings.initial_mode != 0) {
		const long double proposal =
			settings.margin_mode == 0
				? std::ceil(static_cast<long double>(initial) * (1 + settings.margin_percent / 100))
				: static_cast<long double>(initial) + settings.absolute_margin;
		if (proposal > UINT64_MAX || !std::isfinite(proposal))
			return failure();
		initial = uint64_t(proposal);
	}
	const auto reserve = aligned(initial, target);
	if (!reserve)
		return failure();
	initial = *reserve;
	const auto acceptable = [&](const MemoryPlan& plan) {
		return (!settings.final_budget || plan.final_capacity <= settings.final_budget) &&
			   (plan.maximum_instance_growths <= (settings.preset == 3	 ? settings.maximum_growths
												  : settings.preset == 0 ? 0
												  : settings.preset == 1 ? 1
																		 : 2)) &&
			   (!settings.next_target || plan.ladder[1] >= settings.next_target);
	};
	if (settings.factor_mode != 1) {
		auto result = replay_memory_plan(evidence, target, initial, settings.factor);
		if (!result || !acceptable(*result))
			return failure(ErrorCode::UnsupportedBuildFeature);
		result->percentile_demand = percentile;
		result->uses_default = settings.initial_mode == 0;
		return result;
	}
	if (settings.next_target && settings.next_target <= initial)
		return failure(ErrorCode::UnsupportedBuildFeature);
	std::optional<MemoryPlan> best;
	const auto consider = [&](double factor) {
		if (factor < 1.1 || factor > settings.maximum_factor || !std::isfinite(factor))
			return;
		auto result = replay_memory_plan(evidence, target, initial, factor);
		if (!result || !acceptable(*result))
			return;
		const auto better =
			!best || (settings.preference == 1 ? (result->growth_count < best->growth_count ||
												  (result->growth_count == best->growth_count &&
												   result->final_capacity < best->final_capacity))
											   : (result->final_capacity < best->final_capacity ||
												  (result->final_capacity == best->final_capacity &&
												   result->growth_count < best->growth_count)));
		if (better)
			best = std::move(*result);
	};
	consider(settings.factor);
	const auto target_capacity =
		settings.next_target ? settings.next_target : distribution.back().demand;
	if (initial && target_capacity > initial)
		consider(std::clamp(double(target_capacity) / initial, 1.1, settings.maximum_factor));
	for (size_t candidate = 0; candidate <= 256; ++candidate)
		consider(1.1 + (settings.maximum_factor - 1.1) * candidate / 256);
	if (!best)
		return failure(ErrorCode::UnsupportedBuildFeature);
	best->percentile_demand = percentile;
	best->uses_default = settings.initial_mode == 0;
	return std::move(*best);
}
std::string memory_target_value(uint64_t value, MemoryCapacityUnit unit, bool decimal) {
	if (unit == MemoryCapacityUnit::Bytes)
		return memory_bytes(value, decimal);
	return std::to_string(value) + (unit == MemoryCapacityUnit::Entries	   ? " entries"
									: unit == MemoryCapacityUnit::Bindings ? " bindings"
																		   : " pages");
}
std::string memory_public_config_key(std::string_view key) {
	std::string result(key);
	if (result.ends_with("Reserve"))
		result.resize(result.size() - 7);
	return "AppConfig.memoryCapacityProfile." + result;
}
} // namespace FlowUi::devSystems::interface_elements
#endif
