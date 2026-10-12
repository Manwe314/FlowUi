#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/MemoryCapacityProfile.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"
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
using MemorySampleSequence = uint64_t;
using MemoryEventSequence = uint64_t;
using MemoryCaptureId = uint64_t;

enum class MemoryStatisticMetric : uint8_t {
	LogicalLiveBytes = 0, BackingAllocatedBytes, ReusableBytes, RetiredBytes,
	ObjectCount, CapacityCount, PeakLogicalBytes,
};
enum class MemoryWeighting : uint8_t { AppTicks = 0, ElapsedTime, Epochs };
enum class MemoryReportFlag : uint32_t {
	None = 0u,
	SegmentHistoryOverwritten = 1u << 0u,
	EventHistoryOverwritten = 1u << 1u,
	ProducerDetailDropped = 1u << 2u,
	SourceSamplingUnavailable = 1u << 3u,
	ProcessSamplingUnavailable = 1u << 4u,
	GpuSamplingUnavailable = 1u << 5u,
	SharedPhysicalMemory = 1u << 6u,
	NegativeProcessResidual = 1u << 7u,
	CaptureIncomplete = 1u << 8u,
	GrewDuringCapture = 1u << 9u,
	BackingChurnAfterWarmUp = 1u << 10u,
	RetiredExceedsLive = 1u << 11u,
	MonitorOverheadExceeded = 1u << 12u,
	HistoryPaused = 1u << 13u,
};
[[nodiscard]] constexpr MemoryReportFlag operator|(MemoryReportFlag a, MemoryReportFlag b) noexcept {
	return static_cast<MemoryReportFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr MemoryReportFlag& operator|=(MemoryReportFlag& a, MemoryReportFlag b) noexcept { return a = a | b; }

struct MemorySourceKey {
	MemorySourceId source = 0u;
	WindowId window = InvalidWindowId;
	friend bool operator==(const MemorySourceKey&, const MemorySourceKey&) = default;
};
/** One second of live, tick-weighted demand independent of retained history. */
struct MemoryLiveBucket {
	long double active_sum = 0;
	uint64_t second = UINT64_MAX, count = 0, minimum = UINT64_MAX, maximum = 0;
};
/** Bounded 60-second live demand summary; no history-ring dependence. */
struct MemoryLiveStatistics {
	long double mean = 0;
	uint64_t minimum = 0, maximum = 0, count = 0;
};
struct MemoryCurrentSource {
	std::array<MemoryLiveBucket, 60> live_buckets{};
	uint64_t measured_at_ns = 0;
	MemorySourceKey key{};
	MemoryValueSample value{};
	MemoryValueSample sessionPeak{};
	uint64_t cumulativeLogicalChurnBytes = 0u;
	uint64_t cumulativeBackingChurnBytes = 0u;
	uint64_t cumulativeResourceChurnBytes = 0u;
	uint64_t retirementPressureByteTicks = 0u;
	MemorySampleSequence activeSegment = 0u;
	AppTickId lastSampledTick = 0u;
};
struct MemorySampleSegment {
	MemorySampleSequence sequence = 0u;
	uint64_t history_epoch = 1u;
	AppTickId beginTick = 0u;
	AppTickId endTickExclusive = 0u;
	uint64_t beginNs = 0u;
	uint64_t endNs = 0u;
	MemoryValueSample value{};
};
struct RetainedMemoryEvent { MemoryEventSequence sequence = 0u; MemoryOperationRecord operation{}; };
struct MemoryRetentionStatus {
	uint64_t retainedCount = 0u, capacity = 0u, oldestRetainedSequence = 0u,
		newestRetainedSequence = 0u, totalPublished = 0u, overwriteCount = 0u;
	bool hasRetained = false;
};
struct MemoryCaptureInfo {
	MemoryCaptureId id = 0u;
	std::string name{};
	AppTickId beginTick = 0u, measurementBeginTick = 0u, endTickExclusive = 0u;
	MemorySampleSequence firstSegmentSequence = 0u;
	MemoryEventSequence firstEventSequence = 0u;
	uint64_t warmUpTicks = 0u;
	uint64_t begin_ns = 0, measurement_begin_ns = 0, end_ns = 0;
	MemoryReportFlag flags = MemoryReportFlag::None;
	bool active = false, complete = false;
};
struct MemoryPercentile { double percentile = 0.0; uint64_t value = 0u; };
struct MemoryStatisticsQuery {
	MemorySourceId source = 0u;
	WindowId window = InvalidWindowId;
	MemoryStatisticMetric metric = MemoryStatisticMetric::LogicalLiveBytes;
	MemoryWeighting weighting = MemoryWeighting::AppTicks;
	std::optional<MemoryCaptureId> capture{};
	std::vector<double> percentiles{0.50, 0.90, 0.95, 0.99};
	/** Optional time clipping; use ElapsedTime or Epochs weighting. */
	std::optional<uint64_t> begin_ns{}, end_ns{};
};
struct MemoryStatistics {
	long double mean = 0.0;
	std::vector<MemoryPercentile> percentiles{};
	MemorySourceKey key{};
	uint64_t observationCount = 0u, totalWeight = 0u, minimum = 0u, maximum = 0u;
	MemoryStatisticMetric metric = MemoryStatisticMetric::LogicalLiveBytes;
	MemoryWeighting weighting = MemoryWeighting::AppTicks;
	MemoryReportFlag flags = MemoryReportFlag::None;
};
enum class CapacityGrowthPolicy : uint8_t {
	PercentileInitialNormalGrowth = 0, PercentileInitialOneGrowthCoversMaximum,
};
struct CapacityProfileRequest {
	MemoryCaptureId capture = 0u;
	double percentile = 0.95;
	uint64_t safetyMargin = 0u;
	float growthFactor = 1.5f;
	CapacityGrowthPolicy growthPolicy = CapacityGrowthPolicy::PercentileInitialNormalGrowth;
};
struct MemoryCapacityRecommendation {
	MemoryTuningTargetDescriptor target{};
	uint64_t observedMinimum = 0u, observedMaximum = 0u, percentileDemand = 0u,
		proposedInitialCapacity = 0u;
	int64_t deltaFromProductionDefault = 0;
	double predictedNoGrowthCoverage = 0.0;
	MemoryGrowthSimulation simulation{};
	MemoryReportFlag flags = MemoryReportFlag::None;
};
struct MemoryCapacityPreview {
	MemoryCapacityProfile profile{};
	std::vector<MemoryCapacityRecommendation> recommendations{};
	MemoryReportFlag flags = MemoryReportFlag::None;
	uint64_t predictedStartupBytes = 0u;
};
struct MemoryOverheadSnapshot {
	uint64_t retainedCapacityBytes = 0u, currentTableEstimatedBytes = 0u,
		lastConsumeDurationNs = 0u, maximumConsumeDurationNs = 0u,
		storageProbeDurationNs = 0u, processProbeDurationNs = 0u, gpuProbeDurationNs = 0u;
};
struct MemoryReportingStatus {
	MemoryRetentionStatus segments{}, events{};
	MemoryOverheadSnapshot overhead{};
	MemoryQualitySnapshot quality{};
	uint64_t consumedOperations = 0u, consumeFailures = 0u, generation = 0u;
	AppTickId lastConsumedAppTick = 0u;
	uint64_t storageMutationSequence = 0u;
	uint64_t last_consumed_ns = 0;
	uint64_t history_storage_generation = 0, history_max_lock_ns = 0;
	uint64_t history_revision = 0, history_epoch = 1, freeze_owner_count = 0;
	uint64_t frozen_at_ns = 0, operations_not_retained = 0, history_scan_ns = 0;
	MemoryMonitoringLevel compiledLevel = MemoryMonitoringLevel::Disabled;
	MemoryMonitoringLevel runtimeLevel = MemoryMonitoringLevel::Disabled;
	MemoryReportFlag flags = MemoryReportFlag::None;
	bool hasStorageSnapshot = false;
};

/** Coherent live inputs; deliberately contains no retained history copy. */
struct MemoryLiveRead {
	std::vector<MemoryCurrentSource> sources{};
	std::optional<::FlowUi::detail::storage::StorageMemorySnapshot> storage{};
	std::optional<MemoryEnvironmentSnapshot> environment{};
	MemoryReportingStatus status{};
};
/** Move-only request that pauses application-wide retention while live reporting continues.
 * The reporting service must outlive its handles. */
class MemoryHistoryFreeze {
public:
	MemoryHistoryFreeze() noexcept = default;
	~MemoryHistoryFreeze();
	MemoryHistoryFreeze(MemoryHistoryFreeze&& other) noexcept;
	MemoryHistoryFreeze& operator=(MemoryHistoryFreeze&& other) noexcept;
	MemoryHistoryFreeze(const MemoryHistoryFreeze&) = delete;
	MemoryHistoryFreeze& operator=(const MemoryHistoryFreeze&) = delete;
	void reset() noexcept;
	[[nodiscard]] explicit operator bool() const noexcept { return reporting_ != nullptr; }

private:
	friend class DevMemoryReporting;
	explicit MemoryHistoryFreeze(DevMemoryReporting* reporting) noexcept : reporting_(reporting) {}
	DevMemoryReporting* reporting_ = nullptr;
};
class DevMemoryReporting {
public:
	explicit DevMemoryReporting(DevMemory& memory, MemoryReportingConfig config = {});
	~DevMemoryReporting();
	DevMemoryReporting(const DevMemoryReporting&) = delete;
	DevMemoryReporting& operator=(const DevMemoryReporting&) = delete;
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
	/** Read all live inputs under one service lock. */
	/** Traverse a stable sequence prefix in bounded locked slices. Caller clips mutable tails
	 * to snapshot.last_consumed_ns. False means reset/eviction prevented a complete read. */
	[[nodiscard]] bool
	visit_history_chunks(const MemoryReportingStatus& snapshot,
						 const std::function<void(std::span<const MemorySampleSegment>)>& visitor,
						 size_t maximum_records = 2048);
	[[nodiscard]] MemoryLiveRead read_live() const;
	/** Current 60-second tick-weighted active demand; continues while frozen. */
	[[nodiscard]] std::optional<MemoryLiveStatistics> live_statistics(MemorySourceKey key) const;
	[[nodiscard]] MemoryReportingConfig config() const;
	void consume(AppTickId appTick) noexcept;
	void requestEnvironmentCheckpoint() noexcept;
	[[nodiscard]] MemoryCaptureId beginCapture(std::string_view name, uint64_t warmUpTicks = 0u);
	[[nodiscard]] bool endCapture(MemoryCaptureId capture) noexcept;
	[[nodiscard]] std::vector<MemoryCaptureInfo> captureSnapshot() const;
	[[nodiscard]] MemoryReportingStatus status() const noexcept;
	[[nodiscard]] std::vector<MemorySourceDescriptor> descriptorSnapshot() const;
	[[nodiscard]] std::vector<MemoryTuningTargetDescriptor> tuningTargetSnapshot() const;
	[[nodiscard]] std::vector<MemoryCurrentSource> currentSourceSnapshot() const;
	[[nodiscard]] std::vector<MemorySampleSegment> segmentSnapshot() const;
	[[nodiscard]] std::vector<RetainedMemoryEvent> eventSnapshot() const;
	[[nodiscard]] std::optional<MemoryStatistics> statistics(const MemoryStatisticsQuery& query) const;
	[[nodiscard]] MemoryCapacityPreview previewCapacityProfile(const CapacityProfileRequest& request) const;
	[[nodiscard]] std::optional<::FlowUi::detail::storage::StorageMemorySnapshot> storageSnapshot() const;
	[[nodiscard]] std::vector<MemoryValueSample> managerSamples() const;
	[[nodiscard]] std::optional<MemoryEnvironmentSnapshot> environmentSnapshot() const;
private:
	friend class MemoryHistoryFreeze;
	void release_history() noexcept;
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};
} // namespace FlowUi::devSystems
#endif
