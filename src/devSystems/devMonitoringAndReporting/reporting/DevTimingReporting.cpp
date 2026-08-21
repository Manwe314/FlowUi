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
	Impl(DevTiming& cpuTiming, DevGpuTiming& deviceTiming)
		: timing(&cpuTiming), gpuTiming(&deviceTiming) {
		resizeRetention(effectiveCapacity(reportingConfig, maximumFramesInFlight));
	}

	void resizeRetention(uint32_t newCapacity) {
		newCapacity = std::max(1u, newCapacity);
		if (ring.size() == newCapacity) return;
		std::vector<TimingAppTickReport> replacement(newCapacity);
		if (hasTicks) {
			const uint64_t keepCount = std::min<uint64_t>(retainedTickCount, newCapacity);
			const AppTickId firstToKeep = newestTick - keepCount + 1u;
			for (AppTickId tick = firstToKeep; tick <= newestTick; ++tick) {
				if (ring.empty()) break;
				TimingAppTickReport& source = ring[tick % ring.size()];
				if (source.occupied && source.appTick == tick) {
					replacement[tick % newCapacity] = std::move(source);
				}
				if (tick == std::numeric_limits<AppTickId>::max()) break;
			}
			if (retainedTickCount > keepCount) evictedTicks += retainedTickCount - keepCount;
			retainedTickCount = keepCount;
			oldestTick = firstToKeep;
		}
		ring = std::move(replacement);
	}

	void publishOne(AppTickId tick) {
		TimingAppTickReport& destination = ring[tick % ring.size()];
		if (destination.occupied && destination.appTick != tick) {
			++evictedTicks;
		}
		clearReport(destination);
		destination.appTick = tick;
		destination.occupied = true;
		destination.revision = ++mutationSequence;
		destination.captureConfig = currentCaptureConfig;
		destination.cpuQuality = currentCpuQuality;
		destination.gpuQuality = currentGpuQuality;

		if (!hasTicks) {
			hasTicks = true;
			oldestTick = tick;
			newestTick = tick;
			retainedTickCount = 1u;
		} else {
			newestTick = tick;
			retainedTickCount = std::min<uint64_t>(retainedTickCount + 1u, ring.size());
			oldestTick = newestTick - retainedTickCount + 1u;
		}
		++totalPublishedTicks;
	}

	void publishThrough(AppTickId tick) {
		if (!hasTicks) {
			publishOne(tick);
			return;
		}
		if (tick <= newestTick) return;
		for (AppTickId next = newestTick + 1u; next <= tick; ++next) {
			publishOne(next);
			if (next == std::numeric_limits<AppTickId>::max()) break;
		}
	}

	[[nodiscard]] TimingAppTickReport* retained(AppTickId tick) noexcept {
		if (!hasTicks || tick < oldestTick || tick > newestTick || ring.empty()) return nullptr;
		TimingAppTickReport& report = ring[tick % ring.size()];
		return report.occupied && report.appTick == tick ? &report : nullptr;
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
	DevGpuTiming* gpuTiming = nullptr;
	TimingReportingConfig reportingConfig{};
	uint32_t maximumFramesInFlight = 1u;
	mutable std::shared_mutex mutex{};
	std::vector<TimingAppTickReport> ring{};
	std::vector<TimingZoneDescriptor> descriptors{};
	std::vector<TimingTrackDescriptor> cpuTracks{};
	std::unordered_map<RollingKey, RollingSeries, RollingKeyHash> rolling{};
	DevTimingConfig currentCaptureConfig{};
	TimingQualitySnapshot currentCpuQuality{};
	GpuTimingQualitySnapshot currentGpuQuality{};
	AppTickId oldestTick = 0u;
	AppTickId newestTick = 0u;
	uint64_t retainedTickCount = 0u;
	uint64_t totalPublishedTicks = 0u;
	uint64_t evictedTicks = 0u;
	uint64_t lateRecordsAfterEviction = 0u;
	std::atomic<uint64_t> ingestionFailures{0u};
	uint64_t mutationSequence = 0u;
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
		std::vector<CpuTimingRecord> cpuRecords = impl_->timing->drainCompletedRecords();
		std::vector<GpuTimingRecord> gpuRecords = impl_->gpuTiming->drainCompletedRecords();
		std::vector<ElementDefinitionTimingAggregate> elementAggregates =
			impl_->timing->drainElementTimingAggregates();
		std::vector<TimingZoneDescriptor> descriptors = impl_->timing->descriptorSnapshot();
		std::vector<TimingTrackDescriptor> cpuTracks = impl_->timing->trackSnapshot();
		const DevTimingConfig captureConfig = impl_->timing->config();
		const TimingQualitySnapshot cpuQuality = impl_->timing->qualitySnapshot();
		const GpuTimingQualitySnapshot gpuQuality = impl_->gpuTiming->qualitySnapshot();

		std::unique_lock lock(impl_->mutex);
		impl_->currentCaptureConfig = captureConfig;
		impl_->currentCpuQuality = cpuQuality;
		impl_->currentGpuQuality = gpuQuality;
		impl_->publishThrough(completedThroughAppTick);
		impl_->descriptors = std::move(descriptors);
		impl_->cpuTracks = std::move(cpuTracks);
		for (const CpuTimingRecord& record : cpuRecords) {
			impl_->publishThrough(record.appTick);
			TimingAppTickReport* tick = impl_->retained(record.appTick);
			if (!tick) {
				++impl_->lateRecordsAfterEviction;
				continue;
			}
			if (record.frame) {
				findOrAddFrame(*tick, record.frame).cpuZones.push_back(record);
			} else {
				tick->applicationCpuZones.push_back(record);
			}
			impl_->appendRolling({record.typeId, TimingSampleDomain::Cpu}, record.durationNs);
			impl_->revise(*tick);
		}
		for (const GpuTimingRecord& record : gpuRecords) {
			impl_->publishThrough(record.appTick);
			TimingAppTickReport* tick = impl_->retained(record.appTick);
			if (!tick || !record.frame) {
				++impl_->lateRecordsAfterEviction;
				continue;
			}
			findOrAddFrame(*tick, record.frame).gpuZones.push_back(record);
			impl_->appendRolling({record.typeId, TimingSampleDomain::Gpu}, record.durationNs);
			impl_->revise(*tick);
		}
		for (const ElementDefinitionTimingAggregate& aggregate : elementAggregates) {
			impl_->publishThrough(aggregate.appTick);
			TimingAppTickReport* tick = impl_->retained(aggregate.appTick);
			if (!tick || !aggregate.frame) {
				++impl_->lateRecordsAfterEviction;
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
		.hasRetainedTicks = impl_->hasTicks,
	};
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
