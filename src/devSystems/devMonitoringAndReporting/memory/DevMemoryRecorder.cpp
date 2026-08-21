#include "devSystems/devMonitoringAndReporting/memory/DevMemoryRecorder.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <atomic>
#include <mutex>

namespace FlowUi::devSystems {

struct DevMemoryRecorder::Impl {
	explicit Impl(uint32_t requestedCapacity)
		: records(std::max(1u, requestedCapacity)) {}

	mutable std::mutex mutex{};
	std::vector<MemoryOperationRecord> records{};
	uint32_t begin = 0u;
	uint32_t count = 0u;
	std::atomic<uint64_t> recorded{0u};
	std::atomic<uint64_t> suppressed{0u};
	std::atomic<uint64_t> dropped{0u};
	std::atomic<uint8_t> runtimeLevel{
		static_cast<uint8_t>(MemoryMonitoringLevel::SubsystemCapacity)};
	std::atomic<AppTickId> appTick{0u};
};

DevMemoryRecorder::DevMemoryRecorder(uint32_t capacity)
	: impl_(std::make_unique<Impl>(capacity)) {}

DevMemoryRecorder::~DevMemoryRecorder() = default;

bool DevMemoryRecorder::tryRecord(const MemoryOperationRecord& record) noexcept {
	if (static_cast<uint8_t>(record.detailLevel) >
		impl_->runtimeLevel.load(std::memory_order_relaxed)) {
		noteSuppressed();
		return false;
	}
	try {
		std::scoped_lock lock(impl_->mutex);
		if (impl_->count == impl_->records.size()) {
			impl_->dropped.fetch_add(1u, std::memory_order_relaxed);
			return false;
		}
		const uint32_t index = static_cast<uint32_t>(
			(impl_->begin + impl_->count) % impl_->records.size());
		MemoryOperationRecord enriched = record;
		if (enriched.appTick == 0u) {
			enriched.appTick = impl_->appTick.load(std::memory_order_relaxed);
		}
		impl_->records[index] = enriched;
		++impl_->count;
		impl_->recorded.fetch_add(1u, std::memory_order_relaxed);
		return true;
	} catch (...) {
		impl_->dropped.fetch_add(1u, std::memory_order_relaxed);
		return false;
	}
}

void DevMemoryRecorder::setAppTickContext(AppTickId appTick) noexcept {
	impl_->appTick.store(appTick, std::memory_order_relaxed);
}

void DevMemoryRecorder::setLevel(MemoryMonitoringLevel level) noexcept {
	impl_->runtimeLevel.store(static_cast<uint8_t>(level), std::memory_order_relaxed);
}

void DevMemoryRecorder::noteSuppressed() noexcept {
	impl_->suppressed.fetch_add(1u, std::memory_order_relaxed);
}

void DevMemoryRecorder::drainInto(std::vector<MemoryOperationRecord>& destination) {
	std::scoped_lock lock(impl_->mutex);
	destination.reserve(destination.size() + impl_->count);
	for (uint32_t offset = 0u; offset < impl_->count; ++offset) {
		const uint32_t index = static_cast<uint32_t>(
			(impl_->begin + offset) % impl_->records.size());
		destination.push_back(impl_->records[index]);
	}
	impl_->begin = 0u;
	impl_->count = 0u;
}

MemoryQualitySnapshot DevMemoryRecorder::qualitySnapshot() const noexcept {
	return MemoryQualitySnapshot{
		.recordedOperations = impl_->recorded.load(std::memory_order_relaxed),
		.suppressedOperations = impl_->suppressed.load(std::memory_order_relaxed),
		.droppedOperations = impl_->dropped.load(std::memory_order_relaxed),
	};
}

uint32_t DevMemoryRecorder::capacity() const noexcept {
	return static_cast<uint32_t>(impl_->records.size());
}

} // namespace FlowUi::devSystems

#endif
