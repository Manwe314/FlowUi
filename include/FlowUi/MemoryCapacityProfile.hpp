#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace FlowUi {

using MemoryCapacityTargetId = uint64_t;

[[nodiscard]] consteval MemoryCapacityTargetId makeMemoryCapacityTargetId(std::string_view stableName) {
	uint64_t hash = 14695981039346656037ull;
	for (const char character : stableName) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(character));
		hash *= 1099511628211ull;
	}
	return hash == 0u ? 1u : hash;
}

struct MemoryCapacitySetting {
	MemoryCapacityTargetId target = 0u;
	uint64_t value = 0u;
};

struct MemoryCapacityProfileMetadata {
	std::string platform{};
	std::string build{};
	std::string gpu{};
	uint32_t framesInFlight = 0u;
	uint64_t captureBeginTick = 0u;
	uint64_t captureEndTickExclusive = 0u;
	uint64_t warmUpTicks = 0u;
	bool complete = false;
};

struct StorageMemoryCapacities {
	uint64_t initialPersistentCpuBytes = 0u;
	uint64_t initialStringBytes = 0u;
	uint64_t transientBytesPerFramePerWindow = 0u;
	uint64_t transientBytesPerWorker = 0u;
	uint64_t initialDecodeScratchBytes = 0u;
	uint64_t initialUploadStagingBytes = 0u;
	uint64_t initialInstanceBytesPerFrame = 0u;
};

struct ManagerMemoryCapacities {
	uint64_t elements = 0u, inputFields = 0u, inputTextBytes = 0u, fonts = 0u,
		fontAtlasCpuPixelBytes = 0u, icons = 0u, iconDocuments = 0u,
		iconAtlasMetadata = 0u, viewports = 0u, popups = 0u, shortcuts = 0u,
		actions = 0u, themes = 0u, uiLayout = 0u, renderer = 0u;
};

/** Durable artifact shared by dev preview and production initialization. */
struct MemoryCapacityProfile {
	StorageMemoryCapacities storage{};
	ManagerMemoryCapacities managers{};
	std::vector<MemoryCapacitySetting> settings{};
	float growthFactor = 1.5f;
	bool allowRuntimeGrowth = true;
	MemoryCapacityProfileMetadata metadata{};
};

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
