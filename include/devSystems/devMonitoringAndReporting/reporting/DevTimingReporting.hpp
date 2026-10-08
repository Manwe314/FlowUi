#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <span>
#include <shared_mutex>

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
	uint64_t capture_generation = 0, capture_admitted_ticks = 0, capture_overwritten_ticks = 0;
	uint64_t capture_first_app_tick = 0, capture_end_app_tick_exclusive = 0;
	uint64_t not_retained_by_policy = 0, late_after_seal = 0;
	uint64_t capture_dropped_records = 0, capture_ingestion_failures = 0, capture_gpu_failures = 0;
	bool capture_recording = false, capture_sealed = false;
	bool hasRetainedTicks = false;
};

/** Immutable chronological capture lease; never retain it across UI frames. */
struct TimingCaptureReadView {
	std::shared_lock<std::shared_mutex> lock{};
	std::span<const TimingAppTickReport> first{}, second{};
	std::span<const TimingZoneDescriptor> descriptors{};
	uint64_t generation = 0;
};
struct TimingCorrelationSummary {
	uint64_t revision = 0, invocation = 0, zone = 0, generation = 0;
};
struct TimingBudgetEvent {
	uint64_t app_tick = 0, end_ns = 0, duration_ns = 0;
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
	/** Current monotonic App tick, including uncaptured intervals. */
	[[nodiscard]] AppTickId current_app_tick() const noexcept;
	/** Timestamp of the current boundary, before input and interface transitions. */
	[[nodiscard]] uint64_t current_boundary_ns() const noexcept;
	/** Record a non-stack tick boundary; closes the previous cadence interval. */
	void note_tick_boundary(AppTickId app_tick, uint64_t timestamp_ns) noexcept;
	/** Replace the previous generation without freeing reusable slot storage. */
	[[nodiscard]] uint64_t begin_capture(AppTickId first_app_tick,
										 WindowId budget_window = InvalidWindowId,
										 uint64_t budget_ns = UINT64_MAX) noexcept;
	/** Admit exactly one clean application interval. */
	void admit_tick(AppTickId app_tick, uint64_t timestamp_ns) noexcept;
	/** Stop admission; eligible late results remain writable until sealing. */
	void stop_capture(AppTickId end_app_tick_exclusive) noexcept;
	/** Make the generation immutable. */
	void seal_capture() noexcept;
	/** Borrow sealed chronological segments under the reporting lock. */
	[[nodiscard]] TimingCaptureReadView read_capture() const;
	/** Find the first completed eligible budget event, independent of view filters. */
	[[nodiscard]] std::optional<TimingBudgetEvent> budget_event(WindowId window,
																uint64_t budget_ns) const noexcept;
	/** Acquire reports and descriptor metadata consistently. */
	[[nodiscard]] TimingCaptureSnapshot capture_snapshot() const;

	[[nodiscard]] TimingReportingStatus status() const noexcept;
	/** Return compact error correlation evidence under one lock, without copying reports. */
	[[nodiscard]] std::optional<TimingCorrelationSummary>
	correlate_sample(AppTickId app_tick, WindowFrameKey frame, TimingTrackId track,
					 uint64_t timestamp_ns) const noexcept;
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
