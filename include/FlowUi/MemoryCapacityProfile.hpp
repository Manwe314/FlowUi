#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/Error.hpp"
#include "FlowUi/PublicStructs.hpp"

namespace FlowUi {

/** Resolve startup precedence once. Conflicting caller duplicates and unknown active hints are
 * errors. */
[[nodiscard]] inline Result<MemoryCapacityProfile>
resolve_memory_capacity_profile(const MemoryCapacityProfile& caller,
								std::span<const MemoryCapacitySetting> compiled = {}) noexcept {
	try {
		if (!std::isfinite(caller.growthFactor) || caller.growthFactor <= 1)
			return unexpectedError(
				makeError(ErrorCode::StorageConfigurationInvalid, ErrorSite::StorageInitialize));
		MemoryCapacityProfile effective = caller;
		effective.settings.clear();
		effective.settings.reserve(initial_capacity_definitions.size());
		for (const auto& definition : initial_capacity_definitions) {
			uint64_t typed = 0;
			if (definition.target == InitialCapacityTarget::storage_persistent_bytes)
				typed = caller.storage.initialPersistentCpuBytes;
			if (definition.target == InitialCapacityTarget::storage_strings_bytes)
				typed = caller.storage.initialStringBytes;
			if (definition.target == InitialCapacityTarget::storage_frame_bytes)
				typed = caller.storage.transientBytesPerFramePerWindow;
			if (definition.target == InitialCapacityTarget::storage_worker_bytes)
				typed = caller.storage.transientBytesPerWorker;
			if (definition.target == InitialCapacityTarget::storage_decode_bytes)
				typed = caller.storage.initialDecodeScratchBytes;
			if (definition.target == InitialCapacityTarget::storage_staging_bytes)
				typed = caller.storage.initialUploadStagingBytes;
			if (definition.target == InitialCapacityTarget::storage_instance_bytes)
				typed = caller.storage.initialInstanceBytesPerFrame;
			if (typed) {
				if (!valid_initial_capacity(definition, typed))
					return unexpectedError(makeError(ErrorCode::StorageConfigurationInvalid,
													 ErrorSite::StorageInitialize));
				effective.settings.emplace_back(MemoryCapacitySetting{definition.id(), typed});
			}
		}
		for (size_t index = 0; index < caller.settings.size(); ++index) {
			const auto& setting = caller.settings[index];
			const auto* definition = find_initial_capacity(setting.target);
			if (!definition || !valid_initial_capacity(*definition, setting.value))
				return unexpectedError(makeError(ErrorCode::StorageConfigurationInvalid,
												 ErrorSite::StorageInitialize));
			for (size_t prior = 0; prior < index; ++prior)
				if (caller.settings[prior].target == setting.target &&
					caller.settings[prior].value != setting.value)
					return unexpectedError(makeError(ErrorCode::StorageConfigurationInvalid,
													 ErrorSite::StorageInitialize));
			std::erase_if(effective.settings,
						  [&](const auto& current) { return current.target == setting.target; });
			effective.settings.emplace_back(setting);
		}
		for (const auto& setting : compiled) {
			const auto* definition = find_initial_capacity(setting.target);
			if (!definition || !valid_initial_capacity(*definition, setting.value))
				return unexpectedError(makeError(ErrorCode::StorageConfigurationInvalid,
												 ErrorSite::StorageInitialize));
			std::erase_if(effective.settings,
						  [&](const auto& current) { return current.target == setting.target; });
			effective.settings.emplace_back(setting);
		}
		for (const auto& setting : effective.settings) {
			const auto* definition = find_initial_capacity(setting.target);
			const auto value = setting.value ? setting.value : definition->default_value;
			if (definition->target == InitialCapacityTarget::storage_persistent_bytes)
				effective.storage.initialPersistentCpuBytes = value;
			if (definition->target == InitialCapacityTarget::storage_strings_bytes)
				effective.storage.initialStringBytes = value;
			if (definition->target == InitialCapacityTarget::storage_frame_bytes)
				effective.storage.transientBytesPerFramePerWindow = value;
			if (definition->target == InitialCapacityTarget::storage_worker_bytes)
				effective.storage.transientBytesPerWorker = value;
			if (definition->target == InitialCapacityTarget::storage_decode_bytes)
				effective.storage.initialDecodeScratchBytes = value;
			if (definition->target == InitialCapacityTarget::storage_staging_bytes)
				effective.storage.initialUploadStagingBytes = value;
			if (definition->target == InitialCapacityTarget::storage_instance_bytes)
				effective.storage.initialInstanceBytesPerFrame = value;
		}
		return effective;
	} catch (...) {
		return unexpectedError(
			makeError(ErrorCode::AllocationFailed, ErrorSite::StorageInitialize));
	}
}

struct MemoryGrowthSimulation {
	uint64_t initialCapacity = 0u;
	uint64_t finalCapacity = 0u;
	uint64_t maximumDemand = 0u;
	uint64_t growthCount = 0u;
	uint64_t grownBytes = 0u;
	uint64_t coveredObservations = 0u;
	uint64_t observationCount = 0u;
	double initialCoverage = 0.0;
	bool overflowed = false;
};

[[nodiscard]] constexpr uint64_t alignMemoryCapacity(uint64_t value, uint64_t alignment) noexcept {
	if (alignment <= 1u) return value;
	const uint64_t remainder = value % alignment;
	if (remainder == 0u) return value;
	const uint64_t addition = alignment - remainder;
	return value > std::numeric_limits<uint64_t>::max() - addition
		? std::numeric_limits<uint64_t>::max() : value + addition;
}

/** Mirrors FlowStorageSystem's production growth rule. */
[[nodiscard]] inline uint64_t nextMemoryCapacity(
	uint64_t current, uint64_t required, float growthFactor, uint64_t alignment = 1u) noexcept {
	uint64_t capacity = std::max<uint64_t>(current, 1u);
	const double factor = std::max(1.1, static_cast<double>(growthFactor));
	while (capacity < required) {
		const double candidate = std::ceil(static_cast<double>(capacity) * factor);
		if (!std::isfinite(candidate) || candidate >= static_cast<double>(std::numeric_limits<uint64_t>::max())) {
			capacity = required;
			break;
		}
		const uint64_t grown = static_cast<uint64_t>(candidate);
		if (grown <= capacity) { capacity = required; break; }
		capacity = grown;
	}
	(void)alignment;
	return capacity;
}

[[nodiscard]] inline MemoryGrowthSimulation simulateMemoryGrowth(
	std::span<const uint64_t> demandTrace,
	uint64_t initialCapacity,
	float growthFactor,
	uint64_t alignment = 1u) noexcept {
	MemoryGrowthSimulation result{};
	result.initialCapacity = alignMemoryCapacity(initialCapacity, std::max<uint64_t>(1u, alignment));
	result.finalCapacity = result.initialCapacity;
	result.observationCount = demandTrace.size();
	for (const uint64_t demand : demandTrace) {
		result.maximumDemand = std::max(result.maximumDemand, demand);
		if (demand <= result.initialCapacity) ++result.coveredObservations;
		if (demand <= result.finalCapacity) continue;
		const uint64_t before = result.finalCapacity;
		const uint64_t after = nextMemoryCapacity(before, demand, growthFactor, alignment);
		if (after < demand || after < before) {
			result.overflowed = true;
			result.finalCapacity = std::numeric_limits<uint64_t>::max();
			break;
		}
		result.finalCapacity = after;
		result.grownBytes += after - before;
		++result.growthCount;
	}
	result.initialCoverage = result.observationCount == 0u ? 0.0
		: static_cast<double>(result.coveredObservations) / static_cast<double>(result.observationCount);
	return result;
}

} // namespace FlowUi
