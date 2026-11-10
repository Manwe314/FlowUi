#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2

#include <chrono>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryRecorder.hpp"

namespace FlowUi::devSystems {

/** Records a known third-party or temporary buffer without owning its lifetime. */
class DevExternalMemoryScope {
public:
	/**
	 * Record acquisition of bytes from source when recorder is present; recorder is borrowed and must
	 * outlive the scope.
	 */
	DevExternalMemoryScope(
		DevMemoryRecorder* recorder,
		MemorySourceId source,
		uint64_t bytes,
		MemoryLifetimeId lifetime = 0u) noexcept;
	/** Record release of the tracked external bytes without freeing their actual storage. */
	~DevExternalMemoryScope() noexcept;

	DevExternalMemoryScope(const DevExternalMemoryScope&) = delete;
	DevExternalMemoryScope& operator=(const DevExternalMemoryScope&) = delete;
	/** Transfer release-recording responsibility; the moved-from scope becomes inactive. */
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
