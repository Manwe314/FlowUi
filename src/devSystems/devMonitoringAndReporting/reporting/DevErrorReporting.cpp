#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <chrono>
#include <limits>
#include <mutex>
#include <utility>

#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"

namespace FlowUi::devSystems {
namespace {

uint64_t steadyNowNs() noexcept {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
}

uint64_t retainedBytes(const DevErrorOccurrence& occurrence) noexcept {
	return sizeof(DevErrorOccurrence) + occurrence.steps.size() * sizeof(DevErrorRecord) +
		occurrence.breadcrumbs.size() * sizeof(DevErrorBreadcrumb) +
		occurrence.snapshots.size() * sizeof(DevErrorSnapshot) +
		occurrence.advice.size() * sizeof(DevErrorAdviceResult);
}

bool isFinalRecord(const DevErrorRecord& record) noexcept {
	const uint16_t flags = static_cast<uint16_t>(record.flags);
	return record.kind == DevErrorStepKind::Resolution ||
		record.kind == DevErrorStepKind::FatalCapture ||
		record.kind == DevErrorStepKind::BackendDiagnostic ||
		(flags & static_cast<uint16_t>(DevErrorRecordFlag::ExternallyReported)) != 0u ||
		(flags & static_cast<uint16_t>(DevErrorRecordFlag::DevOnlyDiagnostic)) != 0u;
}

DevErrorGroupKey groupKey(const DevErrorOccurrence& occurrence, DevErrorGroupMode mode) noexcept {
	DevErrorGroupKey key{.code = occurrence.error.code, .site = occurrence.error.site};
	if (mode == DevErrorGroupMode::Native) key.nativeCode = occurrence.error.nativeCode;
	if (mode == DevErrorGroupMode::Source && !occurrence.steps.empty()) {
		key.sourceId = occurrence.steps.front().sourceId;
	}
	return key;
}

} // namespace

struct DevErrorReporting::Impl {
	Impl(
		DevErrorMonitoring& owner,
		DevErrorReportingConfig initial,
		DevTimingReporting* timingOwner,
		DevMemoryReporting* memoryOwner)
		: monitoring(&owner), timing(timingOwner), memory(memoryOwner),
		  reportingConfig(normalize(std::move(initial))) {}

	static DevErrorReportingConfig normalize(DevErrorReportingConfig config) noexcept {
		config.retainedOccurrenceCapacity = std::max(1u, config.retainedOccurrenceCapacity);
		config.retainedByteBudget = std::max<uint64_t>(
			sizeof(DevErrorOccurrence) + sizeof(DevErrorRecord), config.retainedByteBudget);
		config.retainedBreadcrumbCapacity = std::max(1u, config.retainedBreadcrumbCapacity);
		config.maxStepsPerOccurrence = std::max(1u, config.maxStepsPerOccurrence);
		config.maximumAdvicePerOccurrence = std::max(1u, config.maximumAdvicePerOccurrence);
		config.retainedCaptureCapacity = std::max(1u, config.retainedCaptureCapacity);
		config.maximumPinnedTimingTicks = std::max(1u, config.maximumPinnedTimingTicks);
		config.maximumPinnedMemoryEvents = std::max(1u, config.maximumPinnedMemoryEvents);
		if (config.triggers.size() > 64u) config.triggers.resize(64u);
		return config;
	}

	void refreshRetainedBytes() noexcept {
		reportingStatus.retainedBytes = 0u;
		for (const auto& occurrence : occurrences) {
			reportingStatus.retainedBytes += retainedBytes(occurrence);
		}
		reportingStatus.retainedOccurrences = static_cast<uint32_t>(occurrences.size());
	}

	void enforceBudget() {
		refreshRetainedBytes();
		while (!occurrences.empty() &&
			(occurrences.size() > reportingConfig.retainedOccurrenceCapacity ||
			 reportingStatus.retainedBytes > reportingConfig.retainedByteBudget)) {
			reportingStatus.retainedBytes -= retainedBytes(occurrences.front());
			occurrences.erase(occurrences.begin());
			++reportingStatus.evictedOccurrences;
		}
		reportingStatus.retainedOccurrences = static_cast<uint32_t>(occurrences.size());
	}

	void attachBreadcrumbs(DevErrorOccurrence& occurrence, const DevErrorRecord& record) {
		if (record.breadcrumbBegin == 0u || record.breadcrumbEnd < record.breadcrumbBegin) return;
		bool foundBegin = false;
		for (const DevErrorBreadcrumb& breadcrumb : breadcrumbHistory) {
			if (breadcrumb.threadTrack != record.threadTrack ||
				breadcrumb.sequence < record.breadcrumbBegin ||
				breadcrumb.sequence > record.breadcrumbEnd) continue;
			foundBegin = foundBegin || breadcrumb.sequence == record.breadcrumbBegin;
			const bool duplicate = std::any_of(
				occurrence.breadcrumbs.begin(), occurrence.breadcrumbs.end(),
				[&](const auto& retained) { return retained.sequence == breadcrumb.sequence; });
			if (!duplicate) occurrence.breadcrumbs.push_back(breadcrumb);
		}
		if (!foundBegin) {
			occurrence.flags |= DevErrorRecordFlag::BreadcrumbHistoryTruncated;
			++reportingStatus.truncatedBreadcrumbRanges;
		}
	}

	bool triggerMatches(const DevErrorTrigger& trigger, const FlowUiError& error) const noexcept {
		if (trigger.code != ErrorCode::None && trigger.code != error.code) return false;
		if (trigger.site != ErrorSite::None && trigger.site != error.site) return false;
		if (trigger.matchNativeCode && trigger.nativeCode != error.nativeCode) return false;
		return true;
	}

	void beginTriggeredCapture(DevErrorOccurrence& occurrence) {
		const auto trigger = std::find_if(
			reportingConfig.triggers.begin(), reportingConfig.triggers.end(),
			[&](const auto& value) { return triggerMatches(value, occurrence.error); });
		if (trigger == reportingConfig.triggers.end()) return;
		if (captures.size() >= reportingConfig.retainedCaptureCapacity) {
			captures.erase(captures.begin());
			++reportingStatus.evictedCaptures;
		}
		const AppTickId first = occurrence.firstAppTick > trigger->preOccurrenceTicks
			? occurrence.firstAppTick - trigger->preOccurrenceTicks : 0u;
		const AppTickId room = std::numeric_limits<AppTickId>::max() - occurrence.lastAppTick;
		const AppTickId last = occurrence.lastAppTick +
			std::min<AppTickId>(room, trigger->postOccurrenceTicks);
		const DevErrorCaptureId id = nextCaptureId++;
		captures.push_back(DevErrorTriggeredCapture{
			.id = id,
			.triggerOccurrence = occurrence.id,
			.firstAppTick = first,
			.lastAppTick = last,
		});
		occurrence.captureId = id;
		++reportingStatus.triggeredCaptures;
	}

	void correlateTiming(DevErrorOccurrence& occurrence) {
		if (!timing || occurrence.steps.empty()) return;
		const DevErrorRecord& raised = occurrence.steps.front();
		const TimingReportingStatus timingStatus = timing->status();
		const auto report = timing->appTickReport(raised.context.appTick);
		if (!report) {
			if (timingStatus.hasRetainedTicks &&
				raised.context.appTick < timingStatus.oldestRetainedAppTick) {
				occurrence.timing.state = DevErrorCorrelationState::Evicted;
				++reportingStatus.timingCorrelationEvictions;
			} else {
				occurrence.timing.state = DevErrorCorrelationState::NotCaptured;
			}
			return;
		}
		occurrence.timing = DevErrorTimingCorrelation{
			.state = DevErrorCorrelationState::Available,
			.confidence = DevErrorCorrelationConfidence::SharedContext,
			.appTick = raised.context.appTick,
			.frame = raised.context.frame,
			.reportRevision = report->revision,
		};

		const std::vector<CpuTimingRecord>* zones = &report->applicationCpuZones;
		if (raised.context.frame) {
			for (const TimingWindowReport& window : report->windows) {
				if (window.window != raised.context.frame.window) continue;
				for (const TimingFrameReport& frame : window.frames) {
					if (frame.key == raised.context.frame) zones = &frame.cpuZones;
				}
			}
		}
		const CpuTimingRecord* containing = nullptr;
		for (const CpuTimingRecord& zone : *zones) {
			if (zone.track != raised.context.timingTrack || raised.timestampNs < zone.startNs ||
				raised.timestampNs > zone.startNs + zone.durationNs) continue;
			if (!containing || zone.depth >= containing->depth) containing = &zone;
		}
		if (containing) {
			occurrence.timing.containingInvocation = containing->invocationId;
			occurrence.timing.containingZone = containing->typeId;
		}
	}

	void correlateMemory(DevErrorOccurrence& occurrence) {
		if (!memory || occurrence.steps.empty()) return;
		const DevErrorRecord& raised = occurrence.steps.front();
		const MemoryReportingStatus memoryStatus = memory->status();
		occurrence.memory.storageMutationSequence = memoryStatus.storageMutationSequence;
		const auto events = memory->eventSnapshot();
		const RetainedMemoryEvent* selected = nullptr;
		bool explicitIdentity = false;
		uint32_t sharedContextCandidates = 0u;
		for (const RetainedMemoryEvent& retained : events) {
			const MemoryOperationRecord& event = retained.operation;
			if (raised.error.subject == 0u ||
				(raised.error.subject != event.lifetime && raised.error.subject != event.source)) {
				continue;
			}
			selected = &retained;
			explicitIdentity = true;
			break;
		}
		for (const RetainedMemoryEvent& retained : events) {
			if (explicitIdentity) break;
			const MemoryOperationRecord& event = retained.operation;
			const bool sameSubmission = raised.context.submissionSerial != 0u &&
				event.submissionSerial == raised.context.submissionSerial;
			const bool sameFrame = raised.context.frame && event.frame == raised.context.frame;
			if (!sameSubmission && !sameFrame) continue;
			++sharedContextCandidates;
			if (!selected || event.timestampNs <= raised.timestampNs) selected = &retained;
		}
		if (!selected) {
			for (const RetainedMemoryEvent& retained : events) {
				if (retained.operation.appTick != raised.context.appTick) continue;
				if (!selected || retained.operation.timestampNs <= raised.timestampNs) {
					selected = &retained;
				}
			}
		}
		if (selected) {
			occurrence.memory.state = !explicitIdentity && sharedContextCandidates > 1u
				? DevErrorCorrelationState::Ambiguous
				: DevErrorCorrelationState::Available;
			occurrence.memory.confidence = explicitIdentity
				? DevErrorCorrelationConfidence::Explicit
				: (sharedContextCandidates != 0u
					? DevErrorCorrelationConfidence::SharedContext
					: DevErrorCorrelationConfidence::TemporalNearby);
			occurrence.memory.eventSequence = selected->sequence;
			occurrence.memory.source = selected->operation.source;
			occurrence.memory.operation = selected->operation.operation;
		} else if (memoryStatus.events.overwriteCount != 0u && !events.empty() &&
			(raised.context.appTick < events.front().operation.appTick ||
			 raised.timestampNs < events.front().operation.timestampNs)) {
			occurrence.memory.state = DevErrorCorrelationState::Evicted;
			++reportingStatus.memoryCorrelationEvictions;
		} else {
			occurrence.memory.state = DevErrorCorrelationState::NotCaptured;
		}
		for (const MemoryCurrentSource& source : memory->currentSourceSnapshot()) {
			if (source.lastSampledTick > raised.context.appTick) continue;
			if (source.key.window != InvalidWindowId && raised.context.frame &&
				source.key.window != raised.context.frame.window) continue;
			occurrence.memory.checkpointTick = std::max(
				occurrence.memory.checkpointTick, source.lastSampledTick);
		}
	}

	void updateTriggeredCaptures(AppTickId appTick) {
		for (DevErrorTriggeredCapture& capture : captures) {
			if (capture.state == DevErrorTriggeredCaptureState::Complete) continue;
			const AppTickId through = std::min(appTick, capture.lastAppTick);
			if (timing) {
				const TimingReportingStatus timingStatus = timing->status();
				capture.timingHistoryEvicted = timingStatus.hasRetainedTicks &&
					capture.firstAppTick < timingStatus.oldestRetainedAppTick;
				const auto ticks = timing->appTickRange(
					capture.firstAppTick, reportingConfig.maximumPinnedTimingTicks);
				for (const TimingAppTickReport& tick : ticks) {
					if (tick.appTick > through) break;
					const bool duplicate = std::any_of(
						capture.timingTicks.begin(), capture.timingTicks.end(),
						[&](const auto& value) { return value.appTick == tick.appTick; });
					if (duplicate) continue;
					if (capture.timingTicks.size() >= reportingConfig.maximumPinnedTimingTicks) {
						capture.timingTruncated = true;
						break;
					}
					capture.timingTicks.push_back(tick);
				}
			}
			if (memory) {
				const MemoryReportingStatus memoryStatus = memory->status();
				const auto events = memory->eventSnapshot();
				capture.memoryHistoryEvicted = memoryStatus.events.overwriteCount != 0u &&
					!events.empty() && events.front().operation.appTick > capture.firstAppTick;
				for (const RetainedMemoryEvent& event : events) {
					if (event.operation.appTick < capture.firstAppTick ||
						event.operation.appTick > through) continue;
					const bool duplicate = std::any_of(
						capture.memoryEvents.begin(), capture.memoryEvents.end(),
						[&](const auto& value) { return value.sequence == event.sequence; });
					if (duplicate) continue;
					if (capture.memoryEvents.size() >= reportingConfig.maximumPinnedMemoryEvents) {
						capture.memoryTruncated = true;
						break;
					}
					capture.memoryEvents.push_back(event);
				}
			}
			if (appTick >= capture.lastAppTick) {
				capture.state = DevErrorTriggeredCaptureState::Complete;
			}
		}
	}

	DevErrorMonitoring* monitoring = nullptr;
	DevTimingReporting* timing = nullptr;
	DevMemoryReporting* memory = nullptr;
	mutable std::mutex mutex{};
	DevErrorReportingConfig reportingConfig{};
	DevErrorReportingStatus reportingStatus{};
	std::vector<DevErrorOccurrence> occurrences{};
	std::vector<DevErrorBreadcrumb> breadcrumbHistory{};
	std::vector<DevErrorTriggeredCapture> captures{};
	DevErrorCaptureId nextCaptureId = 1u;
	uint64_t fatalSafePointRevision = 0u;
};

DevErrorReporting::DevErrorReporting(
	DevErrorMonitoring& monitoring,
	DevErrorReportingConfig config,
	DevTimingReporting* timing,
	DevMemoryReporting* memory)
	: impl_(std::make_unique<Impl>(monitoring, std::move(config), timing, memory)) {
	impl_->occurrences.reserve(impl_->reportingConfig.retainedOccurrenceCapacity);
	impl_->breadcrumbHistory.reserve(impl_->reportingConfig.retainedBreadcrumbCapacity);
	impl_->captures.reserve(impl_->reportingConfig.retainedCaptureCapacity);
}

DevErrorReporting::~DevErrorReporting() = default;

void DevErrorReporting::setConfig(const DevErrorReportingConfig& config) {
	std::scoped_lock lock(impl_->mutex);
	impl_->reportingConfig = Impl::normalize(config);
	if (impl_->breadcrumbHistory.size() > impl_->reportingConfig.retainedBreadcrumbCapacity) {
		const size_t excess = impl_->breadcrumbHistory.size() -
			impl_->reportingConfig.retainedBreadcrumbCapacity;
		impl_->breadcrumbHistory.erase(
			impl_->breadcrumbHistory.begin(), impl_->breadcrumbHistory.begin() + excess);
	}
	while (impl_->captures.size() > impl_->reportingConfig.retainedCaptureCapacity) {
		impl_->captures.erase(impl_->captures.begin());
		++impl_->reportingStatus.evictedCaptures;
	}
	for (DevErrorOccurrence& occurrence : impl_->occurrences) {
		occurrence.advice = evaluateDevErrorAdvice(
			occurrence, impl_->reportingConfig.maximumAdvicePerOccurrence);
	}
	impl_->enforceBudget();
}

DevErrorReportingConfig DevErrorReporting::config() const {
	std::scoped_lock lock(impl_->mutex);
	return impl_->reportingConfig;
}

void DevErrorReporting::consumeThrough(AppTickId appTick) noexcept {
	const uint64_t consumeStarted = steadyNowNs();
	try {
		const uint64_t drainStarted = steadyNowNs();
		impl_->monitoring->captureDeferredSnapshots();
		auto breadcrumbs = impl_->monitoring->drainBreadcrumbs();
		auto records = impl_->monitoring->drainRecords();
		auto snapshots = impl_->monitoring->drainSnapshots();
		const uint64_t drainElapsed = steadyNowNs() - drainStarted;
		std::scoped_lock lock(impl_->mutex);
		impl_->reportingStatus.overhead.drainTimeNs += drainElapsed;
		impl_->reportingStatus.overhead.drainedBytes +=
			records.size() * sizeof(DevErrorRecord) +
			breadcrumbs.size() * sizeof(DevErrorBreadcrumb) +
			snapshots.size() * sizeof(DevErrorSnapshot);

		impl_->breadcrumbHistory.insert(
			impl_->breadcrumbHistory.end(), breadcrumbs.begin(), breadcrumbs.end());
		if (impl_->breadcrumbHistory.size() > impl_->reportingConfig.retainedBreadcrumbCapacity) {
			const size_t excess = impl_->breadcrumbHistory.size() -
				impl_->reportingConfig.retainedBreadcrumbCapacity;
			impl_->breadcrumbHistory.erase(
				impl_->breadcrumbHistory.begin(), impl_->breadcrumbHistory.begin() + excess);
		}
		impl_->reportingStatus.consumedBreadcrumbs += breadcrumbs.size();

		for (DevErrorOccurrence& occurrence : impl_->occurrences) {
			if (occurrence.state == DevErrorOccurrenceState::PostWindow &&
				appTick >= occurrence.closesAfterAppTick) {
				occurrence.state = DevErrorOccurrenceState::Closed;
			}
		}

		for (DevErrorRecord& record : records) {
			auto found = std::find_if(
				impl_->occurrences.begin(), impl_->occurrences.end(),
				[&](const auto& occurrence) { return occurrence.id == record.occurrence; });
			const bool created = found == impl_->occurrences.end();
			if (created) {
				impl_->occurrences.push_back(DevErrorOccurrence{
					.id = record.occurrence,
					.error = record.error,
					.firstAppTick = record.context.appTick,
					.lastAppTick = record.context.appTick,
					.firstTimestampNs = record.timestampNs,
					.lastTimestampNs = record.timestampNs,
					.productionKind = record.productionKind,
					.resolution = record.resolution,
					.flags = record.flags,
				});
				found = std::prev(impl_->occurrences.end());
			}

			DevErrorOccurrence& occurrence = *found;
			if (occurrence.error.code == ErrorCode::None && record.error.code != ErrorCode::None) {
				occurrence.error = record.error;
			}
			if (created) impl_->beginTriggeredCapture(occurrence);
			occurrence.lastAppTick = std::max(occurrence.lastAppTick, record.context.appTick);
			occurrence.lastTimestampNs = std::max(occurrence.lastTimestampNs, record.timestampNs);
			occurrence.productionKind = record.productionKind;
			if (record.resolution != ErrorResolution::None) occurrence.resolution = record.resolution;
			occurrence.flags |= record.flags;
			impl_->attachBreadcrumbs(occurrence, record);

			if (occurrence.steps.size() < impl_->reportingConfig.maxStepsPerOccurrence) {
				occurrence.steps.push_back(record);
			} else {
				occurrence.flags |= DevErrorRecordFlag::StepHistoryTruncated;
				++impl_->reportingStatus.truncatedSteps;
			}

			if (record.kind == DevErrorStepKind::Consequence) {
				occurrence.state = DevErrorOccurrenceState::Closed;
				occurrence.closesAfterAppTick = occurrence.lastAppTick;
			} else if (isFinalRecord(record)) {
				if (impl_->reportingConfig.postOccurrenceTicks == 0u) {
					occurrence.state = DevErrorOccurrenceState::Closed;
					occurrence.closesAfterAppTick = occurrence.lastAppTick;
				} else {
					occurrence.state = DevErrorOccurrenceState::PostWindow;
					const AppTickId room = std::numeric_limits<AppTickId>::max() -
						occurrence.lastAppTick;
					occurrence.closesAfterAppTick = occurrence.lastAppTick +
						std::min<AppTickId>(room, impl_->reportingConfig.postOccurrenceTicks);
				}
			}
		}

		for (const DevErrorSnapshot& snapshot : snapshots) {
			const auto found = std::find_if(
				impl_->occurrences.begin(), impl_->occurrences.end(),
				[&](const auto& occurrence) { return occurrence.id == snapshot.occurrence; });
			if (found == impl_->occurrences.end()) continue;
			found->snapshots.push_back(snapshot);
			switch (snapshot.outcome) {
			case DevErrorSnapshotOutcome::Unavailable:
			case DevErrorSnapshotOutcome::StorageLost:
				found->flags |= DevErrorRecordFlag::SnapshotUnavailable; break;
			case DevErrorSnapshotOutcome::StaleRevision:
				found->flags |= DevErrorRecordFlag::SnapshotStale; break;
			case DevErrorSnapshotOutcome::Truncated:
				found->flags |= DevErrorRecordFlag::SnapshotTruncated; break;
			case DevErrorSnapshotOutcome::Available: break;
			}
		}
		for (DevErrorOccurrence& occurrence : impl_->occurrences) {
			const uint64_t correlationStarted = steadyNowNs();
			if (occurrence.timing.state == DevErrorCorrelationState::NotCaptured) {
				impl_->correlateTiming(occurrence);
			}
			if (occurrence.memory.state == DevErrorCorrelationState::NotCaptured) {
				impl_->correlateMemory(occurrence);
			}
			impl_->reportingStatus.overhead.correlationTimeNs +=
				steadyNowNs() - correlationStarted;
			const uint64_t adviceStarted = steadyNowNs();
			occurrence.advice = evaluateDevErrorAdvice(
				occurrence, impl_->reportingConfig.maximumAdvicePerOccurrence);
			impl_->reportingStatus.overhead.adviceTimeNs += steadyNowNs() - adviceStarted;
		}
		impl_->updateTriggeredCaptures(appTick);

		impl_->reportingStatus.consumedRecords += records.size();
		impl_->reportingStatus.consumedSnapshots += snapshots.size();
		impl_->reportingStatus.lastConsumedAppTick = appTick;
		impl_->reportingStatus.quality = impl_->monitoring->qualitySnapshot();
		const TimingReportingStatus timingStatus = impl_->timing
			? impl_->timing->status() : TimingReportingStatus{};
		const MemoryReportingStatus memoryStatus = impl_->memory
			? impl_->memory->status() : MemoryReportingStatus{};
		impl_->monitoring->publishFatalSafePoint(DevErrorFatalSafePointSummary{
			.revision = ++impl_->fatalSafePointRevision,
			.appTick = appTick,
			.timingMutationSequence = timingStatus.mutationSequence,
			.timingDroppedRecords = timingStatus.quality.droppedRecords,
			.memoryGeneration = memoryStatus.generation,
			.memoryMutationSequence = memoryStatus.storageMutationSequence,
			.memoryDroppedOperations = memoryStatus.quality.droppedOperations,
			.errorRecordedEvents = impl_->reportingStatus.quality.recordedEvents,
			.errorDroppedEvents = impl_->reportingStatus.quality.droppedEvents,
		});
		impl_->enforceBudget();
		++impl_->reportingStatus.overhead.consumeCalls;
		impl_->reportingStatus.overhead.consumeTimeNs += steadyNowNs() - consumeStarted;
	} catch (...) {
		try {
			std::scoped_lock lock(impl_->mutex);
			++impl_->reportingStatus.consumeFailures;
			++impl_->reportingStatus.overhead.consumeCalls;
			impl_->reportingStatus.overhead.consumeTimeNs += steadyNowNs() - consumeStarted;
		} catch (...) {}
	}
}

DevErrorReportingStatus DevErrorReporting::status() const noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		return impl_->reportingStatus;
	} catch (...) {
		return {};
	}
}

std::vector<DevErrorOccurrence> DevErrorReporting::occurrenceSnapshot() const {
	std::scoped_lock lock(impl_->mutex);
	return impl_->occurrences;
}

std::optional<DevErrorOccurrence> DevErrorReporting::occurrence(DevErrorOccurrenceId id) const {
	std::scoped_lock lock(impl_->mutex);
	const auto found = std::find_if(
		impl_->occurrences.begin(), impl_->occurrences.end(),
		[&](const auto& value) { return value.id == id; });
	if (found == impl_->occurrences.end()) return std::nullopt;
	return *found;
}

std::vector<DevErrorOccurrence> DevErrorReporting::occurrenceRange(
	const DevErrorOccurrenceQuery& query) const {
	std::vector<DevErrorOccurrence> result;
	std::scoped_lock lock(impl_->mutex);
	for (const DevErrorOccurrence& occurrence : impl_->occurrences) {
		if (query.code && occurrence.error.code != *query.code) continue;
		if (query.site && occurrence.error.site != *query.site) continue;
		if (query.firstAppTick != 0u && occurrence.lastAppTick < query.firstAppTick) continue;
		if (query.lastAppTick != 0u && occurrence.firstAppTick > query.lastAppTick) continue;
		if (query.window) {
			const bool matches = std::any_of(
				occurrence.steps.begin(), occurrence.steps.end(),
				[&](const auto& step) { return step.context.frame.window == *query.window; });
			if (!matches) continue;
		}
		result.push_back(occurrence);
		if (query.maximumResults != 0u && result.size() >= query.maximumResults) break;
	}
	return result;
}

std::vector<DevErrorGroupSummary> DevErrorReporting::groupSnapshot(
	DevErrorGroupMode mode) const {
	const uint64_t groupingStarted = steadyNowNs();
	std::vector<DevErrorGroupSummary> groups;
	std::scoped_lock lock(impl_->mutex);
	for (const DevErrorOccurrence& occurrence : impl_->occurrences) {
		const DevErrorGroupKey key = groupKey(occurrence, mode);
		auto found = std::find_if(groups.begin(), groups.end(), [&](const auto& group) {
			return group.key == key;
		});
		if (found == groups.end()) {
			groups.push_back(DevErrorGroupSummary{
				.key = key,
				.occurrenceCount = 1u,
				.firstAppTick = occurrence.firstAppTick,
				.lastAppTick = occurrence.lastAppTick,
				.representative = occurrence.id,
				.lastResolution = occurrence.resolution,
			});
		} else {
			++found->occurrenceCount;
			found->firstAppTick = std::min(found->firstAppTick, occurrence.firstAppTick);
			found->lastAppTick = std::max(found->lastAppTick, occurrence.lastAppTick);
			found->representative = occurrence.id;
			found->lastResolution = occurrence.resolution;
		}
	}
	std::sort(groups.begin(), groups.end(), [](const auto& left, const auto& right) {
		if (left.occurrenceCount != right.occurrenceCount) {
			return left.occurrenceCount > right.occurrenceCount;
		}
		if (left.key.code != right.key.code) return left.key.code < right.key.code;
		return left.key.site < right.key.site;
	});
	++impl_->reportingStatus.overhead.groupingCalls;
	impl_->reportingStatus.overhead.groupingTimeNs += steadyNowNs() - groupingStarted;
	return groups;
}

std::optional<DevErrorStackTrace> DevErrorReporting::stack(DevErrorStackId id) const {
	const auto stacks = impl_->monitoring->stackSnapshot();
	const auto found = std::find_if(
		stacks.begin(), stacks.end(), [&](const auto& value) { return value.id == id; });
	if (found == stacks.end()) return std::nullopt;
	return *found;
}

std::vector<DevErrorStackTrace> DevErrorReporting::stackSnapshot() const {
	return impl_->monitoring->stackSnapshot();
}

std::vector<DevErrorTriggeredCapture> DevErrorReporting::captureSnapshot() const {
	std::scoped_lock lock(impl_->mutex);
	return impl_->captures;
}

std::optional<DevErrorTriggeredCapture> DevErrorReporting::capture(
	DevErrorCaptureId id) const {
	std::scoped_lock lock(impl_->mutex);
	const auto found = std::find_if(
		impl_->captures.begin(), impl_->captures.end(),
		[&](const auto& value) { return value.id == id; });
	if (found == impl_->captures.end()) return std::nullopt;
	return *found;
}

std::vector<DevErrorAdviceResult> DevErrorReporting::advice(DevErrorOccurrenceId id) const {
	std::scoped_lock lock(impl_->mutex);
	const auto found = std::find_if(
		impl_->occurrences.begin(), impl_->occurrences.end(),
		[&](const auto& occurrence) { return occurrence.id == id; });
	return found == impl_->occurrences.end()
		? std::vector<DevErrorAdviceResult>{}
		: found->advice;
}

std::span<const DevErrorAdviceDescriptor> DevErrorReporting::adviceCatalogue() const noexcept {
	return devErrorAdviceCatalogue();
}

std::optional<DevErrorFatalCapsule> DevErrorReporting::fatalCapsule() const noexcept {
	return impl_->monitoring->fatalCapsuleSnapshot();
}

} // namespace FlowUi::devSystems

#endif
