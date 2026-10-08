#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"

namespace FlowUi::devSystems {

namespace {

[[nodiscard]] uint32_t clampedMultiplier(uint32_t value) noexcept {
	return std::max(1u, value);
}

[[nodiscard]] uint32_t effectiveCapacity(
	const TimingReportingConfig& config,
	uint32_t maximumFramesInFlight) noexcept {
	const uint64_t minimum = static_cast<uint64_t>(std::max(1u, maximumFramesInFlight)) *
		clampedMultiplier(config.minimumFramesInFlightMultiplier);
	const uint64_t requested = std::max<uint64_t>(1u, config.retainedAppTickCapacity);
	return static_cast<uint32_t>(std::min<uint64_t>(
		std::max(minimum, requested), std::numeric_limits<uint32_t>::max()));
}

void clearReport(TimingAppTickReport& report) {
	report.appTick = 0u;
	report.revision = 0u;
	report.boundary_start_ns = report.boundary_end_ns = 0;
	report.boundary_open = false;
	report.applicationCpuZones.clear();
	for (TimingWindowReport& window : report.windows) {
		window.window = InvalidWindowId;
		window.occupied = false;
		for (TimingFrameReport& frame : window.frames) {
			frame.key = {};
			frame.cpuZones.clear();
			frame.gpuZones.clear();
			frame.elementDefinitions.clear();
			frame.occupied = false;
		}
	}
	report.captureConfig = {};
	report.cpuQuality = {};
	report.gpuQuality = {};
	report.occupied = false;
}

[[nodiscard]] TimingWindowReport& findOrAddWindow(
	TimingAppTickReport& report,
	WindowId window) {
	const auto found = std::find_if(report.windows.begin(), report.windows.end(),
		[window](const TimingWindowReport& candidate) {
			return candidate.occupied && candidate.window == window;
		});
	if (found != report.windows.end()) return *found;
	const auto reusable = std::find_if(report.windows.begin(), report.windows.end(),
		[](const TimingWindowReport& candidate) { return !candidate.occupied; });
	if (reusable != report.windows.end()) {
		reusable->window = window;
		reusable->occupied = true;
		return *reusable;
	}
	report.windows.push_back(TimingWindowReport{.window = window, .occupied = true});
	return report.windows.back();
}

[[nodiscard]] TimingFrameReport& findOrAddFrame(
	TimingAppTickReport& report,
	WindowFrameKey frame) {
	TimingWindowReport& window = findOrAddWindow(report, frame.window);
	const auto found = std::find_if(window.frames.begin(), window.frames.end(),
		[frame](const TimingFrameReport& candidate) {
			return candidate.occupied && candidate.key == frame;
		});
	if (found != window.frames.end()) return *found;
	const auto reusable = std::find_if(window.frames.begin(), window.frames.end(),
		[](const TimingFrameReport& candidate) { return !candidate.occupied; });
	if (reusable != window.frames.end()) {
		reusable->key = frame;
		reusable->occupied = true;
		return *reusable;
	}
	window.frames.push_back(TimingFrameReport{.key = frame, .occupied = true});
	return window.frames.back();
}

[[nodiscard]] TimingAppTickReport publicSnapshot(const TimingAppTickReport& source) {
	TimingAppTickReport result{
		.appTick = source.appTick,
		.revision = source.revision,
		.boundary_start_ns = source.boundary_start_ns,
		.boundary_end_ns = source.boundary_end_ns,
		.boundary_open = source.boundary_open,
		.applicationCpuZones = source.applicationCpuZones,
		.captureConfig = source.captureConfig,
		.cpuQuality = source.cpuQuality,
		.gpuQuality = source.gpuQuality,
		.occupied = source.occupied,
	};
	for (const TimingWindowReport& sourceWindow : source.windows) {
		if (!sourceWindow.occupied) continue;
		TimingWindowReport window{
			.window = sourceWindow.window,
			.occupied = true,
		};
		for (const TimingFrameReport& sourceFrame : sourceWindow.frames) {
			if (!sourceFrame.occupied) continue;
			window.frames.push_back(sourceFrame);
		}
		result.windows.push_back(std::move(window));
	}
	return result;
}

struct RollingKey {
	TimingZoneTypeId typeId = 0u;
	TimingSampleDomain domain = TimingSampleDomain::Cpu;
	bool operator==(const RollingKey&) const noexcept = default;
};

struct RollingKeyHash {
	[[nodiscard]] size_t operator()(const RollingKey& key) const noexcept {
		return static_cast<size_t>(key.typeId ^
			(static_cast<uint64_t>(key.domain) * 0x9e3779b97f4a7c15ull));
	}
};

struct RollingSeries {
	std::vector<uint64_t> samples{};
	uint32_t next = 0u;
	uint32_t count = 0u;
	long double sum = 0.0L;

	void append(uint64_t value, uint32_t capacity) {
		if (samples.size() != capacity) {
			samples.assign(capacity, 0u);
			next = 0u;
			count = 0u;
			sum = 0.0L;
		}
		if (count == capacity) {
			sum -= static_cast<long double>(samples[next]);
		} else {
			++count;
		}
		samples[next] = value;
		sum += static_cast<long double>(value);
		next = (next + 1u) % capacity;
	}
};

} // namespace

struct DevTimingReporting::Impl {
	TimingQualitySnapshot currentCpuQuality{};
	Impl(DevTiming& cpuTiming, DevGpuTiming& deviceTiming)
		: timing(&cpuTiming), gpuTiming(&deviceTiming) {
		resizeRetention(effectiveCapacity(reportingConfig, maximumFramesInFlight));
	}

	void resizeRetention(uint32_t new_capacity) {
		new_capacity = std::max(1u, new_capacity);
		if (ring.size() == new_capacity || hasTicks || capture_recording || capture_finalizing)
			return;
		ring.resize(new_capacity);
		head = next_write_slot = 0;
	}
	void publishOne(AppTickId tick) {
		if (!capture_recording || (hasTicks && tick <= newestTick))
			return;
		auto& destination = ring[next_write_slot];
		clearReport(destination);
		destination.appTick = tick;
		destination.occupied = true;
		destination.revision = ++mutationSequence;
		destination.captureConfig = currentCaptureConfig;
		destination.cpuQuality = currentCpuQuality;
		destination.gpuQuality = currentGpuQuality;
		if (retainedTickCount == ring.size()) {
			head = (head + 1) % ring.size();
			++capture_overwritten_ticks;
			++evictedTicks;
		} else {
			if (!retainedTickCount)
				head = next_write_slot;
			++retainedTickCount;
		}
		next_write_slot = (next_write_slot + 1) % ring.size();
		oldestTick = ring[head].appTick;
		newestTick = tick;
		hasTicks = true;
		++capture_admitted_ticks;
		++totalPublishedTicks;
	}
	[[nodiscard]] TimingAppTickReport* retained(AppTickId tick) noexcept {
		if (!hasTicks || tick < oldestTick || tick > newestTick)
			return nullptr;
		const auto offset = tick - oldestTick;
		if (offset < retainedTickCount) {
			auto& direct = ring[(head + offset) % ring.size()];
			if (direct.occupied && direct.appTick == tick)
				return &direct;
		}
		// Recoverable polling failures or explicit callers can leave admission gaps.
		size_t first = 0, last = retainedTickCount;
		while (first < last) {
			const auto middle = first + (last - first) / 2;
			auto& candidate = ring[(head + middle) % ring.size()];
			if (candidate.appTick < tick)
				first = middle + 1;
			else
				last = middle;
		}
		if (first == retainedTickCount)
			return nullptr;
		auto& report = ring[(head + first) % ring.size()];
		return report.occupied && report.appTick == tick ? &report : nullptr;
	}
	[[nodiscard]] TimingAppTickReport* writable(AppTickId tick) noexcept {
		if (tick < capture_first_tick || tick >= capture_end_tick ||
			(!capture_recording && !capture_finalizing && !capture_sealed)) {
			++not_retained_by_policy;
			return nullptr;
		}
		auto* report = retained(tick);
		if (!report) {
			++lateRecordsAfterEviction;
			return nullptr;
		}
		if (capture_sealed) {
			++late_after_seal;
			return nullptr;
		}
		return report;
	}
	void revise(TimingAppTickReport& report) {
		report.revision = ++mutationSequence;
		report.captureConfig = currentCaptureConfig;
		report.cpuQuality = currentCpuQuality;
		report.gpuQuality = currentGpuQuality;
	}

	void appendRolling(RollingKey key, uint64_t durationNs) {
		rolling[key].append(durationNs, std::max(1u, reportingConfig.rollingSampleCapacity));
	}

	DevTiming* timing = nullptr;
	GpuTimingQualitySnapshot currentGpuQuality{};
	DevTimingConfig currentCaptureConfig{};
	mutable std::shared_mutex mutex{};
	std::unordered_map<RollingKey, RollingSeries, RollingKeyHash> rolling{};
	TimingReportingConfig reportingConfig{};
	std::vector<TimingAppTickReport> ring{};
	std::vector<TimingZoneDescriptor> descriptors{};
	std::vector<TimingTrackDescriptor> cpuTracks{};
	DevGpuTiming* gpuTiming = nullptr;
	AppTickId current_tick = 0;
	uint64_t current_boundary_ns = 0;
	std::mutex ingestion_mutex{};
	AppTickId oldestTick = 0u;
	AppTickId newestTick = 0u;
	uint64_t retainedTickCount = 0u;
	uint64_t totalPublishedTicks = 0u;
	uint64_t evictedTicks = 0u;
	uint64_t lateRecordsAfterEviction = 0u;
	std::atomic<uint64_t> ingestionFailures{0u};
	uint64_t mutationSequence = 0u;
	uint32_t maximumFramesInFlight = 1u;
	size_t head = 0, next_write_slot = 0;
	uint64_t capture_generation = 0, capture_admitted_ticks = 0, capture_overwritten_ticks = 0;
	uint64_t not_retained_by_policy = 0, late_after_seal = 0;
	WindowId budget_window = InvalidWindowId;
	uint64_t budget_ns = UINT64_MAX;
	std::optional<TimingBudgetEvent> latched_budget_event{};
	uint64_t capture_dropped_baseline = 0, capture_failure_baseline = 0,
			 capture_gpu_failure_baseline = 0;
	uint64_t capture_dropped_records = 0, capture_ingestion_failures = 0, capture_gpu_failures = 0;
	uint64_t metadata_revision = UINT64_MAX;
	std::vector<std::string> descriptor_strings{};
	AppTickId completed_through_tick = 0;
	AppTickId capture_first_tick = 0, capture_end_tick = UINT64_MAX;
	std::vector<CpuTimingRecord> cpu_scratch{};
	std::vector<ElementDefinitionTimingAggregate> element_scratch{};
	std::vector<GpuTimingRecord> gpu_scratch{};
	bool capture_recording = false, capture_finalizing = false, capture_sealed = false;
	bool hasTicks = false;
};

DevTimingReporting::DevTimingReporting(DevTiming& timing, DevGpuTiming& gpuTiming)
	: impl_(std::make_unique<Impl>(timing, gpuTiming)) {}

DevTimingReporting::~DevTimingReporting() = default;

void DevTimingReporting::setConfig(const TimingReportingConfig& config) {
	TimingReportingConfig normalized = config;
	normalized.retainedAppTickCapacity = std::max(1u, normalized.retainedAppTickCapacity);
	normalized.minimumFramesInFlightMultiplier =
		clampedMultiplier(normalized.minimumFramesInFlightMultiplier);
	normalized.rollingSampleCapacity = std::max(1u, normalized.rollingSampleCapacity);
	normalized.percentilePoints.erase(
		std::remove_if(normalized.percentilePoints.begin(), normalized.percentilePoints.end(),
			[](double value) { return !std::isfinite(value) || value < 0.0 || value > 1.0; }),
		normalized.percentilePoints.end());
	std::sort(normalized.percentilePoints.begin(), normalized.percentilePoints.end());
	normalized.percentilePoints.erase(
		std::unique(normalized.percentilePoints.begin(), normalized.percentilePoints.end()),
		normalized.percentilePoints.end());

	std::unique_lock lock(impl_->mutex);
	const bool rollingShapeChanged =
		impl_->reportingConfig.rollingSampleCapacity != normalized.rollingSampleCapacity;
	impl_->reportingConfig = std::move(normalized);
	impl_->resizeRetention(effectiveCapacity(
		impl_->reportingConfig, impl_->maximumFramesInFlight));
	if (rollingShapeChanged) impl_->rolling.clear();
	++impl_->mutationSequence;
}

TimingReportingConfig DevTimingReporting::config() const {
	std::shared_lock lock(impl_->mutex);
	return impl_->reportingConfig;
}

void DevTimingReporting::noteFramesInFlight(uint32_t framesInFlight) {
	std::unique_lock lock(impl_->mutex);
	const uint32_t previousMaximum = impl_->maximumFramesInFlight;
	impl_->maximumFramesInFlight = std::max(previousMaximum, std::max(1u, framesInFlight));
	impl_->resizeRetention(effectiveCapacity(
		impl_->reportingConfig, impl_->maximumFramesInFlight));
	if (impl_->maximumFramesInFlight != previousMaximum) ++impl_->mutationSequence;
}

void DevTimingReporting::consumeThrough(AppTickId completedThroughAppTick) noexcept {
	try {
		std::scoped_lock ingestion_lock(impl_->ingestion_mutex);
		auto& cpuRecords = impl_->cpu_scratch;
		cpuRecords.clear();
		impl_->timing->drain_completed_records_into(cpuRecords);
		auto& gpuRecords = impl_->gpu_scratch;
		gpuRecords.clear();
		impl_->gpuTiming->drain_completed_records_into(gpuRecords);
		auto& elementAggregates = impl_->element_scratch;
		elementAggregates.clear();
		impl_->timing->drain_element_aggregates_into(elementAggregates);
		const auto metadata_revision = impl_->timing->metadata_revision();
		std::vector<TimingZoneDescriptor> descriptors;
		std::vector<TimingTrackDescriptor> cpuTracks;
		bool refresh_metadata = false;
		{
			std::shared_lock metadata_lock(impl_->mutex);
			refresh_metadata =
				!impl_->capture_sealed && metadata_revision != impl_->metadata_revision;
		}
		if (refresh_metadata) {
			descriptors = impl_->timing->descriptorSnapshot();
			cpuTracks = impl_->timing->trackSnapshot();
		}
		const DevTimingConfig captureConfig = impl_->timing->config();
		const TimingQualitySnapshot cpuQuality = impl_->timing->qualitySnapshot();
		const GpuTimingQualitySnapshot gpuQuality = impl_->gpuTiming->qualitySnapshot();

		std::unique_lock lock(impl_->mutex);
		impl_->currentCaptureConfig = captureConfig;
		impl_->currentCpuQuality = cpuQuality;
		impl_->currentGpuQuality = gpuQuality;
		// Publication is driven only by clean capture boundaries.
		impl_->completed_through_tick = completedThroughAppTick;
		if (!impl_->capture_sealed && refresh_metadata) {
			impl_->metadata_revision = metadata_revision;
			impl_->descriptors = std::move(descriptors);
			impl_->cpuTracks = std::move(cpuTracks);
		}
		for (const CpuTimingRecord& record : cpuRecords) {
			impl_->appendRolling({record.typeId, TimingSampleDomain::Cpu}, record.durationNs);
			TimingAppTickReport* tick = impl_->writable(record.appTick);
			if (!tick) {
				continue;
			}
			if (record.frame) {
				findOrAddFrame(*tick, record.frame).cpuZones.push_back(record);
			} else {
				tick->applicationCpuZones.push_back(record);
			}
			if (!impl_->latched_budget_event && impl_->budget_window &&
				record.frame.window == impl_->budget_window &&
				record.typeId == timing_zones::kWindowFrameTotal.typeId &&
				record.durationNs > impl_->budget_ns &&
				record.flags == timingRecordFlags(TimingRecordFlag::Completed))
				impl_->latched_budget_event = TimingBudgetEvent{
					record.appTick, record.startNs + record.durationNs, record.durationNs};
			impl_->revise(*tick);
		}
		for (const GpuTimingRecord& record : gpuRecords) {
			impl_->appendRolling({record.typeId, TimingSampleDomain::Gpu}, record.durationNs);
			TimingAppTickReport* tick = impl_->writable(record.appTick);
			if (!tick || !record.frame) {
				continue;
			}
			findOrAddFrame(*tick, record.frame).gpuZones.push_back(record);
			impl_->revise(*tick);
		}
		for (const ElementDefinitionTimingAggregate& aggregate : elementAggregates) {
			TimingAppTickReport* tick = impl_->writable(aggregate.appTick);
			if (!tick || !aggregate.frame) {
				continue;
			}
			findOrAddFrame(*tick, aggregate.frame).elementDefinitions.push_back(aggregate);
			impl_->revise(*tick);
		}
	} catch (...) {
		// Development reporting must not replace application control flow.
		impl_->ingestionFailures.fetch_add(1u, std::memory_order_relaxed);
	}
}

AppTickId DevTimingReporting::current_app_tick() const noexcept {
	std::shared_lock lock(impl_->mutex);
	return impl_->current_tick;
}
uint64_t DevTimingReporting::current_boundary_ns() const noexcept {
	std::shared_lock lock(impl_->mutex);
	return impl_->current_boundary_ns;
}
void DevTimingReporting::note_tick_boundary(AppTickId app_tick, uint64_t timestamp_ns) noexcept {
	try {
		std::unique_lock lock(impl_->mutex);
		impl_->current_tick = app_tick;
		impl_->current_boundary_ns = timestamp_ns;

		if (app_tick > 0 && !impl_->capture_sealed)
			if (auto* previous = impl_->retained(app_tick - 1)) {
				previous->boundary_end_ns = timestamp_ns;
				previous->boundary_open = false;
				if (!impl_->budget_window && !impl_->latched_budget_event &&
					timestamp_ns >= previous->boundary_start_ns &&
					timestamp_ns - previous->boundary_start_ns > impl_->budget_ns)
					impl_->latched_budget_event =
						TimingBudgetEvent{previous->appTick, timestamp_ns,
										  timestamp_ns - previous->boundary_start_ns};
				impl_->revise(*previous);
			}

	} catch (...) {
		impl_->ingestionFailures.fetch_add(1u, std::memory_order_relaxed);
	}
}
uint64_t DevTimingReporting::begin_capture(AppTickId first_app_tick, WindowId budget_window,
										   uint64_t budget_ns) noexcept {
	std::unique_lock lock(impl_->mutex);
	impl_->hasTicks = false;
	impl_->capture_recording = impl_->capture_finalizing = false;
	try {
		impl_->resizeRetention(
			effectiveCapacity(impl_->reportingConfig, impl_->maximumFramesInFlight));
	} catch (...) {
		impl_->ingestionFailures.fetch_add(1u, std::memory_order_relaxed);
	}
	impl_->metadata_revision = UINT64_MAX;
	impl_->retainedTickCount = 0;
	impl_->oldestTick = impl_->newestTick = 0;
	impl_->head = impl_->next_write_slot;
	impl_->capture_first_tick = first_app_tick;
	impl_->budget_window = budget_window;
	impl_->budget_ns = budget_ns;
	impl_->latched_budget_event.reset();
	impl_->capture_dropped_records = impl_->capture_ingestion_failures =
		impl_->capture_gpu_failures = 0;
	impl_->capture_dropped_baseline =
		impl_->currentCpuQuality.droppedRecords + impl_->currentGpuQuality.droppedRecords;
	impl_->capture_gpu_failure_baseline = impl_->currentGpuQuality.queryReadFailures +
										  impl_->currentGpuQuality.queryPoolFailures +
										  impl_->currentGpuQuality.unavailableQueries;
	impl_->capture_failure_baseline = impl_->ingestionFailures.load(std::memory_order_relaxed);
	impl_->capture_end_tick = UINT64_MAX;
	impl_->capture_admitted_ticks = impl_->capture_overwritten_ticks = 0;
	impl_->capture_recording = true;
	impl_->capture_finalizing = impl_->capture_sealed = false;
	return ++impl_->capture_generation;
}
void DevTimingReporting::admit_tick(AppTickId app_tick, uint64_t timestamp_ns) noexcept {
	std::unique_lock lock(impl_->mutex);
	if (!impl_->capture_recording)
		return;
	impl_->publishOne(app_tick);
	if (auto* report = impl_->retained(app_tick)) {
		report->boundary_start_ns = timestamp_ns;
		report->boundary_open = true;
	}
}
void DevTimingReporting::stop_capture(AppTickId end_app_tick_exclusive) noexcept {
	std::unique_lock lock(impl_->mutex);
	impl_->capture_recording = false;
	impl_->capture_finalizing = true;
	impl_->capture_end_tick = end_app_tick_exclusive;
}
void DevTimingReporting::seal_capture() noexcept {
	std::unique_lock lock(impl_->mutex);
	if (impl_->capture_sealed)
		return;
	impl_->capture_recording = impl_->capture_finalizing = false;
	try {
		// Refresh cold metadata once before sealing, including registrations that
		// arrived after the last producer drain. Keep old owned strings alive until then.
		impl_->descriptors = impl_->timing->descriptorSnapshot();
		impl_->cpuTracks = impl_->timing->trackSnapshot();
		impl_->descriptor_strings.clear();
		impl_->descriptor_strings.reserve(impl_->descriptors.size() * 3);
		for (auto& descriptor : impl_->descriptors) {
			impl_->descriptor_strings.emplace_back(descriptor.name);
			impl_->descriptor_strings.emplace_back(descriptor.source.file);
			impl_->descriptor_strings.emplace_back(descriptor.source.function);
		}
		size_t string_index = 0;
		for (auto& descriptor : impl_->descriptors) {
			descriptor.name = impl_->descriptor_strings[string_index++];
			descriptor.source.file = impl_->descriptor_strings[string_index++];
			descriptor.source.function = impl_->descriptor_strings[string_index++];
		}
	} catch (...) {
		impl_->ingestionFailures.fetch_add(1u, std::memory_order_relaxed);
	}
	impl_->capture_dropped_records = impl_->currentCpuQuality.droppedRecords +
									 impl_->currentGpuQuality.droppedRecords -
									 impl_->capture_dropped_baseline;
	impl_->capture_ingestion_failures =
		impl_->ingestionFailures.load(std::memory_order_relaxed) - impl_->capture_failure_baseline;
	impl_->capture_gpu_failures =
		impl_->currentGpuQuality.queryReadFailures + impl_->currentGpuQuality.queryPoolFailures +
		impl_->currentGpuQuality.unavailableQueries - impl_->capture_gpu_failure_baseline;
	impl_->capture_sealed = true;
}
TimingCaptureReadView DevTimingReporting::read_capture() const {
	TimingCaptureReadView view;
	view.lock = std::shared_lock(impl_->mutex);
	if (!impl_->capture_sealed)
		return view;
	view.generation = impl_->capture_generation;
	view.descriptors = impl_->descriptors;
	const auto first_count =
		std::min<size_t>(impl_->retainedTickCount, impl_->ring.size() - impl_->head);
	view.first = std::span(impl_->ring).subspan(impl_->head, first_count);
	view.second = std::span(impl_->ring).first(impl_->retainedTickCount - first_count);
	return view;
}
std::optional<TimingBudgetEvent>
DevTimingReporting::budget_event(WindowId window, uint64_t budget_ns) const noexcept {
	std::shared_lock lock(impl_->mutex);
	return window == impl_->budget_window && budget_ns == impl_->budget_ns
			   ? impl_->latched_budget_event
			   : std::nullopt;
}
TimingCaptureSnapshot DevTimingReporting::capture_snapshot() const {
	TimingCaptureSnapshot result;
	std::shared_lock lock(impl_->mutex);
	result.mutation_sequence = impl_->mutationSequence;
	result.descriptors = impl_->descriptors;
	result.reports.reserve(impl_->retainedTickCount);
	if (impl_->hasTicks)
		for (auto tick = impl_->oldestTick; tick <= impl_->newestTick; ++tick) {
			if (const auto* report = impl_->retained(tick)) {
				result.reports.emplace_back(publicSnapshot(*report));
				if (result.reports.back().boundary_open)
					result.reports.back().boundary_end_ns = impl_->timing->nowNs();
			}
			if (tick == UINT64_MAX)
				break;
		}
	return result;
}

TimingReportingStatus DevTimingReporting::status() const noexcept {
	std::shared_lock lock(impl_->mutex);
	return TimingReportingStatus{
		.configuredCapacity = impl_->reportingConfig.retainedAppTickCapacity,
		.effectiveCapacity = static_cast<uint32_t>(impl_->ring.size()),
		.rollingSampleCapacity = impl_->reportingConfig.rollingSampleCapacity,
		.maximumFramesInFlight = impl_->maximumFramesInFlight,
		.retainedTickCount = impl_->retainedTickCount,
		.oldestRetainedAppTick = impl_->oldestTick,
		.newestRetainedAppTick = impl_->newestTick,
		.totalPublishedTicks = impl_->totalPublishedTicks,
		.evictedTicks = impl_->evictedTicks,
		.lateRecordsAfterEviction = impl_->lateRecordsAfterEviction,
		.ingestionFailures = impl_->ingestionFailures.load(std::memory_order_relaxed),
		.mutationSequence = impl_->mutationSequence,
		.quality = impl_->currentCpuQuality,
		.capture_generation = impl_->capture_generation,
		.capture_admitted_ticks = impl_->capture_admitted_ticks,
		.capture_overwritten_ticks = impl_->capture_overwritten_ticks,
		.capture_first_app_tick = impl_->capture_first_tick,
		.capture_end_app_tick_exclusive = impl_->capture_end_tick,
		.not_retained_by_policy = impl_->not_retained_by_policy,
		.late_after_seal = impl_->late_after_seal,
		.capture_dropped_records = impl_->capture_dropped_records,
		.capture_ingestion_failures = impl_->capture_ingestion_failures,
		.capture_gpu_failures = impl_->capture_gpu_failures,
		.capture_recording = impl_->capture_recording,
		.capture_sealed = impl_->capture_sealed,
		.hasRetainedTicks = impl_->hasTicks,
	};
}

std::optional<TimingCorrelationSummary>
DevTimingReporting::correlate_sample(AppTickId app_tick, WindowFrameKey frame, TimingTrackId track,
									 uint64_t timestamp_ns) const noexcept {
	std::shared_lock lock(impl_->mutex);
	const auto* report = impl_->retained(app_tick);
	if (!report)
		return std::nullopt;
	const auto* zones = &report->applicationCpuZones;
	if (frame)
		for (const auto& window : report->windows) {
			if (!window.occupied || window.window != frame.window)
				continue;
			for (const auto& candidate : window.frames)
				if (candidate.occupied && candidate.key == frame)
					zones = &candidate.cpuZones;
		}
	const CpuTimingRecord* containing = nullptr;
	for (const auto& sample : *zones)
		if (sample.track == track && timestamp_ns >= sample.startNs &&
			timestamp_ns - sample.startNs <= sample.durationNs &&
			(!containing || sample.depth >= containing->depth))
			containing = &sample;
	return TimingCorrelationSummary{report->revision, containing ? containing->invocationId : 0,
									containing ? containing->typeId : 0, impl_->capture_generation};
}
std::optional<TimingAppTickReport> DevTimingReporting::appTickReport(AppTickId appTick) const {
	std::shared_lock lock(impl_->mutex);
	TimingAppTickReport* report = impl_->retained(appTick);
	if (!report) return std::nullopt;
	return publicSnapshot(*report);
}

std::vector<TimingAppTickReport> DevTimingReporting::appTickRange(
	AppTickId firstAppTick,
	size_t maximumCount) const {
	std::vector<TimingAppTickReport> result;
	if (maximumCount == 0u) return result;
	std::shared_lock lock(impl_->mutex);
	if (!impl_->hasTicks) return result;
	AppTickId tick = std::max(firstAppTick, impl_->oldestTick);
	const AppTickId last = impl_->newestTick;
	result.reserve(std::min<size_t>(maximumCount, impl_->retainedTickCount));
	while (tick <= last && result.size() < maximumCount) {
		if (TimingAppTickReport* report = impl_->retained(tick)) {
			result.push_back(publicSnapshot(*report));
		}
		if (tick == std::numeric_limits<AppTickId>::max()) break;
		++tick;
	}
	return result;
}

std::vector<TimingZoneDescriptor> DevTimingReporting::descriptorSnapshot() const {
	std::shared_lock lock(impl_->mutex);
	return impl_->descriptors;
}

std::vector<TimingTrackDescriptor> DevTimingReporting::cpuTrackSnapshot() const {
	std::shared_lock lock(impl_->mutex);
	return impl_->cpuTracks;
}

std::vector<TimingRollingStatistics> DevTimingReporting::rollingStatistics() const {
	std::vector<TimingRollingStatistics> result;
	std::shared_lock lock(impl_->mutex);
	result.reserve(impl_->rolling.size());
	for (const auto& [key, series] : impl_->rolling) {
		if (series.count == 0u) continue;
		std::vector<uint64_t> sorted(series.samples.begin(), series.samples.begin() + series.count);
		std::sort(sorted.begin(), sorted.end());
		TimingRollingStatistics statistics{
			.typeId = key.typeId,
			.domain = key.domain,
			.sampleCount = series.count,
			.minimumNs = sorted.front(),
			.maximumNs = sorted.back(),
			.averageNs = static_cast<double>(series.sum / static_cast<long double>(series.count)),
		};
		statistics.percentiles.reserve(impl_->reportingConfig.percentilePoints.size());
		for (double percentile : impl_->reportingConfig.percentilePoints) {
			const size_t index = static_cast<size_t>(std::ceil(
				percentile * static_cast<double>(sorted.size()))) - (percentile > 0.0 ? 1u : 0u);
			statistics.percentiles.push_back(TimingPercentile{
				.percentile = percentile,
				.durationNs = sorted[std::min(index, sorted.size() - 1u)],
			});
		}
		result.push_back(std::move(statistics));
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		if (left.domain != right.domain) return left.domain < right.domain;
		return left.typeId < right.typeId;
	});
	return result;
}

} // namespace FlowUi::devSystems

#endif
