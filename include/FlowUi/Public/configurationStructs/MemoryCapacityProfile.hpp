#pragma once

#include "FlowUi/Public/configurationStructs/StorageConfig.hpp"
#include "FlowUi/Public/identificationTypes/MemoryCapacityTargets.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace FlowUi {

/**
 * One startup reserve hint for a registered target. Unknown targets and conflicting duplicate values
 * reject app creation.
 */
struct MemoryCapacitySetting {
	/** Stable ID returned by makeMemoryCapacityTargetId() for the registered target key. */
	MemoryCapacityTargetId target = 0u;
	/** Initial capacity in the target's registered units (bytes or entries); zero selects its default. */
	uint64_t value = 0u;
};

/**
 * Owned provenance copied from the profiling run that produced a capacity profile; metadata does not
 * change allocation policy.
 */
struct MemoryCapacityProfileMetadata {
	/** Platform description from the capture environment. */
	std::string platform{};
	/** Build description from the capture environment. */
	std::string build{};
	/** GPU description from the capture environment. */
	std::string gpu{};
	/** First included application tick. */
	uint64_t captureBeginTick = 0u;
	/** First application tick outside the captured interval. */
	uint64_t captureEndTickExclusive = 0u;
	/** Warm-up ticks excluded from the profile evidence. */
	uint64_t warmUpTicks = 0u;
	/** Frame concurrency observed during capture. */
	uint32_t framesInFlight = 0u;
	/** Whether the profile producer considers the captured evidence complete. */
	bool complete = false;
};

/**
 * Startup storage capacities, explicit container reserves, growth policy, and profiling provenance.
 * storage provides typed byte-capacity hints for storage pools and buffers. settings addresses
 * individual registered targets by stable ID, including storage byte capacities and manager
 * container capacities in each target's registered units (bytes or entries).
 * Nonzero storage hints are converted to settings during startup resolution; zero storage hints
 * preserve library defaults. Explicit settings override storage hints for the same target, and a
 * zero setting selects that target's default. Enabled baked capacities take precedence over both.
 */
struct MemoryCapacityProfile {
	/** Typed startup byte-capacity hints for storage pools and buffers; zero preserves defaults. */
	StorageMemoryCapacities storage{};
	/**
	 * Owned per-target startup hints in registered units; override matching storage hints.
	 * Zero selects the target default. Unknown targets and conflicting duplicates reject app
	 * creation; identical duplicates are accepted.
	 */
	std::vector<MemoryCapacitySetting> settings{};
	/** Storage growth multiplier; must be finite and greater than one. */
	float growthFactor = 1.5f;
	/** Permit storage growth subject to error policies and budgets; does not cap manager containers. */
	bool allowRuntimeGrowth = true;
	/** Optional evidence provenance retained with the profile. */
	MemoryCapacityProfileMetadata metadata{};
};

} // namespace FlowUi
