#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/Private/storageSystem/capacity/MemoryDemandPeaks.hpp"
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"

namespace FlowUi::devSystems {

/**
 * Estimate aggregate container payload and backing without owning or allocating source data. Node-
 * container estimates omit allocator/node overhead; strings may use inline storage.
 */
struct DevContainerMemoryAccumulator {
	/** Accumulated live element payload bytes. */
	uint64_t liveBytes = 0u;
	/** Accumulated estimated backing capacity bytes. */
	uint64_t capacityBytes = 0u;
	/** Accumulated live element or character count. */
	uint64_t objectCount = 0u;
	/** Accumulated reserved element/character capacity, including estimated hash buckets. */
	uint64_t capacityCount = 0u;
	/** Qualifiers applied to samples produced by this accumulator. */
	MemorySampleFlag flags = MemorySampleFlag::Estimate;

	template<class T, class Allocator>
	/** Accumulate vector size/capacity multiplied by sizeof(T); nested payloads are excluded. */
	void add(const std::vector<T, Allocator>& value) noexcept {
		liveBytes += static_cast<uint64_t>(value.size()) * sizeof(T);
		capacityBytes += static_cast<uint64_t>(value.capacity()) * sizeof(T);
		objectCount += value.size();
		capacityCount += value.capacity();
	}

	template<class Char, class Traits, class Allocator>
	/** Accumulate string character size/capacity; no claim is made about heap versus inline storage. */
	void add(const std::basic_string<Char, Traits, Allocator>& value) noexcept {
		liveBytes += static_cast<uint64_t>(value.size()) * sizeof(Char);
		capacityBytes += static_cast<uint64_t>(value.capacity()) * sizeof(Char);
		objectCount += value.size();
		capacityCount += value.capacity();
	}

	template<class Container>
	/**
	 * Estimate node-container value payload and pointer-sized hash buckets; allocator/node metadata is
	 * excluded.
	 */
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

	/** Return aggregate values attributed to source and window; zero window indicates app-wide data. */
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

/** Sample actual entries, preserving pre-clear demand. Hash buckets remain diagnostic backing. */
template <class Container>
void append_capacity_demand(MemorySampleSink& sink, FlowUi::detail::InitialCapacityTarget target,
							const Container& container,
							const ::FlowUi::detail::MemoryDemandPeaks& peaks,
							WindowId window = InvalidWindowId, uint64_t sentinel = 0) noexcept {
	DevContainerMemoryAccumulator estimate{};
	if constexpr (requires { container.bucket_count(); })
		estimate.addNodeContainer(container);
	else
		estimate.add(container);
	const auto count = container.size() > sentinel ? container.size() - sentinel : 0;
	auto sample = estimate.sample(
		FlowUi::detail::initial_capacity_definitions[static_cast<size_t>(target)].id(), window);
	sample.objectCount = peaks.consume(target, count);
	sample.peakObjectCount = sample.objectCount;
	sample.flags = sample.flags | MemorySampleFlag::TuningAggregate;
	sample.value_fields =
		uint16_t(MemoryValueField::Objects) | uint16_t(MemoryValueField::Capacity) |
		uint16_t(MemoryValueField::Backing) | uint16_t(MemoryValueField::LogicalLive);
	if (FlowUi::detail::initial_capacity_definitions[static_cast<size_t>(target)].unit ==
		FlowUi::detail::InitialCapacityUnit::Bytes) {
		sample.peakLogicalBytes = sample.objectCount;
		sample.value_fields |= uint16_t(MemoryValueField::PeakLogical);
	}
	(void)sink.append(sample);
}

/**
 * Append an estimated manager-container sample synchronously; the sink may reject it without changing
 * the accumulator.
 */
inline void appendManagerSample(
	MemorySampleSink& sink,
	MemorySourceId source,
	const DevContainerMemoryAccumulator& accumulator,
	WindowId window = InvalidWindowId) noexcept {
	(void)sink.append(accumulator.sample(source, window));
}

} // namespace FlowUi::devSystems

#endif
