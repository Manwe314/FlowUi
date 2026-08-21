#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2

#include <chrono>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryRecorder.hpp"

namespace FlowUi::devSystems {

/** Records a known third-party or temporary buffer without owning its lifetime. */
class DevExternalMemoryScope {
public:
	DevExternalMemoryScope(
		DevMemoryRecorder* recorder,
		MemorySourceId source,
		uint64_t bytes,
		MemoryLifetimeId lifetime = 0u) noexcept;
	~DevExternalMemoryScope() noexcept;

	DevExternalMemoryScope(const DevExternalMemoryScope&) = delete;
	DevExternalMemoryScope& operator=(const DevExternalMemoryScope&) = delete;
	DevExternalMemoryScope(DevExternalMemoryScope&& other) noexcept;
	DevExternalMemoryScope& operator=(DevExternalMemoryScope&&) = delete;

private:
	DevMemoryRecorder* recorder_ = nullptr;
	MemorySourceId source_ = 0u;
	MemoryLifetimeId lifetime_ = 0u;
	uint64_t bytes_ = 0u;
};

} // namespace FlowUi::devSystems

#endif
