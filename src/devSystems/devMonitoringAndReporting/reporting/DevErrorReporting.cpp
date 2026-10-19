#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devMonitoringAndReporting/reporting/DevReportingRing.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>
#include <mutex>
#include <unordered_map>
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

	struct PendingHandle {
		DevErrorOccurrenceId id = 0;
		uint64_t admission = 0;
	};
	struct PendingState {
		DevErrorOccurrence* occurrence = nullptr;
		uint64_t admission = 0;
		uint64_t deadline = 0, memory_revision = UINT64_MAX, timing_revision = UINT64_MAX;
		uint64_t timing_generation = UINT64_MAX;
		size_t bytes = 0;
		bool scheduled = false, advice_dirty = true, event_capture_enabled = false;
	};
	void schedule(PendingState& state) {
		if (!state.scheduled) {
			pending_ids.emplace_back(PendingHandle{state.occurrence->id, state.admission});
			state.scheduled = true;
		}
	}
	void evict_front() {
		const auto id = occurrences.front().id;
		const auto found = states.find(id);
		if (found != states.end()) {
			reportingStatus.retainedBytes -= found->second.bytes;
			states.erase(found);
		}
		auto& occurrence = occurrences.front();
		const auto reusable_budget = reportingConfig.retainedByteBudget / occurrences.capacity();
		const auto buffer_bytes = occurrence.steps.capacity() * sizeof(DevErrorRecord) +
								  occurrence.breadcrumbs.capacity() * sizeof(DevErrorBreadcrumb) +
								  occurrence.snapshots.capacity() * sizeof(DevErrorSnapshot) +
								  occurrence.advice.capacity() * sizeof(DevErrorAdviceResult);
		if (buffer_bytes > reusable_budget) {
			std::vector<DevErrorRecord>{}.swap(occurrence.steps);
			std::vector<DevErrorBreadcrumb>{}.swap(occurrence.breadcrumbs);
			std::vector<DevErrorSnapshot>{}.swap(occurrence.snapshots);
			std::vector<DevErrorAdviceResult>{}.swap(occurrence.advice);
		} else {
			occurrence.steps.clear();
			occurrence.breadcrumbs.clear();
			occurrence.snapshots.clear();
			occurrence.advice.clear();
		}
		occurrences.pop_front();
		++reportingStatus.evictedOccurrences;
	}
	void enforceBudget() {
		while (!occurrences.empty() &&
			   reportingStatus.retainedBytes > reportingConfig.retainedByteBudget)
			evict_front();
		reportingStatus.retainedOccurrences = uint32_t(occurrences.size());
	}
	[[nodiscard]] DevErrorOccurrence& admit(const DevErrorRecord& record) {
		if (occurrences.size() == occurrences.capacity())
			evict_front();
		auto& slot = occurrences.append_slot();
		DevErrorOccurrence fresh{.id = record.occurrence,
								 .error = record.error,
								 .firstAppTick = record.context.appTick,
								 .lastAppTick = record.context.appTick,
								 .firstTimestampNs = record.timestampNs,
								 .lastTimestampNs = record.timestampNs,
								 .productionKind = record.productionKind,
								 .resolution = record.resolution,
								 .flags = record.flags};
		fresh.steps.swap(slot.steps);
		fresh.steps.clear();
		fresh.breadcrumbs.swap(slot.breadcrumbs);
		fresh.breadcrumbs.clear();
		fresh.snapshots.swap(slot.snapshots);
		fresh.snapshots.clear();
		fresh.advice.swap(slot.advice);
		fresh.advice.clear();
		slot = std::move(fresh);
		const auto room = UINT64_MAX - slot.firstAppTick;
		try {
			auto [entry, inserted] = states.emplace(
				slot.id, PendingState{.occurrence = &slot,
									  .admission = next_admission++,
									  .deadline = slot.firstAppTick +
												  std::min<uint64_t>(
													  room, reportingConfig.postOccurrenceTicks),
									  .bytes = retainedBytes(slot)});
			assert(inserted);
			reportingStatus.retainedBytes += entry->second.bytes;
		} catch (...) {
			occurrences.pop_back();
			throw;
		}
		return slot;
	}
	void refresh_bytes(PendingState& state) noexcept {
		const auto bytes = retainedBytes(*state.occurrence);
		reportingStatus.retainedBytes -= state.bytes;
		reportingStatus.retainedBytes += bytes;
		state.bytes = bytes;
	}
	void apply_memory(DevErrorOccurrence& occurrence, const MemoryCorrelationResult& result) {
		auto& correlation = occurrence.memory;
		correlation = {.eventSequence = result.event_sequence,
					   .source = result.source,
					   .checkpointTick =
						   std::max(correlation.checkpointTick, result.checkpoint_tick),
					   .storageMutationSequence = result.storage_revision,
					   .history_generation = result.history_generation,
					   .operation = result.operation,
					   .incomplete = correlation.incomplete || result.incomplete};
		switch (result.match) {
		case MemoryCorrelationMatch::Explicit:
			correlation.state = DevErrorCorrelationState::Available;
			correlation.confidence = DevErrorCorrelationConfidence::Explicit;
			break;
		case MemoryCorrelationMatch::SharedContext:
			correlation.state = DevErrorCorrelationState::Available;
			correlation.confidence = DevErrorCorrelationConfidence::SharedContext;
			break;
		case MemoryCorrelationMatch::Ambiguous:
			correlation.state = DevErrorCorrelationState::Ambiguous;
			correlation.confidence = DevErrorCorrelationConfidence::SharedContext;
			break;
		case MemoryCorrelationMatch::TemporalNearby:
			correlation.state = DevErrorCorrelationState::Available;
			correlation.confidence = DevErrorCorrelationConfidence::TemporalNearby;
			break;
		case MemoryCorrelationMatch::Evicted:
			correlation.state = DevErrorCorrelationState::Evicted;
			++reportingStatus.memoryCorrelationEvictions;
			break;
		case MemoryCorrelationMatch::NotCaptured:
			break;
		}
	}
	void process_pending(AppTickId app_tick) {
		const auto timing_status = timing ? timing->status() : TimingReportingStatus{};
		const auto memory_status = memory ? memory->status() : MemoryReportingStatus{};
		memory_queries.clear();
		memory_query_ids.clear();
		memory_queries.reserve(std::min(pending_ids.size(), states.size()));
		memory_query_ids.reserve(std::min(pending_ids.size(), states.size()));
		for (const auto handle : pending_ids) {
			const auto found = states.find(handle.id);
			if (found == states.end() || found->second.admission != handle.admission)
				continue;
			auto& state = found->second;
			auto& occurrence = *state.occurrence;
			if (occurrence.steps.empty())
				continue;
			if (occurrence.state == DevErrorOccurrenceState::PostWindow &&
				app_tick >= occurrence.closesAfterAppTick)
				occurrence.state = DevErrorOccurrenceState::Closed;
			if (memory && !occurrence.memory.sealed && state.memory_revision != UINT64_MAX &&
				occurrence.memory.history_generation != memory_status.history_storage_generation) {
				if (occurrence.memory.eventSequence)
					occurrence.memory.state = DevErrorCorrelationState::Evicted;
				occurrence.memory.sealed = occurrence.memory.incomplete = true;
				state.advice_dirty = true;
				++reportingStatus.overhead.sealedCorrelations;
			}
			if (memory && !occurrence.memory.sealed &&
				state.memory_revision != memory_status.correlation_revision) {
				const auto& raised = occurrence.steps.front();
				memory_queries.emplace_back(
					MemoryCorrelationQuery{.frame = raised.context.frame,
										   .subject = raised.error.subject,
										   .app_tick = raised.context.appTick,
										   .submission = raised.context.submissionSerial,
										   .timestamp_ns = raised.timestampNs});
				memory_query_ids.emplace_back(handle);
			}
		}
		const auto correlation_started = steadyNowNs();
		const auto advice_before = reportingStatus.overhead.adviceTimeNs;
		if (!memory_queries.empty()) {
			const auto work = memory->correlate_errors(memory_queries);
			++reportingStatus.overhead.memoryCorrelationBatches;
			reportingStatus.overhead.memoryCorrelationQueries += memory_queries.size();
			reportingStatus.overhead.memoryEventCandidates += work.event_candidates;
			reportingStatus.overhead.memorySourceChecks += work.source_checks;
			for (size_t offset = 0; offset < memory_queries.size(); ++offset) {
				auto& state = states.at(memory_query_ids[offset].id);
				auto& occurrence = *state.occurrence;
				const auto& result = memory_queries[offset].result;
				if (state.memory_revision != UINT64_MAX &&
					occurrence.memory.history_generation != result.history_generation) {
					if (occurrence.memory.eventSequence)
						occurrence.memory.state = DevErrorCorrelationState::Evicted;
					occurrence.memory.sealed = occurrence.memory.incomplete = true;
					state.advice_dirty = true;
					++reportingStatus.overhead.sealedCorrelations;
					continue;
				}
				apply_memory(occurrence, result);
				state.memory_revision = result.revision;
				state.event_capture_enabled = result.event_capture_enabled;
				state.advice_dirty = true;
				if ((!result.event_capture_enabled &&
					 result.consumed_tick >= occurrence.firstAppTick) ||
					app_tick >= state.deadline) {
					occurrence.memory.sealed = true;
					occurrence.memory.incomplete =
						occurrence.memory.incomplete ||
						result.consumed_tick < (result.event_capture_enabled
													? state.deadline
													: occurrence.firstAppTick);
					++reportingStatus.overhead.sealedCorrelations;
				}
			}
		}
		size_t pending_count = 0;
		for (const auto handle : pending_ids) {
			const auto found = states.find(handle.id);
			if (found == states.end() || found->second.admission != handle.admission)
				continue;
			auto& state = found->second;
			auto& occurrence = *state.occurrence;
			if (!occurrence.timing.sealed && timing && state.timing_generation != UINT64_MAX &&
				state.timing_generation != timing_status.capture_generation) {
				if (occurrence.timing.capture_generation)
					occurrence.timing.state = DevErrorCorrelationState::Evicted;
				occurrence.timing.sealed = true;
				state.advice_dirty = true;
				++reportingStatus.overhead.sealedCorrelations;
			}
			if (!occurrence.timing.sealed) {
				if (timing && state.timing_revision != timing_status.mutationSequence) {
					state.timing_generation = timing_status.capture_generation;
					correlateTiming(occurrence);
					state.timing_revision = timing_status.mutationSequence;
					++reportingStatus.overhead.timingCorrelationQueries;
					state.advice_dirty = true;
				}
				const bool outside_capture =
					!timing_status.capture_generation ||
					occurrence.firstAppTick < timing_status.capture_first_app_tick ||
					occurrence.firstAppTick >= timing_status.capture_end_app_tick_exclusive;
				if (!timing || outside_capture || app_tick >= state.deadline) {
					if (timing && !outside_capture &&
						occurrence.timing.state == DevErrorCorrelationState::NotCaptured &&
						!timing_status.capture_sealed) {
						occurrence.timing.incomplete = true;
						state.advice_dirty = true;
					}
					occurrence.timing.sealed = true;
					++reportingStatus.overhead.sealedCorrelations;
				}
			}
			if (!occurrence.memory.sealed && (!memory || app_tick >= state.deadline)) {
				occurrence.memory.sealed = true;
				if (memory &&
					memory_status.lastConsumedAppTick <
						(state.event_capture_enabled ? state.deadline : occurrence.firstAppTick)) {
					occurrence.memory.incomplete = true;
					state.advice_dirty = true;
				}
				++reportingStatus.overhead.sealedCorrelations;
			}
			if (state.advice_dirty) {
				const auto advice_started = steadyNowNs();
				occurrence.advice =
					evaluateDevErrorAdvice(occurrence, reportingConfig.maximumAdvicePerOccurrence);
				reportingStatus.overhead.adviceTimeNs += steadyNowNs() - advice_started;
				++reportingStatus.overhead.adviceUpdates;
				state.advice_dirty = false;
				refresh_bytes(state);
			}
			state.scheduled = !occurrence.memory.sealed || !occurrence.timing.sealed ||
							  occurrence.state == DevErrorOccurrenceState::PostWindow;
			if (state.scheduled)
				pending_ids[pending_count++] = handle;
		}
		pending_ids.resize(pending_count);
		reportingStatus.pendingOccurrences = uint32_t(pending_count);
		// Advice has its own counter; correlation timing excludes it.
		reportingStatus.overhead.correlationTimeNs +=
			steadyNowNs() - correlation_started -
			(reportingStatus.overhead.adviceTimeNs - advice_before);
	}
	[[nodiscard]] DevErrorOccurrence project_occurrence(const DevErrorOccurrence& retained) const {
		auto result = retained;
		bool changed = false;
		if (timing && result.timing.capture_generation) {
			const auto status = timing->status();
			if (result.timing.capture_generation != status.capture_generation ||
				result.timing.appTick < status.oldestRetainedAppTick) {
				changed = result.timing.state != DevErrorCorrelationState::Evicted;
				result.timing.state = DevErrorCorrelationState::Evicted;
			}
		}
		if (memory && result.memory.eventSequence) {
			const auto status = memory->status();
			if (result.memory.history_generation != status.history_storage_generation ||
				!status.events.hasRetained ||
				result.memory.eventSequence < status.events.oldestRetainedSequence) {
				changed = changed || result.memory.state != DevErrorCorrelationState::Evicted;
				result.memory.state = DevErrorCorrelationState::Evicted;
			}
		}
		if (changed)
			result.advice =
				evaluateDevErrorAdvice(result, reportingConfig.maximumAdvicePerOccurrence);
		return result;
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
			captures.pop_front();
			++reportingStatus.evictedCaptures;
		}
		const AppTickId first = occurrence.firstAppTick > trigger->preOccurrenceTicks
			? occurrence.firstAppTick - trigger->preOccurrenceTicks : 0u;
		const AppTickId room = std::numeric_limits<AppTickId>::max() - occurrence.lastAppTick;
		const AppTickId last = occurrence.lastAppTick +
			std::min<AppTickId>(room, trigger->postOccurrenceTicks);
		const DevErrorCaptureId id = nextCaptureId++;
		auto& slot = captures.append_slot();
		DevErrorTriggeredCapture fresh{.id = id,
									   .triggerOccurrence = occurrence.id,
									   .firstAppTick = first,
									   .lastAppTick = last};
		try {
			fresh.memoryEvents.swap(slot.memoryEvents);
			fresh.memoryEvents.clear();
			if (fresh.memoryEvents.capacity() > reportingConfig.maximumPinnedMemoryEvents)
				std::vector<RetainedMemoryEvent>{}.swap(fresh.memoryEvents);
			fresh.memoryEvents.reserve(reportingConfig.maximumPinnedMemoryEvents);
			slot = std::move(fresh);
		} catch (...) {
			captures.pop_back();
			throw;
		}
		occurrence.captureId = id;
		++reportingStatus.triggeredCaptures;
	}

	void correlateTiming(DevErrorOccurrence& occurrence) {
		if (!timing || occurrence.steps.empty()) return;
		const DevErrorRecord& raised = occurrence.steps.front();
		const TimingReportingStatus timingStatus = timing->status();
		const auto report =
			timing->correlate_sample(raised.context.appTick, raised.context.frame,
									 raised.context.timingTrack, raised.timestampNs);
		if (!report) {
			if (timingStatus.hasRetainedTicks &&
				raised.context.appTick >= timingStatus.capture_first_app_tick &&
				raised.context.appTick < timingStatus.oldestRetainedAppTick) {
				occurrence.timing.state = DevErrorCorrelationState::Evicted;
				++reportingStatus.timingCorrelationEvictions;
			} else {
				occurrence.timing.state = DevErrorCorrelationState::NotCaptured;
			}
			return;
		}
		occurrence.timing = DevErrorTimingCorrelation{
			.entity = report->entity,
			.state = DevErrorCorrelationState::Available,
			.confidence = DevErrorCorrelationConfidence::SharedContext,
			.appTick = raised.context.appTick,
			.frame = raised.context.frame,
			.reportRevision = report->revision,
			.capture_generation = report->generation,
		};
		occurrence.timing.containingInvocation = report->invocation;
		occurrence.timing.containingZone = report->zone;
	}

	void project_capture_timing(DevErrorTriggeredCapture& capture, AppTickId app_tick) const {
		const auto through = std::min(app_tick, capture.lastAppTick);
		if (timing) {
			const TimingReportingStatus timingStatus = timing->status();
			capture.timingHistoryEvicted =
				timingStatus.hasRetainedTicks &&
				capture.firstAppTick >= timingStatus.capture_first_app_tick &&
				capture.firstAppTick < timingStatus.oldestRetainedAppTick;
			if (!capture.timing_generation && timingStatus.hasRetainedTicks &&
				capture.firstAppTick <= timingStatus.newestRetainedAppTick &&
				through >= timingStatus.oldestRetainedAppTick)
				capture.timing_generation = timingStatus.capture_generation;
			if (capture.timing_generation &&
				capture.timing_generation != timingStatus.capture_generation) {
				capture.timingHistoryEvicted = true;
				capture.timing_state = DevErrorCorrelationState::Evicted;
				capture.timing_retained_ticks = 0;
			} else if (capture.timing_generation) {
				const auto first =
					std::max(capture.firstAppTick, timingStatus.oldestRetainedAppTick);
				const auto last = std::min(through, timingStatus.newestRetainedAppTick);
				const auto available_count = last >= first ? last - first + 1 : 0;
				capture.timing_retained_ticks =
					std::min<uint64_t>(available_count, reportingConfig.maximumPinnedTimingTicks);
				const bool outside_capture =
					capture.firstAppTick < timingStatus.capture_first_app_tick ||
					through >= timingStatus.capture_end_app_tick_exclusive;
				capture.timingTruncated =
					outside_capture || available_count > reportingConfig.maximumPinnedTimingTicks;
				capture.timing_state = capture.timingHistoryEvicted
										   ? DevErrorCorrelationState::Evicted
									   : outside_capture ? DevErrorCorrelationState::NotCaptured
														 : DevErrorCorrelationState::Available;
			}
		}
	}
	void updateTriggeredCaptures(AppTickId app_tick) {
		for (auto& capture : captures) {
			if (capture.state == DevErrorTriggeredCaptureState::Complete)
				continue;
			project_capture_timing(capture, app_tick);
			if (memory && !capture.memory_sealed) {
				const auto progress = memory->append_event_range(
					capture.memory_generation, capture.memory_cursor, capture.firstAppTick,
					capture.lastAppTick, reportingConfig.maximumPinnedMemoryEvents,
					capture.memoryEvents);
				if (!progress.reset) {
					capture.memory_generation = progress.generation;
					capture.memory_cursor = progress.cursor;
				}
				capture.memoryHistoryEvicted = capture.memoryHistoryEvicted || progress.evicted;
				capture.memoryTruncated =
					capture.memoryTruncated || progress.truncated || progress.reset;
				capture.memory_sealed = progress.truncated || progress.reset;
				if (app_tick >= capture.lastAppTick && progress.consumed_tick < capture.lastAppTick)
					capture.memoryTruncated = true;
			}
			if (app_tick >= capture.lastAppTick) {
				capture.memory_sealed = true;
				capture.state = DevErrorTriggeredCaptureState::Complete;
			}
		}
	}

	DevErrorMonitoring* monitoring = nullptr;
	DevTimingReporting* timing = nullptr;
	DevMemoryReporting* memory = nullptr;
	mutable std::mutex mutex{};
	std::mutex ingestion_mutex{};
	DevErrorReportingConfig reportingConfig{};
	DevErrorReportingStatus reportingStatus{};
	detail::ReportingRing<DevErrorOccurrence> occurrences{};
	std::unordered_map<DevErrorOccurrenceId, PendingState> states{};
	std::vector<PendingHandle> pending_ids{}, memory_query_ids{};
	uint64_t next_admission = 1;
	std::vector<MemoryCorrelationQuery> memory_queries{};
	std::vector<DevErrorRecord> record_scratch{};
	std::vector<DevErrorBreadcrumb> breadcrumb_scratch{};
	std::vector<DevErrorSnapshot> snapshot_scratch{};
	detail::ReportingRing<DevErrorBreadcrumb> breadcrumbHistory{};
	detail::ReportingRing<DevErrorTriggeredCapture> captures{};
	DevErrorCaptureId nextCaptureId = 1u;
	uint64_t fatalSafePointRevision = 0u;
};

DevErrorReporting::DevErrorReporting(
	DevErrorMonitoring& monitoring,
	DevErrorReportingConfig config,
	DevTimingReporting* timing,
	DevMemoryReporting* memory)
	: impl_(std::make_unique<Impl>(monitoring, std::move(config), timing, memory)) {
	impl_->occurrences.set_capacity(impl_->reportingConfig.retainedOccurrenceCapacity);
	impl_->states.reserve(impl_->reportingConfig.retainedOccurrenceCapacity);
	impl_->pending_ids.reserve(impl_->reportingConfig.retainedOccurrenceCapacity);
	impl_->breadcrumbHistory.set_capacity(impl_->reportingConfig.retainedBreadcrumbCapacity);
	impl_->captures.set_capacity(impl_->reportingConfig.retainedCaptureCapacity);
}

DevErrorReporting::~DevErrorReporting() = default;

void DevErrorReporting::setConfig(const DevErrorReportingConfig& config) {
	std::scoped_lock lock(impl_->mutex);
	impl_->reportingConfig = Impl::normalize(config);
	while (impl_->occurrences.size() > impl_->reportingConfig.retainedOccurrenceCapacity)
		impl_->evict_front();
	impl_->occurrences.set_capacity(impl_->reportingConfig.retainedOccurrenceCapacity);
	impl_->states.reserve(impl_->reportingConfig.retainedOccurrenceCapacity);
	impl_->breadcrumbHistory.set_capacity(impl_->reportingConfig.retainedBreadcrumbCapacity);
	while (impl_->captures.size() > impl_->reportingConfig.retainedCaptureCapacity) {
		impl_->captures.pop_front();
		++impl_->reportingStatus.evictedCaptures;
	}
	impl_->captures.set_capacity(impl_->reportingConfig.retainedCaptureCapacity);
	for (auto& occurrence : impl_->occurrences) {
		auto& state = impl_->states.at(occurrence.id);
		state.occurrence = &occurrence;
		occurrence.advice =
			evaluateDevErrorAdvice(occurrence, impl_->reportingConfig.maximumAdvicePerOccurrence);
		state.advice_dirty = false;
		++impl_->reportingStatus.overhead.adviceUpdates;
		impl_->refresh_bytes(state);
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
		std::scoped_lock ingestion_lock(impl_->ingestion_mutex);
		const uint64_t drainStarted = steadyNowNs();
		impl_->monitoring->captureDeferredSnapshots();
		auto& breadcrumbs = impl_->breadcrumb_scratch;
		auto& records = impl_->record_scratch;
		auto& snapshots = impl_->snapshot_scratch;
		impl_->monitoring->drain_breadcrumbs_into(breadcrumbs);
		impl_->monitoring->drain_records_into(records);
		impl_->monitoring->drain_snapshots_into(snapshots);
		const uint64_t drainElapsed = steadyNowNs() - drainStarted;
		std::scoped_lock lock(impl_->mutex);
		impl_->reportingStatus.overhead.drainTimeNs += drainElapsed;
		impl_->reportingStatus.overhead.drainedBytes +=
			records.size() * sizeof(DevErrorRecord) +
			breadcrumbs.size() * sizeof(DevErrorBreadcrumb) +
			snapshots.size() * sizeof(DevErrorSnapshot);

		for (const auto& breadcrumb : breadcrumbs)
			impl_->breadcrumbHistory.push_back(breadcrumb);
		impl_->reportingStatus.consumedBreadcrumbs += breadcrumbs.size();

		for (DevErrorRecord& record : records) {
			const bool created = !impl_->states.contains(record.occurrence);
			DevErrorOccurrence& occurrence =
				created ? impl_->admit(record) : *impl_->states.at(record.occurrence).occurrence;
			auto& progress = impl_->states.at(occurrence.id);
			progress.advice_dirty = true;
			impl_->schedule(progress);
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
			if (!occurrence.timing.sealed || !occurrence.memory.sealed)
				progress.deadline = std::max(progress.deadline, occurrence.closesAfterAppTick);
			impl_->refresh_bytes(progress);
		}

		for (const DevErrorSnapshot& snapshot : snapshots) {
			const auto found = impl_->states.find(snapshot.occurrence);
			if (found == impl_->states.end())
				continue;
			auto& progress = found->second;
			auto* occurrence = progress.occurrence;
			occurrence->snapshots.emplace_back(snapshot);
			progress.advice_dirty = true;
			impl_->schedule(progress);
			switch (snapshot.outcome) {
			case DevErrorSnapshotOutcome::Unavailable:
			case DevErrorSnapshotOutcome::StorageLost:
				occurrence->flags |= DevErrorRecordFlag::SnapshotUnavailable;
				break;
			case DevErrorSnapshotOutcome::StaleRevision:
				occurrence->flags |= DevErrorRecordFlag::SnapshotStale;
				break;
			case DevErrorSnapshotOutcome::Truncated:
				occurrence->flags |= DevErrorRecordFlag::SnapshotTruncated;
				break;
			case DevErrorSnapshotOutcome::Available: break;
			}
		}
		impl_->process_pending(appTick);
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

std::vector<DevErrorOccurrenceSummary> DevErrorReporting::occurrence_summaries() const {
	std::scoped_lock lock(impl_->mutex);
	const auto timing = impl_->timing ? impl_->timing->status() : TimingReportingStatus{};
	const auto memory = impl_->memory ? impl_->memory->status() : MemoryReportingStatus{};
	std::vector<DevErrorOccurrenceSummary> result;
	result.reserve(impl_->occurrences.size());
	for (const auto& occurrence : impl_->occurrences) {
		auto correlation = occurrence.timing;
		const bool timing_lost = correlation.capture_generation &&
								 (correlation.capture_generation != timing.capture_generation ||
								  correlation.appTick < timing.oldestRetainedAppTick);
		const bool memory_lost =
			occurrence.memory.eventSequence &&
			(occurrence.memory.history_generation != memory.history_storage_generation ||
			 !memory.events.hasRetained ||
			 occurrence.memory.eventSequence < memory.events.oldestRetainedSequence);
		if (timing_lost)
			correlation.state = DevErrorCorrelationState::Evicted;
		result.emplace_back(DevErrorOccurrenceSummary{
			.timing = correlation,
			.error = occurrence.error,
			.suggested_action = !timing_lost && !memory_lost && !occurrence.advice.empty()
									? occurrence.advice.front().suggestedAction
									: std::string_view{},
			.id = occurrence.id,
			.last_app_tick = occurrence.lastAppTick});
	}
	return result;
}

std::vector<DevErrorOccurrence> DevErrorReporting::occurrenceSnapshot() const {
	std::scoped_lock lock(impl_->mutex);
	std::vector<DevErrorOccurrence> result;
	result.reserve(impl_->occurrences.size());
	for (const auto& occurrence : impl_->occurrences)
		result.emplace_back(impl_->project_occurrence(occurrence));
	return result;
}

std::optional<DevErrorOccurrence> DevErrorReporting::occurrence(DevErrorOccurrenceId id) const {
	std::scoped_lock lock(impl_->mutex);
	const auto found = std::find_if(
		impl_->occurrences.begin(), impl_->occurrences.end(),
		[&](const auto& value) { return value.id == id; });
	if (found == impl_->occurrences.end()) return std::nullopt;
	return impl_->project_occurrence(*found);
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
		result.emplace_back(impl_->project_occurrence(occurrence));
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
	std::vector<DevErrorTriggeredCapture> result;
	result.reserve(impl_->captures.size());
	for (const auto& capture : impl_->captures) {
		result.emplace_back(capture);
		impl_->project_capture_timing(result.back(), impl_->reportingStatus.lastConsumedAppTick);
	}
	return result;
}

std::optional<DevErrorTriggeredCapture> DevErrorReporting::capture(
	DevErrorCaptureId id) const {
	std::scoped_lock lock(impl_->mutex);
	const auto found = std::find_if(
		impl_->captures.begin(), impl_->captures.end(),
		[&](const auto& value) { return value.id == id; });
	if (found == impl_->captures.end()) return std::nullopt;
	auto result = *found;
	impl_->project_capture_timing(result, impl_->reportingStatus.lastConsumedAppTick);
	return result;
}

std::vector<DevErrorAdviceResult> DevErrorReporting::advice(DevErrorOccurrenceId id) const {
	std::scoped_lock lock(impl_->mutex);
	const auto found = std::find_if(
		impl_->occurrences.begin(), impl_->occurrences.end(),
		[&](const auto& occurrence) { return occurrence.id == id; });
	return found == impl_->occurrences.end() ? std::vector<DevErrorAdviceResult>{}
											 : impl_->project_occurrence(*found).advice;
}

std::span<const DevErrorAdviceDescriptor> DevErrorReporting::adviceCatalogue() const noexcept {
	return devErrorAdviceCatalogue();
}

std::optional<DevErrorFatalCapsule> DevErrorReporting::fatalCapsule() const noexcept {
	return impl_->monitoring->fatalCapsuleSnapshot();
}

} // namespace FlowUi::devSystems

#endif
