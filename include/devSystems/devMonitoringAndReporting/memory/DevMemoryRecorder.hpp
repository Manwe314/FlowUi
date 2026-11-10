#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <vector>

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"

namespace FlowUi::devSystems {

/** Bounded, allocation-free-on-append memory operation producer. */
class DevMemoryRecorder {
public:
	/** Allocate the bounded producer buffer once; subsequent tryRecord() calls do not reallocate. */
	explicit DevMemoryRecorder(uint32_t capacity = 8192u);
	/** Destroy the producer buffer and its retained records. */
	~DevMemoryRecorder();

	DevMemoryRecorder(const DevMemoryRecorder&) = delete;
	DevMemoryRecorder& operator=(const DevMemoryRecorder&) = delete;

	/**
	 * Record one operation if its detail level is enabled and buffer capacity is available; false
	 * records suppression/drop quality rather than throwing.
	 */
	[[nodiscard]] bool tryRecord(const MemoryOperationRecord& record) noexcept;
	/** Set the tick used to enrich operations without explicit tick attribution. */
	void setAppTickContext(AppTickId appTick) noexcept;
	/** Set the runtime threshold used to suppress operation detail. */
	void setLevel(MemoryMonitoringLevel level) noexcept;
	/** Increment the suppressed-operation counter without storing an event. */
	void noteSuppressed() noexcept;
	/**
	 * Append retained records in producer order to caller-owned destination, then empty the buffer.
	 * May allocate in destination.
	 */
	void drainInto(std::vector<MemoryOperationRecord>& destination);
	/** Copy cumulative recorded, suppressed, and dropped counters. */
	[[nodiscard]] MemoryQualitySnapshot qualitySnapshot() const noexcept;
	/** Return the fixed number of producer event slots. */
	[[nodiscard]] uint32_t capacity() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};

} // namespace FlowUi::devSystems

#if FLOWUI_DEV_MEMORY_LEVEL >= 1
/**
 * Record memory detail when instrumentation level 1 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_STORAGE(recorder, record) \
	((void)(recorder).tryRecord((record)))
#else
/**
 * Record memory detail when instrumentation level 1 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_STORAGE(recorder, record) ((void)0)
#endif

#if FLOWUI_DEV_MEMORY_LEVEL >= 2
/**
 * Record memory detail when instrumentation level 2 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_SUBSYSTEM(recorder, record) \
	((void)(recorder).tryRecord((record)))
#else
/**
 * Record memory detail when instrumentation level 2 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_SUBSYSTEM(recorder, record) ((void)0)
#endif

#if FLOWUI_DEV_MEMORY_LEVEL >= 3
/**
 * Record memory detail when instrumentation level 3 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_LIFETIME(recorder, record) \
	((void)(recorder).tryRecord((record)))
#else
/**
 * Record memory detail when instrumentation level 3 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_LIFETIME(recorder, record) ((void)0)
#endif

#if FLOWUI_DEV_MEMORY_LEVEL >= 4
/**
 * Record memory detail when instrumentation level 4 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_DEEP(recorder, record) \
	((void)(recorder).tryRecord((record)))
#else
/**
 * Record memory detail when instrumentation level 4 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_DEEP(recorder, record) ((void)0)
#endif

#else

/**
 * Record memory detail when instrumentation level 1 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_STORAGE(recorder, record) ((void)0)
/**
 * Record memory detail when instrumentation level 2 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_SUBSYSTEM(recorder, record) ((void)0)
/**
 * Record memory detail when instrumentation level 3 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_LIFETIME(recorder, record) ((void)0)
/**
 * Record memory detail when instrumentation level 4 is compiled in; disabled forms do not evaluate
 * arguments.
 */
#define FLOWUI_DEV_MEMORY_DEEP(recorder, record) ((void)0)

#endif
