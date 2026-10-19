#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>

using namespace FlowUi;
using namespace FlowUi::devSystems;
namespace {
constexpr auto error_source = makeDevErrorSource("test.reporting.repeated_condition");
[[nodiscard]] FlowUiError test_error(uint64_t subject = 0) noexcept {
	return makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ResourceAllocatePersistent,
					 subject);
}
struct Probe {
	size_t sources = 0;
	static void sample(const void* owner, MemoryProbeContext& context) noexcept {
		const auto& probe = *static_cast<const Probe*>(owner);
		for (size_t offset = 0; offset < probe.sources; ++offset) {
			if (!context.sink.append(MemoryValueSample{
					.source = offset + 1000, .logicalLiveBytes = 32, .backingAllocatedBytes = 64}))
				std::abort();
		}
	}
};
struct MemoryFixture {
	DevMemory memory{DevMemoryConfig{.gpuMemory = false, .processMemory = false}};
	DevMemoryReporting reporting;
	explicit MemoryFixture(bool events = false, size_t capacity = 64)
		: reporting(memory, MemoryReportingConfig{.eventByteCapacity =
													  capacity * sizeof(RetainedMemoryEvent),
												  .segmentCapacity = 1024,
												  .managerSampleEveryTicks = 1,
												  .retainLifetimeEvents = events}) {}
	void append(MemoryOperationRecord operation) { assert(memory.recorder().tryRecord(operation)); }
};

void indexed_queries_and_append_cursors() {
	MemoryFixture fixture(true, 4);
	assert(fixture.reporting.eventSnapshot().empty());
	assert(fixture.reporting.segmentSnapshot().empty());
	std::array queries{MemoryCorrelationQuery{.app_tick = 0}};
	assert(fixture.reporting.correlate_errors(queries).event_candidates == 0);
	assert(queries[0].result.match == MemoryCorrelationMatch::NotCaptured);
	fixture.append({.source = 10,
					.lifetime = 100,
					.operation = MemoryOperation::BackingGrow,
					.appTick = 10,
					.frame = {1, 1},
					.submissionSerial = 7,
					.timestampNs = 10});
	fixture.append({.source = 11,
					.lifetime = 101,
					.operation = MemoryOperation::BackingGrow,
					.appTick = 10,
					.frame = {1, 1},
					.submissionSerial = 7,
					.timestampNs = 20});
	fixture.append({.source = 12,
					.lifetime = 102,
					.operation = MemoryOperation::BackingGrow,
					.appTick = 11,
					.frame = {2, 1},
					.timestampNs = 30});
	fixture.reporting.consume(11);
	std::array batch{MemoryCorrelationQuery{.subject = 100},
					 MemoryCorrelationQuery{
						 .frame = {1, 1}, .app_tick = 10, .submission = 7, .timestamp_ns = 15},
					 MemoryCorrelationQuery{.app_tick = 11, .timestamp_ns = 31},
					 MemoryCorrelationQuery{.app_tick = 0}};
	const auto work = fixture.reporting.correlate_errors(batch);
	assert(work.event_candidates <
		   12); // Indexed candidate chains, not four complete history reads.
	assert(batch[0].result.event_sequence == 1 &&
		   batch[0].result.match == MemoryCorrelationMatch::Explicit);
	assert(batch[1].result.event_sequence == 1 &&
		   batch[1].result.match == MemoryCorrelationMatch::Ambiguous);
	assert(batch[2].result.event_sequence == 3 &&
		   batch[2].result.match == MemoryCorrelationMatch::TemporalNearby);
	assert(batch[3].result.match == MemoryCorrelationMatch::NotCaptured);
	std::vector<RetainedMemoryEvent> pinned;
	pinned.reserve(8);
	auto progress = fixture.reporting.append_event_range(UINT64_MAX, 0, 10, 11, 8, pinned);
	assert(pinned.size() == 3 && progress.cursor == 3 && !progress.evicted);
	progress = fixture.reporting.append_event_range(progress.generation, progress.cursor, 10, 11, 8,
													pinned);
	assert(pinned.size() == 3);
	fixture.append({.source = 13,
					.lifetime = 103,
					.operation = MemoryOperation::BackingGrow,
					.appTick = 10,
					.timestampNs = 5}); // A worker can publish an older tick later.
	fixture.reporting.consume(12);
	progress = fixture.reporting.append_event_range(progress.generation, progress.cursor, 10, 11, 8,
													pinned);
	assert(pinned.size() == 4 && pinned.back().sequence == 4);
	fixture.append({.source = 14,
					.lifetime = 104,
					.operation = MemoryOperation::BackingGrow,
					.appTick = 12,
					.timestampNs = 40});
	fixture.reporting.consume(12);
	batch[0] = MemoryCorrelationQuery{.subject = 100, .app_tick = 9, .timestamp_ns = 1};
	batch[1] = MemoryCorrelationQuery{.subject = 104};
	static_cast<void>(fixture.reporting.correlate_errors(batch));
	assert(batch[0].result.match == MemoryCorrelationMatch::Evicted);
	assert(batch[1].result.event_sequence == 5 &&
		   batch[1].result.match == MemoryCorrelationMatch::Explicit);
	std::vector<RetainedMemoryEvent> bounded;
	bounded.reserve(1);
	const auto truncated = fixture.reporting.append_event_range(UINT64_MAX, 0, 0, 20, 1, bounded);
	assert(truncated.truncated && bounded.size() == 1);
	auto config = fixture.reporting.config();
	config.eventByteCapacity = 2 * sizeof(RetainedMemoryEvent);
	assert(fixture.reporting.setConfig(config));
	const auto reset = fixture.reporting.append_event_range(progress.generation, progress.cursor,
															10, 11, 8, pinned);
	assert(reset.reset && reset.evicted && pinned.size() == 4);
	assert(fixture.reporting.eventSnapshot().empty());
	auto freeze = fixture.reporting.freeze_history();
	fixture.append({.source = 500, .operation = MemoryOperation::BackingGrow, .appTick = 30});
	fixture.reporting.consume(30);
	batch[0] = MemoryCorrelationQuery{.subject = 500, .app_tick = 30};
	static_cast<void>(fixture.reporting.correlate_errors(batch));
	assert(batch[0].result.incomplete && batch[0].result.event_sequence == 0);
	assert(fixture.reporting.eventSnapshot().empty());
	freeze.reset();
	fixture.append({.source = 500, .operation = MemoryOperation::BackingGrow, .appTick = 31});
	fixture.reporting.consume(31);
	static_cast<void>(fixture.reporting.correlate_errors(batch));
	assert(batch[0].result.match == MemoryCorrelationMatch::Explicit &&
		   !batch[0].result.incomplete);
}

void reference_index_selection() {
	MemoryFixture fixture(true, 17);
	std::mt19937 generator(71);
	for (uint64_t round = 1; round <= 100; ++round) {
		fixture.append({.source = generator() % 7 + 1,
						.lifetime = generator() % 9 + 100,
						.operation = MemoryOperation::BackingGrow,
						.appTick = generator() % 8 + 1,
						.frame = {generator() % 3 + 1, generator() % 5 + 1},
						.submissionSerial = generator() % 4,
						.timestampNs = generator() % 100});
		fixture.reporting.consume(round);
		const auto events =
			fixture.reporting.eventSnapshot(); // Explicit test oracle, outside ingestion.
		std::array<MemoryCorrelationQuery, 16> queries{};
		for (auto& query : queries)
			query = {.frame = {generator() % 4, generator() % 6},
					 .subject = generator() % 2 ? generator() % 9 + 100 : generator() % 8,
					 .app_tick = generator() % 9,
					 .submission = generator() % 5,
					 .timestamp_ns = generator() % 100};
		static_cast<void>(fixture.reporting.correlate_errors(queries));
		for (const auto& query : queries) {
			const RetainedMemoryEvent* selected = nullptr;
			MemoryCorrelationMatch match = MemoryCorrelationMatch::NotCaptured;
			for (const auto& event : events)
				if (query.subject && (query.subject == event.operation.source ||
									  query.subject == event.operation.lifetime)) {
					selected = &event;
					match = MemoryCorrelationMatch::Explicit;
					break;
				}
			if (!selected) {
				size_t candidates = 0;
				for (const auto& event : events)
					if ((query.submission &&
						 query.submission == event.operation.submissionSerial) ||
						(query.frame && query.frame == event.operation.frame)) {
						++candidates;
						if (!selected || event.operation.timestampNs <= query.timestamp_ns)
							selected = &event;
					}
				if (selected)
					match = candidates > 1 ? MemoryCorrelationMatch::Ambiguous
										   : MemoryCorrelationMatch::SharedContext;
			}
			if (!selected) {
				for (const auto& event : events)
					if (event.operation.appTick == query.app_tick)
						if (!selected || event.operation.timestampNs <= query.timestamp_ns)
							selected = &event;
				if (selected)
					match = MemoryCorrelationMatch::TemporalNearby;
			}
			assert(query.result.event_sequence == (selected ? selected->sequence : 0));
			if (selected)
				assert(query.result.match == match);
		}
	}
}

void pending_and_sealed_evidence() {
	MemoryFixture fixture(true);
	DevErrorMonitoring monitoring;
	auto attachment = monitoring.attachCurrentThread("pending");
	attachment.recorder().setContext(DevErrorContext{.appTick = 10, .frame = {2, 10}});
	DevErrorReporting reporting(monitoring, {}, nullptr, &fixture.reporting);
	const auto id = monitoring.recordRaised(test_error(42), error_source,
											DevErrorRecordFlag::DevOnlyDiagnostic);
	fixture.reporting.consume(10);
	reporting.consumeThrough(10);
	assert(!reporting.occurrence(id)->memory.sealed);
	fixture.append({.source = 100,
					.lifetime = 42,
					.operation = MemoryOperation::BackingGrow,
					.appTick = 10,
					.frame = {2, 10}});
	fixture.reporting.consume(11);
	reporting.consumeThrough(11);
	assert(reporting.occurrence(id)->memory.confidence == DevErrorCorrelationConfidence::Explicit);
	fixture.reporting.consume(12);
	reporting.consumeThrough(12);
	const auto sealed = reporting.occurrence(id);
	assert(sealed->memory.sealed && sealed->timing.sealed &&
		   sealed->state == DevErrorOccurrenceState::Closed);
	const auto before = reporting.status();
	fixture.append(
		{.source = 101, .lifetime = 42, .operation = MemoryOperation::BackingGrow, .appTick = 10});
	fixture.reporting.consume(13);
	reporting.consumeThrough(13);
	assert(reporting.status().overhead.memoryCorrelationQueries ==
		   before.overhead.memoryCorrelationQueries);
	assert(reporting.occurrence(id)->memory.eventSequence == sealed->memory.eventSequence);
	assert(reporting.status().overhead.adviceUpdates == before.overhead.adviceUpdates);
	monitoring.recordStep(id, DevErrorStepKind::Resolution, test_error(42),
						  ErrorResolution::EvictedAndRetried);
	reporting.consumeThrough(14);
	assert(reporting.occurrence(id)->steps.size() == 2);
	assert(reporting.occurrence(id)->resolution == ErrorResolution::EvictedAndRetried);
	assert(reporting.status().overhead.memoryCorrelationQueries ==
		   before.overhead.memoryCorrelationQueries);
	assert(reporting.status().overhead.adviceUpdates > before.overhead.adviceUpdates);
	auto config = fixture.reporting.config();
	config.eventByteCapacity *= 2;
	assert(fixture.reporting.setConfig(config));
	assert(reporting.occurrence(id)->memory.state == DevErrorCorrelationState::Evicted);
	// Changing generations during the pending window must not substitute a new event stream.
	attachment.recorder().setContext(DevErrorContext{.appTick = 20});
	const auto pending = monitoring.recordRaised(test_error(80), error_source);
	fixture.reporting.consume(20);
	reporting.consumeThrough(20);
	config.eventByteCapacity *= 2;
	assert(fixture.reporting.setConfig(config));
	fixture.append(
		{.source = 500, .lifetime = 80, .operation = MemoryOperation::BackingGrow, .appTick = 20});
	fixture.reporting.consume(21);
	reporting.consumeThrough(21);
	assert(reporting.occurrence(pending)->memory.sealed &&
		   reporting.occurrence(pending)->memory.incomplete);
	assert(reporting.occurrence(pending)->memory.eventSequence == 0);
}

void unavailable_history_and_retention_stress() {
	MemoryFixture fixture;
	Probe probe{246};
	assert(
		fixture.memory.registerProbe({.source = 1000, .owner = &probe, .sample = Probe::sample}));
	DevErrorMonitoring monitoring;
	auto attachment = monitoring.attachCurrentThread("stress");
	DevErrorReporting reporting(monitoring, {}, nullptr, &fixture.reporting);
	uint64_t newest_id = 0;
	for (uint64_t tick = 1; tick <= 160; ++tick) {
		attachment.recorder().setContext(DevErrorContext{.appTick = tick, .frame = {1, tick}});
		for (size_t occurrence_index = 0; occurrence_index < 8; ++occurrence_index)
			newest_id = monitoring.recordRaised(test_error(), error_source,
												DevErrorRecordFlag::DevOnlyDiagnostic);
		fixture.reporting.consume(tick);
		reporting.consumeThrough(tick);
	}
	fixture.reporting.consume(162);
	reporting.consumeThrough(162);
	uint64_t backing_bytes = 0;
	for (const auto& source : fixture.reporting.currentSourceSnapshot())
		backing_bytes += source.value.backingAllocatedBytes;
	assert(backing_bytes >= 246 * 64 && fixture.reporting.current_backing_bytes() == backing_bytes);
	const auto history = reporting.occurrenceSnapshot();
	assert(history.size() == 512 && history.back().id == newest_id);
	for (size_t offset = 0; offset < history.size(); ++offset) {
		assert(history[offset].id == newest_id - history.size() + offset + 1);
		assert(history[offset].steps.size() == 1 && history[offset].memory.sealed);
		assert(history[offset].memory.checkpointTick == history[offset].firstAppTick);
		assert(history[offset].memory.eventSequence == 0);
	}
	const auto before = reporting.status();
	std::cout << "Repeated library errors mean_ms="
			  << before.overhead.consumeTimeNs / (1e6 * before.overhead.consumeCalls)
			  << " records=" << before.consumedRecords
			  << " queries=" << before.overhead.memoryCorrelationQueries << '\n';
	assert(before.consumeFailures == 0 && before.consumedRecords == 1280 &&
		   before.retainedOccurrences == 512);
	assert(before.pendingOccurrences == 0 && before.overhead.memoryCorrelationQueries == 1280);
	assert(before.overhead.memoryEventCandidates == 0);
	for (uint64_t tick = 163; tick <= 182; ++tick) {
		fixture.reporting.consume(tick);
		reporting.consumeThrough(tick);
	}
	const auto after = reporting.status();
	assert(after.overhead.memoryCorrelationQueries == before.overhead.memoryCorrelationQueries);
	assert(after.overhead.memorySourceChecks == before.overhead.memorySourceChecks);
	assert(after.overhead.adviceUpdates == before.overhead.adviceUpdates);
	std::cout << "Quiet saturated history mean_ms="
			  << (after.overhead.consumeTimeNs - before.overhead.consumeTimeNs) / 20e6 << '\n';
	auto config = reporting.config();
	config.retainedOccurrenceCapacity = 16;
	reporting.setConfig(config);
	assert(reporting.occurrenceSnapshot().size() == 16);
	monitoring.recordStep(newest_id, DevErrorStepKind::Resolution, test_error(),
						  ErrorResolution::EvictedAndRetried);
	reporting.consumeThrough(183);
	assert(reporting.occurrence(newest_id)->steps.size() == 2);
	assert(reporting.status().overhead.memoryCorrelationQueries ==
		   after.overhead.memoryCorrelationQueries);
	config.retainedByteBudget = sizeof(DevErrorOccurrence) + sizeof(DevErrorRecord) * 4;
	reporting.setConfig(config);
	for (uint64_t tick = 184; tick <= 200; ++tick) {
		attachment.recorder().setContext(DevErrorContext{.appTick = tick});
		assert(monitoring.recordRaised(test_error(), error_source));
		reporting.consumeThrough(tick);
		assert(reporting.status().retainedBytes <= config.retainedByteBudget);
	}
	assert(reporting.status().consumeFailures == 0);
}

void delayed_timing_and_capture_generations() {
	DevTiming timing;
	DevGpuTiming gpu(timing);
	DevTimingReporting timing_reporting(timing, gpu);
	assert(timing_reporting.begin_capture(10) == 1);
	timing_reporting.admit_tick(10, timing.nowNs());
	auto timing_attachment = timing.attachCurrentThread("late.timing");
	timing_attachment.recorder().setFrameContext({1, 10}, 10);
	const FlowDefinitionID definition{.value = 9000};
	const FlowElementID instance{.value = 77};
	const auto descriptor = makeTimingDescriptor(TimingCategory::Element, TimingZoneRole::Work,
												 "test.element.evidence");
	const auto token = timing_attachment.recorder().tryBegin(
		descriptor, TimingEntityRef::element(definition, instance));
	assert(token);
	DevErrorMonitoring monitoring;
	auto attachment = monitoring.attachCurrentThread("late.error");
	attachment.recorder().setContext(DevErrorContext{
		.appTick = 10, .frame = {1, 10}, .timingTrack = timing_attachment.recorder().trackId()});
	const auto id = monitoring.recordRaised(test_error(), error_source);
	DevErrorReporting reporting(monitoring, {}, &timing_reporting);
	timing_reporting.consumeThrough(10);
	reporting.consumeThrough(10);
	assert(!reporting.occurrence(id)->timing.sealed);
	timing_attachment.recorder().end(token);
	timing_reporting.consumeThrough(11);
	reporting.consumeThrough(11);
	assert(reporting.occurrence(id)->timing.containingInvocation == token.invocationId);
	const auto summaries = reporting.occurrence_summaries();
	assert(summaries.size() == 1 && summaries.front().id == id);
	assert(summaries.front().timing.entity.kind == TimingEntityKind::ElementInstance);
	assert(summaries.front().timing.entity.primaryId == instance.value);
	assert(summaries.front().timing.entity.secondaryId == definition.value);
	reporting.consumeThrough(12);
	assert(reporting.occurrence(id)->timing.sealed);
	const auto before = reporting.status();
	assert(timing_reporting.begin_capture(20) == 2);
	reporting.consumeThrough(13);
	assert(reporting.status().overhead.timingCorrelationQueries ==
		   before.overhead.timingCorrelationQueries);
	assert(reporting.occurrence(id)->timing.state == DevErrorCorrelationState::Evicted);
	assert(reporting.occurrence_summaries().front().timing.state ==
		   DevErrorCorrelationState::Evicted);
	assert(reporting.status().overhead.adviceUpdates == before.overhead.adviceUpdates);
}

void evicted_identity_reappearing_in_the_same_batch() {
	MemoryFixture memory;
	memory.reporting.consume(10);
	DevErrorMonitoring monitoring;
	auto attachment = monitoring.attachCurrentThread("reappearing");
	attachment.recorder().setContext(DevErrorContext{.appTick = 10});
	DevErrorReportingConfig config;
	config.retainedOccurrenceCapacity = 1;
	DevErrorReporting reporting(monitoring, config, nullptr, &memory.reporting);
	const auto first = monitoring.recordRaised(test_error(), error_source);
	assert(monitoring.recordRaised(test_error(), error_source));
	monitoring.recordStep(first, DevErrorStepKind::Resolution, test_error(),
						  ErrorResolution::EvictedAndRetried);
	reporting.consumeThrough(10);
	assert(reporting.status().consumeFailures == 0);
	assert(reporting.status().retainedOccurrences == 1);
	assert(reporting.status().overhead.memoryCorrelationQueries == 1);
	assert(reporting.occurrence(first)->steps.size() == 1);
	assert(reporting.occurrence(first)->memory.sealed);
}

void retained_event_stream_stress() {
	MemoryFixture fixture(true, 4096);
	DevErrorMonitoring monitoring;
	auto attachment = monitoring.attachCurrentThread("retained.events");
	DevErrorReporting reporting(monitoring, {}, nullptr, &fixture.reporting);
	for (uint64_t tick = 1; tick <= 64; ++tick) {
		for (uint64_t offset = 0; offset < 1024; ++offset)
			fixture.append({.source = offset % 256 + 1,
							.lifetime = tick * 1024 + offset,
							.operation = MemoryOperation::BackingGrow,
							.appTick = tick});
		fixture.reporting.consume(tick);
		attachment.recorder().setContext(DevErrorContext{.appTick = tick});
		for (uint64_t offset = 0; offset < 8; ++offset)
			assert(monitoring.recordRaised(test_error(tick * 1024 + offset), error_source,
										   DevErrorRecordFlag::DevOnlyDiagnostic));
		reporting.consumeThrough(tick);
	}
	fixture.reporting.consume(66);
	reporting.consumeThrough(66);
	const auto status = reporting.status();
	assert(status.consumeFailures == 0 && status.consumedRecords == 512 &&
		   status.pendingOccurrences == 0);
	assert(status.overhead.memoryEventCandidates == status.overhead.memoryCorrelationQueries);
	std::cout << "Retained event stream error mean_ms="
			  << status.overhead.consumeTimeNs / (1e6 * status.overhead.consumeCalls)
			  << " events=" << fixture.reporting.status().events.retainedCount
			  << " candidates=" << status.overhead.memoryEventCandidates << '\n';
	const auto before = status.overhead.memoryCorrelationQueries;
	fixture.reporting.consume(67);
	reporting.consumeThrough(67);
	assert(reporting.status().overhead.memoryCorrelationQueries == before);
}

void pinned_capture_preserves_generation_on_reset() {
	MemoryFixture fixture(true, 4);
	fixture.append(
		{.source = 100, .lifetime = 42, .operation = MemoryOperation::BackingGrow, .appTick = 10});
	fixture.reporting.consume(10);
	DevErrorMonitoring monitoring;
	auto attachment = monitoring.attachCurrentThread("pinned.reset");
	attachment.recorder().setContext(DevErrorContext{.appTick = 10});
	DevErrorReportingConfig config;
	config.triggers.emplace_back(DevErrorTrigger{.code = ErrorCode::StorageCapacityExceeded,
												 .preOccurrenceTicks = 0,
												 .postOccurrenceTicks = 5});
	DevErrorReporting reporting(monitoring, config, nullptr, &fixture.reporting);
	const auto id = monitoring.recordRaised(test_error(42), error_source,
											DevErrorRecordFlag::DevOnlyDiagnostic);
	reporting.consumeThrough(10);
	const auto capture_id = reporting.occurrence(id)->captureId;
	const auto before = reporting.capture(capture_id);
	assert(before && before->memoryEvents.size() == 1 && before->memory_cursor == 1);
	auto memory_config = fixture.reporting.config();
	memory_config.eventByteCapacity *= 2;
	assert(fixture.reporting.setConfig(memory_config));
	fixture.reporting.consume(11);
	reporting.consumeThrough(11);
	const auto after = reporting.capture(capture_id);
	assert(after && after->memory_sealed && after->memoryTruncated && after->memoryHistoryEvicted);
	assert(after->memory_generation == before->memory_generation &&
		   after->memory_cursor == before->memory_cursor);
	assert(after->memoryEvents.size() == 1 && after->memoryEvents.front().operation.source == 100);
	fixture.reporting.consume(15);
	reporting.consumeThrough(15);
	assert(reporting.capture(capture_id)->state == DevErrorTriggeredCaptureState::Complete);
}

void stalled_publication_reports_incomplete_evidence() {
	MemoryFixture fixture(true);
	fixture.reporting.consume(10);
	DevErrorMonitoring monitoring;
	auto attachment = monitoring.attachCurrentThread("stalled.publication");
	attachment.recorder().setContext(DevErrorContext{.appTick = 10});
	DevErrorReporting reporting(monitoring, {}, nullptr, &fixture.reporting);
	const auto id = monitoring.recordRaised(test_error(42), error_source);
	reporting.consumeThrough(10);
	const auto queries = reporting.status().overhead.memoryCorrelationQueries;
	reporting.consumeThrough(12); // Memory has not serviced the rest of the grace window.
	const auto occurrence = reporting.occurrence(id);
	assert(occurrence && occurrence->memory.sealed && occurrence->memory.incomplete);
	assert(std::ranges::any_of(occurrence->advice,
							   [](const auto& advice) { return advice.descriptorId == 0x1802u; }));
	fixture.append(
		{.source = 100, .lifetime = 42, .operation = MemoryOperation::BackingGrow, .appTick = 10});
	fixture.reporting.consume(13);
	reporting.consumeThrough(13);
	assert(reporting.status().overhead.memoryCorrelationQueries == queries);
	assert(reporting.occurrence(id)->memory.eventSequence == 0);
}

void indexed_large_history() {
	MemoryFixture fixture(true, 4096);
	for (uint64_t tick = 1; tick <= 8; ++tick) {
		for (uint64_t offset = 0; offset < 1024; ++offset)
			fixture.append({.source = offset % 32 + 1,
							.lifetime = tick * 1024 + offset,
							.operation = MemoryOperation::BackingGrow,
							.appTick = tick});
		fixture.reporting.consume(tick);
	}
	std::array<MemoryCorrelationQuery, 512> queries{};
	for (size_t offset = 0; offset < queries.size(); ++offset)
		queries[offset].subject = 8 * 1024 + offset;
	const auto work = fixture.reporting.correlate_errors(queries);
	assert(work.event_candidates == queries.size());
	for (const auto& query : queries)
		assert(query.result.match == MemoryCorrelationMatch::Explicit);
	assert(fixture.reporting.status().events.retainedCount == 4096);
}
} // namespace
int main() {
	indexed_queries_and_append_cursors();
	reference_index_selection();
	pending_and_sealed_evidence();
	unavailable_history_and_retention_stress();
	delayed_timing_and_capture_generations();
	evicted_identity_reappearing_in_the_same_batch();
	retained_event_stream_stress();
	pinned_capture_preserves_generation_on_reset();
	stalled_publication_reports_incomplete_evidence();
	indexed_large_history();
	std::cout << "Error reporting ring, pending lifecycle, indexed correlation, and cursor checks "
				 "passed\n";
}
