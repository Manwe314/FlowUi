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
 * Aggregate manager demand recorded by profiling tools. These summaries do not reserve individual
 * runtime containers; use MemoryCapacityProfile::settings for registered startup targets. Values are
 * target-specific counts or byte sizes.
 */
struct ManagerMemoryCapacities {
	uint64_t elements = 0u, /**< Element manager aggregate demand. */
		inputFields = 0u, /**< Input-field registry aggregate demand. */
		inputTextBytes = 0u, /**< Input text payload bytes. */
		fonts = 0u, /**< Font manager aggregate demand. */
		fontAtlasCpuPixelBytes = 0u, /**< Retained CPU font atlas pixels in bytes. */
		icons = 0u, /**< Icon manager aggregate demand. */
		iconDocuments = 0u, /**< Retained SVG document aggregate demand. */
		iconAtlasMetadata = 0u, /**< Icon atlas metadata aggregate demand. */
		viewports = 0u, /**< Viewport manager aggregate demand. */
		popups = 0u, /**< Popup manager aggregate demand. */
		shortcuts = 0u, /**< Shortcut manager aggregate demand. */
		actions = 0u, /**< Action manager aggregate demand. */
		themes = 0u, /**< Theme manager aggregate demand. */
		uiLayout = 0u, /**< UI layout aggregate demand. */
		renderer = 0u; /**< Renderer aggregate demand. */
};

/**
 * Startup storage capacities, explicit container reserves, growth policy, and profiling provenance.
 * Zero storage hints preserve library defaults. Explicit settings override typed storage hints;
 * enabled baked capacities take precedence over both.
 */
struct MemoryCapacityProfile {
	/** Typed startup storage hints in bytes. */
	StorageMemoryCapacities storage{};
	/** Aggregate profiling summaries; use settings to tune individual runtime containers. */
	ManagerMemoryCapacities managers{};
	/** Owned target hints; identical duplicates are accepted, conflicting duplicates are rejected. */
	std::vector<MemoryCapacitySetting> settings{};
	/** Storage growth multiplier; must be finite and greater than one. */
	float growthFactor = 1.5f;
	/** Permit storage growth subject to the configured capacity error policies. */
	bool allowRuntimeGrowth = true;
	/** Optional evidence provenance retained with the profile. */
	MemoryCapacityProfileMetadata metadata{};
};

} // namespace FlowUi
