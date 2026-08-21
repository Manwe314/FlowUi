#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"

namespace FlowUi::devSystems {

class MemorySampleSink {
public:
	virtual ~MemorySampleSink() = default;
	virtual bool append(const MemoryValueSample& sample) noexcept = 0;
};

struct MemoryProbeContext {
	AppTickId appTick = 0u;
	uint64_t nowNs = 0u;
	MemoryMonitoringLevel detail = MemoryMonitoringLevel::Disabled;
	MemorySampleSink& sink;
};

using MemoryProbeFunction = void (*)(const void*, MemoryProbeContext&) noexcept;

struct RegisteredMemoryProbe {
	MemorySourceId source = 0u;
	const void* owner = nullptr;
	MemoryProbeFunction sample = nullptr;
};

} // namespace FlowUi::devSystems

#endif
