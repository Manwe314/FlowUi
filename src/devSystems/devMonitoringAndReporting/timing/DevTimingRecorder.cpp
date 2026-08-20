#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <string>
#include <thread>
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

	void closeTop(TimingRecordFlag result, uint64_t endNs) noexcept {
		if (activeCount == 0u) return;
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

		append(CpuTimingRecord{
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
		});
	}

	void flushIncomplete() noexcept {
		const uint64_t endNs = owner->nowNs();
		while (activeCount > 0u) {
			incompleteZones.fetch_add(1u, std::memory_order_relaxed);
			closeTop(TimingRecordFlag::Incomplete, endNs);
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
	std::unordered_set<TimingZoneTypeId> registeredDescriptors{};
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
	if (!impl_->attached || !impl_->isEnabled(descriptor)) {
		impl_->suppressedZones.fetch_add(1u, std::memory_order_relaxed);
		return {};
	}
	if (impl_->activeCount >= kMaximumActiveZoneDepth) {
		impl_->stackOverflows.fetch_add(1u, std::memory_order_relaxed);
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
	if (invocationId == 0u) return {};
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
	return ActiveZoneToken{
		.invocationId = invocationId,
		.stackIndex = stackIndex,
		.active = true,
	};
}

void DevTimingRecorder::end(ActiveZoneToken token, TimingRecordFlag result) noexcept {
	if (!token) return;
	if (impl_->activeCount == 0u ||
		token.stackIndex != impl_->activeCount - 1u ||
		impl_->activeZones[impl_->activeCount - 1u].invocationId != token.invocationId) {
		impl_->misnestedZones.fetch_add(1u, std::memory_order_relaxed);
		impl_->flushIncomplete();
		return;
	}
	if ((timingRecordFlags(result) & timingRecordFlags(TimingRecordFlag::Incomplete)) != 0u) {
		impl_->incompleteZones.fetch_add(1u, std::memory_order_relaxed);
	}
	impl_->closeTop(result, impl_->owner->nowNs());
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

TimingQualitySnapshot DevTimingRecorder::qualitySnapshot() const noexcept {
	return TimingQualitySnapshot{
		.recordedZones = impl_->recordedZones.load(std::memory_order_relaxed),
		.suppressedZones = impl_->suppressedZones.load(std::memory_order_relaxed),
		.droppedRecords = impl_->droppedRecords.load(std::memory_order_relaxed),
		.stackOverflows = impl_->stackOverflows.load(std::memory_order_relaxed),
		.misnestedZones = impl_->misnestedZones.load(std::memory_order_relaxed),
		.incompleteZones = impl_->incompleteZones.load(std::memory_order_relaxed),
		.clockAnomalies = impl_->clockAnomalies.load(std::memory_order_relaxed),
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
