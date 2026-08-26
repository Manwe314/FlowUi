#include "devSystems/devTooling/DevTooling.hpp"

#if FLOW_UI_DEV_MODE

#if FLOWUI_DEV_MEMORY_LEVEL >= 2
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif
#include "managers/ThemeManager.hpp"

namespace FlowUi::devSystems {

DevTooling::DevTooling() noexcept : overrides_(schemas_) {}
DevTooling::~DevTooling() = default;

void DevTooling::commitAtSafePoint(
	ThemeManager& themes,
	DevTimingRecorder* timing) noexcept {
	overrides_.commitAtSafePoint(themes, timing);
}

#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void DevTooling::appendDevMemorySamples(MemorySampleSink& sink) const noexcept {
	try {
		DevContainerMemoryAccumulator memory{};
		const std::size_t bytes = overrides_.memoryFootprintBytes();
		memory.liveBytes = bytes;
		memory.capacityBytes = bytes;
		memory.objectCount = overrides_.stats().activeElementOverrides +
			overrides_.stats().activeThemeOverrides;
		memory.capacityCount = memory.objectCount;
		appendManagerSample(sink, memory_sources::kDevTooling.id, memory);
	} catch (...) {}
}
#endif

} // namespace FlowUi::devSystems

#endif
