#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryCorrelation.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Private/storageSystem/capacity/MemoryCapacityProfile.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"
#include "FlowUi/Private/storageSystem/StorageTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace FlowUi::devSystems {
class DevMemory;
class DevMemoryReporting;
/** Monotonic retained segment identity. */
using MemorySampleSequence = uint64_t;
/** Monotonic retained operation identity. */
using MemoryEventSequence = uint64_t;
/** Capture registration identity; zero marks an unsuccessful capture request. */
using MemoryCaptureId = uint64_t;

/** Metric selected from a retained memory sample for weighted statistics. */
enum class MemoryStatisticMetric : uint8_t {
	LogicalLiveBytes = 0, /**< Logical live payload bytes. */
	BackingAllocatedBytes, /**< Allocated backing bytes. */
	ReusableBytes, /**< Allocated bytes currently reusable. */
	RetiredBytes, /**< Bytes awaiting safe reclamation. */
	ObjectCount, /**< Live object/entry count. */
	CapacityCount, /**< Reserved entry capacity. */
	PeakLogicalBytes, /**< Peak logical live byte demand. */
};
/** Weight observations by represented application ticks, monotonic elapsed time, or retained epochs. */
enum class MemoryWeighting : uint8_t {
	AppTicks = 0, /**< Weight by represented application ticks. */
	ElapsedTime, /**< Weight by elapsed monotonic nanoseconds. */
	Epochs /**< Weight retained observation epochs equally. */
};
/** Bit flags carrying retention loss, unsupported sampling, overlap, or analysis limitations. */
enum class MemoryReportFlag : uint32_t {
	None = 0u, /**< No report qualifiers. */
	SegmentHistoryOverwritten = 1u << 0u, /**< Older segment history has been overwritten. */
	EventHistoryOverwritten = 1u << 1u, /**< Older operation history has been overwritten. */
	ProducerDetailDropped = 1u << 2u, /**< Bounded producers dropped operation detail. */
	SourceSamplingUnavailable = 1u << 3u, /**< Source measurement could not be collected. */
	ProcessSamplingUnavailable = 1u << 4u, /**< Process metrics were unavailable. */
	GpuSamplingUnavailable = 1u << 5u, /**< GPU metrics were unavailable. */
	SharedPhysicalMemory = 1u << 6u, /**< Some domains share physical backing and must not be added twice. */
	NegativeProcessResidual = 1u << 7u, /**< Attributed CPU bytes exceed the resident-memory sample. */
	CaptureIncomplete = 1u << 8u, /**< Capture evidence is incomplete. */
	GrewDuringCapture = 1u << 9u, /**< A measured backing allocation grew during capture. */
	BackingChurnAfterWarmUp = 1u << 10u, /**< Backing churn continued after the warm-up interval. */
	RetiredExceedsLive = 1u << 11u, /**< Retired-byte pressure exceeds logical live bytes. */
	MonitorOverheadExceeded = 1u << 12u, /**< Instrumentation overhead exceeded configured limits. */
	HistoryPaused = 1u << 13u, /**< A history pause interrupted retained evidence. */
};
/** Combine independent report-quality flag bits. */
[[nodiscard]] constexpr MemoryReportFlag operator|(MemoryReportFlag a, MemoryReportFlag b) noexcept {
	return static_cast<MemoryReportFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
/** Accumulate report-quality flags into a and return a. */
constexpr MemoryReportFlag& operator|=(MemoryReportFlag& a, MemoryReportFlag b) noexcept { return a = a | b; }

/** Source identity plus outer window scope; zero window denotes an app-wide source. */
struct MemorySourceKey {
	MemorySourceId source = 0u; /**< Stable source identity. */
	WindowId window = InvalidWindowId; /**< Owning window, or zero for app-wide measurements. */
	/** Compare both the source identity and outer window scope. */
	friend bool operator==(const MemorySourceKey&, const MemorySourceKey&) = default;
};
/** One second of live, tick-weighted demand independent of retained history. */
struct MemoryLiveBucket {
	long double active_sum = 0; /**< Sum of active demand values in this one-second bucket. */
	uint64_t second = UINT64_MAX, /**< Monotonic second represented by this bucket; UINT64_MAX marks an unused bucket. */
		count = 0, /**< Number of contributing measurements. */
		minimum = UINT64_MAX, /**< Minimum observed/requested metric value, in the selected metric units. */
		maximum = 0; /**< Maximum observed/requested metric value, in the selected metric units. */
};
/** Bounded 60-second live demand summary; no history-ring dependence. */
struct MemoryLiveStatistics {
	long double mean = 0; /**< Weighted arithmetic mean in the selected metric units. */
	uint64_t minimum = 0, /**< Minimum observed/requested metric value, in the selected metric units. */
		maximum = 0, /**< Maximum observed/requested metric value, in the selected metric units. */
		count = 0; /**< Number of contributing measurements. */
};
/** Current measurements, session peaks, churn, and bounded live-demand buckets for one source. */
struct MemoryCurrentSource {
	std::array<MemoryLiveBucket, 60> live_buckets{}; /**< Bounded one-second buckets for the last 60 seconds of live demand. */
	uint64_t measured_at_ns = 0; /**< Monotonic nanosecond timestamp of the latest live measurement. */
	MemorySourceKey key{}; /**< Source and window attribution. */
	MemoryValueSample value{}; /**< Measured metric value or source sample for this record. */
	MemoryValueSample sessionPeak{}; /**< Per-metric high-water values since monitoring began. */
	uint64_t cumulativeLogicalChurnBytes = 0u; /**< Cumulative logical allocation/release byte traffic. */
	uint64_t cumulativeBackingChurnBytes = 0u; /**< Cumulative backing growth/reallocation byte traffic. */
	uint64_t cumulativeResourceChurnBytes = 0u; /**< Cumulative resource create/destroy byte traffic. */
	uint64_t retirementPressureByteTicks = 0u; /**< Retired bytes integrated over observed application ticks. */
	MemorySampleSequence activeSegment = 0u; /**< Sequence of the currently open retained segment, or zero when none. */
	AppTickId lastSampledTick = 0u; /**< Most recent application tick observed for the source. */
};
/**
 * One retained half-open interval during which source values remain constant. Timestamps use monotonic
 * nanoseconds.
 */
struct MemorySampleSegment {
	MemorySampleSequence sequence = 0u; /**< Monotonic identity assigned when the record is retained. */
	uint64_t history_epoch = 1u; /**< Epoch separating retained intervals after pauses/reconfiguration. */
	AppTickId beginTick = 0u; /**< First included application tick. */
	AppTickId endTickExclusive = 0u; /**< First application tick outside the interval. */
	uint64_t beginNs = 0u; /**< Inclusive monotonic interval timestamp in nanoseconds. */
	uint64_t endNs = 0u; /**< Exclusive monotonic interval timestamp in nanoseconds. */
	MemoryValueSample value{}; /**< Measured metric value or source sample for this record. */
};
/** Owned operation record with a monotonic retained-event sequence. */
struct RetainedMemoryEvent {
	MemoryEventSequence sequence = 0u; /**< Monotonic identity assigned when the record is retained. */
	MemoryOperationRecord operation{}; /**< Owned producer operation evidence. */
};
/** Bounded history-ring occupancy and overwrite quality counters. */
struct MemoryRetentionStatus {
	uint64_t retainedCount = 0u, /**< Number of records still present in the bounded ring. */
		capacity = 0u, /**< Maximum number of retained records. */
		oldestRetainedSequence = 0u, /**< Sequence of the oldest retained record; meaningful when hasRetained is true. */
		newestRetainedSequence = 0u, /**< Sequence of the newest retained record; meaningful when hasRetained is true. */
		totalPublished = 0u, /**< Total number of records published since ring reset. */
		overwriteCount = 0u; /**< Number of older records overwritten by the bounded ring. */
	bool hasRetained = false; /**< Whether any valid history records remain. */
};
/**
 * Owned capture metadata and half-open measurement boundaries. A completed capture may still be
 * incomplete due to lost history.
 */
struct MemoryCaptureInfo {
	MemoryCaptureId id = 0u; /**< Capture identity; zero is invalid. */
	std::string name{}; /**< Owned display name for the capture. */
	AppTickId beginTick = 0u, /**< First included application tick. */
		measurementBeginTick = 0u, /**< First measured tick after capture warm-up. */
		endTickExclusive = 0u; /**< First application tick outside the interval. */
	MemorySampleSequence firstSegmentSequence = 0u; /**< First segment sequence expected for this capture. */
	MemoryEventSequence firstEventSequence = 0u; /**< First event sequence expected for this capture. */
	uint64_t warmUpTicks = 0u; /**< Application ticks excluded from measured evidence at capture start. */
	uint64_t begin_ns = 0, /**< Inclusive monotonic interval/capture start in nanoseconds. */
		measurement_begin_ns = 0, /**< Monotonic measurement start after warm-up in nanoseconds. */
		end_ns = 0; /**< Exclusive monotonic interval/capture end in nanoseconds. */
	MemoryReportFlag flags = MemoryReportFlag::None; /**< Completeness, overlap, and analysis-quality flags. */
	bool active = false, /**< Whether the capture is still collecting evidence. */
		complete = false; /**< Whether the ended capture retains complete required evidence. */
};
/** Requested percentile fraction in [0,1] and its weighted metric value. */
struct MemoryPercentile {
	double percentile = 0.0; /**< Percentile fraction in [0,1], such as 0.95 for the 95th percentile. */
	uint64_t value = 0u; /**< Measured metric value or source sample for this record. */
};
/**
 * Select source, metric, interval/capture, weighting, and percentile fractions for a retained-history
 * calculation.
 */
struct MemoryStatisticsQuery {
	MemorySourceId source = 0u; /**< Stable source identity. */
	WindowId window = InvalidWindowId; /**< Owning window, or zero for app-wide measurements. */
	MemoryStatisticMetric metric = MemoryStatisticMetric::LogicalLiveBytes; /**< Selected byte/count metric; determines result units. */
	MemoryWeighting weighting = MemoryWeighting::AppTicks; /**< How each retained observation contributes weight. */
	std::optional<MemoryCaptureId> capture{}; /**< Optional capture identity; absent selects available history. */
	std::vector<double> percentiles{0.50, 0.90, 0.95, 0.99}; /**< Requested fractions or computed percentile/value pairs. */
	/** Optional time clipping; use ElapsedTime or Epochs weighting. */
	std::optional<uint64_t> begin_ns{}, /**< Inclusive monotonic interval/capture start in nanoseconds. */
		end_ns{}; /**< Exclusive monotonic interval/capture end in nanoseconds. */
};
/** Owned weighted statistics and evidence-quality flags for one query. */
struct MemoryStatistics {
	long double mean = 0.0; /**< Weighted arithmetic mean in the selected metric units. */
	std::vector<MemoryPercentile> percentiles{}; /**< Requested fractions or computed percentile/value pairs. */
	MemorySourceKey key{}; /**< Source and window attribution. */
	uint64_t observationCount = 0u, /**< Number of retained observations included. */
		totalWeight = 0u, /**< Sum of represented tick, nanosecond, or epoch weights. */
		minimum = 0u, /**< Minimum observed/requested metric value, in the selected metric units. */
		maximum = 0u; /**< Maximum observed/requested metric value, in the selected metric units. */
	MemoryStatisticMetric metric = MemoryStatisticMetric::LogicalLiveBytes; /**< Selected byte/count metric; determines result units. */
	MemoryWeighting weighting = MemoryWeighting::AppTicks; /**< How each retained observation contributes weight. */
	MemoryReportFlag flags = MemoryReportFlag::None; /**< Completeness, overlap, and analysis-quality flags. */
};
/**
 * Choose percentile startup capacity with normal growth or one growth step sufficient for the observed
 * maximum.
 */
enum class CapacityGrowthPolicy : uint8_t {
	PercentileInitialNormalGrowth = 0, /**< Start at percentile demand and simulate normal repeated growth. */
	PercentileInitialOneGrowthCoversMaximum, /**< Choose an initial reserve whose first growth step covers the observed maximum. */
};
/** Capture and percentile policy used to preview startup reserves without mutating runtime capacities. */
struct CapacityProfileRequest {
	MemoryCaptureId capture = 0u; /**< Optional capture identity; absent selects available history. */
	double percentile = 0.95; /**< Percentile fraction in [0,1], such as 0.95 for the 95th percentile. */
	uint64_t safetyMargin = 0u; /**< Additional capacity in each target's units after percentile selection. */
	float growthFactor = 1.5f; /**< Storage growth multiplier used for preview; must be finite and greater than one. */
	CapacityGrowthPolicy growthPolicy = CapacityGrowthPolicy::PercentileInitialNormalGrowth; /**< How startup reserves and a later growth step cover observed demand. */
};
/**
 * One target recommendation with measured demand, proposed capacity, predicted coverage, and growth
 * simulation.
 */
struct MemoryCapacityRecommendation {
	MemoryTuningTargetDescriptor target{}; /**< Owned capacity target metadata. */
	uint64_t observedMinimum = 0u, /**< Smallest measured target demand. */
		observedMaximum = 0u, /**< Largest measured target demand. */
		percentileDemand = 0u, /**< Weighted demand selected by the requested percentile. */
		proposedInitialCapacity = 0u; /**< Aligned/clamped initial reserve proposed in target units. */
	int64_t deltaFromProductionDefault = 0; /**< Signed proposed capacity minus the target's production default. */
	double predictedNoGrowthCoverage = 0.0; /**< Predicted fraction of observations covered by the proposed initial reserve. */
	FlowUi::detail::MemoryGrowthSimulation simulation{}; /**< Predicted growth behavior across retained demand observations. */
	MemoryReportFlag flags = MemoryReportFlag::None; /**< Completeness, overlap, and analysis-quality flags. */
};
/**
 * Aggregate manager demand recorded by profiling tools. These summaries do not reserve individual
 * runtime containers; use MemoryCapacityPreview::profile settings for startup targets. Values are
 * target-specific counts or byte sizes.
 */
struct ManagerMemoryCapacities {
	uint64_t elements = 0u, /**< Element manager aggregate demand. */
		inputFields = 0u, /**< Input-field registry aggregate demand. */
		inputTextBytes = 0u, /**< Input text payload bytes. */
		fonts = 0u, /**< Font manager aggregate demand. */
		fontAtlasCpuPixelBytes = 0u, /**< Legacy font_atlas_layers aggregate in producer-defined units. */
		icons = 0u, /**< Icon manager aggregate demand. */
		iconDocuments = 0u, /**< Retained SVG document aggregate demand. */
		iconAtlasMetadata = 0u, /**< Icon atlas metadata aggregate demand. */
		viewports = 0u, /**< Viewport manager aggregate demand. */
		popups = 0u, /**< Popup manager aggregate demand. */
		shortcuts = 0u, /**< Shortcut manager aggregate demand. */
		actions = 0u, /**< Action manager aggregate demand. */
		themes = 0u, /**< Theme manager aggregate demand. */
		uiLayout = 0u, /**< UI layout aggregate demand. */
		renderer = 0u; /**< Renderer aggregate demand. */
};

/** Owned proposed profile and per-target evidence; previewing does not apply or bake the recommendations. */
struct MemoryCapacityPreview {
	MemoryCapacityProfile profile{}; /**< Owned startup profile proposed by the preview. */
	ManagerMemoryCapacities managers{}; /**< Aggregate demand summaries; do not configure allocations. */
	std::vector<MemoryCapacityRecommendation> recommendations{}; /**< Owned recommendations and evidence for each measured target. */
	MemoryReportFlag flags = MemoryReportFlag::None; /**< Completeness, overlap, and analysis-quality flags. */
	uint64_t predictedStartupBytes = 0u; /**< Estimated startup bytes where target units permit a byte estimate. */
};
/** Observed memory and sampling-time cost of the memory reporting service. */
struct MemoryOverheadSnapshot {
	uint64_t retainedCapacityBytes = 0u, /**< Allocated byte capacity of retained histories. */
		currentTableEstimatedBytes = 0u, /**< Estimated backing bytes of the live source table. */
		lastConsumeDurationNs = 0u, /**< Duration of the last consume pass in nanoseconds. */
		maximumConsumeDurationNs = 0u, /**< Largest observed consume duration in nanoseconds. */
		storageProbeDurationNs = 0u, /**< Latest storage checkpoint sampling duration in nanoseconds. */
		processProbeDurationNs = 0u, /**< Latest process sampling duration in nanoseconds. */
		gpuProbeDurationNs = 0u; /**< Latest GPU sampling duration in nanoseconds. */
};
/** Copied retention, quality, publication-generation, freeze, and sampling status. */
struct MemoryReportingStatus {
	MemoryRetentionStatus segments{}, /**< Segment-ring retention state. */
		events{}; /**< Operation-ring retention state. */
	MemoryOverheadSnapshot overhead{}; /**< Instrumentation memory/time overhead. */
	MemoryQualitySnapshot quality{}; /**< Producer and source-registration quality counters. */
	uint64_t consumedOperations = 0u, /**< Total producer operations consumed by reporting. */
		consumeFailures = 0u, /**< Number of consume passes that could not complete. */
		generation = 0u; /**< Live publication generation used to detect changed reporting state. */
	AppTickId lastConsumedAppTick = 0u; /**< Application tick of the latest completed consume pass. */
	uint64_t storageMutationSequence = 0u; /**< Storage mutation checkpoint revision. */
	uint64_t last_consumed_ns = 0; /**< Monotonic nanosecond timestamp of the latest consume pass. */
	/** Advances on live publication and reconfiguration, including while history is frozen. */
	uint64_t correlation_revision = 0;
	uint64_t history_storage_generation = 0, /**< Generation of the history backing storage. */
		history_max_lock_ns = 0; /**< Largest measured history lock hold duration in nanoseconds. */
	uint64_t history_revision = 0, /**< Revision of retained-history contents. */
		history_epoch = 1, /**< Epoch separating retained intervals after pauses/reconfiguration. */
		freeze_owner_count = 0; /**< Number of outstanding history pause handles. */
	uint64_t frozen_at_ns = 0, /**< Monotonic nanosecond timestamp at which history was paused. */
		operations_not_retained = 0, /**< Operations observed while retention was paused. */
		history_scan_ns = 0; /**< Most recently measured history scan duration in nanoseconds. */
	MemoryMonitoringLevel compiledLevel = MemoryMonitoringLevel::Disabled; /**< Maximum memory detail compiled into this build. */
	MemoryMonitoringLevel runtimeLevel = MemoryMonitoringLevel::Disabled; /**< Currently enabled memory detail level. */
	MemoryReportFlag flags = MemoryReportFlag::None; /**< Completeness, overlap, and analysis-quality flags. */
	bool hasStorageSnapshot = false; /**< Whether a valid storage checkpoint has been captured. */
};

/** Run-owned active demand maxima; independent of history and owner destruction. */
struct MemoryCapacityHighWater {
	uint64_t active_bytes = 0, /**< Highest run-owned active byte demand. */
		active_entries = 0, /**< Highest run-owned active entry demand. */
		first_measured_ns = 0, /**< Monotonic nanosecond timestamp of the first valid measurement. */
		peak_ns = 0; /**< Monotonic nanosecond timestamp of the observed peak. */
	uint64_t revision = 0; /**< Revision of the corresponding live or high-water evidence. */
	uint16_t valid_fields = 0; /**< Availability bits identifying measured high-water fields. */
};
/** Coherent live inputs; deliberately contains no retained history copy. */
struct MemoryLiveRead {
	/** Per-target run-owned active demand maxima in registered target order. */
	std::array<MemoryCapacityHighWater,
			   static_cast<size_t>(FlowUi::detail::InitialCapacityTarget::count)>
		capacity_high_water{};
	FlowUi::detail::InitialMemoryCapacities initial_capacities{}; /**< Resolved startup reserves copied before owner creation. */
	float storage_growth_factor = 1.5f; /**< Effective storage growth multiplier. */
	bool allow_runtime_growth = true; /**< Whether runtime storage growth is enabled. */

	std::vector<MemoryCurrentSource> sources{}; /**< Owned coherent live source snapshots. */
	std::optional<::FlowUi::detail::storage::StorageMemorySnapshot> storage{}; /**< Optional owned storage checkpoint. */
	std::optional<MemoryEnvironmentSnapshot> environment{}; /**< Optional owned process/GPU checkpoint. */
	MemoryReportingStatus status{}; /**< Coherent reporting status sampled with the live inputs. */
};
/** Move-only request that pauses application-wide retention while live reporting continues.
 * The reporting service must outlive its handles. */
class MemoryHistoryFreeze {
public:
	/** Construct an inactive history-pause handle. */
	MemoryHistoryFreeze() noexcept = default;
	/** Release this handle's pause request. */
	~MemoryHistoryFreeze();
	/** Transfer a pause request; other becomes inactive. */
	MemoryHistoryFreeze(MemoryHistoryFreeze&& other) noexcept;
	/** Release any current pause and transfer the request from other. */
	MemoryHistoryFreeze& operator=(MemoryHistoryFreeze&& other) noexcept;
	MemoryHistoryFreeze(const MemoryHistoryFreeze&) = delete;
	MemoryHistoryFreeze& operator=(const MemoryHistoryFreeze&) = delete;
	/** Release the pause request once; safe on an inactive handle. */
	void reset() noexcept;
	/** Return whether this handle currently owns a pause request. */
	[[nodiscard]] explicit operator bool() const noexcept { return reporting_ != nullptr; }

private:
	friend class DevMemoryReporting;
	explicit MemoryHistoryFreeze(DevMemoryReporting* reporting) noexcept : reporting_(reporting) {}
	DevMemoryReporting* reporting_ = nullptr;
};
/**
 * Owns bounded memory history and coherent live snapshots. The borrowed DevMemory service must outlive
 * this reporting service; callbacks execute synchronously and must obey their documented reentry
 * restrictions.
 */
class DevMemoryReporting {
public:
	/** Create bounded reporting histories attached to borrowed memory. */
	explicit DevMemoryReporting(DevMemory& memory, MemoryReportingConfig config = {});
	/** Destroy retained history; all MemoryHistoryFreeze handles must already be released. */
	~DevMemoryReporting();
	DevMemoryReporting(const DevMemoryReporting&) = delete;
	DevMemoryReporting& operator=(const DevMemoryReporting&) = delete;
	/**
	 * Apply normalized retention controls. Returns false while history is frozen; changing capacities
	 * resets affected rings and marks captures incomplete.
	 */
	[[nodiscard]] bool setConfig(const MemoryReportingConfig& config);
	/** Acquire a counted history pause; does not stop live measurements. */
	[[nodiscard]] MemoryHistoryFreeze freeze_history() noexcept;
	/** Scoped chronological traversal. Visitor must not reenter reporting or retain references. */
	void visit_history(const std::function<void(const MemorySampleSegment&)>& visitor) const;
	/** One coherent no-copy view. Spans are chronological and valid only during callback.
	 * Frozen reads hold a transient pause request rather than blocking live publication. */
	void read_history(const std::function<void(std::span<const MemorySampleSegment>,
											   std::span<const MemorySampleSegment>,
											   const MemoryReportingStatus&)>& visitor);
	/** Traverse a stable sequence prefix in bounded locked slices. Caller clips mutable tails
	 * to snapshot.last_consumed_ns. False means reset/eviction prevented a complete read. */
	[[nodiscard]] bool
	visit_history_chunks(const MemoryReportingStatus& snapshot,
						 const std::function<void(std::span<const MemorySampleSegment>)>& visitor,
						 size_t maximum_records = 2048);
	/** Copy all current live inputs coherently under the service lock; no retained history is copied. */
	[[nodiscard]] MemoryLiveRead read_live() const;
	/** Current 60-second tick-weighted active demand; continues while frozen. */
	[[nodiscard]] std::optional<MemoryLiveStatistics> live_statistics(MemorySourceKey key) const;
	/** Copy normalized reporting configuration. */
	[[nodiscard]] MemoryReportingConfig config() const;
	/** Capture resolved startup hints before owners are created. */
	void set_capacity_baseline(const MemoryCapacityProfile& profile);
	/**
	 * Drain producer events and publish source/checkpoint samples for appTick; failures increment
	 * quality counters instead of escaping.
	 */
	void consume(AppTickId appTick) noexcept;
	/** Request a process/GPU checkpoint on the next consume pass. */
	void requestEnvironmentCheckpoint() noexcept;
	/**
	 * Copy name and begin a capture at the next tick, excluding warmUpTicks from measurements; returns
	 * zero while history is frozen.
	 */
	[[nodiscard]] MemoryCaptureId beginCapture(std::string_view name, uint64_t warmUpTicks = 0u);
	/**
	 * End an active capture and assess retained-evidence completeness; false means the ID is unknown
	 * or already ended.
	 */
	[[nodiscard]] bool endCapture(MemoryCaptureId capture) noexcept;
	/** Copy all capture metadata without extending retained-ring history. */
	[[nodiscard]] std::vector<MemoryCaptureInfo> captureSnapshot() const;
	/** Copy current reporting and retention quality state. */
	[[nodiscard]] MemoryReportingStatus status() const noexcept;
	/** Copy registered source metadata into owned storage. */
	[[nodiscard]] std::vector<MemorySourceDescriptor> descriptorSnapshot() const;
	/** Copy registered capacity target metadata into owned storage. */
	[[nodiscard]] std::vector<MemoryTuningTargetDescriptor> tuningTargetSnapshot() const;
	/** Resolve a batch against indexed retained events and live checkpoint metadata without copies.
	 */
	[[nodiscard]] MemoryCorrelationWork
	correlate_errors(std::span<MemoryCorrelationQuery> queries) const noexcept;
	/** Append only events newer than cursor. Use UINT64_MAX generation for the initial read.
	 * A reset never mixes generations in destination.
	 * The caller owns destination; maximum_events bounds its total retained size. */
	[[nodiscard]] MemoryEventRead
	append_event_range(uint64_t generation, uint64_t cursor, AppTickId first_tick,
					   AppTickId last_tick, size_t maximum_events, std::vector<RetainedMemoryEvent>& destination) const;
	/** Sum current source backing values without allocating or copying source payloads. */
	[[nodiscard]] uint64_t current_backing_bytes() const noexcept;
	/** Copy live source measurements and session peaks. */
	[[nodiscard]] std::vector<MemoryCurrentSource> currentSourceSnapshot() const;
	/** Copy currently retained segments in chronological order. */
	[[nodiscard]] std::vector<MemorySampleSegment> segmentSnapshot() const;
	/** Copy currently retained operation events in chronological order. */
	[[nodiscard]] std::vector<RetainedMemoryEvent> eventSnapshot() const;
	/**
	 * Compute owned weighted statistics from retained evidence; nullopt means no usable observations.
	 * Inspect flags for incomplete coverage.
	 */
	[[nodiscard]] std::optional<MemoryStatistics> statistics(const MemoryStatisticsQuery& query) const;
	/**
	 * Calculate proposed initial capacities and growth coverage without applying changes. Inspect
	 * flags and recommendation evidence before using the profile.
	 */
	[[nodiscard]] MemoryCapacityPreview previewCapacityProfile(const CapacityProfileRequest& request) const;
	/** Copy the most recent storage checkpoint, or nullopt before one is available. */
	[[nodiscard]] std::optional<::FlowUi::detail::storage::StorageMemorySnapshot> storageSnapshot() const;
	/** Copy the current manager probe samples. */
	[[nodiscard]] std::vector<MemoryValueSample> managerSamples() const;
	/** Copy the most recent process/GPU checkpoint, or nullopt when unavailable. */
	[[nodiscard]] std::optional<MemoryEnvironmentSnapshot> environmentSnapshot() const;
private:
	friend class MemoryHistoryFreeze;
	void release_history() noexcept;
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};
} // namespace FlowUi::devSystems
#endif
