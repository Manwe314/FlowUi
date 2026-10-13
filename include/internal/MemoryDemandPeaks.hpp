#pragma once
#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/MemoryCapacityTargets.hpp"
#include <algorithm>
#include <atomic>
#include <utility>
namespace FlowUi::detail {
/** Runtime switch shared by one application's owners; generation discards disabled intervals. */
struct MemoryDemandControl {
	std::atomic<uint64_t> generation{1};
	std::atomic<bool> enabled{true};
};
/** Interval maxima; the empty array removes instrumentation in production and levels 0/1. */
struct MemoryDemandPeaks {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	static constexpr size_t target_count = static_cast<size_t>(InitialCapacityTarget::count);
#else
	static constexpr size_t target_count = 0;
#endif
	mutable std::array<uint64_t, target_count> values{};
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	const MemoryDemandControl* runtime_control = nullptr;
	mutable uint64_t observed_generation = 0;
#endif
	/** Bind a runtime gate with owner/service lifetime; null means standalone collection. */
	void bind(const MemoryDemandControl* control) noexcept {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		runtime_control = control;
		observed_generation = 0;
		values.fill(0);
#else
		if (control && target_count)
			values.fill(0);
#endif
	}
	[[nodiscard]] bool enabled() const noexcept {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		if (runtime_control) {
			const auto generation = runtime_control->generation.load(std::memory_order_relaxed);
			if (generation != observed_generation) {
				values.fill(0);
				observed_generation = generation;
			}
			return runtime_control->enabled.load(std::memory_order_relaxed);
		}
#endif
		return target_count != 0;
	}
	void record(InitialCapacityTarget target, uint64_t demand) const noexcept {
		const auto index = static_cast<size_t>(target);
		if (enabled() && index < values.size())
			values[index] = std::max(values[index], demand);
	}
	[[nodiscard]] uint64_t consume(InitialCapacityTarget target, uint64_t current) const noexcept {
		const auto index = static_cast<size_t>(target);
		return enabled() && index < values.size()
				   ? std::max(current, std::exchange(values[index], 0))
				   : current;
	}
};
} // namespace FlowUi::detail

/** Mutation instrumentation is removed entirely from production and memory levels 0/1. */
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
#define FLOWUI_MEMORY_DEMAND_RECORD(Peaks, Target, Demand) (Peaks).record((Target), (Demand))
#else
#define FLOWUI_MEMORY_DEMAND_RECORD(Peaks, Target, Demand) \
	do {                                                   \
	} while (false)
#endif
