#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <optional>
#include <vector>

#include "devSystems/devMonitoringAndReporting/errors/DevErrorTypes.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorAdvice.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"

namespace FlowUi::devSystems {

class DevErrorMonitoring;
struct DevDiagnosticsSnapshot;
struct DevDiagnosticsInvestigation;

/** Stable identity of an error-triggered retained evidence window. */
using DevErrorCaptureId = uint64_t;

/** Availability of evidence requested for a diagnostic correlation. */
enum class DevErrorCorrelationState : uint8_t {
	/** Requested evidence is present. */
	Available = 0u,
	/** Requested evidence fell outside retained history. */
	Evicted,
	/** Requested evidence was not admitted by capture policy. */
	NotCaptured,
	/** Multiple candidate records prevent unique attribution. */
	Ambiguous,
};

/** Basis for associating diagnostic and timing or memory evidence. */
enum class DevErrorCorrelationConfidence : uint8_t {
	/** Association follows an explicit captured identity. */
	Explicit = 0u,
	/** Association follows shared tick, frame, track or entity context. */
	SharedContext,
	/** Association follows temporal proximity only. */
	TemporalNearby,
	/** No evidence supports an association. */
	None,
};

/** Compact attribution of an occurrence to retained timing evidence and its generation. */
struct DevErrorTimingCorrelation {
	/** Semantic entity attributed to this timing zone or correlation. */
	TimingEntityRef entity{};
	/** Retention, correlation or collection state of this record. */
	DevErrorCorrelationState state = DevErrorCorrelationState::NotCaptured;
	/** Strength or basis of the available supporting evidence. */
	DevErrorCorrelationConfidence confidence = DevErrorCorrelationConfidence::None;
	/** Application tick associated with this evidence; zero means unassigned. */
	AppTickId appTick = 0u;
	/** Window and frame sequence associated with this evidence. */
	WindowFrameKey frame{};
	/** Identity of the retained CPU span containing the diagnostic timestamp. */
	TimingInvocationId containingInvocation = 0u;
	/** Descriptor identity of the containing timing span. */
	TimingZoneTypeId containingZone = 0u;
	/** Revision of the timing report used for correlation. */
	uint64_t reportRevision = 0u;
	/** Timing capture generation associated with this evidence. */
	uint64_t capture_generation = 0;
	/** Sealed evidence is not retried; late records still attach to the occurrence. */
	/** @var DevErrorTimingCorrelation::sealed
	 * Whether this capture or correlation is finalized against further evidence admission.
	 */
	/** @var DevErrorTimingCorrelation::incomplete
	 * Whether some expected measurement or evidence failed to arrive.
	 */
	bool sealed = false, incomplete = false;
};

/** Compact attribution of an occurrence to retained memory evidence and its history generation. */
struct DevErrorMemoryCorrelation {
	/** Retained memory operation sequence associated with the occurrence. */
	MemoryEventSequence eventSequence = 0u;
	/** Snapshot source or evidence identity associated with this record. */
	MemorySourceId source = 0u;
	/** Application tick of the selected memory checkpoint. */
	AppTickId checkpointTick = 0u;
	/** Storage mutation sequence associated with the selected memory evidence. */
	uint64_t storageMutationSequence = 0u;
	/** Generation of the memory history used for correlation. */
	uint64_t history_generation = 0;
	/** Retention, correlation or collection state of this record. */
	DevErrorCorrelationState state = DevErrorCorrelationState::NotCaptured;
	/** Strength or basis of the available supporting evidence. */
	DevErrorCorrelationConfidence confidence = DevErrorCorrelationConfidence::None;
	/** Kind of correlated memory operation. */
	MemoryOperation operation = MemoryOperation::LogicalAllocate;
	/** Correlation closes after the configured evidence window, independently of retention. */
	/** @var DevErrorMemoryCorrelation::sealed
	 * Whether this capture or correlation is finalized against further evidence admission.
	 */
	/** @var DevErrorMemoryCorrelation::incomplete
	 * Whether some expected measurement or evidence failed to arrive.
	 */
	bool sealed = false, incomplete = false;
};

/** Lifecycle of an occurrence while causal and post-event evidence is collected. */
enum class DevErrorOccurrenceState : uint8_t {
	/** Occurrence is still collecting causal evidence. */
	Open = 0u,
	/** Occurrence is collecting its configured post-event evidence window. */
	PostWindow,
	/** Occurrence evidence window has closed. */
	Closed,
};

/**
 * Owned retained diagnostic occurrence with causal steps, breadcrumbs, snapshots and evaluated
 * advice.
 */
struct DevErrorOccurrence {
	/** Stable identity of this retained entity; zero means unassigned. */
	DevErrorOccurrenceId id = 0u;
	/** Production error contract and compact payload captured for this occurrence. */
	FlowUiError error{};
	/** First application tick included in the occurrence or query range. */
	AppTickId firstAppTick = 0u;
	/** Last application tick included in the occurrence or query range. */
	AppTickId lastAppTick = 0u;
	/** First observed occurrence timestamp in nanoseconds. */
	uint64_t firstTimestampNs = 0u;
	/** Last observed occurrence timestamp in nanoseconds. */
	uint64_t lastTimestampNs = 0u;
	/** Production error event kind represented by this diagnostic step. */
	ErrorEventKind productionKind = ErrorEventKind::Reported;
	/** Resolution applied or reported for the error contract. */
	ErrorResolution resolution = ErrorResolution::None;
	/** Bitmask of completion, provenance and capture-quality conditions. */
	DevErrorRecordFlag flags = DevErrorRecordFlag::None;
	/** Retention, correlation or collection state of this record. */
	DevErrorOccurrenceState state = DevErrorOccurrenceState::Open;
	/** Post-event evidence deadline expressed as an application tick. */
	AppTickId closesAfterAppTick = 0u;
	/** Identity of the error-triggered capture associated with this occurrence. */
	DevErrorCaptureId captureId = 0u;
	/** Correlated timing evidence and its availability. */
	DevErrorTimingCorrelation timing{};
	/** Correlated memory evidence and its availability. */
	DevErrorMemoryCorrelation memory{};
	/** Owned retained causal steps or their compact searchable references. */
	std::vector<DevErrorRecord> steps{};
	/** Owned bounded breadcrumb evidence; only breadcrumbCount entries are valid for fatal capsules. */
	std::vector<DevErrorBreadcrumb> breadcrumbs{};
	/** Owned retained deferred subsystem snapshots. */
	std::vector<DevErrorSnapshot> snapshots{};
	/** Evaluated advice with references to retained supporting evidence. */
	std::vector<DevErrorAdviceResult> advice{};
};

/** Compact chronological presentation row. Action text refers to the static advice catalogue. */
struct DevErrorOccurrenceSummary {
	/** Correlated timing evidence and its availability. */
	DevErrorTimingCorrelation timing{};
	/** Production error contract and compact payload captured for this occurrence. */
	FlowUiError error{};
	/** Borrowed suggested action text from the static advice catalogue. */
	std::string_view suggested_action{};
	/** @var DevErrorOccurrenceSummary::id
	 * Stable identity of this retained entity; zero means unassigned.
	 */
	/** @var DevErrorOccurrenceSummary::last_app_tick
	 * Last application tick associated with the occurrence.
	 */
	uint64_t id = 0, last_app_tick = 0;
};

/** Whether an error-triggered evidence window is still collecting or complete. */
enum class DevErrorTriggeredCaptureState : uint8_t {
	/** Triggered evidence window is still collecting. */
	Collecting = 0u,
	/** Triggered evidence window is finalized. */
	Complete,
};

/** Bounded timing and memory evidence window opened by one diagnostic occurrence. */
struct DevErrorTriggeredCapture {
	/** Stable identity of this retained entity; zero means unassigned. */
	DevErrorCaptureId id = 0u;
	/** Occurrence whose error opened the capture window. */
	DevErrorOccurrenceId triggerOccurrence = 0u;
	/** First application tick included in the occurrence or query range. */
	AppTickId firstAppTick = 0u;
	/** Last application tick included in the occurrence or query range. */
	AppTickId lastAppTick = 0u;
	/** Retention, correlation or collection state of this record. */
	DevErrorTriggeredCaptureState state = DevErrorTriggeredCaptureState::Collecting;
	/** Whether timing evidence was evicted before it could be retained. */
	bool timingHistoryEvicted = false;
	/** Whether memory evidence was evicted before it could be retained. */
	bool memoryHistoryEvicted = false;
	/** Whether the timing evidence window was truncated. */
	bool timingTruncated = false;
	/** Whether the memory evidence window was truncated. */
	bool memoryTruncated = false;
	/** @var DevErrorTriggeredCapture::timing_generation
	 * Timing capture generation pinned by the triggered capture.
	 */
	/** @var DevErrorTriggeredCapture::timing_retained_ticks
	 * Number of timing ticks retained for the triggered capture.
	 */
	uint64_t timing_generation = 0, timing_retained_ticks = 0;
	/** Append cursor and generation for explicitly pinned operation evidence. */
	/** @var DevErrorTriggeredCapture::memory_generation
	 * Memory history generation pinned by the capture; UINT64_MAX is unassigned.
	 */
	/** @var DevErrorTriggeredCapture::memory_cursor
	 * Append cursor for pinned memory operation evidence.
	 */
	uint64_t memory_generation = UINT64_MAX, memory_cursor = 0;
	/** Whether memory evidence collection for the capture has closed. */
	bool memory_sealed = false;
	/** Availability of timing evidence for the triggered capture. */
	DevErrorCorrelationState timing_state = DevErrorCorrelationState::NotCaptured;
	/** Owned retained memory operations attached to the capture. */
	std::vector<RetainedMemoryEvent> memoryEvents{};
};

/** Fields used to group retained diagnostic occurrences. */
enum class DevErrorGroupMode : uint8_t {
	/** Group by error code and site. */
	Contract = 0u,
	/** Include the native backend error value in grouping. */
	Native,
	/** Include the registered diagnostic source in grouping. */
	Source,
};

/** Error contract, native code and source fields used by the selected grouping mode. */
struct DevErrorGroupKey {
	/** Production error code matched by this rule, query or group. */
	ErrorCode code = ErrorCode::None;
	/** Production error site matched by this rule, query or group. */
	ErrorSite site = ErrorSite::None;
	/** Native backend error value used by native grouping. */
	uint32_t nativeCode = 0u;
	/** Registered diagnostic source identity; zero means unavailable. */
	uint64_t sourceId = 0u;

	/** Compare identity fields by value. */
	[[nodiscard]] friend bool operator==(
		const DevErrorGroupKey&,
		const DevErrorGroupKey&) noexcept = default;
};

/** Aggregate counts and range for one retained diagnostic group. */
struct DevErrorGroupSummary {
	/** Contract, native code and source fields selected by the grouping mode. */
	DevErrorGroupKey key{};
	/** Number of retained occurrences in the group. */
	uint64_t occurrenceCount = 0u;
	/** First application tick included in the occurrence or query range. */
	AppTickId firstAppTick = 0u;
	/** Last application tick included in the occurrence or query range. */
	AppTickId lastAppTick = 0u;
	/** Occurrence identity chosen to represent the group. */
	DevErrorOccurrenceId representative = 0u;
	/** Most recently observed error resolution in the group. */
	ErrorResolution lastResolution = ErrorResolution::None;
};

/**
 * Filters for retained occurrences. Empty optional fields impose no filter; zero tick limits and
 * maximumResults mean unrestricted.
 */
struct DevErrorOccurrenceQuery {
	/** Production error code matched by this rule, query or group. */
	std::optional<ErrorCode> code{};
	/** Production error site matched by this rule, query or group. */
	std::optional<ErrorSite> site{};
	/** Window identity; InvalidWindowId means no window attribution. */
	std::optional<WindowId> window{};
	/** First application tick included in the occurrence or query range. */
	AppTickId firstAppTick = 0u;
	/** Last application tick included in the occurrence or query range. */
	AppTickId lastAppTick = 0u;
	/** Maximum returned occurrences; zero applies no result limit. */
	uint32_t maximumResults = 0u;
};

/** Retention, ingestion, correlation and instrumentation state of the diagnostic consumer. */
struct DevErrorReportingStatus {
	/** Cumulative diagnostic reporting operation counts and nanosecond costs. */
	struct Overhead {
		/** Number of consume calls. */
		uint64_t consumeCalls = 0u;
		/** Cumulative consume time in nanoseconds. */
		uint64_t consumeTimeNs = 0u;
		/** Cumulative drain time in nanoseconds. */
		uint64_t drainTimeNs = 0u;
		/** Cumulative correlation time in nanoseconds. */
		uint64_t correlationTimeNs = 0u;
		/** Cumulative advice time in nanoseconds. */
		uint64_t adviceTimeNs = 0u;
		/** Number of grouping calls. */
		uint64_t groupingCalls = 0u;
		/** Cumulative grouping time in nanoseconds. */
		uint64_t groupingTimeNs = 0u;
		/** Cumulative drained volume in bytes. */
		uint64_t drainedBytes = 0u;
		/** @var Overhead::memoryCorrelationBatches
		 * Number of memory correlation batches.
		 */
		/** @var Overhead::memoryCorrelationQueries
		 * Number of memory correlation queries.
		 */
		uint64_t memoryCorrelationBatches = 0, memoryCorrelationQueries = 0;
		/** @var Overhead::memoryEventCandidates
		 * Number of memory event candidates.
		 */
		/** @var Overhead::memorySourceChecks
		 * Number of memory source checks.
		 */
		uint64_t memoryEventCandidates = 0, memorySourceChecks = 0;
		/** @var Overhead::timingCorrelationQueries
		 * Number of timing correlation queries.
		 */
		/** @var Overhead::adviceUpdates
		 * Number of advice updates.
		 */
		/** @var Overhead::sealedCorrelations
		 * Number of sealed correlations.
		 */
		uint64_t timingCorrelationQueries = 0, adviceUpdates = 0, sealedCorrelations = 0;
	} overhead{}; /**< Cumulative diagnostic reporting operation counts and nanosecond costs. */
	/** Number of consumed records. */
	uint64_t consumedRecords = 0u;
	/** Number of consumed breadcrumbs. */
	uint64_t consumedBreadcrumbs = 0u;
	/** Number of consumed snapshots. */
	uint64_t consumedSnapshots = 0u;
	/** Number of evicted occurrences. */
	uint64_t evictedOccurrences = 0u;
	/** Number of consume failures. */
	uint64_t consumeFailures = 0u;
	/** Number of truncated steps. */
	uint64_t truncatedSteps = 0u;
	/** Number of truncated breadcrumb ranges. */
	uint64_t truncatedBreadcrumbRanges = 0u;
	/** Number of triggered captures. */
	uint64_t triggeredCaptures = 0u;
	/** Number of evicted captures. */
	uint64_t evictedCaptures = 0u;
	/** Number of timing correlation evictions. */
	uint64_t timingCorrelationEvictions = 0u;
	/** Number of memory correlation evictions. */
	uint64_t memoryCorrelationEvictions = 0u;
	/** Latest application tick admitted by the reporting consumer. */
	AppTickId lastConsumedAppTick = 0u;
	/** Capture quality counters associated with this reporting state. */
	DevErrorQualitySnapshot quality{};
	/** Number of diagnostic occurrences currently retained. */
	uint32_t retainedOccurrences = 0u;
	/** Number of occurrences still collecting evidence. */
	uint32_t pendingOccurrences = 0;
	/** Estimated bytes retained by reporting storage. */
	uint64_t retainedBytes = 0u;
	/** Changes for ingestion, late evidence, reporting configuration and retention updates. */
	uint64_t revision = 0u;
};

/**
 * Consumer and bounded retained history of monitored diagnostic evidence. Borrowed monitoring,
 * timing and memory services must outlive it.
 */
class DevErrorReporting {
public:
	/**
	 * Consumer and bounded retained history of monitored diagnostic evidence. Borrowed monitoring,
	 * timing and memory services must outlive it.
	 */
	explicit DevErrorReporting(
		DevErrorMonitoring& monitoring,
		DevErrorReportingConfig config = {},
		DevTimingReporting* timing = nullptr,
		DevMemoryReporting* memory = nullptr);
	/** Release owned monitoring state after dependent recorders, attachments and scopes are finished. */
	~DevErrorReporting();
	/** Copying is disabled to preserve ownership and producer identity. */
	DevErrorReporting(const DevErrorReporting&) = delete;
	/** Copying is disabled to preserve ownership and producer identity. */
	DevErrorReporting& operator=(const DevErrorReporting&) = delete;

	/**
	 * Apply monitoring or reporting configuration; retained storage and pending work follow the
	 * owning service policy.
	 */
	void setConfig(const DevErrorReportingConfig& config);
	/** Return a copy of the current effective configuration. */
	[[nodiscard]] DevErrorReportingConfig config() const;
	/**
	 * Drain newly produced evidence and update retained reporting state through the supplied
	 * application tick at a consumer safe point.
	 */
	void consumeThrough(AppTickId appTick) noexcept;
	/**
	 * Return the current scalar progress, retention and quality state; borrowed reference overloads
	 * remain valid until the controller changes.
	 */
	[[nodiscard]] DevErrorReportingStatus status() const noexcept;
	/** Read all retained start rows and source contexts under one reporting lock. */
	[[nodiscard]] Result<DevDiagnosticsSnapshot> diagnostics_snapshot() const noexcept;
	/** Materialize all retained investigations, or one exact ID, within an owned byte budget. */
	[[nodiscard]] Result<DevDiagnosticsInvestigation>
	/**
	 * Materialize an owned bounded investigation for all occurrences or one exact selected identity.
	 * Return a Result error when the identity is unavailable or the byte budget is exceeded.
	 */
	diagnostics_investigation(std::optional<DevErrorOccurrenceId> selected_id = std::nullopt,
							  uint64_t byte_budget = 32u * 1024u * 1024u) const noexcept;
	/** Read scalar issue rows without copying steps, breadcrumbs, snapshots, or advice buffers. */
	[[nodiscard]] std::vector<DevErrorOccurrenceSummary> occurrence_summaries() const;
	/** Copy all currently retained diagnostic occurrences and their bounded evidence. */
	[[nodiscard]] std::vector<DevErrorOccurrence> occurrenceSnapshot() const;
	/** Copy one retained diagnostic occurrence, or return empty when its identity is unavailable. */
	[[nodiscard]] std::optional<DevErrorOccurrence> occurrence(
		DevErrorOccurrenceId id) const;
	/** Copy retained occurrences matching the optional contract, window, tick range and result limit. */
	[[nodiscard]] std::vector<DevErrorOccurrence> occurrenceRange(
		const DevErrorOccurrenceQuery& query) const;
	/** Aggregate retained occurrences by the requested contract, native-code or source grouping. */
	[[nodiscard]] std::vector<DevErrorGroupSummary> groupSnapshot(
		DevErrorGroupMode mode = DevErrorGroupMode::Contract) const;
	/** Copy the retained raw stack with the requested identity, or return empty when unavailable. */
	[[nodiscard]] std::optional<DevErrorStackTrace> stack(DevErrorStackId id) const;
	/** Copy retained raw stack traces into owned storage. */
	[[nodiscard]] std::vector<DevErrorStackTrace> stackSnapshot() const;
	/** Copy all retained error-triggered evidence windows. */
	[[nodiscard]] std::vector<DevErrorTriggeredCapture> captureSnapshot() const;
	/** Copy a retained error-triggered capture, or return empty when its identity is unavailable. */
	[[nodiscard]] std::optional<DevErrorTriggeredCapture> capture(
		DevErrorCaptureId id) const;
	/**
	 * Copy evaluated advice for one retained occurrence; prose views remain backed by the static
	 * catalogue.
	 */
	[[nodiscard]] std::vector<DevErrorAdviceResult> advice(
		DevErrorOccurrenceId id) const;
	/** Borrow immutable static advice catalogue entries for the process lifetime. */
	[[nodiscard]] std::span<const DevErrorAdviceDescriptor> adviceCatalogue() const noexcept;
	/** Copy the most recent retained fatal capsule, or return empty when unavailable. */
	[[nodiscard]] std::optional<DevErrorFatalCapsule> fatalCapsule() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};

} // namespace FlowUi::devSystems

#endif
