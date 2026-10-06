#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp"

namespace FlowUi::devSystems {

class DevTiming;
class DevGpuTiming;

struct TimingFrameReport {
	WindowFrameKey key{};
	std::vector<CpuTimingRecord> cpuZones{};
	std::vector<GpuTimingRecord> gpuZones{};
	std::vector<ElementDefinitionTimingAggregate> elementDefinitions{};
	bool occupied = false;
};

struct TimingWindowReport {
	WindowId window = InvalidWindowId;
	std::vector<TimingFrameReport> frames{};
	bool occupied = false;
};

struct TimingAppTickReport {
	AppTickId appTick = 0u;
	uint64_t revision = 0u;
	uint64_t boundary_start_ns = 0, boundary_end_ns = 0;
	bool boundary_open = false;
	std::vector<CpuTimingRecord> applicationCpuZones{};
	std::vector<TimingWindowReport> windows{};
	DevTimingConfig captureConfig{};
	TimingQualitySnapshot cpuQuality{};
	GpuTimingQualitySnapshot gpuQuality{};
	bool occupied = false;
};

enum class TimingSampleDomain : uint8_t {
	Cpu = 0,
	Gpu,
};

struct TimingPercentile {
	double percentile = 0.0;
	uint64_t durationNs = 0u;
};

struct TimingRollingStatistics {
	TimingZoneTypeId typeId = 0u;
	TimingSampleDomain domain = TimingSampleDomain::Cpu;
	uint64_t sampleCount = 0u;
	uint64_t minimumNs = 0u;
	uint64_t maximumNs = 0u;
	double averageNs = 0.0;
	std::vector<TimingPercentile> percentiles{};
};

struct TimingReportingStatus {
	uint32_t configuredCapacity = 0u;
	uint32_t effectiveCapacity = 0u;
	uint32_t rollingSampleCapacity = 0u;
	uint32_t maximumFramesInFlight = 1u;
	uint64_t retainedTickCount = 0u;
	AppTickId oldestRetainedAppTick = 0u;
	AppTickId newestRetainedAppTick = 0u;
	uint64_t totalPublishedTicks = 0u;
	uint64_t evictedTicks = 0u;
	uint64_t lateRecordsAfterEviction = 0u;
	uint64_t ingestionFailures = 0u;
	uint64_t mutationSequence = 0u;
	TimingQualitySnapshot quality{};
	bool hasRetainedTicks = false;
};

/** Consistent owned report set and metadata from one reporting lock. */
struct TimingCaptureSnapshot {
	std::vector<TimingAppTickReport> reports{};
	std::vector<TimingZoneDescriptor> descriptors{};
	uint64_t mutation_sequence = 0;
};
/** Central retained-timeline owner and lightweight timing post-processor. */
class DevTimingReporting {
public:
	DevTimingReporting(DevTiming& timing, DevGpuTiming& gpuTiming);
	~DevTimingReporting();

	DevTimingReporting(const DevTimingReporting&) = delete;
	DevTimingReporting& operator=(const DevTimingReporting&) = delete;

	void setConfig(const TimingReportingConfig& config);
	[[nodiscard]] TimingReportingConfig config() const;
	void noteFramesInFlight(uint32_t framesInFlight);

	/** Drain producer data and publish every app tick through the supplied identity. */
	void consumeThrough(AppTickId completedThroughAppTick) noexcept;
	/** Record a non-stack tick boundary; closes the previous cadence interval. */
	void note_tick_boundary(AppTickId app_tick, uint64_t timestamp_ns) noexcept;
	/** Acquire reports and descriptor metadata consistently. */
	[[nodiscard]] TimingCaptureSnapshot capture_snapshot() const;

	[[nodiscard]] TimingReportingStatus status() const noexcept;
	[[nodiscard]] std::optional<TimingAppTickReport> appTickReport(AppTickId appTick) const;
	[[nodiscard]] std::vector<TimingAppTickReport> appTickRange(
		AppTickId firstAppTick,
		size_t maximumCount) const;
	[[nodiscard]] std::vector<TimingZoneDescriptor> descriptorSnapshot() const;
	[[nodiscard]] std::vector<TimingTrackDescriptor> cpuTrackSnapshot() const;
	[[nodiscard]] std::vector<TimingRollingStatistics> rollingStatistics() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};

} // namespace FlowUi::devSystems

#endif
