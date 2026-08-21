#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace FlowUi::devSystems {

namespace {

constexpr uint16_t kMaximumActiveZoneDepth = 256u;

struct ActiveCpuZone {
	TimingInvocationId invocationId = 0u;
	TimingInvocationId parentInvocationId = 0u;
	TimingZoneTypeId typeId = 0u;
	uint64_t startNs = 0u;
	uint64_t directChildNs = 0u;
	WindowFrameKey frame{};
	AppTickId appTick = 0u;
	TimingEntityRef entity{};
	uint8_t depth = 0u;
};

struct ElementAggregateKey {
	FlowDefinitionID definition{};
	WindowFrameKey frame{};
	AppTickId appTick = 0u;

	bool operator==(const ElementAggregateKey&) const noexcept = default;
};

struct ElementAggregateKeyHash {
	[[nodiscard]] size_t operator()(const ElementAggregateKey& key) const noexcept {
		size_t hash = static_cast<size_t>(key.definition.value);
		hash ^= static_cast<size_t>(key.frame.window) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
		hash ^= static_cast<size_t>(key.frame.frameNumber) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
		hash ^= static_cast<size_t>(key.appTick) + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
		return hash;
	}
};

void addSaturated(uint64_t& destination, uint64_t value) noexcept {
	if (std::numeric_limits<uint64_t>::max() - destination < value) {
		destination = std::numeric_limits<uint64_t>::max();
		return;
	}
	destination += value;
}

} // namespace

struct DevTimingRecorder::Impl {
	Impl(
		DevTiming& timingOwner,
		TimingTrackId timingTrack,
		std::string_view name,
		uint32_t requestedCapacity)
		: owner(&timingOwner),
		  track(timingTrack),
		  trackName(name),
		  producerThread(std::this_thread::get_id()),
		  records(std::max(64u, requestedCapacity)) {
		cachedConfig = owner->recorderConfig(cachedConfigGeneration);
	}

	[[nodiscard]] bool isEnabled(const TimingZoneDescriptor& descriptor) noexcept {
		const uint64_t currentGeneration = owner->configGeneration();
		if (currentGeneration != cachedConfigGeneration) {
			cachedConfig = owner->recorderConfig(cachedConfigGeneration);
		}
		return static_cast<uint8_t>(cachedConfig.cpuLevel) >=
				static_cast<uint8_t>(descriptor.minimumCpuLevel) &&
			(cachedConfig.enabledCategoryMask & timingCategoryBit(descriptor.category)) != 0u;
	}

	[[nodiscard]] TimingInvocationId nextInvocationId() noexcept {
		if (invocationIdsExhausted) return 0u;
		const uint32_t local = nextLocalInvocation++;
		if (local == std::numeric_limits<uint32_t>::max()) invocationIdsExhausted = true;
		return (static_cast<uint64_t>(track) << 32u) | local;
	}

	void append(const CpuTimingRecord& record) noexcept {
		const uint64_t write = writeSequence.load(std::memory_order_relaxed);
		const uint64_t read = readSequence.load(std::memory_order_acquire);
		if (write - read >= records.size()) {
			droppedRecords.fetch_add(1u, std::memory_order_relaxed);
			return;
		}
		records[write % records.size()] = record;
		writeSequence.store(write + 1u, std::memory_order_release);
		recordedZones.fetch_add(1u, std::memory_order_relaxed);
	}

	void addTimingOverhead(uint64_t startNs) noexcept {
		const uint64_t endNs = owner->nowNs();
		if (endNs >= startNs) {
			timingOverheadNs.fetch_add(endNs - startNs, std::memory_order_relaxed);
		}
	}

	[[nodiscard]] CpuTimingRecord closeTop(
		TimingRecordFlag result,
		uint64_t endNs,
		bool retainRecord = true) noexcept {
		if (activeCount == 0u) return {};
		ActiveCpuZone active = activeZones[activeCount - 1u];
		--activeCount;

		uint64_t durationNs = 0u;
		uint16_t flags = timingRecordFlags(result);
		if (endNs >= active.startNs) {
			durationNs = endNs - active.startNs;
		} else {
			flags |= timingRecordFlags(TimingRecordFlag::ClockAnomaly);
			clockAnomalies.fetch_add(1u, std::memory_order_relaxed);
		}
		if (activeCount > 0u) {
			addSaturated(activeZones[activeCount - 1u].directChildNs, durationNs);
		}

		CpuTimingRecord record{
			.startNs = active.startNs,
			.durationNs = durationNs,
			.directChildNs = std::min(active.directChildNs, durationNs),
			.invocationId = active.invocationId,
			.parentInvocationId = active.parentInvocationId,
			.typeId = active.typeId,
			.frame = active.frame,
			.appTick = active.appTick,
			.primaryEntityId = active.entity.primaryId,
			.secondaryEntityId = active.entity.secondaryId,
			.track = track,
			.entityKind = active.entity.kind,
			.depth = active.depth,
			.flags = flags,
		};
		if (retainRecord) append(record);
		return record;
	}

	void flushIncomplete() noexcept {
		const uint64_t endNs = owner->nowNs();
		while (activeCount > 0u) {
			incompleteZones.fetch_add(1u, std::memory_order_relaxed);
			(void)closeTop(TimingRecordFlag::Incomplete, endNs);
		}
	}

	DevTiming* owner = nullptr;
	TimingTrackId track = 0u;
	std::string trackName{};
	std::thread::id producerThread{};
	bool attached = true;
	DevTimingConfig cachedConfig{};
	uint64_t cachedConfigGeneration = 0u;
	WindowFrameKey currentFrame{};
	AppTickId currentAppTick = 0u;
	uint32_t nextLocalInvocation = 1u;
	bool invocationIdsExhausted = false;
	std::array<ActiveCpuZone, kMaximumActiveZoneDepth> activeZones{};
	uint16_t activeCount = 0u;
	std::vector<CpuTimingRecord> records{};
	std::atomic<uint64_t> writeSequence{0u};
	std::atomic<uint64_t> readSequence{0u};
	std::atomic<uint64_t> recordedZones{0u};
	std::atomic<uint64_t> suppressedZones{0u};
	std::atomic<uint64_t> droppedRecords{0u};
	std::atomic<uint64_t> stackOverflows{0u};
	std::atomic<uint64_t> misnestedZones{0u};
	std::atomic<uint64_t> incompleteZones{0u};
	std::atomic<uint64_t> clockAnomalies{0u};
	std::atomic<uint64_t> timingOverheadNs{0u};
	std::unordered_set<TimingZoneTypeId> registeredDescriptors{};
	std::unordered_map<ElementAggregateKey, ElementDefinitionTimingAggregate, ElementAggregateKeyHash>
		elementAggregates{};
};

DevTimingRecorder::DevTimingRecorder(
	DevTiming& owner,
	TimingTrackId track,
	std::string_view trackName,
	uint32_t recordCapacity)
	: impl_(std::make_unique<Impl>(owner, track, trackName, recordCapacity)) {}

DevTimingRecorder::~DevTimingRecorder() = default;

ActiveZoneToken DevTimingRecorder::tryBegin(
	const TimingZoneDescriptor& descriptor,
	TimingEntityRef entity) noexcept {
	const uint64_t overheadStartNs = impl_->owner->nowNs();
	if (!impl_->attached || !impl_->isEnabled(descriptor)) {
		impl_->suppressedZones.fetch_add(1u, std::memory_order_relaxed);
		impl_->addTimingOverhead(overheadStartNs);
		return {};
	}
	if (impl_->activeCount >= kMaximumActiveZoneDepth) {
		impl_->stackOverflows.fetch_add(1u, std::memory_order_relaxed);
		impl_->addTimingOverhead(overheadStartNs);
		return {};
	}

	try {
		if (impl_->registeredDescriptors.insert(descriptor.typeId).second) {
			impl_->owner->registerDescriptor(descriptor);
		}
	} catch (...) {
		// Descriptor caching is metadata-only and must not alter application flow.
	}
	const TimingInvocationId invocationId = impl_->nextInvocationId();
	if (invocationId == 0u) {
		impl_->addTimingOverhead(overheadStartNs);
		return {};
	}
	const uint16_t stackIndex = impl_->activeCount;
	const TimingInvocationId parentId = stackIndex > 0u
		? impl_->activeZones[stackIndex - 1u].invocationId
		: 0u;
	impl_->activeZones[stackIndex] = ActiveCpuZone{
		.invocationId = invocationId,
		.parentInvocationId = parentId,
		.typeId = descriptor.typeId,
		.startNs = impl_->owner->nowNs(),
		.frame = impl_->currentFrame,
		.appTick = impl_->currentAppTick,
		.entity = entity,
		.depth = static_cast<uint8_t>(stackIndex),
	};
	++impl_->activeCount;
	const ActiveZoneToken result{
		.invocationId = invocationId,
		.stackIndex = stackIndex,
		.active = true,
	};
	impl_->addTimingOverhead(overheadStartNs);
	return result;
}

void DevTimingRecorder::end(ActiveZoneToken token, TimingRecordFlag result) noexcept {
	if (!token) return;
	const uint64_t overheadStartNs = impl_->owner->nowNs();
	if (impl_->activeCount == 0u ||
		token.stackIndex != impl_->activeCount - 1u ||
		impl_->activeZones[impl_->activeCount - 1u].invocationId != token.invocationId) {
		impl_->misnestedZones.fetch_add(1u, std::memory_order_relaxed);
		impl_->flushIncomplete();
		impl_->addTimingOverhead(overheadStartNs);
		return;
	}
	if ((timingRecordFlags(result) & timingRecordFlags(TimingRecordFlag::Incomplete)) != 0u) {
		impl_->incompleteZones.fetch_add(1u, std::memory_order_relaxed);
	}
	(void)impl_->closeTop(result, impl_->owner->nowNs());
	impl_->addTimingOverhead(overheadStartNs);
}

void DevTimingRecorder::endElement(
	ActiveZoneToken token,
	FlowDefinitionID definition,
	TimingRecordFlag result) noexcept {
	if (!token) return;
	const uint64_t overheadStartNs = impl_->owner->nowNs();
	if (impl_->activeCount == 0u ||
		token.stackIndex != impl_->activeCount - 1u ||
		impl_->activeZones[impl_->activeCount - 1u].invocationId != token.invocationId) {
		impl_->misnestedZones.fetch_add(1u, std::memory_order_relaxed);
		impl_->flushIncomplete();
		impl_->addTimingOverhead(overheadStartNs);
		return;
	}
	if ((timingRecordFlags(result) & timingRecordFlags(TimingRecordFlag::Incomplete)) != 0u) {
		impl_->incompleteZones.fetch_add(1u, std::memory_order_relaxed);
	}
	const uint64_t endNs = impl_->owner->nowNs();
	const ActiveCpuZone& active = impl_->activeZones[impl_->activeCount - 1u];
	const uint64_t durationNs = endNs >= active.startNs ? endNs - active.startNs : 0u;
	const CpuTimingLevel level = impl_->cachedConfig.cpuLevel;
	const bool selected =
		impl_->cachedConfig.selectedElementDefinition == definition ||
		impl_->cachedConfig.selectedElementInstance.value == active.entity.primaryId;
	const bool retain = level == CpuTimingLevel::Deep ||
		(level == CpuTimingLevel::Balanced &&
			(selected || durationNs >= impl_->cachedConfig.balancedElementRetentionThresholdNs));
	const CpuTimingRecord record = impl_->closeTop(result, endNs, retain);
	try {
		const ElementAggregateKey aggregateKey{definition, active.frame, active.appTick};
		auto& aggregate = impl_->elementAggregates[aggregateKey];
		aggregate.definition = definition;
		aggregate.frame = active.frame;
		aggregate.appTick = active.appTick;
		++aggregate.invocationCount;
		addSaturated(aggregate.totalInclusiveNs, record.durationNs);
		aggregate.maximumInclusiveNs = std::max(aggregate.maximumInclusiveNs, record.durationNs);
		if ((timingRecordFlags(result) & timingRecordFlags(TimingRecordFlag::Completed)) == 0u) {
			++aggregate.canceledInvocationCount;
		}
	} catch (...) {
		impl_->droppedRecords.fetch_add(1u, std::memory_order_relaxed);
	}
	impl_->addTimingOverhead(overheadStartNs);
}

void DevTimingRecorder::setFrameContext(WindowFrameKey frame, AppTickId appTick) noexcept {
	impl_->currentFrame = frame;
	impl_->currentAppTick = appTick;
}

void DevTimingRecorder::clearFrameContext() noexcept {
	impl_->currentFrame = {};
	impl_->currentAppTick = 0u;
}

TimingTrackId DevTimingRecorder::trackId() const noexcept { return impl_->track; }
std::string_view DevTimingRecorder::trackName() const noexcept { return impl_->trackName; }

void DevTimingRecorder::detachCurrentThread() noexcept {
	if (!impl_->attached) return;
	if (std::this_thread::get_id() != impl_->producerThread) {
		impl_->misnestedZones.fetch_add(1u, std::memory_order_relaxed);
	}
	impl_->flushIncomplete();
	impl_->attached = false;
}

void DevTimingRecorder::drainInto(std::vector<CpuTimingRecord>& output) {
	uint64_t read = impl_->readSequence.load(std::memory_order_relaxed);
	const uint64_t write = impl_->writeSequence.load(std::memory_order_acquire);
	if (write <= read) return;
	output.reserve(output.size() + static_cast<size_t>(write - read));
	while (read < write) {
		output.push_back(impl_->records[read % impl_->records.size()]);
		++read;
	}
	impl_->readSequence.store(read, std::memory_order_release);
}

void DevTimingRecorder::drainElementAggregatesInto(
	std::vector<ElementDefinitionTimingAggregate>& output) {
	output.reserve(output.size() + impl_->elementAggregates.size());
	for (const auto& [_, aggregate] : impl_->elementAggregates) output.push_back(aggregate);
	impl_->elementAggregates.clear();
}

TimingQualitySnapshot DevTimingRecorder::qualitySnapshot() const noexcept {
	return TimingQualitySnapshot{
		.recordedZones = impl_->recordedZones.load(std::memory_order_relaxed),
		.suppressedZones = impl_->suppressedZones.load(std::memory_order_relaxed),
		.droppedRecords = impl_->droppedRecords.load(std::memory_order_relaxed),
		.stackOverflows = impl_->stackOverflows.load(std::memory_order_relaxed),
		.misnestedZones = impl_->misnestedZones.load(std::memory_order_relaxed),
		.incompleteZones = impl_->incompleteZones.load(std::memory_order_relaxed),
		.clockAnomalies = impl_->clockAnomalies.load(std::memory_order_relaxed),
		.timingOverheadNs = impl_->timingOverheadNs.load(std::memory_order_relaxed),
	};
}

DevTimingThreadAttachment::~DevTimingThreadAttachment() { reset(); }

DevTimingThreadAttachment::DevTimingThreadAttachment(
	DevTimingThreadAttachment&& other) noexcept
	: recorder_(std::exchange(other.recorder_, nullptr)) {}

DevTimingThreadAttachment& DevTimingThreadAttachment::operator=(
	DevTimingThreadAttachment&& other) noexcept {
	if (this == &other) return *this;
	reset();
	recorder_ = std::exchange(other.recorder_, nullptr);
	return *this;
}

void DevTimingThreadAttachment::reset() noexcept {
	if (!recorder_) return;
	recorder_->detachCurrentThread();
	recorder_ = nullptr;
}

} // namespace FlowUi::devSystems

#endif
