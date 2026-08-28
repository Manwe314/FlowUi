#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "FlowUi/MemoryCapacityProfile.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"

namespace FlowUi::devSystems {
class DevMemory;
using MemorySampleSequence = uint64_t;
using MemoryEventSequence = uint64_t;
using MemoryCaptureId = uint64_t;

enum class MemoryStatisticMetric : uint8_t {
	LogicalLiveBytes = 0, BackingAllocatedBytes, ReusableBytes, RetiredBytes,
	ObjectCount, CapacityCount, PeakLogicalBytes,
};
enum class MemoryWeighting : uint8_t { AppTicks = 0, ElapsedTime, Epochs };
enum class MemoryReportFlag : uint32_t {
	None = 0u, SegmentHistoryOverwritten = 1u << 0u, EventHistoryOverwritten = 1u << 1u,
	ProducerDetailDropped = 1u << 2u, SourceSamplingUnavailable = 1u << 3u,
	ProcessSamplingUnavailable = 1u << 4u, GpuSamplingUnavailable = 1u << 5u,
	SharedPhysicalMemory = 1u << 6u, NegativeProcessResidual = 1u << 7u,
	CaptureIncomplete = 1u << 8u, GrewDuringCapture = 1u << 9u,
	BackingChurnAfterWarmUp = 1u << 10u, RetiredExceedsLive = 1u << 11u,
	MonitorOverheadExceeded = 1u << 12u,
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
struct MemoryCurrentSource {
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
};
struct MemoryStatistics {
	MemorySourceKey key{};
	MemoryStatisticMetric metric = MemoryStatisticMetric::LogicalLiveBytes;
	MemoryWeighting weighting = MemoryWeighting::AppTicks;
	uint64_t observationCount = 0u, totalWeight = 0u, minimum = 0u, maximum = 0u;
	long double mean = 0.0;
	std::vector<MemoryPercentile> percentiles{};
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
	uint64_t consumedOperations = 0u, consumeFailures = 0u, generation = 0u;
	AppTickId lastConsumedAppTick = 0u;
	MemoryMonitoringLevel compiledLevel = MemoryMonitoringLevel::Disabled;
	MemoryMonitoringLevel runtimeLevel = MemoryMonitoringLevel::Disabled;
	MemoryQualitySnapshot quality{};
	MemoryRetentionStatus segments{}, events{};
	MemoryReportFlag flags = MemoryReportFlag::None;
	MemoryOverheadSnapshot overhead{};
	uint64_t storageMutationSequence = 0u;
	bool hasStorageSnapshot = false;
};

class DevMemoryReporting {
public:
	explicit DevMemoryReporting(DevMemory& memory, MemoryReportingConfig config = {});
	~DevMemoryReporting();
	DevMemoryReporting(const DevMemoryReporting&) = delete;
	DevMemoryReporting& operator=(const DevMemoryReporting&) = delete;
	void setConfig(const MemoryReportingConfig& config);
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
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};
} // namespace FlowUi::devSystems
#endif
