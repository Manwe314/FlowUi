#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"

namespace FlowUi::devSystems {

struct DevContainerMemoryAccumulator {
	uint64_t liveBytes = 0u;
	uint64_t capacityBytes = 0u;
	uint64_t objectCount = 0u;
	uint64_t capacityCount = 0u;
	MemorySampleFlag flags = MemorySampleFlag::Estimate;

	template<class T, class Allocator>
	void add(const std::vector<T, Allocator>& value) noexcept {
		liveBytes += static_cast<uint64_t>(value.size()) * sizeof(T);
		capacityBytes += static_cast<uint64_t>(value.capacity()) * sizeof(T);
		objectCount += value.size();
		capacityCount += value.capacity();
	}

	template<class Char, class Traits, class Allocator>
	void add(const std::basic_string<Char, Traits, Allocator>& value) noexcept {
		liveBytes += static_cast<uint64_t>(value.size()) * sizeof(Char);
		capacityBytes += static_cast<uint64_t>(value.capacity()) * sizeof(Char);
		objectCount += value.size();
		capacityCount += value.capacity();
	}

	template<class Container>
	void addNodeContainer(const Container& value) noexcept {
		liveBytes += static_cast<uint64_t>(value.size()) * sizeof(typename Container::value_type);
		capacityBytes += static_cast<uint64_t>(value.size()) * sizeof(typename Container::value_type);
		objectCount += value.size();
		capacityCount += value.size();
		if constexpr (requires { value.bucket_count(); }) {
			capacityBytes += static_cast<uint64_t>(value.bucket_count()) * sizeof(void*);
			capacityCount += value.bucket_count();
		}
	}

	[[nodiscard]] MemoryValueSample sample(
		MemorySourceId source,
		WindowId window = InvalidWindowId) const noexcept {
		return MemoryValueSample{
			.source = source,
			.window = window,
			.logicalLiveBytes = liveBytes,
			.reusableBytes = capacityBytes > liveBytes ? capacityBytes - liveBytes : 0u,
			.backingAllocatedBytes = capacityBytes,
			.objectCount = objectCount,
			.capacityCount = capacityCount,
			.flags = flags,
		};
	}
};

inline void appendManagerSample(
	MemorySampleSink& sink,
	MemorySourceId source,
	const DevContainerMemoryAccumulator& accumulator,
	WindowId window = InvalidWindowId) noexcept {
	(void)sink.append(accumulator.sample(source, window));
}

} // namespace FlowUi::devSystems

#endif
