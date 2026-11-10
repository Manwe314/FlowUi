#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"
#include <cstdint>

namespace FlowUi::devSystems {
/** Evidence strength from the retained operation stream, independent of error presentation. */
enum class MemoryCorrelationMatch : uint8_t {
	/** No retained evidence was captured for the query. */
	NotCaptured,
	/** A retained operation explicitly matches the queried identity. */
	Explicit,
	/** A retained operation shares the queried frame/submission context. */
	SharedContext,
	/** Multiple plausible operations prevent a unique match. */
	Ambiguous,
	/** Only nearby timestamp evidence is available. */
	TemporalNearby,
	/** Relevant evidence has already left bounded retained history. */
	Evicted
};
/** Owned scalar evidence; contains no references into mutable source tables or rings. */
struct MemoryCorrelationResult {
	uint64_t event_sequence = 0, /**< Sequence of the matched retained event, or zero when absent. */
		source = 0, /**< Stable memory source of the matched evidence. */
		checkpoint_tick = 0; /**< Tick of the correlated storage checkpoint. */
	uint64_t storage_revision = 0, /**< Storage mutation revision used for correlation. */
		history_generation = 0, /**< Retained-history generation used by the lookup. */
		consumed_tick = 0, /**< Latest consumed application tick visible to the lookup. */
		revision = 0; /**< Coherent live-correlation revision. */
	/** Matched operation kind, meaningful when match identifies retained evidence. */
	MemoryOperation operation = MemoryOperation::LogicalAllocate;
	/** Strength or unavailability of the evidence match. */
	MemoryCorrelationMatch match = MemoryCorrelationMatch::NotCaptured;
	bool event_capture_enabled = false, /**< Whether operation detail capture was enabled. */
		incomplete = false; /**< Whether evidence limitations prevent complete attribution. */
};
/** One batched lookup. Results are overwritten coherently under the memory service lock. */
struct MemoryCorrelationQuery {
	/** Owned output replaced when the batch is evaluated. */
	MemoryCorrelationResult result{};
	/** Optional window/frame scope constraining the lookup. */
	WindowFrameKey frame{};
	uint64_t subject = 0, /**< Numeric resource/owner identity being correlated. */
		app_tick = 0, /**< Application tick associated with the query. */
		submission = 0, /**< Optional GPU submission identity. */
		timestamp_ns = 0; /**< Monotonic occurrence timestamp in nanoseconds. */
};
/** Actual candidates visited; no full payload snapshots are made by a correlation batch. */
struct MemoryCorrelationWork {
	uint64_t event_candidates = 0, /**< Actual retained operation candidates examined. */
		source_checks = 0; /**< Actual source-index candidates examined. */
};
/** Progress of a bounded append into a caller's explicitly pinned event collection. */
struct MemoryEventRead {
	uint64_t generation = 0, /**< History generation represented by destination/cursor. */
		cursor = 0, /**< Last appended retained event sequence. */
		consumed_tick = 0; /**< Latest consumed application tick visible to the lookup. */
	bool evicted = false, /**< Whether required older events were evicted. */
		truncated = false, /**< Whether the caller's maximum event count stopped the read. */
		reset = false; /**< Whether history generation changed and the destination was reset. */
};
} // namespace FlowUi::devSystems
#endif
