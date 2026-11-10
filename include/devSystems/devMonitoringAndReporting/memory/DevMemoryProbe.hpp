#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"

namespace FlowUi::devSystems {

/**
 * Synchronous bounded destination for memory probe samples. A false append result means the sample
 * could not be retained.
 */
class MemorySampleSink {
public:
	/** Destroy a polymorphic sink. */
	virtual ~MemorySampleSink() = default;
	/** Copy one sample into the destination; return false if unavailable or full. */
	virtual bool append(const MemoryValueSample& sample) noexcept = 0;
};

/** Borrowed sample destination and temporal/detail context for one synchronous probe invocation. */
struct MemoryProbeContext {
	/** Current application tick. */
	AppTickId appTick = 0u;
	/** Monotonic sample timestamp in nanoseconds. */
	uint64_t nowNs = 0u;
	/** Runtime detail level requested by the reporting consumer. */
	MemoryMonitoringLevel detail = MemoryMonitoringLevel::Disabled;
	/** Borrowed output destination valid during the probe call. */
	MemorySampleSink& sink;
};

/**
 * Nonthrowing source sampler; the first argument is the registered borrowed owner. Append samples
 * through context.sink without retaining context.
 */
using MemoryProbeFunction = void (*)(const void*, MemoryProbeContext&) noexcept;

/**
 * Borrowed probe registration. Unregister before destroying owner; source must identify the
 * corresponding registered descriptor.
 */
struct RegisteredMemoryProbe {
	/** Registered source identity. */
	MemorySourceId source = 0u;
	/** Borrowed source object passed to sample. */
	const void* owner = nullptr;
	/** Nonthrowing synchronous sampler; null is not a valid registration. */
	MemoryProbeFunction sample = nullptr;
};

} // namespace FlowUi::devSystems

#endif
