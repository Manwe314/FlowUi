#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"
#include <cstdint>

namespace FlowUi::devSystems {
/** Evidence strength from the retained operation stream, independent of error presentation. */
enum class MemoryCorrelationMatch : uint8_t {
	NotCaptured,
	Explicit,
	SharedContext,
	Ambiguous,
	TemporalNearby,
	Evicted
};
/** Owned scalar evidence; contains no references into mutable source tables or rings. */
struct MemoryCorrelationResult {
	uint64_t event_sequence = 0, source = 0, checkpoint_tick = 0;
	uint64_t storage_revision = 0, history_generation = 0, consumed_tick = 0, revision = 0;
	MemoryOperation operation = MemoryOperation::LogicalAllocate;
	MemoryCorrelationMatch match = MemoryCorrelationMatch::NotCaptured;
	bool event_capture_enabled = false, incomplete = false;
};
/** One batched lookup. Results are overwritten coherently under the memory service lock. */
struct MemoryCorrelationQuery {
	MemoryCorrelationResult result{};
	WindowFrameKey frame{};
	uint64_t subject = 0, app_tick = 0, submission = 0, timestamp_ns = 0;
};
/** Actual candidates visited; no full payload snapshots are made by a correlation batch. */
struct MemoryCorrelationWork {
	uint64_t event_candidates = 0, source_checks = 0;
};
/** Progress of a bounded append into a caller's explicitly pinned event collection. */
struct MemoryEventRead {
	uint64_t generation = 0, cursor = 0, consumed_tick = 0;
	bool evicted = false, truncated = false, reset = false;
};
} // namespace FlowUi::devSystems
#endif
