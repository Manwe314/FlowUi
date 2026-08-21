#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <vector>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"

namespace FlowUi::devSystems {

/** Bounded, allocation-free-on-append memory operation producer. */
class DevMemoryRecorder {
public:
	explicit DevMemoryRecorder(uint32_t capacity = 8192u);
	~DevMemoryRecorder();

	DevMemoryRecorder(const DevMemoryRecorder&) = delete;
	DevMemoryRecorder& operator=(const DevMemoryRecorder&) = delete;

	[[nodiscard]] bool tryRecord(const MemoryOperationRecord& record) noexcept;
	void setAppTickContext(AppTickId appTick) noexcept;
	void setLevel(MemoryMonitoringLevel level) noexcept;
	void noteSuppressed() noexcept;
	void drainInto(std::vector<MemoryOperationRecord>& destination);
	[[nodiscard]] MemoryQualitySnapshot qualitySnapshot() const noexcept;
	[[nodiscard]] uint32_t capacity() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};

} // namespace FlowUi::devSystems

#if FLOWUI_DEV_MEMORY_LEVEL >= 1
#define FLOWUI_DEV_MEMORY_STORAGE(recorder, record) \
	((void)(recorder).tryRecord((record)))
#else
#define FLOWUI_DEV_MEMORY_STORAGE(recorder, record) ((void)0)
#endif

#if FLOWUI_DEV_MEMORY_LEVEL >= 2
#define FLOWUI_DEV_MEMORY_SUBSYSTEM(recorder, record) \
	((void)(recorder).tryRecord((record)))
#else
#define FLOWUI_DEV_MEMORY_SUBSYSTEM(recorder, record) ((void)0)
#endif

#if FLOWUI_DEV_MEMORY_LEVEL >= 3
#define FLOWUI_DEV_MEMORY_LIFETIME(recorder, record) \
	((void)(recorder).tryRecord((record)))
#else
#define FLOWUI_DEV_MEMORY_LIFETIME(recorder, record) ((void)0)
#endif

#if FLOWUI_DEV_MEMORY_LEVEL >= 4
#define FLOWUI_DEV_MEMORY_DEEP(recorder, record) \
	((void)(recorder).tryRecord((record)))
#else
#define FLOWUI_DEV_MEMORY_DEEP(recorder, record) ((void)0)
#endif

#else

#define FLOWUI_DEV_MEMORY_STORAGE(recorder, record) ((void)0)
#define FLOWUI_DEV_MEMORY_SUBSYSTEM(recorder, record) ((void)0)
#define FLOWUI_DEV_MEMORY_LIFETIME(recorder, record) ((void)0)
#define FLOWUI_DEV_MEMORY_DEEP(recorder, record) ((void)0)

#endif
