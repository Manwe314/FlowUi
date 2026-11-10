#pragma once

#include "FlowUi/Public/errorManagement/Result.hpp"

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <vector>

#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp"

namespace FlowUi::devSystems {

class DevTiming;
class DevGpuTiming;

/** Owned CPU, GPU and element timing evidence for one window frame. */
struct TimingFrameReport {
	/** Window and frame identity to which this report belongs. */
	WindowFrameKey key{};
	/** Owned completed CPU spans attributed to this window frame. */
	std::vector<CpuTimingRecord> cpuZones{};
	/** Owned resolved GPU spans attributed to this window frame. */
	std::vector<GpuTimingRecord> gpuZones{};
	/** Owned per-definition inclusive invocation aggregates for the frame. */
	std::vector<ElementDefinitionTimingAggregate> elementDefinitions{};
	/** Whether this reusable ring slot currently contains a valid report. */
	bool occupied = false;
};

/** Owned frame evidence for one window within an app tick. */
struct TimingWindowReport {
	/** Window identity; InvalidWindowId means no window attribution. */
	WindowId window = InvalidWindowId;
	/** Owned timing evidence for this window's frames within the application tick. */
	std::vector<TimingFrameReport> frames{};
	/** Whether this reusable ring slot currently contains a valid report. */
	bool occupied = false;
};

/** Owned timing evidence, cadence boundaries and capture quality for one application tick. */
struct TimingAppTickReport {
	/** Application tick associated with this evidence; zero means unassigned. */
	AppTickId appTick = 0u;
	/** Revision identifying the observed metadata, report or safe-point state. */
	uint64_t revision = 0u;
	/** @var TimingAppTickReport::boundary_start_ns
	 * Start of the application cadence interval, in nanoseconds.
	 */
	/** @var TimingAppTickReport::boundary_end_ns
	 * End of the application cadence interval, in nanoseconds.
	 */
	uint64_t boundary_start_ns = 0, boundary_end_ns = 0;
	/** Whether the cadence interval is still awaiting its next tick boundary. */
	bool boundary_open = false;
	/** CPU spans attributed to the application tick rather than a window frame. */
	std::vector<CpuTimingRecord> applicationCpuZones{};
	/** Owned window reports belonging to this application tick. */
	std::vector<TimingWindowReport> windows{};
	/** Capture configuration effective for the reported tick. */
	DevTimingConfig captureConfig{};
	/** CPU capture quality counters observed with this report. */
	TimingQualitySnapshot cpuQuality{};
	/** GPU capture capability and quality observed with this report. */
	GpuTimingQualitySnapshot gpuQuality{};
	/** Whether this reusable ring slot currently contains a valid report. */
	bool occupied = false;
};

/** Clock domain from which a rolling duration sample was captured. */
enum class TimingSampleDomain : uint8_t {
	/** CPU timestamp domain. */
	Cpu = 0,
	/** GPU timestamp domain. */
	Gpu,
};

/** One requested percentile and its observed duration in nanoseconds. */
struct TimingPercentile {
	/** Requested percentile in the statistics distribution. */
	double percentile = 0.0;
	/** Measured duration in nanoseconds. */
	uint64_t durationNs = 0u;
};

/** Rolling duration statistics for one zone type and sample domain. */
struct TimingRollingStatistics {
	/** Stable timing descriptor identity; zero is unassigned. */
	TimingZoneTypeId typeId = 0u;
	/** CPU or GPU domain of the duration samples. */
	TimingSampleDomain domain = TimingSampleDomain::Cpu;
	/** Number of samples included in this measurement. */
	uint64_t sampleCount = 0u;
	/** Smallest retained sample duration in nanoseconds. */
	uint64_t minimumNs = 0u;
	/** Largest retained sample duration in nanoseconds. */
	uint64_t maximumNs = 0u;
	/** Arithmetic mean of retained sample durations in nanoseconds. */
	double averageNs = 0.0;
	/** Requested percentile durations computed from retained samples. */
	std::vector<TimingPercentile> percentiles{};
};

/** Retained history capacity, capture generation and ingestion-quality counters. */
struct TimingReportingStatus {
	/** Configured retained application tick capacity. */
	uint32_t configuredCapacity = 0u;
	/** Effective retained tick capacity after frames-in-flight requirements. */
	uint32_t effectiveCapacity = 0u;
	/** Maximum retained duration samples per zone and domain. */
	uint32_t rollingSampleCapacity = 0u;
	/** Largest renderer frames-in-flight count observed by reporting. */
	uint32_t maximumFramesInFlight = 1u;
	/** Number of valid application ticks currently retained. */
	uint64_t retainedTickCount = 0u;
	/** Oldest application tick still retained. */
	AppTickId oldestRetainedAppTick = 0u;
	/** Newest application tick currently retained. */
	AppTickId newestRetainedAppTick = 0u;
	/** Number of total published ticks. */
	uint64_t totalPublishedTicks = 0u;
	/** Number of evicted ticks. */
	uint64_t evictedTicks = 0u;
	/** Number of late timing records whose target tick was already evicted. */
	uint64_t lateRecordsAfterEviction = 0u;
	/** Number of ingestion failures. */
	uint64_t ingestionFailures = 0u;
	/** Monotonic revision changed when retained timing evidence mutates. */
	uint64_t mutationSequence = 0u;
	/** Capture quality counters associated with this reporting state. */
	TimingQualitySnapshot quality{};
	/** @var TimingReportingStatus::capture_generation
	 * Timing capture generation associated with this evidence.
	 */
	/** @var TimingReportingStatus::capture_admitted_ticks
	 * Number of application ticks admitted to the current capture.
	 */
	/** @var TimingReportingStatus::capture_overwritten_ticks
	 * Number of admitted ticks overwritten by the capture ring.
	 */
	uint64_t capture_generation = 0, capture_admitted_ticks = 0, capture_overwritten_ticks = 0;
	/** @var TimingReportingStatus::capture_first_app_tick
	 * Inclusive beginning of the capture tick interval.
	 */
	/** @var TimingReportingStatus::capture_end_app_tick_exclusive
	 * Exclusive end of the capture tick interval.
	 */
	uint64_t capture_first_app_tick = 0, capture_end_app_tick_exclusive = 0;
	/** @var TimingReportingStatus::not_retained_by_policy
	 * Measurements excluded by the active capture retention policy.
	 */
	/** @var TimingReportingStatus::late_after_seal
	 * Late measurements rejected after the generation was sealed.
	 */
	uint64_t not_retained_by_policy = 0, late_after_seal = 0;
	/** @var TimingReportingStatus::capture_dropped_records
	 * Producer records lost during the capture interval.
	 */
	/** @var TimingReportingStatus::capture_ingestion_failures
	 * Consumer ingestion failures during the capture interval.
	 */
	/** @var TimingReportingStatus::capture_gpu_failures
	 * GPU capture failures observed during the capture interval.
	 */
	uint64_t capture_dropped_records = 0, capture_ingestion_failures = 0, capture_gpu_failures = 0;
	/** @var TimingReportingStatus::capture_recording
	 * Whether the current generation is accepting new ticks.
	 */
	/** @var TimingReportingStatus::capture_sealed
	 * Whether the current generation is immutable.
	 */
	bool capture_recording = false, capture_sealed = false;
	/** Whether at least one valid timing tick is retained. */
	bool hasRetainedTicks = false;
};

/** Immutable chronological capture lease; never retain it across UI frames. */
struct TimingCaptureReadView {
	/** Shared reporting lock held for the lifetime of the borrowed capture view. */
	std::shared_lock<std::shared_mutex> lock{};
	/** @var TimingCaptureReadView::first
	 * First chronological ring segment borrowed while lock is held.
	 */
	/** @var TimingCaptureReadView::second
	 * Second chronological ring segment; empty when the capture does not wrap.
	 */
	std::span<const TimingAppTickReport> first{}, second{};
	/** Timing descriptor metadata associated with the captured records. */
	std::span<const TimingZoneDescriptor> descriptors{};
	/** Identity of this capture generation. */
	uint64_t generation = 0;
};
/** Compact copied attribution of a diagnostic sample to a retained timing invocation. */
struct TimingCorrelationSummary {
	/** Semantic entity attributed to this timing zone or correlation. */
	TimingEntityRef entity{};
	/** @var TimingCorrelationSummary::revision
	 * Revision identifying the observed metadata, report or safe-point state.
	 */
	/** @var TimingCorrelationSummary::invocation
	 * Identity of the containing retained timing invocation.
	 */
	/** @var TimingCorrelationSummary::zone
	 * Descriptor identity of the containing retained timing invocation.
	 */
	/** @var TimingCorrelationSummary::generation
	 * Identity of this capture generation.
	 */
	uint64_t revision = 0, invocation = 0, zone = 0, generation = 0;
};
/** First eligible completed window frame that exceeded its configured nanosecond budget. */
struct TimingBudgetEvent {
	/** @var TimingBudgetEvent::app_tick
	 * Application tick associated with this event.
	 */
	/** @var TimingBudgetEvent::end_ns
	 * Monotonic end timestamp in nanoseconds.
	 */
	/** @var TimingBudgetEvent::duration_ns
	 * Recording duration in nanoseconds for duration-based captures.
	 */
	uint64_t app_tick = 0, end_ns = 0, duration_ns = 0;
};
/** Consistent owned report set and metadata from one reporting lock. */
struct TimingCaptureSnapshot {
	/** Chronological owned application tick reports. */
	std::vector<TimingAppTickReport> reports{};
	/** Timing descriptor metadata associated with the captured records. */
	std::vector<TimingZoneDescriptor> descriptors{};
	/** Owned descriptor name, source file and source function strings, in that order. */
	std::vector<std::array<std::string, 3>> descriptor_text{};
	/** Reporting mutation sequence captured with the owned dataset. */
	uint64_t mutation_sequence = 0;
	/** @var TimingCaptureSnapshot::generation
	 * Identity of this capture generation.
	 */
	/** @var TimingCaptureSnapshot::first_app_tick
	 * First tick admitted to the capture or occurrence.
	 */
	/** @var TimingCaptureSnapshot::end_app_tick_exclusive
	 * First application tick excluded from the capture interval.
	 */
	uint64_t generation = 0, first_app_tick = 0, end_app_tick_exclusive = 0;
	/** Whether this capture or correlation is finalized against further evidence admission. */
	bool sealed = false;

	/** Construct an empty owned capture dataset. */
	TimingCaptureSnapshot() = default;
	/** Transfer owned reports, descriptor text and its views from the source snapshot. */
	TimingCaptureSnapshot(TimingCaptureSnapshot&&) noexcept = default;
	/** Assign owned state, transferring moves or rebinding borrowed views after a copy as required. */
	TimingCaptureSnapshot& operator=(TimingCaptureSnapshot&&) noexcept = default;
	/**
	 * Copy or move owned evidence and ensure descriptor or advice views refer to this dataset's
	 * owned text.
	 */
	TimingCaptureSnapshot(const TimingCaptureSnapshot& other)
		: reports(other.reports), descriptors(other.descriptors),
		  descriptor_text(other.descriptor_text), mutation_sequence(other.mutation_sequence),
		  generation(other.generation), first_app_tick(other.first_app_tick),
		  end_app_tick_exclusive(other.end_app_tick_exclusive), sealed(other.sealed) {
		bind_descriptor_text();
	}
	/** Assign owned state, transferring moves or rebinding borrowed views after a copy as required. */
	TimingCaptureSnapshot& operator=(const TimingCaptureSnapshot& other) {
		if (this != &other) {
			TimingCaptureSnapshot owned(other);
			*this = std::move(owned);
		}
		return *this;
	}
	/** Rebind descriptor views to this snapshot's owned strings after copying. */
	void bind_descriptor_text() noexcept {
		for (size_t descriptor_index = 0; descriptor_index < descriptor_text.size();
			 ++descriptor_index) {
			auto& descriptor = descriptors[descriptor_index];
			descriptor.name = descriptor_text[descriptor_index][0];
			descriptor.source.file = descriptor_text[descriptor_index][1];
			descriptor.source.function = descriptor_text[descriptor_index][2];
		}
	}
};
/** Central retained-timeline owner and lightweight timing post-processor. */
class DevTimingReporting {
public:
	/** Bind the reporting consumer to CPU and GPU services, which must outlive it. */
	DevTimingReporting(DevTiming& timing, DevGpuTiming& gpuTiming);
	/** Release owned monitoring state after dependent recorders, attachments and scopes are finished. */
	~DevTimingReporting();

	/** Copying is disabled to preserve ownership and producer identity. */
	DevTimingReporting(const DevTimingReporting&) = delete;
	/** Copying is disabled to preserve ownership and producer identity. */
	DevTimingReporting& operator=(const DevTimingReporting&) = delete;

	/**
	 * Apply monitoring or reporting configuration; retained storage and pending work follow the
	 * owning service policy.
	 */
	void setConfig(const TimingReportingConfig& config);
	/** Return a copy of the current effective configuration. */
	[[nodiscard]] TimingReportingConfig config() const;
	/**
	 * Record a renderer frames-in-flight requirement when calculating effective retained tick
	 * capacity.
	 */
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
	/** Copy a sealed generation and all nested descriptor text under one lock and byte limit. */
	[[nodiscard]] Result<TimingCaptureSnapshot>
	/**
	 * Copy one sealed generation and all nested descriptor text under a byte limit; return a Result
	 * error for unavailable generation or exhausted budget.
	 */
	owned_capture_snapshot(uint64_t expected_generation, uint64_t maximum_bytes) const noexcept;

	/**
	 * Return the current scalar progress, retention and quality state; borrowed reference overloads
	 * remain valid until the controller changes.
	 */
	[[nodiscard]] TimingReportingStatus status() const noexcept;
	/** Return compact error correlation evidence under one lock, without copying reports. */
	[[nodiscard]] std::optional<TimingCorrelationSummary>
	/**
	 * Return compact retained timing attribution for a timestamp, tick, frame and producer track
	 * without copying full reports.
	 */
	correlate_sample(AppTickId app_tick, WindowFrameKey frame, TimingTrackId track,
					 uint64_t timestamp_ns) const noexcept;
	/** Copy one retained application tick report, or return empty when no longer retained. */
	[[nodiscard]] std::optional<TimingAppTickReport> appTickReport(AppTickId appTick) const;
	/** Copy at most maximumCount retained reports starting at firstAppTick. */
	[[nodiscard]] std::vector<TimingAppTickReport> appTickRange(
		AppTickId firstAppTick,
		size_t maximumCount) const;
	/**
	 * Copy registered timing descriptor metadata. Descriptor string views remain borrowed from their
	 * original static storage.
	 */
	[[nodiscard]] std::vector<TimingZoneDescriptor> descriptorSnapshot() const;
	/** Copy registered CPU track identities and owned names. */
	[[nodiscard]] std::vector<TimingTrackDescriptor> cpuTrackSnapshot() const;
	/** Copy rolling duration statistics for captured zone types and clock domains. */
	[[nodiscard]] std::vector<TimingRollingStatistics> rollingStatistics() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};

} // namespace FlowUi::devSystems

#endif
