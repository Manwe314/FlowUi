#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

namespace FlowUi::devSystems {

namespace {

using TimingClock = std::chrono::steady_clock;

[[nodiscard]] constexpr CpuTimingLevel clampToCompiledCpuLevel(CpuTimingLevel requested) noexcept {
	constexpr uint8_t compiledMaximum = static_cast<uint8_t>(FLOWUI_DEV_TIMING_LEVEL);
	return static_cast<CpuTimingLevel>(
		std::min(static_cast<uint8_t>(requested), compiledMaximum));
}

[[nodiscard]] TimingClockCalibration calibrateClock() {
	constexpr uint32_t kSampleCount = 1024u;
	std::vector<uint64_t> samples;
	samples.reserve(kSampleCount);
	for (uint32_t sampleIndex = 0u; sampleIndex < kSampleCount; ++sampleIndex) {
		const auto start = TimingClock::now();
		const auto end = TimingClock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
		samples.push_back(elapsed > 0 ? static_cast<uint64_t>(elapsed) : 0u);
	}
	std::sort(samples.begin(), samples.end());
	return TimingClockCalibration{
		.minimumPairNs = samples.front(),
		.medianPairNs = samples[samples.size() / 2u],
		.p95PairNs = samples[(samples.size() - 1u) * 95u / 100u],
		.sampleCount = kSampleCount,
	};
}

[[nodiscard]] bool descriptorsMatch(
	const TimingZoneDescriptor& left,
	const TimingZoneDescriptor& right) noexcept {
	return left.typeId == right.typeId &&
		left.name == right.name &&
		left.category == right.category &&
		left.role == right.role &&
		left.minimumCpuLevel == right.minimumCpuLevel &&
		left.source.file == right.source.file &&
		left.source.line == right.source.line &&
		left.source.column == right.source.column;
}

void accumulateQuality(TimingQualitySnapshot& destination, const TimingQualitySnapshot& source) {
	destination.recordedZones += source.recordedZones;
	destination.suppressedZones += source.suppressedZones;
	destination.droppedRecords += source.droppedRecords;
	destination.stackOverflows += source.stackOverflows;
	destination.misnestedZones += source.misnestedZones;
	destination.incompleteZones += source.incompleteZones;
	destination.clockAnomalies += source.clockAnomalies;
	destination.descriptorCollisions += source.descriptorCollisions;
	destination.timingOverheadNs += source.timingOverheadNs;
}

} // namespace

struct DevTiming::Impl {
	explicit Impl(const DevTimingConfig& initialConfig)
		: epoch(TimingClock::now()),
		  calibration(calibrateClock()),
		  cpuLevel(static_cast<uint8_t>(clampToCompiledCpuLevel(initialConfig.cpuLevel))),
		  categoryMask(initialConfig.enabledCategoryMask),
		  gpuEnabled(initialConfig.gpuTimingEnabled),
		  gpuQueryCapacity(std::max(16u, initialConfig.gpuQueryCapacityPerFrame & ~1u)),
		  producerCapacity(std::max(64u, initialConfig.producerRecordCapacity)),
		  balancedElementRetentionThresholdNs(initialConfig.balancedElementRetentionThresholdNs),
		  selectedElementDefinition(initialConfig.selectedElementDefinition.value),
		  selectedElementInstance(initialConfig.selectedElementInstance.value) {}

	TimingClock::time_point epoch{};
	TimingClockCalibration calibration{};
	std::atomic<uint8_t> cpuLevel{static_cast<uint8_t>(CpuTimingLevel::Summary)};
	std::atomic<uint32_t> categoryMask{0xFFFFFFFFu};
	std::atomic<bool> gpuEnabled{true};
	std::atomic<uint32_t> gpuQueryCapacity{512u};
	std::atomic<uint32_t> producerCapacity{8192u};
	std::atomic<uint64_t> balancedElementRetentionThresholdNs{50'000u};
	std::atomic<uint64_t> selectedElementDefinition{0u};
	std::atomic<uint64_t> selectedElementInstance{0u};
	std::atomic<uint64_t> configGeneration{1u};
	std::atomic<uint32_t> nextTrackId{1u};
	std::atomic<uint64_t> descriptorCollisions{0u};
	mutable std::mutex mutex{};
	std::vector<std::unique_ptr<DevTimingRecorder>> recorders{};
	std::unordered_map<TimingZoneTypeId, TimingZoneDescriptor> descriptors{};
};

DevTiming::DevTiming(DevTimingConfig config)
	: impl_(std::make_unique<Impl>(config)) {
	registerDescriptor(timing_zones::kWindowFrameTotal);
}

DevTiming::~DevTiming() = default;

DevTimingThreadAttachment DevTiming::attachCurrentThread(std::string_view trackName) {
	if (trackName.empty()) trackName = "unnamed";
	const uint32_t track = impl_->nextTrackId.fetch_add(1u, std::memory_order_relaxed);
	if (track == 0u) {
		throw std::overflow_error("FlowUi development timing track identity space exhausted.");
	}
	auto recorder = std::unique_ptr<DevTimingRecorder>(new DevTimingRecorder(
		*this,
		track,
		trackName,
		impl_->producerCapacity.load(std::memory_order_relaxed)));
	DevTimingRecorder& result = *recorder;
	{
		std::lock_guard lock(impl_->mutex);
		impl_->recorders.push_back(std::move(recorder));
	}
	return DevTimingThreadAttachment(result);
}

std::vector<CpuTimingRecord> DevTiming::drainCompletedRecords() {
	std::vector<CpuTimingRecord> result;
	std::lock_guard lock(impl_->mutex);
	for (const auto& recorder : impl_->recorders) {
		recorder->drainInto(result);
	}
	return result;
}

std::vector<ElementDefinitionTimingAggregate> DevTiming::drainElementTimingAggregates() {
	std::vector<ElementDefinitionTimingAggregate> result;
	std::lock_guard lock(impl_->mutex);
	for (const auto& recorder : impl_->recorders) {
		recorder->drainElementAggregatesInto(result);
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		if (left.appTick != right.appTick) return left.appTick < right.appTick;
		if (left.frame != right.frame) return left.frame < right.frame;
		return left.definition.value < right.definition.value;
	});
	std::vector<ElementDefinitionTimingAggregate> merged;
	merged.reserve(result.size());
	for (const ElementDefinitionTimingAggregate& aggregate : result) {
		if (merged.empty() || merged.back().definition != aggregate.definition ||
			merged.back().frame != aggregate.frame || merged.back().appTick != aggregate.appTick) {
			merged.push_back(aggregate);
			continue;
		}
		auto& destination = merged.back();
		destination.invocationCount += aggregate.invocationCount;
		destination.totalInclusiveNs += aggregate.totalInclusiveNs;
		destination.maximumInclusiveNs =
			std::max(destination.maximumInclusiveNs, aggregate.maximumInclusiveNs);
		destination.canceledInvocationCount += aggregate.canceledInvocationCount;
	}
	return merged;
}

std::vector<TimingZoneDescriptor> DevTiming::descriptorSnapshot() const {
	std::vector<TimingZoneDescriptor> result;
	std::lock_guard lock(impl_->mutex);
	result.reserve(impl_->descriptors.size());
	for (const auto& [_, descriptor] : impl_->descriptors) {
		result.push_back(descriptor);
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		return left.typeId < right.typeId;
	});
	return result;
}

std::vector<TimingTrackDescriptor> DevTiming::trackSnapshot() const {
	std::vector<TimingTrackDescriptor> result;
	std::lock_guard lock(impl_->mutex);
	result.reserve(impl_->recorders.size());
	for (const auto& recorder : impl_->recorders) {
		result.push_back(TimingTrackDescriptor{
			.id = recorder->trackId(),
			.name = std::string(recorder->trackName()),
		});
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		return left.id < right.id;
	});
	return result;
}

TimingQualitySnapshot DevTiming::qualitySnapshot() const {
	TimingQualitySnapshot result{};
	result.descriptorCollisions = impl_->descriptorCollisions.load(std::memory_order_relaxed);
	std::lock_guard lock(impl_->mutex);
	for (const auto& recorder : impl_->recorders) {
		accumulateQuality(result, recorder->qualitySnapshot());
	}
	return result;
}

void DevTiming::setConfig(const DevTimingConfig& config) noexcept {
	impl_->cpuLevel.store(
		static_cast<uint8_t>(clampToCompiledCpuLevel(config.cpuLevel)),
		std::memory_order_relaxed);
	impl_->categoryMask.store(config.enabledCategoryMask, std::memory_order_relaxed);
	impl_->gpuEnabled.store(config.gpuTimingEnabled, std::memory_order_relaxed);
	impl_->gpuQueryCapacity.store(
		std::max(16u, config.gpuQueryCapacityPerFrame & ~1u), std::memory_order_relaxed);
	impl_->producerCapacity.store(
		std::max(64u, config.producerRecordCapacity), std::memory_order_relaxed);
	impl_->balancedElementRetentionThresholdNs.store(
		config.balancedElementRetentionThresholdNs, std::memory_order_relaxed);
	impl_->selectedElementDefinition.store(
		config.selectedElementDefinition.value, std::memory_order_relaxed);
	impl_->selectedElementInstance.store(
		config.selectedElementInstance.value, std::memory_order_relaxed);
	impl_->configGeneration.fetch_add(1u, std::memory_order_release);
}

DevTimingConfig DevTiming::config() const noexcept {
	return DevTimingConfig{
		.cpuLevel = static_cast<CpuTimingLevel>(impl_->cpuLevel.load(std::memory_order_relaxed)),
		.enabledCategoryMask = impl_->categoryMask.load(std::memory_order_relaxed),
		.gpuTimingEnabled = impl_->gpuEnabled.load(std::memory_order_relaxed),
		.gpuQueryCapacityPerFrame = impl_->gpuQueryCapacity.load(std::memory_order_relaxed),
		.producerRecordCapacity = impl_->producerCapacity.load(std::memory_order_relaxed),
		.balancedElementRetentionThresholdNs =
			impl_->balancedElementRetentionThresholdNs.load(std::memory_order_relaxed),
		.selectedElementDefinition = FlowDefinitionID{
			impl_->selectedElementDefinition.load(std::memory_order_relaxed)},
		.selectedElementInstance = FlowElementID{
			impl_->selectedElementInstance.load(std::memory_order_relaxed)},
	};
}

const TimingClockCalibration& DevTiming::clockCalibration() const noexcept {
	return impl_->calibration;
}

uint64_t DevTiming::nowNs() const noexcept {
	const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
		TimingClock::now() - impl_->epoch).count();
	return elapsed > 0 ? static_cast<uint64_t>(elapsed) : 0u;
}

uint64_t DevTiming::configGeneration() const noexcept {
	return impl_->configGeneration.load(std::memory_order_acquire);
}

DevTimingConfig DevTiming::recorderConfig(uint64_t& generation) const noexcept {
	generation = impl_->configGeneration.load(std::memory_order_acquire);
	return config();
}

void DevTiming::registerDescriptor(const TimingZoneDescriptor& descriptor) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		const auto [iterator, inserted] = impl_->descriptors.try_emplace(descriptor.typeId, descriptor);
		if (!inserted && !descriptorsMatch(iterator->second, descriptor)) {
			impl_->descriptorCollisions.fetch_add(1u, std::memory_order_relaxed);
		}
	} catch (...) {
		// Instrumentation metadata failure must never alter application control flow.
	}
}

} // namespace FlowUi::devSystems

#endif
