#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"

namespace FlowUi::devSystems {
namespace {
using Clock = std::chrono::steady_clock;
uint64_t nowNs() noexcept {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		Clock::now().time_since_epoch()).count());
}

struct SourceKeyHash {
	size_t operator()(const MemorySourceKey& key) const noexcept {
		uint64_t value = key.source;
		value ^= key.window + 0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
		return static_cast<size_t>(value);
	}
};

template <typename T> class SequenceRing {
public:
	explicit SequenceRing(size_t capacity = 1u) { reset(capacity); }
	void reset(size_t capacity) {
		values_.clear();
		values_.resize(std::max<size_t>(1u, capacity));
		count_ = 0u; next_ = 1u; overwritten_ = 0u;
	}
	T& append(T value) {
		value.sequence = next_++;
		T& slot = values_[(value.sequence - 1u) % values_.size()];
		if (count_ == values_.size()) ++overwritten_; else ++count_;
		slot = std::move(value);
		return slot;
	}
	T* retained(uint64_t sequence) noexcept {
		if (sequence == 0u || count_ == 0u || sequence < oldest() || sequence >= next_) return nullptr;
		T& value = values_[(sequence - 1u) % values_.size()];
		return value.sequence == sequence ? &value : nullptr;
	}
	uint64_t oldest() const noexcept { return count_ == 0u ? 0u : next_ - count_; }
	MemoryRetentionStatus status() const noexcept {
		return {.retainedCount = count_, .capacity = values_.size(),
			.oldestRetainedSequence = oldest(), .newestRetainedSequence = count_ ? next_ - 1u : 0u,
			.totalPublished = next_ - 1u, .overwriteCount = overwritten_, .hasRetained = count_ != 0u};
	}
	std::vector<T> snapshot() const {
		std::vector<T> result; result.reserve(count_);
		for (uint64_t sequence = oldest(); sequence < next_; ++sequence) {
			const T& value = values_[(sequence - 1u) % values_.size()];
			if (value.sequence == sequence) result.push_back(value);
		}
		return result;
	}
private:
	std::vector<T> values_{};
	uint64_t count_ = 0u, next_ = 1u, overwritten_ = 0u;
};

bool equalValue(const MemoryValueSample& a, const MemoryValueSample& b) noexcept {
	return a.source == b.source && a.window == b.window &&
		a.logicalLiveBytes == b.logicalLiveBytes && a.retiredBytes == b.retiredBytes &&
		a.reusableBytes == b.reusableBytes && a.backingAllocatedBytes == b.backingAllocatedBytes &&
		a.peakLogicalBytes == b.peakLogicalBytes &&
		a.peakBackingAllocatedBytes == b.peakBackingAllocatedBytes &&
		a.peakObjectCount == b.peakObjectCount && a.objectCount == b.objectCount &&
		a.capacityCount == b.capacityCount && a.allocationOps == b.allocationOps &&
		a.logicalReleaseOps == b.logicalReleaseOps && a.physicalReleaseOps == b.physicalReleaseOps &&
		a.growthOps == b.growthOps && a.flags == b.flags;
}

uint64_t metricValue(const MemoryValueSample& sample, MemoryStatisticMetric metric) noexcept {
	switch (metric) {
	case MemoryStatisticMetric::LogicalLiveBytes: return sample.logicalLiveBytes;
	case MemoryStatisticMetric::BackingAllocatedBytes: return sample.backingAllocatedBytes;
	case MemoryStatisticMetric::ReusableBytes: return sample.reusableBytes;
	case MemoryStatisticMetric::RetiredBytes: return sample.retiredBytes;
	case MemoryStatisticMetric::ObjectCount: return sample.objectCount;
	case MemoryStatisticMetric::CapacityCount: return sample.capacityCount;
	case MemoryStatisticMetric::PeakLogicalBytes: return sample.peakLogicalBytes;
	}
	return 0u;
}

MemoryStatisticMetric statisticMetric(MemoryTuningMetric metric) noexcept {
	switch (metric) {
	case MemoryTuningMetric::LogicalLiveBytes: return MemoryStatisticMetric::LogicalLiveBytes;
	case MemoryTuningMetric::BackingAllocatedBytes: return MemoryStatisticMetric::BackingAllocatedBytes;
	case MemoryTuningMetric::PeakLogicalBytes: return MemoryStatisticMetric::PeakLogicalBytes;
	case MemoryTuningMetric::ObjectCount: return MemoryStatisticMetric::ObjectCount;
	case MemoryTuningMetric::CapacityCount: return MemoryStatisticMetric::CapacityCount;
	}
	return MemoryStatisticMetric::BackingAllocatedBytes;
}

MemorySourceId storageClassSource(::FlowUi::detail::storage::MemoryClass memoryClass) noexcept {
	using Class = ::FlowUi::detail::storage::MemoryClass;
	switch (memoryClass) {
	case Class::Persistent: return memory_sources::kStoragePersistent.id;
	case Class::StringPool: return memory_sources::kStorageStringPool.id;
	case Class::FrameTransient: return memory_sources::kStorageFrameTransient.id;
	case Class::WorkerTransient: return memory_sources::kStorageWorkerTransient.id;
	case Class::DecodeTransient: return memory_sources::kStorageDecodeTransient.id;
	case Class::UploadStaging: return memory_sources::kStorageUploadStaging.id;
	case Class::ResourceMetadata: return memory_sources::kStorageMetadata.id;
	default: return memory_sources::kStorageCpu.id;
	}
}

class VectorMemorySampleSink final : public MemorySampleSink {
public:
	explicit VectorMemorySampleSink(std::vector<MemoryValueSample>& output) : output_(&output) {}
	bool append(const MemoryValueSample& sample) noexcept override {
		try { output_->push_back(sample); return true; } catch (...) { return false; }
	}
private: std::vector<MemoryValueSample>* output_;
};

uint64_t saturatedAdd(uint64_t left, uint64_t right) noexcept {
	return left > UINT64_MAX - right ? UINT64_MAX : left + right;
}
uint64_t alignedProposal(uint64_t value, const MemoryTuningTargetDescriptor& target) noexcept {
	value = saturatedAdd(value, 0u);
	value = alignMemoryCapacity(value, std::max<uint64_t>(1u, target.alignment));
	return std::clamp(value, target.minimum, target.maximum);
}
bool assignTypedCapacity(MemoryCapacityProfile& profile, MemoryTuningTargetId id, uint64_t value) noexcept {
#define FLOWUI_ASSIGN_CAPACITY(Stable, Field) \
	if (id == makeMemorySourceId(Stable)) { profile.Field = value; return true; }
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.storage.initial_persistent_cpu_bytes", storage.initialPersistentCpuBytes)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.storage.initial_string_bytes", storage.initialStringBytes)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.storage.transient_bytes_per_frame_per_window", storage.transientBytesPerFramePerWindow)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.storage.transient_bytes_per_worker", storage.transientBytesPerWorker)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.storage.initial_decode_scratch_bytes", storage.initialDecodeScratchBytes)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.storage.initial_upload_staging_bytes", storage.initialUploadStagingBytes)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.storage.initial_instance_bytes_per_frame", storage.initialInstanceBytesPerFrame)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.elements", managers.elements)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.input_fields", managers.inputFields)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.input_text_bytes", managers.inputTextBytes)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.fonts", managers.fonts)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.font_atlas_layers", managers.fontAtlasCpuPixelBytes)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.icons", managers.icons)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.icon_documents", managers.iconDocuments)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.icon_atlas_metadata", managers.iconAtlasMetadata)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.viewports", managers.viewports)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.popups", managers.popups)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.shortcuts", managers.shortcuts)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.actions", managers.actions)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.themes", managers.themes)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.ui_layout", managers.uiLayout)
	FLOWUI_ASSIGN_CAPACITY("flowui.tuning.renderer", managers.renderer)
#undef FLOWUI_ASSIGN_CAPACITY
	return false;
}
} // namespace

struct DevMemoryReporting::Impl {
	explicit Impl(DevMemory& owner, MemoryReportingConfig initial)
		: memory(&owner), reportingConfig(normalize(initial)),
		  segments(owner.compiledLevel() == MemoryMonitoringLevel::Disabled ||
			owner.config().level == MemoryMonitoringLevel::Disabled ? 1u : reportingConfig.segmentCapacity),
		  events(eventCapacity(reportingConfig)) {}
	static MemoryReportingConfig normalize(MemoryReportingConfig config) noexcept {
		config.segmentCapacity = std::max(1u, config.segmentCapacity);
		config.managerSampleEveryTicks = std::max(1u, config.managerSampleEveryTicks);
		config.quantileWindowSegments = std::max(1u, config.quantileWindowSegments);
		return config;
	}
	static size_t eventCapacity(const MemoryReportingConfig& config) noexcept {
		if (!config.retainLifetimeEvents) return 1u;
		return std::max<uint64_t>(1u, config.eventByteCapacity / sizeof(RetainedMemoryEvent));
	}

	DevMemory* memory = nullptr;
	mutable std::mutex mutex{};
	MemoryReportingConfig reportingConfig{};
	MemoryReportingStatus status{};
	std::vector<MemoryOperationRecord> drainScratch{};
	::FlowUi::detail::storage::StorageMemorySnapshot storageSnapshot{};
	std::vector<MemoryValueSample> managerSamples{};
	std::optional<MemoryEnvironmentSnapshot> environmentSnapshot{};
	std::unordered_map<MemorySourceKey, MemoryCurrentSource, SourceKeyHash> current{};
	SequenceRing<MemorySampleSegment> segments;
	SequenceRing<RetainedMemoryEvent> events;
	std::vector<MemoryCaptureInfo> captures{};
	MemoryCaptureId nextCapture = 1u;
	bool forceEnvironmentCheckpoint = true;
	bool forceCompleteCheckpoint = true;
	uint64_t lastManagerSampleTick = 0u;

	void publishSample(const MemoryValueSample& sample, AppTickId tick, uint64_t timestamp) {
		const MemorySourceKey key{sample.source, sample.window};
		auto [it, inserted] = current.try_emplace(key);
		MemoryCurrentSource& state = it->second;
		if (inserted) { state.key = key; state.sessionPeak = sample; }
		state.sessionPeak.logicalLiveBytes = std::max(state.sessionPeak.logicalLiveBytes, sample.logicalLiveBytes);
		state.sessionPeak.backingAllocatedBytes = std::max(
			state.sessionPeak.backingAllocatedBytes, sample.backingAllocatedBytes);
		state.sessionPeak.retiredBytes = std::max(state.sessionPeak.retiredBytes, sample.retiredBytes);
		state.sessionPeak.reusableBytes = std::max(state.sessionPeak.reusableBytes, sample.reusableBytes);
		state.sessionPeak.objectCount = std::max(state.sessionPeak.objectCount, sample.objectCount);
		state.sessionPeak.capacityCount = std::max(state.sessionPeak.capacityCount, sample.capacityCount);
		state.sessionPeak.growthOps = std::max(state.sessionPeak.growthOps, sample.growthOps);
		MemorySampleSegment* active = segments.retained(state.activeSegment);
		if (active && equalValue(active->value, sample)) {
			active->endTickExclusive = std::max(active->endTickExclusive, tick + 1u);
			active->endNs = timestamp;
		} else {
			MemorySampleSegment& segment = segments.append(MemorySampleSegment{
				.beginTick = tick, .endTickExclusive = tick + 1u, .beginNs = timestamp,
				.endNs = timestamp, .value = sample});
			state.activeSegment = segment.sequence;
		}
		if (sample.retiredBytes > sample.logicalLiveBytes) status.flags |= MemoryReportFlag::RetiredExceedsLive;
		state.value = sample;
		state.lastSampledTick = tick;
	}

	void applyOperation(const MemoryOperationRecord& operation) {
		MemoryCurrentSource& state = current[MemorySourceKey{operation.source, operation.frame.window}];
		state.key = {operation.source, operation.frame.window};
		switch (operation.operation) {
		case MemoryOperation::LogicalAllocate:
		case MemoryOperation::ExternalAcquire:
			state.cumulativeLogicalChurnBytes = saturatedAdd(state.cumulativeLogicalChurnBytes, operation.bytesChanged); break;
		case MemoryOperation::LogicalRelease:
		case MemoryOperation::ExternalRelease:
			state.cumulativeLogicalChurnBytes = saturatedAdd(state.cumulativeLogicalChurnBytes, operation.bytesChanged); break;
		case MemoryOperation::BackingGrow:
		case MemoryOperation::BackingTrim:
		case MemoryOperation::ContainerReserve:
		case MemoryOperation::ContainerReallocate:
		case MemoryOperation::ContainerRehash:
			state.cumulativeBackingChurnBytes = saturatedAdd(state.cumulativeBackingChurnBytes, operation.bytesChanged); break;
		case MemoryOperation::GpuCreate:
		case MemoryOperation::GpuDestroy:
			state.cumulativeResourceChurnBytes = saturatedAdd(state.cumulativeResourceChurnBytes, operation.bytesChanged); break;
		default: break;
		}
	}
};

DevMemoryReporting::DevMemoryReporting(DevMemory& memory, MemoryReportingConfig config)
	: impl_(std::make_unique<Impl>(memory, config)) {
	impl_->status.compiledLevel = memory.compiledLevel();
	impl_->status.runtimeLevel = memory.config().level;
	impl_->status.segments = impl_->segments.status();
	impl_->status.events = impl_->events.status();
}
DevMemoryReporting::~DevMemoryReporting() = default;

void DevMemoryReporting::setConfig(const MemoryReportingConfig& config) {
	std::scoped_lock lock(impl_->mutex);
	const auto normalized = Impl::normalize(config);
	const uint32_t effectiveSegmentCapacity = impl_->memory->compiledLevel() == MemoryMonitoringLevel::Disabled
		? 1u : normalized.segmentCapacity;
	if (effectiveSegmentCapacity != impl_->segments.status().capacity) {
		impl_->segments.reset(effectiveSegmentCapacity);
		for (auto& [_, source] : impl_->current) source.activeSegment = 0u;
	}
	const size_t effectiveEventCapacity = impl_->memory->compiledLevel() == MemoryMonitoringLevel::Disabled
		? 1u : Impl::eventCapacity(normalized);
	if (effectiveEventCapacity != impl_->events.status().capacity)
		impl_->events.reset(effectiveEventCapacity);
	impl_->reportingConfig = normalized;
	impl_->forceCompleteCheckpoint = true;
}
MemoryReportingConfig DevMemoryReporting::config() const {
	std::scoped_lock lock(impl_->mutex); return impl_->reportingConfig;
}

void DevMemoryReporting::consume(AppTickId appTick) noexcept {
	const uint64_t consumeBegin = nowNs();
	try {
		impl_->drainScratch.clear();
		impl_->memory->recorder().drainInto(impl_->drainScratch);
		const bool eventCheckpoint = std::any_of(impl_->drainScratch.begin(), impl_->drainScratch.end(), [](const auto& op) {
			return op.operation == MemoryOperation::BackingGrow || op.operation == MemoryOperation::BackingTrim ||
				op.operation == MemoryOperation::GpuCreate || op.operation == MemoryOperation::GpuDestroy;
		});
		std::vector<MemoryValueSample> samples;
		bool managerSampled = false;
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
		const DevMemoryConfig memoryConfig = impl_->memory->config();
		const bool managerDue = impl_->forceCompleteCheckpoint || eventCheckpoint || impl_->lastManagerSampleTick == 0u ||
			appTick - impl_->lastManagerSampleTick >= impl_->reportingConfig.managerSampleEveryTicks;
		if (memoryConfig.level >= MemoryMonitoringLevel::SubsystemCapacity && managerDue) {
			managerSampled = true;
			const auto probes = impl_->memory->probeSnapshot();
			samples.reserve(probes.size() * 2u + 16u);
			VectorMemorySampleSink sink(samples);
			MemoryProbeContext context{.appTick = appTick, .nowNs = consumeBegin,
				.detail = memoryConfig.level, .sink = sink};
			for (const RegisteredMemoryProbe& probe : probes)
				if (probe.owner && probe.sample) probe.sample(probe.owner, context);
			std::unordered_map<MemorySourceId, MemoryValueSample> tuningAggregates;
			for (const auto& value : samples) if (value.window != InvalidWindowId) {
				auto& aggregate = tuningAggregates[value.source]; aggregate.source = value.source;
				aggregate.logicalLiveBytes = std::max(aggregate.logicalLiveBytes, value.logicalLiveBytes);
				aggregate.backingAllocatedBytes = std::max(aggregate.backingAllocatedBytes, value.backingAllocatedBytes);
				aggregate.reusableBytes = std::max(aggregate.reusableBytes, value.reusableBytes);
				aggregate.objectCount = std::max(aggregate.objectCount, value.objectCount);
				aggregate.capacityCount = std::max(aggregate.capacityCount, value.capacityCount);
			}
			for (auto& [_, aggregate] : tuningAggregates) samples.push_back(aggregate);
			impl_->lastManagerSampleTick = appTick;
		}
#endif
		const size_t managerSampleCount = samples.size();
		::FlowUi::detail::storage::StorageMemorySnapshot storage;
		bool hasStorage = false;
		uint64_t storageDuration = 0u;
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
		if (impl_->memory->config().level != MemoryMonitoringLevel::Disabled) {
			const uint64_t begin = nowNs();
			hasStorage = impl_->memory->appendStorageSnapshot({
				.detail = impl_->memory->config().level >= MemoryMonitoringLevel::DetailedLifetimes
					? ::FlowUi::detail::storage::StorageMemoryDetail::IndividualResources
					: ::FlowUi::detail::storage::StorageMemoryDetail::Summary}, storage);
			storageDuration = nowNs() - begin;
			if (hasStorage) {
				std::unordered_map<MemorySourceKey, MemoryValueSample, SourceKeyHash> aggregated;
				std::unordered_map<MemorySourceId, MemoryValueSample> tuningAggregates;
				for (const auto& allocator : storage.allocators) {
					const MemorySourceId source = storageClassSource(allocator.memoryClass);
					MemorySourceKey key{source, allocator.window};
					auto& value = aggregated[key]; value.source = key.source; value.window = key.window;
					value.logicalLiveBytes += allocator.liveBytes; value.reusableBytes += allocator.reusableBytes;
					value.backingAllocatedBytes += allocator.reservedBytes;
					value.peakLogicalBytes += allocator.peakLiveBytes;
					value.objectCount += allocator.allocationCount;
					value.capacityCount += allocator.backingAllocationCount;
					value.allocationOps += allocator.cumulativeAllocationCount;
					value.logicalReleaseOps += allocator.logicalReleaseCount;
					value.physicalReleaseOps += allocator.physicalReleaseCount;
					value.growthOps += allocator.growthCount;
					if (allocator.window != InvalidWindowId) {
						auto& tuning = tuningAggregates[source]; tuning.source = source;
						tuning.logicalLiveBytes = std::max(tuning.logicalLiveBytes, allocator.liveBytes);
						tuning.backingAllocatedBytes = std::max(tuning.backingAllocatedBytes, allocator.reservedBytes);
						tuning.peakLogicalBytes = std::max(tuning.peakLogicalBytes, allocator.peakLiveBytes);
						tuning.reusableBytes = std::max(tuning.reusableBytes, allocator.reusableBytes);
						tuning.objectCount = std::max(tuning.objectCount, allocator.allocationCount);
						tuning.capacityCount = std::max<uint64_t>(tuning.capacityCount, allocator.backingAllocationCount);
						tuning.growthOps = std::max(tuning.growthOps, allocator.growthCount);
					}
				}
				for (auto& [_, value] : aggregated) samples.push_back(value);
				for (auto& [_, value] : tuningAggregates) samples.push_back(value);
			}
		}
#endif
		if (impl_->memory->config().level != MemoryMonitoringLevel::Disabled) {
			const MemoryReportingConfig reportingConfig = config();
			const uint64_t monitorCapacityBytes =
				static_cast<uint64_t>(reportingConfig.segmentCapacity) * sizeof(MemorySampleSegment) +
				static_cast<uint64_t>(Impl::eventCapacity(reportingConfig)) * sizeof(RetainedMemoryEvent);
			samples.push_back(MemoryValueSample{.source = memory_sources::kMonitoring.id,
				.logicalLiveBytes = monitorCapacityBytes, .backingAllocatedBytes = monitorCapacityBytes,
				.peakLogicalBytes = monitorCapacityBytes, .peakBackingAllocatedBytes = monitorCapacityBytes,
				.capacityCount = static_cast<uint64_t>(reportingConfig.segmentCapacity) + Impl::eventCapacity(reportingConfig)});
		}
		uint64_t attributableCpu = 0u;
		if (hasStorage) for (const auto& memoryClass : storage.totals.cpu) attributableCpu += memoryClass.reservedBytes;
		const auto managerRoot = [](MemorySourceId source) noexcept {
			return source == memory_sources::kElements.id || source == memory_sources::kInputFields.id ||
				source == memory_sources::kFonts.id || source == memory_sources::kIcons.id ||
				source == memory_sources::kViewports.id || source == memory_sources::kPopups.id ||
				source == memory_sources::kShortcuts.id || source == memory_sources::kActions.id ||
				source == memory_sources::kThemes.id || source == memory_sources::kUiLayout.id ||
				source == memory_sources::kRenderer.id;
		};
		for (const auto& sample : samples) {
			if (sample.source == memory_sources::kMonitoring.id) attributableCpu += sample.backingAllocatedBytes;
			else if (managerRoot(sample.source) && sample.window != InvalidWindowId)
				attributableCpu += sample.backingAllocatedBytes;
			else if (managerRoot(sample.source) && std::none_of(samples.begin(), samples.end(), [&](const auto& candidate) {
				return candidate.source == sample.source && candidate.window != InvalidWindowId;
			})) attributableCpu += sample.backingAllocatedBytes;
		}
		std::optional<MemoryEnvironmentSnapshot> environment;
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
		if (impl_->memory->config().level != MemoryMonitoringLevel::Disabled) {
			environment = impl_->memory->sampleEnvironment(consumeBegin, attributableCpu,
				impl_->forceEnvironmentCheckpoint || eventCheckpoint || impl_->forceCompleteCheckpoint);
			if (environment->process.availableFields != 0u) samples.push_back(MemoryValueSample{
				.source = memory_sources::kProcess.id,
				.logicalLiveBytes = environment->process.residentBytes,
				.backingAllocatedBytes = environment->process.virtualBytes,
				.peakLogicalBytes = environment->process.peakResidentBytes,
				.flags = MemorySampleFlag::Estimate});
			if (environment->gpu.available) {
				MemoryValueSample gpu{.source = memory_sources::kVulkanHeaps.id};
				for (const auto& heap : environment->gpu.heaps) {
					gpu.logicalLiveBytes += heap.allocationBytes; gpu.backingAllocatedBytes += heap.blockBytes;
					gpu.objectCount += heap.allocationCount; gpu.capacityCount += heap.blockCount; gpu.flags = gpu.flags | heap.flags;
				}
				samples.push_back(gpu);
			}
		}
#endif
		const uint64_t timestamp = nowNs();
		std::scoped_lock lock(impl_->mutex);
		impl_->status.consumedOperations += impl_->drainScratch.size();
		for (const auto& operation : impl_->drainScratch) {
			impl_->applyOperation(operation);
			if (impl_->reportingConfig.retainLifetimeEvents)
				impl_->events.append(RetainedMemoryEvent{.operation = operation});
		}
		for (size_t index = 0u; index < managerSampleCount; ++index) {
			MemoryValueSample& sample = samples[index];
			sample.peakLogicalBytes = std::max(sample.peakLogicalBytes, sample.logicalLiveBytes);
			sample.peakBackingAllocatedBytes = std::max(
				sample.peakBackingAllocatedBytes, sample.backingAllocatedBytes);
			sample.peakObjectCount = std::max(sample.peakObjectCount, sample.objectCount);
			const auto existing = impl_->current.find(MemorySourceKey{sample.source, sample.window});
			if (existing != impl_->current.end()) {
				const auto& previous = existing->second;
				sample.peakLogicalBytes = std::max({sample.peakLogicalBytes, sample.logicalLiveBytes,
					previous.sessionPeak.logicalLiveBytes});
				sample.peakBackingAllocatedBytes = std::max({sample.peakBackingAllocatedBytes,
					sample.backingAllocatedBytes, previous.sessionPeak.backingAllocatedBytes});
				sample.peakObjectCount = std::max({sample.peakObjectCount, sample.objectCount,
					previous.sessionPeak.objectCount});
				sample.growthOps = previous.sessionPeak.growthOps +
					(previous.value.backingAllocatedBytes != 0u &&
					 sample.backingAllocatedBytes > previous.value.backingAllocatedBytes ? 1u : 0u);
			}
		}
		if (managerSampled) impl_->managerSamples.assign(
			samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(managerSampleCount));
		impl_->storageSnapshot = std::move(storage);
		impl_->status.hasStorageSnapshot = hasStorage;
		if (hasStorage) impl_->status.storageMutationSequence = impl_->storageSnapshot.mutationSequence;
		impl_->environmentSnapshot = std::move(environment);
		for (const auto& sample : samples) impl_->publishSample(sample, appTick, timestamp);
		for (auto& [_, source] : impl_->current) {
			if (auto* active = impl_->segments.retained(source.activeSegment)) {
				active->endTickExclusive = std::max(active->endTickExclusive, appTick + 1u);
				active->endNs = timestamp;
				source.retirementPressureByteTicks = saturatedAdd(
					source.retirementPressureByteTicks, source.value.retiredBytes);
			}
		}
		impl_->status.lastConsumedAppTick = appTick;
		impl_->status.runtimeLevel = impl_->memory->config().level;
		impl_->status.quality = impl_->memory->qualitySnapshot();
		impl_->status.segments = impl_->segments.status();
		impl_->status.events = impl_->events.status();
		impl_->status.flags = MemoryReportFlag::None;
		if (impl_->status.segments.overwriteCount) impl_->status.flags |= MemoryReportFlag::SegmentHistoryOverwritten;
		if (impl_->status.events.overwriteCount) impl_->status.flags |= MemoryReportFlag::EventHistoryOverwritten;
		if (impl_->status.quality.droppedOperations) impl_->status.flags |= MemoryReportFlag::ProducerDetailDropped;
		if (impl_->memory->config().processMemory && (!impl_->environmentSnapshot ||
			impl_->environmentSnapshot->process.availableFields == 0u)) impl_->status.flags |= MemoryReportFlag::ProcessSamplingUnavailable;
		if (impl_->memory->config().gpuMemory && (!impl_->environmentSnapshot ||
			!impl_->environmentSnapshot->gpu.available)) impl_->status.flags |= MemoryReportFlag::GpuSamplingUnavailable;
		if (impl_->environmentSnapshot) {
			if (impl_->environmentSnapshot->signedResidentDiscrepancy < 0) impl_->status.flags |= MemoryReportFlag::NegativeProcessResidual;
			for (const auto& heap : impl_->environmentSnapshot->gpu.heaps)
				if ((static_cast<uint16_t>(heap.flags) & static_cast<uint16_t>(MemorySampleFlag::SharedPhysicalMemory)) != 0u)
					impl_->status.flags |= MemoryReportFlag::SharedPhysicalMemory;
		}
		impl_->status.overhead.storageProbeDurationNs = storageDuration;
		impl_->status.overhead.processProbeDurationNs = impl_->environmentSnapshot ? impl_->environmentSnapshot->process.sampleDurationNs : 0u;
		impl_->status.overhead.gpuProbeDurationNs = impl_->environmentSnapshot ? impl_->environmentSnapshot->gpu.sampleDurationNs : 0u;
		impl_->status.overhead.retainedCapacityBytes =
			impl_->status.segments.capacity * sizeof(MemorySampleSegment) +
			impl_->status.events.capacity * sizeof(RetainedMemoryEvent);
		impl_->status.overhead.currentTableEstimatedBytes = impl_->current.size() *
			(sizeof(MemoryCurrentSource) + sizeof(MemorySourceKey) + sizeof(void*) * 3u);
		impl_->status.overhead.lastConsumeDurationNs = nowNs() - consumeBegin;
		impl_->status.overhead.maximumConsumeDurationNs = std::max(
			impl_->status.overhead.maximumConsumeDurationNs, impl_->status.overhead.lastConsumeDurationNs);
		if (impl_->status.overhead.lastConsumeDurationNs > impl_->reportingConfig.consumeWarningThresholdNs)
			impl_->status.flags |= MemoryReportFlag::MonitorOverheadExceeded;
		for (auto& capture : impl_->captures) if (capture.active) {
			if (appTick >= capture.measurementBeginTick) {
				for (const auto& op : impl_->drainScratch) {
					if (op.operation == MemoryOperation::BackingGrow || op.operation == MemoryOperation::ContainerReallocate)
						capture.flags |= MemoryReportFlag::GrewDuringCapture;
					if (op.operation == MemoryOperation::BackingGrow || op.operation == MemoryOperation::BackingTrim)
						capture.flags |= MemoryReportFlag::BackingChurnAfterWarmUp;
				}
			}
		}
		for (auto& capture : impl_->captures) {
			const bool segmentEvicted = impl_->status.segments.hasRetained &&
				capture.firstSegmentSequence < impl_->status.segments.oldestRetainedSequence;
			const bool eventEvicted = impl_->reportingConfig.retainLifetimeEvents &&
				impl_->status.events.hasRetained &&
				capture.firstEventSequence < impl_->status.events.oldestRetainedSequence;
			if (segmentEvicted || eventEvicted) {
				capture.complete = false;
				capture.flags |= MemoryReportFlag::CaptureIncomplete;
			}
		}
		impl_->forceEnvironmentCheckpoint = false;
		impl_->forceCompleteCheckpoint = false;
		++impl_->status.generation;
	} catch (...) {
		std::scoped_lock lock(impl_->mutex); ++impl_->status.consumeFailures;
	}
}

void DevMemoryReporting::requestEnvironmentCheckpoint() noexcept {
	std::scoped_lock lock(impl_->mutex); impl_->forceEnvironmentCheckpoint = true;
}
MemoryCaptureId DevMemoryReporting::beginCapture(std::string_view name, uint64_t warmUpTicks) {
	std::scoped_lock lock(impl_->mutex);
	const MemoryCaptureId id = impl_->nextCapture++;
	const AppTickId begin = impl_->status.lastConsumedAppTick + 1u;
	impl_->captures.push_back(MemoryCaptureInfo{.id = id, .name = std::string(name),
		.beginTick = begin, .measurementBeginTick = begin + warmUpTicks,
		.firstSegmentSequence = impl_->status.segments.totalPublished + 1u,
		.firstEventSequence = impl_->status.events.totalPublished + 1u,
		.warmUpTicks = warmUpTicks, .active = true});
	impl_->forceCompleteCheckpoint = true;
	impl_->forceEnvironmentCheckpoint = true;
	return id;
}
bool DevMemoryReporting::endCapture(MemoryCaptureId captureId) noexcept {
	std::scoped_lock lock(impl_->mutex);
	for (auto& capture : impl_->captures) if (capture.id == captureId && capture.active) {
		capture.active = false; capture.complete = true;
		capture.endTickExclusive = impl_->status.lastConsumedAppTick + 1u;
		if ((impl_->status.segments.hasRetained && capture.firstSegmentSequence < impl_->status.segments.oldestRetainedSequence) ||
			(impl_->reportingConfig.retainLifetimeEvents && impl_->status.events.hasRetained &&
			 capture.firstEventSequence < impl_->status.events.oldestRetainedSequence)) {
			capture.complete = false; capture.flags |= MemoryReportFlag::CaptureIncomplete;
		}
		impl_->forceCompleteCheckpoint = true; impl_->forceEnvironmentCheckpoint = true;
		return true;
	}
	return false;
}

MemoryReportingStatus DevMemoryReporting::status() const noexcept { std::scoped_lock lock(impl_->mutex); return impl_->status; }
std::vector<MemoryCaptureInfo> DevMemoryReporting::captureSnapshot() const { std::scoped_lock lock(impl_->mutex); return impl_->captures; }
std::vector<MemorySourceDescriptor> DevMemoryReporting::descriptorSnapshot() const { return impl_->memory->descriptorSnapshot(); }
std::vector<MemoryTuningTargetDescriptor> DevMemoryReporting::tuningTargetSnapshot() const { return impl_->memory->tuningTargetSnapshot(); }
std::vector<MemoryCurrentSource> DevMemoryReporting::currentSourceSnapshot() const {
	std::scoped_lock lock(impl_->mutex); std::vector<MemoryCurrentSource> result; result.reserve(impl_->current.size());
	for (const auto& [_, value] : impl_->current) result.push_back(value);
	std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.key.source != b.key.source ? a.key.source < b.key.source : a.key.window < b.key.window; });
	return result;
}
std::vector<MemorySampleSegment> DevMemoryReporting::segmentSnapshot() const { std::scoped_lock lock(impl_->mutex); return impl_->segments.snapshot(); }
std::vector<RetainedMemoryEvent> DevMemoryReporting::eventSnapshot() const { std::scoped_lock lock(impl_->mutex); return impl_->events.snapshot(); }

std::optional<MemoryStatistics> DevMemoryReporting::statistics(const MemoryStatisticsQuery& query) const {
	std::vector<MemorySampleSegment> segments;
	MemoryRetentionStatus retention{};
	MemoryReportingConfig reportingConfig{};
	std::optional<MemoryCaptureInfo> capture;
	{
		std::scoped_lock lock(impl_->mutex); segments = impl_->segments.snapshot(); retention = impl_->segments.status();
		reportingConfig = impl_->reportingConfig;
		if (query.capture) for (const auto& candidate : impl_->captures) if (candidate.id == *query.capture) capture = candidate;
	}
	if (query.capture && !capture) return std::nullopt;
	if (!capture && segments.size() > reportingConfig.quantileWindowSegments)
		segments.erase(segments.begin(), segments.end() - reportingConfig.quantileWindowSegments);
	struct Weighted { uint64_t value; uint64_t weight; };
	std::vector<Weighted> values;
	long double weightedSum = 0.0; uint64_t totalWeight = 0u;
	uint64_t minimum = UINT64_MAX, maximum = 0u;
	for (const auto& segment : segments) {
		if (segment.value.source != query.source || segment.value.window != query.window) continue;
		AppTickId begin = segment.beginTick, end = segment.endTickExclusive;
		if (capture) {
			begin = std::max(begin, capture->measurementBeginTick);
			const AppTickId captureEnd = capture->endTickExclusive ? capture->endTickExclusive : UINT64_MAX;
			end = std::min(end, captureEnd);
		}
		if (end <= begin) continue;
		uint64_t weight = end - begin;
		if (query.weighting == MemoryWeighting::ElapsedTime) weight = std::max<uint64_t>(1u, segment.endNs - segment.beginNs);
		else if (query.weighting == MemoryWeighting::Epochs) weight = 1u;
		const uint64_t value = metricValue(segment.value, query.metric);
		values.push_back({value, weight}); totalWeight = saturatedAdd(totalWeight, weight);
		weightedSum += static_cast<long double>(value) * static_cast<long double>(weight);
		minimum = std::min(minimum, value); maximum = std::max(maximum, value);
	}
	if (values.empty() || totalWeight == 0u) return std::nullopt;
	std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) { return a.value < b.value; });
	MemoryStatistics result{.key = {query.source, query.window}, .metric = query.metric,
		.weighting = query.weighting, .observationCount = values.size(), .totalWeight = totalWeight,
		.minimum = minimum, .maximum = maximum, .mean = weightedSum / static_cast<long double>(totalWeight)};
	for (const double requested : query.percentiles) {
		const double percentile = std::clamp(requested, 0.0, 1.0);
		const uint64_t threshold = std::max<uint64_t>(1u, static_cast<uint64_t>(std::ceil(percentile * static_cast<double>(totalWeight))));
		uint64_t cumulative = 0u, selected = values.back().value;
		for (const auto& value : values) { cumulative = saturatedAdd(cumulative, value.weight); if (cumulative >= threshold) { selected = value.value; break; } }
		result.percentiles.push_back({percentile, selected});
	}
	if (capture && (!capture->complete || capture->firstSegmentSequence < retention.oldestRetainedSequence)) result.flags |= MemoryReportFlag::CaptureIncomplete;
	if (retention.overwriteCount) result.flags |= MemoryReportFlag::SegmentHistoryOverwritten;
	return result;
}

MemoryCapacityPreview DevMemoryReporting::previewCapacityProfile(const CapacityProfileRequest& request) const {
	MemoryCapacityPreview preview{};
	const auto targets = impl_->memory->tuningTargetSnapshot();
	auto captures = captureSnapshot();
	const auto captureIt = std::find_if(captures.begin(), captures.end(), [&](const auto& value) { return value.id == request.capture; });
	if (request.capture == 0u || captureIt == captures.end()) { preview.flags |= MemoryReportFlag::CaptureIncomplete; return preview; }
	preview.profile.growthFactor = std::max(1.1f, request.growthFactor);
	preview.profile.metadata.captureBeginTick = captureIt->beginTick;
	preview.profile.metadata.captureEndTickExclusive = captureIt->endTickExclusive;
	preview.profile.metadata.warmUpTicks = captureIt->warmUpTicks;
	preview.profile.metadata.complete = captureIt->complete;
#if defined(_WIN32)
	preview.profile.metadata.platform = "windows";
#elif defined(__APPLE__)
	preview.profile.metadata.platform = "macos";
#elif defined(__linux__)
	preview.profile.metadata.platform = "linux";
#else
	preview.profile.metadata.platform = "unknown";
#endif
	const auto allSegments = segmentSnapshot();
	if (request.growthPolicy == CapacityGrowthPolicy::PercentileInitialOneGrowthCoversMaximum) {
		for (const auto& target : targets) {
			auto stats = statistics(MemoryStatisticsQuery{.source = target.source,
				.metric = statisticMetric(target.metric), .capture = request.capture,
				.percentiles = {std::clamp(request.percentile, 0.0, 1.0)}});
			if (!stats || stats->percentiles.empty()) continue;
			const uint64_t proposed = alignedProposal(
				saturatedAdd(stats->percentiles.front().value, request.safetyMargin), target);
			if (proposed != 0u && stats->maximum > proposed) preview.profile.growthFactor = std::max(
				preview.profile.growthFactor,
				static_cast<float>(static_cast<double>(stats->maximum) / static_cast<double>(proposed)));
		}
	}
	for (const auto& target : targets) {
		MemoryStatisticsQuery query{.source = target.source, .metric = statisticMetric(target.metric),
			.capture = request.capture, .percentiles = {std::clamp(request.percentile, 0.0, 1.0)}};
		auto stats = statistics(query); if (!stats || stats->percentiles.empty()) continue;
		uint64_t proposed = alignedProposal(saturatedAdd(stats->percentiles.front().value, request.safetyMargin), target);
		std::vector<uint64_t> trace;
		uint64_t coveredWeight = 0u, totalWeight = 0u;
		for (const auto& segment : allSegments) if (segment.value.source == target.source &&
			segment.value.window == InvalidWindowId && segment.endTickExclusive > captureIt->measurementBeginTick &&
			(captureIt->endTickExclusive == 0u || segment.beginTick < captureIt->endTickExclusive)) {
			const AppTickId begin = std::max(segment.beginTick, captureIt->measurementBeginTick);
			const AppTickId end = std::min(segment.endTickExclusive,
				captureIt->endTickExclusive == 0u ? UINT64_MAX : captureIt->endTickExclusive);
			const uint64_t weight = end > begin ? end - begin : 0u;
			const uint64_t demand = metricValue(segment.value, statisticMetric(target.metric));
			trace.push_back(demand); totalWeight = saturatedAdd(totalWeight, weight);
			if (demand <= proposed) coveredWeight = saturatedAdd(coveredWeight, weight);
		}
		const float targetFactor = preview.profile.growthFactor;
		MemoryCapacityRecommendation recommendation{.target = target, .observedMinimum = stats->minimum,
			.observedMaximum = stats->maximum, .percentileDemand = stats->percentiles.front().value,
			.proposedInitialCapacity = proposed,
			.deltaFromProductionDefault = proposed >= target.productionDefault
				? static_cast<int64_t>(std::min<uint64_t>(proposed - target.productionDefault, INT64_MAX))
				: -static_cast<int64_t>(std::min<uint64_t>(target.productionDefault - proposed, INT64_MAX)),
			.predictedNoGrowthCoverage = static_cast<double>(stats->totalWeight == 0u ? 0u : 1u),
			.simulation = simulateMemoryGrowth(trace, proposed, targetFactor, target.alignment),
			.flags = stats->flags};
		recommendation.predictedNoGrowthCoverage = totalWeight == 0u ? 0.0
			: static_cast<double>(coveredWeight) / static_cast<double>(totalWeight);
		recommendation.simulation.initialCoverage = recommendation.predictedNoGrowthCoverage;
		if (recommendation.simulation.growthCount) recommendation.flags |= MemoryReportFlag::GrewDuringCapture;
		preview.predictedStartupBytes = saturatedAdd(preview.predictedStartupBytes,
			target.unit == MemoryCapacityUnit::Bytes ? proposed : 0u);
		if (!assignTypedCapacity(preview.profile, target.id, proposed))
			preview.profile.settings.push_back({target.id, proposed});
		preview.flags |= recommendation.flags;
		preview.recommendations.push_back(std::move(recommendation));
	}
	if (!captureIt->complete) preview.flags |= MemoryReportFlag::CaptureIncomplete;
	return preview;
}

std::optional<::FlowUi::detail::storage::StorageMemorySnapshot> DevMemoryReporting::storageSnapshot() const {
	std::scoped_lock lock(impl_->mutex); if (!impl_->status.hasStorageSnapshot) return std::nullopt; return impl_->storageSnapshot;
}
std::vector<MemoryValueSample> DevMemoryReporting::managerSamples() const { std::scoped_lock lock(impl_->mutex); return impl_->managerSamples; }
std::optional<MemoryEnvironmentSnapshot> DevMemoryReporting::environmentSnapshot() const { std::scoped_lock lock(impl_->mutex); return impl_->environmentSnapshot; }
} // namespace FlowUi::devSystems
#endif
