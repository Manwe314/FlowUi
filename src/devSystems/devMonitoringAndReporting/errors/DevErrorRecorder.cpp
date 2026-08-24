#include "devSystems/devMonitoringAndReporting/errors/DevErrorRecorder.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>

#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"

namespace FlowUi::devSystems {

struct DevErrorRecorder::Impl {
	Impl(
		DevErrorMonitoring& recorderOwner,
		uint32_t recorderTrack,
		std::string_view recorderName,
		const DevErrorConfig& config)
		: owner(&recorderOwner), track(recorderTrack), name(recorderName),
		  records(std::max(1u, config.producerRecordCapacity)),
		  breadcrumbs(std::max(1u, config.breadcrumbCapacity)),
		  recentBreadcrumbCount(std::max(1u, config.recentBreadcrumbCount)) {}

	DevErrorMonitoring* owner = nullptr;
	uint32_t track = 0u;
	std::string name{};
	mutable std::mutex mutex{};
	DevErrorContext currentContext{};
	std::atomic<uint64_t> contextSequence{0u};
	std::atomic<AppTickId> emergencyAppTick{0u};
	std::atomic<WindowId> emergencyWindow{InvalidWindowId};
	std::atomic<uint64_t> emergencyFrame{0u};
	std::atomic<uint64_t> emergencySubmission{0u};
	std::atomic<uint64_t> emergencyPrimary{0u};
	std::atomic<uint64_t> emergencySecondary{0u};
	std::atomic<uint32_t> emergencyTimingTrack{0u};
	std::atomic<uint16_t> emergencyPhase{0u};
	std::vector<DevErrorRecord> records{};
	std::vector<DevErrorBreadcrumb> breadcrumbs{};
	uint32_t recentBreadcrumbCount = 1u;
	uint32_t recordBegin = 0u;
	uint32_t recordCount = 0u;
	uint32_t breadcrumbBegin = 0u;
	uint32_t breadcrumbCount = 0u;
	std::atomic<uint64_t> recordedEvents{0u};
	std::atomic<uint64_t> recordedBreadcrumbs{0u};
	std::atomic<uint64_t> suppressedEvents{0u};
	std::atomic<uint64_t> droppedEvents{0u};
	std::atomic<uint64_t> droppedBreadcrumbs{0u};
	std::atomic<uint64_t> overwrittenBreadcrumbs{0u};
	std::atomic<uint64_t> recursiveEvents{0u};
	std::atomic<uint64_t> nativeTextTruncations{0u};

	void publishContext(const DevErrorContext& context) noexcept {
		contextSequence.fetch_add(1u, std::memory_order_acq_rel);
		emergencyAppTick.store(context.appTick, std::memory_order_relaxed);
		emergencyWindow.store(context.frame.window, std::memory_order_relaxed);
		emergencyFrame.store(context.frame.frameNumber, std::memory_order_relaxed);
		emergencySubmission.store(context.submissionSerial, std::memory_order_relaxed);
		emergencyPrimary.store(context.primaryEntity, std::memory_order_relaxed);
		emergencySecondary.store(context.secondaryEntity, std::memory_order_relaxed);
		emergencyTimingTrack.store(context.timingTrack, std::memory_order_relaxed);
		emergencyPhase.store(context.phase, std::memory_order_relaxed);
		contextSequence.fetch_add(1u, std::memory_order_release);
	}
};

DevErrorRecorder::DevErrorRecorder(
	DevErrorMonitoring& owner,
	uint32_t track,
	std::string_view name,
	const DevErrorConfig& config)
	: impl_(std::make_unique<Impl>(owner, track, name, config)) {}

DevErrorRecorder::~DevErrorRecorder() = default;

void DevErrorRecorder::setContext(const DevErrorContext& context) noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		impl_->currentContext = context;
		impl_->publishContext(context);
	} catch (...) {
	}
}

void DevErrorRecorder::clearFrameContext() noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		impl_->currentContext.frame = {};
		impl_->currentContext.submissionSerial = 0u;
		impl_->currentContext.primaryEntity = 0u;
		impl_->currentContext.secondaryEntity = 0u;
		impl_->currentContext.phase = 0u;
		impl_->publishContext(impl_->currentContext);
	} catch (...) {
	}
}

DevErrorContext DevErrorRecorder::context() const noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		return impl_->currentContext;
	} catch (...) {
		return {};
	}
}

DevErrorContext DevErrorRecorder::emergencyContext() const noexcept {
	for (uint32_t attempt = 0u; attempt < 3u; ++attempt) {
		const uint64_t before = impl_->contextSequence.load(std::memory_order_acquire);
		if ((before & 1u) != 0u) continue;
		const DevErrorContext result{
			.appTick = impl_->emergencyAppTick.load(std::memory_order_relaxed),
			.frame = WindowFrameKey{
				impl_->emergencyWindow.load(std::memory_order_relaxed),
				impl_->emergencyFrame.load(std::memory_order_relaxed),
			},
			.submissionSerial = impl_->emergencySubmission.load(std::memory_order_relaxed),
			.primaryEntity = impl_->emergencyPrimary.load(std::memory_order_relaxed),
			.secondaryEntity = impl_->emergencySecondary.load(std::memory_order_relaxed),
			.timingTrack = impl_->emergencyTimingTrack.load(std::memory_order_relaxed),
			.phase = impl_->emergencyPhase.load(std::memory_order_relaxed),
		};
		const uint64_t after = impl_->contextSequence.load(std::memory_order_acquire);
		if (before == after) return result;
	}
	return {};
}

uint32_t DevErrorRecorder::trackId() const noexcept { return impl_->track; }
std::string_view DevErrorRecorder::trackName() const noexcept { return impl_->name; }

bool DevErrorRecorder::tryRecord(DevErrorRecord record) noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		if (impl_->recordCount == impl_->records.size()) {
			impl_->droppedEvents.fetch_add(1u, std::memory_order_relaxed);
			return false;
		}
		if (record.context.appTick == 0u) record.context = impl_->currentContext;
		if (record.breadcrumbEnd == 0u && impl_->breadcrumbCount > 0u) {
			const uint32_t retainedCount = std::min(
				impl_->breadcrumbCount, impl_->recentBreadcrumbCount);
			const uint32_t firstOffset = impl_->breadcrumbCount - retainedCount;
			const uint32_t firstIndex = static_cast<uint32_t>(
				(impl_->breadcrumbBegin + firstOffset) % impl_->breadcrumbs.size());
			const uint32_t lastIndex = static_cast<uint32_t>(
				(impl_->breadcrumbBegin + impl_->breadcrumbCount - 1u) % impl_->breadcrumbs.size());
			record.breadcrumbEnd = impl_->breadcrumbs[lastIndex].sequence;
			record.breadcrumbBegin = impl_->breadcrumbs[firstIndex].sequence;
		}
		record.threadTrack = impl_->track;
		const uint32_t index = static_cast<uint32_t>(
			(impl_->recordBegin + impl_->recordCount) % impl_->records.size());
		impl_->records[index] = record;
		++impl_->recordCount;
		impl_->recordedEvents.fetch_add(1u, std::memory_order_relaxed);
		return true;
	} catch (...) {
		impl_->droppedEvents.fetch_add(1u, std::memory_order_relaxed);
		return false;
	}
}

bool DevErrorRecorder::tryBreadcrumb(DevErrorBreadcrumb breadcrumb) noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		if (impl_->breadcrumbCount == impl_->breadcrumbs.size()) {
			impl_->breadcrumbBegin = static_cast<uint32_t>(
				(impl_->breadcrumbBegin + 1u) % impl_->breadcrumbs.size());
			--impl_->breadcrumbCount;
			impl_->overwrittenBreadcrumbs.fetch_add(1u, std::memory_order_relaxed);
		}
		if (breadcrumb.context.appTick == 0u) breadcrumb.context = impl_->currentContext;
		breadcrumb.threadTrack = impl_->track;
		const uint32_t index = static_cast<uint32_t>(
			(impl_->breadcrumbBegin + impl_->breadcrumbCount) % impl_->breadcrumbs.size());
		impl_->breadcrumbs[index] = breadcrumb;
		++impl_->breadcrumbCount;
		impl_->recordedBreadcrumbs.fetch_add(1u, std::memory_order_relaxed);
		return true;
	} catch (...) {
		impl_->droppedBreadcrumbs.fetch_add(1u, std::memory_order_relaxed);
		return false;
	}
}

void DevErrorRecorder::drainRecordsInto(std::vector<DevErrorRecord>& destination) {
	std::scoped_lock lock(impl_->mutex);
	destination.reserve(destination.size() + impl_->recordCount);
	for (uint32_t offset = 0u; offset < impl_->recordCount; ++offset) {
		const uint32_t index = static_cast<uint32_t>(
			(impl_->recordBegin + offset) % impl_->records.size());
		destination.push_back(impl_->records[index]);
	}
	impl_->recordBegin = 0u;
	impl_->recordCount = 0u;
}

void DevErrorRecorder::drainBreadcrumbsInto(
	std::vector<DevErrorBreadcrumb>& destination) {
	std::scoped_lock lock(impl_->mutex);
	destination.reserve(destination.size() + impl_->breadcrumbCount);
	for (uint32_t offset = 0u; offset < impl_->breadcrumbCount; ++offset) {
		const uint32_t index = static_cast<uint32_t>(
			(impl_->breadcrumbBegin + offset) % impl_->breadcrumbs.size());
		destination.push_back(impl_->breadcrumbs[index]);
	}
	impl_->breadcrumbBegin = 0u;
	impl_->breadcrumbCount = 0u;
}

DevErrorQualitySnapshot DevErrorRecorder::qualitySnapshot() const noexcept {
	DevErrorQualitySnapshot result{
		.recordedEvents = impl_->recordedEvents.load(std::memory_order_relaxed),
		.recordedBreadcrumbs = impl_->recordedBreadcrumbs.load(std::memory_order_relaxed),
		.suppressedEvents = impl_->suppressedEvents.load(std::memory_order_relaxed),
		.droppedEvents = impl_->droppedEvents.load(std::memory_order_relaxed),
		.droppedBreadcrumbs = impl_->droppedBreadcrumbs.load(std::memory_order_relaxed),
		.overwrittenBreadcrumbs = impl_->overwrittenBreadcrumbs.load(std::memory_order_relaxed),
		.recursiveEvents = impl_->recursiveEvents.load(std::memory_order_relaxed),
		.nativeTextTruncations = impl_->nativeTextTruncations.load(std::memory_order_relaxed),
	};
	result.overhead.recordedBytes =
		result.recordedEvents * sizeof(DevErrorRecord) +
		result.recordedBreadcrumbs * sizeof(DevErrorBreadcrumb);
	return result;
}

void DevErrorRecorder::noteSuppressed() noexcept {
	impl_->suppressedEvents.fetch_add(1u, std::memory_order_relaxed);
}

void DevErrorRecorder::noteRecursive() noexcept {
	impl_->recursiveEvents.fetch_add(1u, std::memory_order_relaxed);
}

void DevErrorRecorder::noteNativeTextTruncation() noexcept {
	impl_->nativeTextTruncations.fetch_add(1u, std::memory_order_relaxed);
}

DevErrorThreadAttachment::~DevErrorThreadAttachment() { reset(); }

DevErrorThreadAttachment::DevErrorThreadAttachment(
	DevErrorThreadAttachment&& other) noexcept
	: recorder_(other.recorder_) {
	other.recorder_ = nullptr;
}

DevErrorThreadAttachment& DevErrorThreadAttachment::operator=(
	DevErrorThreadAttachment&& other) noexcept {
	if (this == &other) return *this;
	reset();
	recorder_ = other.recorder_;
	other.recorder_ = nullptr;
	return *this;
}

void DevErrorThreadAttachment::reset() noexcept {
	if (!recorder_) return;
	recorder_->impl_->owner->detach(*recorder_);
	recorder_ = nullptr;
}

} // namespace FlowUi::devSystems

#endif
