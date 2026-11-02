#ifdef NDEBUG
#undef NDEBUG
#endif

#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevDiagnosticsExport.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>

using namespace FlowUi;
using namespace FlowUi::devSystems;

namespace {

constexpr auto first_source = makeDevErrorSource("diagnostics.first");
constexpr auto later_source = makeDevErrorSource("diagnostics.later");
constexpr auto breadcrumb_descriptor = makeDevErrorBreadcrumb("diagnostics.context");

[[nodiscard]] FlowUiError test_error(uint64_t subject = 0) noexcept {
	return makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ResourceAllocatePersistent,
					 subject, 19, 73);
}

void occurrence_starts_and_late_evidence() {
	DevErrorMonitoring monitoring;
	DevErrorReporting reporting(monitoring);
	auto attachment = monitoring.attachCurrentThread("diagnostics-tests");
	DevErrorOccurrenceId last_id = 0;
	for (uint64_t tick = 1; tick <= 600; ++tick) {
		attachment.recorder().setContext({.appTick = tick, .frame = {1, tick}});
		last_id = monitoring.recordRaised(test_error(tick), first_source);
		assert(last_id);
		reporting.consumeThrough(tick);
	}
	auto snapshot = reporting.diagnostics_snapshot();
	assert(snapshot && snapshot->rows.size() == 512);
	assert(snapshot->rows.front().first_app_tick == 89);
	assert(snapshot->status.lastConsumedAppTick == 600);
	assert(snapshot->status.evictedOccurrences == 88);
	assert(reporting.groupSnapshot().size() == 1);
	assert(reporting.groupSnapshot().front().occurrenceCount == 512);
	const auto first_timestamp = snapshot->rows.back().first_timestamp_ns;
	const auto revision = snapshot->revision;
	attachment.recorder().setContext({.appTick = 601, .frame = {2, 900}, .timingTrack = 4});
	monitoring.recordStep(last_id, DevErrorStepKind::Resolution, test_error(600),
						  ErrorResolution::UsedFallback, later_source);
	reporting.consumeThrough(601);
	snapshot = reporting.diagnostics_snapshot();
	assert(snapshot && snapshot->revision > revision);
	assert(snapshot->rows.size() == 512);
	const auto& row = snapshot->rows.back();
	assert(row.first_app_tick == 600 && row.last_app_tick == 601);
	assert(row.first_timestamp_ns == first_timestamp);
	assert(row.resolution == ErrorResolution::UsedFallback);
	assert(row.steps.size() == 2);
	assert(row.steps[0].context.frame.window == 1 && row.steps[1].context.frame.window == 2);
	assert(row.steps[0].source_id == first_source.id && row.steps[1].source_id == later_source.id);
	assert(row.steps[1].context.frame.frameNumber == 900 && row.steps[1].context.timingTrack == 4);
	assert(snapshot->sources.size() == 2);
	assert(reporting.groupSnapshot().front().occurrenceCount == 512);
	assert(!reporting.diagnostics_investigation(last_id, 1));
	assert(!reporting.diagnostics_investigation(UINT64_MAX));
	const auto latest_revision = snapshot->revision;
	reporting.consumeThrough(602);
	snapshot = reporting.diagnostics_snapshot();
	assert(snapshot && snapshot->revision > latest_revision &&
		   snapshot->status.lastConsumedAppTick == 602);
	auto config = reporting.config();
	config.maximumAdvicePerOccurrence = 2;
	reporting.setConfig(config);
	assert(reporting.status().revision > snapshot->revision);
}

void frozen_ownership_and_export() {
	DevErrorMonitoring monitoring;
	DevErrorReporting reporting(monitoring,
								{.triggers = {{.preOccurrenceTicks = 1, .postOccurrenceTicks = 1}},
								 .retainedOccurrenceCapacity = 2});
	auto attachment = monitoring.attachCurrentThread("diagnostics-ownership");
	attachment.recorder().setContext({.appTick = 10, .frame = {7, 25}});
	monitoring.recordBreadcrumb(breadcrumb_descriptor, UINT64_MAX, 42);
	const auto occurrence_id =
		monitoring.recordRaised(test_error(UINT64_MAX - 7), first_source, DevErrorRecordFlag::None,
								"native \"message\"\nwith tab\tand slash\\");
	reporting.consumeThrough(10);
	auto frozen_result = reporting.diagnostics_investigation();
	assert(frozen_result && frozen_result->occurrences.size() == 1);
	assert(!frozen_result->snapshot.sources.empty());
	assert(frozen_result->snapshot.sources.front().name == first_source.name);
	assert(!frozen_result->captures.empty());
	assert(frozen_result->owned_bytes == diagnostics_owned_bytes(*frozen_result));
	DevDiagnosticsInvestigation frozen = *frozen_result;
	DevDiagnosticsInvestigation moved = std::move(frozen);
	assert(!moved.advice_text.empty());
	assert(moved.occurrences.front().advice.front().title.data() ==
		   moved.advice_text.front().title.data());
	for (uint64_t tick = 11; tick <= 15; ++tick) {
		attachment.recorder().setContext({.appTick = tick});
		assert(monitoring.recordRaised(test_error(tick), later_source));
		reporting.consumeThrough(tick);
	}
	assert(!reporting.occurrence(occurrence_id));
	assert(moved.snapshot.status.lastConsumedAppTick == 10);
	assert(moved.occurrences.front().id == occurrence_id);
	assert(moved.occurrences.front().steps.front().context.frame.window == 7);
	auto selected = select_diagnostics_investigation(moved, occurrence_id);
	assert(selected && selected->selected_id == occurrence_id && selected->occurrences.size() == 1);
	assert(!select_diagnostics_investigation(moved, UINT64_MAX));
	DevDiagnosticsExportMetadata metadata{
		.filter_description = "Source unavailable or name contains \"first\"", .frozen = true};
	auto json = serialize_diagnostics_json(*selected, metadata);
	auto markdown = serialize_diagnostics_markdown(*selected, metadata);
	assert(json && markdown);
	assert(json->find("18446744073709551608") != std::string::npos);
	assert(json->find("native \\\"message\\\"\\u000awith tab\\u0009and slash\\\\") !=
		   std::string::npos);
	assert(json->find("\"schema_version\":1") != std::string::npos);
	assert(json->find("\"frozen\":true") != std::string::npos);
	assert(markdown->find(*json) != std::string::npos);
	assert(markdown->find("Recommended action") != std::string::npos);
	const auto unique_suffix =
		std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	const auto directory =
		std::filesystem::temp_directory_path() / ("flowui-diagnostics-" + unique_suffix);
	std::filesystem::create_directory(directory);
	const auto prefix = directory / "investigation";
	assert(write_diagnostics_export(*selected, prefix, metadata));
	assert(std::filesystem::exists(directory / "investigation.json"));
	assert(std::filesystem::exists(directory / "investigation.md"));
	assert(!write_diagnostics_export(*selected, prefix, metadata));
	assert(!write_diagnostics_export(*selected, directory / "missing" / "investigation", metadata));
	assert(!write_diagnostics_export(DevDiagnosticsInvestigation{}, directory / "empty", metadata));
	std::ifstream written(directory / "investigation.json", std::ios::binary);
	std::string written_text((std::istreambuf_iterator<char>(written)),
							 std::istreambuf_iterator<char>());
	assert(written_text == *json);
	std::filesystem::remove_all(directory);
}

[[nodiscard]] DevDiagnosticsInvestigation complete_evidence_fixture() {
	DevDiagnosticsInvestigation investigation;
	investigation.snapshot.status.lastConsumedAppTick = 140;
	investigation.snapshot.revision = 18;
	investigation.snapshot.status.revision = 18;
	investigation.snapshot.status.quality.lostFatalCapsules = 31;
	investigation.snapshot.sources.emplace_back(
		DevDiagnosticsSource{.name = "source \"with quotes\"",
							 .file = "project/source.cpp",
							 .function = "raise",
							 .id = UINT64_MAX - 1,
							 .line = 47,
							 .column = 3});
	DevErrorOccurrence occurrence;
	occurrence.id = UINT64_MAX;
	occurrence.error = test_error(UINT64_MAX - 7);
	occurrence.firstAppTick = 139;
	occurrence.firstTimestampNs = UINT64_MAX - 2;
	occurrence.flags = DevErrorRecordFlag::SnapshotStale;
	occurrence.captureId = 8;
	occurrence.timing = {.state = DevErrorCorrelationState::Ambiguous,
						 .confidence = DevErrorCorrelationConfidence::TemporalNearby,
						 .appTick = 139,
						 .reportRevision = 95,
						 .capture_generation = 71,
						 .sealed = true,
						 .incomplete = true};
	occurrence.memory = {.eventSequence = 54,
						 .source = 100,
						 .checkpointTick = 139,
						 .storageMutationSequence = UINT64_MAX - 3,
						 .history_generation = 72,
						 .state = DevErrorCorrelationState::Evicted,
						 .confidence = DevErrorCorrelationConfidence::SharedContext};
	DevErrorRecord step;
	step.sequence = UINT64_MAX - 4;
	step.occurrence = occurrence.id;
	step.sourceId = UINT64_MAX - 1;
	step.stackId = 9;
	step.context = {.appTick = 139,
					.frame = {7, 99},
					.submissionSerial = 112,
					.primaryEntity = 123,
					.secondaryEntity = 234,
					.timingTrack = 88,
					.phase = 65535};
	step.threadTrack = 44;
	step.nativeCategory = 1234;
	step.nativeTextLength = 7;
	step.nativeText[0] = static_cast<char>(0xff);
	step.nativeText[1] = '"';
	step.nativeText[2] = '\0';
	step.nativeText[3] = 'a';
	step.nativeText[4] = static_cast<char>(0xe2);
	step.nativeText[5] = static_cast<char>(0x98);
	step.nativeText[6] = static_cast<char>(0x83);
	assert(step.evidence.append(DevErrorEvidenceKey::Custom, UINT64_MAX, 65535));
	occurrence.steps.emplace_back(step);
	DevErrorSnapshot snapshot{.occurrence = occurrence.id,
							  .source = 200,
							  .expectedRevision = 12,
							  .capturedRevision = 11,
							  .outcome = DevErrorSnapshotOutcome::StaleRevision,
							  .valueCount = 1};
	snapshot.values[0] = {.key = DevErrorEvidenceKey::Custom, .index = 17, .value = UINT64_MAX - 5};
	occurrence.snapshots.emplace_back(snapshot);
	investigation.occurrences.emplace_back(std::move(occurrence));
	DevErrorStackTrace stack{.id = 9,
							 .hash = UINT64_MAX - 6,
							 .moduleIdentity = 16,
							 .buildIdentity = 17,
							 .status = DevErrorStackStatus::Truncated,
							 .frameCount = 2};
	stack.frames[0] = UINTPTR_MAX;
	stack.frames[1] = 0x12345678;
	investigation.stacks.emplace_back(stack);
	DevErrorTriggeredCapture capture{.id = 8,
									 .triggerOccurrence = UINT64_MAX,
									 .firstAppTick = 130,
									 .lastAppTick = 150,
									 .timingTruncated = true,
									 .memoryTruncated = true,
									 .timing_generation = 71,
									 .timing_retained_ticks = 5,
									 .memory_generation = 72,
									 .memory_cursor = 54,
									 .memory_sealed = true};
	capture.memoryEvents.emplace_back(
		RetainedMemoryEvent{.sequence = 54,
							.operation = {.source = 100,
										  .lifetime = 101,
										  .operation = MemoryOperation::BackingGrow,
										  .appTick = 139,
										  .frame = {7, 99},
										  .submissionSerial = 112,
										  .timestampNs = UINT64_MAX - 8,
										  .bytesBefore = 1024,
										  .bytesAfter = 2048,
										  .bytesChanged = 1024,
										  .threadTrack = 43,
										  .debugName = 33}});
	investigation.captures.emplace_back(std::move(capture));
	DevErrorFatalCapsule fatal;
	fatal.occurrence = UINT64_MAX;
	fatal.byteSize = sizeof(DevErrorFatalCapsule);
	fatal.availableInputs = DevErrorFatalCapabilityInput::EmergencyCapsule |
							DevErrorFatalCapabilityInput::RecentBreadcrumbs;
	fatal.safePoint.revision = UINT64_MAX - 9;
	fatal.safePoint.memoryDroppedOperations = 56;
	fatal.breadcrumbCount = 1;
	fatal.breadcrumbs[0] = {.sequence = 78,
							.timestampNs = 79,
							.descriptorId = 80,
							.primaryValue = 81,
							.secondaryValue = 82,
							.occurrence = UINT64_MAX,
							.threadTrack = 83};
	fatal.stackFrameCount = 1;
	fatal.stackFrames[0] = UINTPTR_MAX;
	investigation.fatal_capsule = fatal;
	investigation.owned_bytes = diagnostics_owned_bytes(investigation);
	return investigation;
}

void complete_evidence_serialization() {
	const auto investigation = complete_evidence_fixture();
	const auto json = serialize_diagnostics_json(investigation);
	assert(json);
	for (const auto field :
		 {"\"sequence\":18446744073709551611", "\"threadTrack\":44", "\"timingTrack\":88",
		  "\"index\":65535", "\"value\":18446744073709551615", "\"expectedRevision\":12",
		  "\"capturedRevision\":11", "\"frame_count\":2",
		  "\"storageMutationSequence\":18446744073709551612", "\"bytesBefore\":1024",
		  "\"bytesAfter\":2048", "\"bytesChanged\":1024", "\"debugName\":33",
		  "\"memoryDroppedOperations\":56", "\"lostFatalCapsules\":31",
		  "\"fatal_capsule_available\":true", "\"native_text_bytes\":[255,34,0,97,226,152,131]"})
		assert(json->find(field) != std::string::npos);
}

} // namespace

int main(int argument_count, char** arguments) {
	if (argument_count == 2 && std::string_view(arguments[1]) == "--json") {
		const auto json = serialize_diagnostics_json(complete_evidence_fixture());
		if (!json)
			return 1;
		std::cout << *json;
		return 0;
	}
	occurrence_starts_and_late_evidence();
	frozen_ownership_and_export();
	complete_evidence_serialization();
	std::cout << "Diagnostics reporting, owned snapshots and export checks passed\n";
}
