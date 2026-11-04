#include "devSystems/devAgenticWorkflow/AgenticAdapters.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevErrorRecorder.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevDiagnosticsExport.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceExport.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include <algorithm>
#include <filesystem>
#include <ostream>
#include <sstream>
#include <streambuf>

namespace FlowUi::devSystems::agentic {
namespace {
class BoundedOutput final : public std::streambuf {
public:
	explicit BoundedOutput(std::uint64_t maximum) : maximum_(maximum) {
		bytes_.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(maximum, 1024u * 1024u)));
	}
	[[nodiscard]] std::string take() noexcept { return std::move(bytes_); }

protected:
	std::streamsize xsputn(const char* source, std::streamsize count) override {
		if (count < 0 || static_cast<std::uint64_t>(count) > maximum_ - bytes_.size())
			return 0;
		bytes_.append(source, static_cast<std::size_t>(count));
		return count;
	}
	int_type overflow(int_type character) override {
		if (traits_type::eq_int_type(character, traits_type::eof()))
			return traits_type::not_eof(character);
		if (bytes_.size() >= maximum_)
			return traits_type::eof();
		bytes_.push_back(traits_type::to_char_type(character));
		return character;
	}

private:
	std::string bytes_;
	std::uint64_t maximum_;
};
[[nodiscard]] std::uint64_t counter_delta(std::uint64_t current, std::uint64_t earlier,
										  bool& reset) noexcept {
	if (current < earlier) {
		reset = true;
		return current;
	}
	return current - earlier;
}
[[nodiscard]] std::string relative_source(std::string_view file) {
	if (file.empty())
		return {};
	auto path = std::filesystem::path(file);
	if (path.is_absolute())
		path = path.lexically_relative(std::filesystem::current_path());
	path = path.lexically_normal();
	if (path.empty())
		return {};
	for (const auto& component : path)
		if (component == "..")
			return {};
	return path.generic_string();
}
[[nodiscard]] Json source(std::uint64_t identity, std::string_view source_name,
						  std::string_view file, std::string_view function, std::uint32_t line,
						  std::uint32_t column, std::string_view owner_scope) {
	return Json::make_object({{"source_id", decimal(identity)},
							  {"owner_scope", owner_scope},
							  {"name", source_name},
							  {"file", relative_source(file)},
							  {"symbol", function},
							  {"line", Json::number(line)},
							  {"column", Json::number(column)},
							  {"repository_id", "application_or_flowui"}});
}
[[nodiscard]] std::string_view correlation_state(DevErrorCorrelationState state) noexcept {
	switch (state) {
	case DevErrorCorrelationState::Available:
		return "available";
	case DevErrorCorrelationState::Evicted:
		return "evicted";
	case DevErrorCorrelationState::NotCaptured:
		return "not_captured";
	case DevErrorCorrelationState::Ambiguous:
		return "ambiguous";
	}
	return "not_captured";
}
[[nodiscard]] std::string_view
correlation_method(DevErrorCorrelationConfidence confidence) noexcept {
	switch (confidence) {
	case DevErrorCorrelationConfidence::Explicit:
		return "explicit_identity";
	case DevErrorCorrelationConfidence::SharedContext:
		return "shared_context";
	case DevErrorCorrelationConfidence::TemporalNearby:
		return "temporal_proximity";
	case DevErrorCorrelationConfidence::None:
		return "unresolved";
	}
	return "unresolved";
}
[[nodiscard]] std::string_view
correlation_confidence(DevErrorCorrelationConfidence confidence) noexcept {
	switch (confidence) {
	case DevErrorCorrelationConfidence::Explicit:
		return "explicit";
	case DevErrorCorrelationConfidence::SharedContext:
		return "shared_context";
	case DevErrorCorrelationConfidence::TemporalNearby:
		return "temporal_nearby";
	case DevErrorCorrelationConfidence::None:
		return "none";
	}
	return "none";
}
void redact_paths(Json& value) {
	for (auto& [key, member] : value.object) {
		if (key == "file" || key == "source_file" || key == "header_source_file" ||
			key == "source_path")
			member = Json{};
		else
			redact_paths(member);
	}
	for (auto& member : value.array)
		redact_paths(member);
}
[[nodiscard]] Json occurrence_selector(const DevErrorOccurrence& occurrence) {
	auto selector = Json::make_object(
		{{"artifact_id", "runtime_diagnostics"}, {"occurrence_id", decimal(occurrence.id)}});
	if (!occurrence.steps.empty())
		selector.set("record_sequence", decimal(occurrence.steps.front().sequence));
	return selector;
}
void note_failure(AgenticCaptureSnapshot& snapshot, std::string_view section,
				  SectionCoverage coverage, std::string_view reason) {
	coverage.status = SectionStatus::failed;
	coverage.reasons.emplace_back(reason);
	snapshot.coverage.emplace_back(section, std::move(coverage));
	snapshot.partial = true;
}
void timing_coverage(AgenticCaptureSnapshot& snapshot, const TimingCaptureSnapshot& timing,
					 const TimingReportingStatus& status,
					 const PerformanceCaptureStatus& controller) {
	SectionCoverage coverage;
	coverage.generation = timing.generation;
	coverage.producer_records = status.capture_admitted_ticks;
	coverage.retained_records = timing.reports.size();
	coverage.dropped_records = status.capture_dropped_records + status.capture_ingestion_failures +
							   status.capture_gpu_failures;
	coverage.evicted_records = status.capture_overwritten_ticks;
	coverage.pending_records = controller.pending_measurements;
	coverage.finalization_watermark = snapshot.end_app_tick_exclusive;
	auto expected = snapshot.first_app_tick;
	for (const auto& tick : timing.reports) {
		if (tick.appTick < snapshot.first_app_tick ||
			tick.appTick >= snapshot.end_app_tick_exclusive)
			continue;
		if (tick.appTick > expected)
			coverage.gaps.emplace_back(TickRange{expected, tick.appTick});
		if (!coverage.covered_ranges.empty() &&
			coverage.covered_ranges.back().end_app_tick_exclusive == tick.appTick)
			coverage.covered_ranges.back().end_app_tick_exclusive = tick.appTick + 1;
		else
			coverage.covered_ranges.emplace_back(TickRange{tick.appTick, tick.appTick + 1});
		expected = tick.appTick + 1;
	}
	if (expected < snapshot.end_app_tick_exclusive)
		coverage.gaps.emplace_back(TickRange{expected, snapshot.end_app_tick_exclusive});
	coverage.status = coverage.gaps.empty() && !coverage.dropped_records &&
							  !coverage.evicted_records && !coverage.pending_records &&
							  !controller.incomplete
						  ? SectionStatus::complete
						  : SectionStatus::partial;
	if (coverage.status == SectionStatus::partial)
		snapshot.partial = true;
	snapshot.coverage.emplace_back("performance", std::move(coverage));
}
[[nodiscard]] Status copy_memory(AgenticCaptureSnapshot& snapshot, DevMemoryReporting& reporting,
								 const MemoryReportingStatus& baseline) {
	SectionCoverage coverage;
	const auto status = reporting.status();
	coverage.generation = status.generation;
	coverage.finalization_watermark = status.lastConsumedAppTick;
	// Publication generation advances on every consume tick; history identity detects resets.
	coverage.uncertain = status.history_epoch != baseline.history_epoch ||
						 status.history_storage_generation != baseline.history_storage_generation;
	coverage.dropped_records = counter_delta(
		status.quality.droppedOperations, baseline.quality.droppedOperations, coverage.uncertain);
	coverage.evicted_records = counter_delta(status.segments.overwriteCount,
											 baseline.segments.overwriteCount, coverage.uncertain);
	std::vector<MemorySampleSegment> segments;
	const auto maximum_records = snapshot.request.limits.maximum_records;
	segments.reserve(static_cast<std::size_t>(
		std::min<std::uint64_t>(status.segments.retainedCount, maximum_records)));
	bool truncated = false;
	const bool coherent = reporting.visit_history_chunks(
		status,
		[&](std::span<const MemorySampleSegment> chunk) {
			for (const auto& retained : chunk) {
				if (retained.endTickExclusive <= snapshot.first_app_tick ||
					retained.beginTick >= snapshot.end_app_tick_exclusive)
					continue;
				if (segments.size() >= maximum_records ||
					(segments.size() + 1) * sizeof(MemorySampleSegment) >
						snapshot.request.limits.maximum_snapshot_bytes / 4) {
					truncated = true;
					break;
				}
				auto clipped = retained;
				clipped.beginTick = std::max(retained.beginTick, snapshot.first_app_tick);
				clipped.endTickExclusive =
					std::min(retained.endTickExclusive, snapshot.end_app_tick_exclusive);
				clipped.beginNs = std::max(retained.beginNs, snapshot.measurement_begin_ns);
				clipped.endNs = std::min(
					{retained.endNs, snapshot.measurement_end_ns, status.last_consumed_ns});
				if (clipped.endNs > clipped.beginNs)
					segments.emplace_back(std::move(clipped));
			}
		},
		256);
	std::vector<RetainedMemoryEvent> events;
	events.reserve(static_cast<std::size_t>(
		std::min<std::uint64_t>(status.events.retainedCount, maximum_records)));
	const auto event_read = reporting.append_event_range(
		status.history_storage_generation, 0, snapshot.first_app_tick,
		snapshot.end_app_tick_exclusive ? snapshot.end_app_tick_exclusive - 1 : 0,
		static_cast<std::size_t>(maximum_records), events);
	const auto after_read = reporting.status();
	coverage.uncertain = coverage.uncertain || !coherent || event_read.reset ||
						 event_read.evicted || after_read.generation != status.generation ||
						 after_read.history_epoch != status.history_epoch;
	coverage.retained_records = segments.size() + events.size();
	coverage.producer_records =
		counter_delta(status.consumedOperations, baseline.consumedOperations, coverage.uncertain);
	coverage.dropped_records += truncated || event_read.truncated ? 1 : 0;
	std::vector<TickRange> ranges;
	ranges.reserve(segments.size());
	for (const auto& segment : segments)
		ranges.emplace_back(TickRange{segment.beginTick, segment.endTickExclusive});
	std::sort(ranges.begin(), ranges.end(), [](const auto& first, const auto& second) {
		return first.first_app_tick < second.first_app_tick;
	});
	for (const auto& range : ranges) {
		if (!coverage.covered_ranges.empty() &&
			range.first_app_tick <= coverage.covered_ranges.back().end_app_tick_exclusive)
			coverage.covered_ranges.back().end_app_tick_exclusive =
				std::max(coverage.covered_ranges.back().end_app_tick_exclusive,
						 range.end_app_tick_exclusive);
		else
			coverage.covered_ranges.emplace_back(range);
	}
	auto expected_tick = snapshot.first_app_tick;
	for (const auto& range : coverage.covered_ranges) {
		if (range.first_app_tick > expected_tick)
			coverage.gaps.emplace_back(TickRange{expected_tick, range.first_app_tick});
		expected_tick = std::max(expected_tick, range.end_app_tick_exclusive);
	}
	if (expected_tick < snapshot.end_app_tick_exclusive)
		coverage.gaps.emplace_back(TickRange{expected_tick, snapshot.end_app_tick_exclusive});
	if (segments.empty() || !coverage.gaps.empty()) {
		coverage.uncertain = true;
		coverage.reasons.emplace_back(segments.empty() ? "no_interval_samples"
													   : "memory_interval_has_gaps");
	}
	coverage.reasons.emplace_back("coverage_is_union_of_observed_source_segments");
	coverage.status = coverage.uncertain || coverage.dropped_records || coverage.evicted_records
						  ? SectionStatus::partial
						  : SectionStatus::complete;
	if (coverage.status == SectionStatus::partial) {
		snapshot.partial = true;
		coverage.reasons.emplace_back("memory_epoch_loss_or_uncertain_interval_coverage");
	}
	BoundedOutput csv_buffer(snapshot.request.limits.maximum_artifact_bytes);
	std::ostream csv(&csv_buffer);
	csv << "source_id,window_id,sequence,epoch,first_app_tick,end_app_tick_exclusive,begin_ns,end_"
		   "ns,logical_live_bytes,backing_bytes,reusable_bytes,retired_bytes,object_count,capacity_"
		   "count,value_fields,flags\n";
	long double weighted_live = 0;
	std::uint64_t total_weight = 0;
	for (const auto& segment : segments) {
		const auto& value = segment.value;
		csv << value.source << ',' << value.window << ',' << segment.sequence << ','
			<< segment.history_epoch << ',' << segment.beginTick << ',' << segment.endTickExclusive
			<< ',' << segment.beginNs << ',' << segment.endNs << ',' << value.logicalLiveBytes
			<< ',' << value.backingAllocatedBytes << ',' << value.reusableBytes << ','
			<< value.retiredBytes << ',' << value.objectCount << ',' << value.capacityCount << ','
			<< value.value_fields << ',' << static_cast<std::uint16_t>(value.flags) << '\n';
		if (value.value_fields & static_cast<std::uint16_t>(MemoryValueField::LogicalLive)) {
			const auto weight = segment.endNs - segment.beginNs;
			weighted_live += static_cast<long double>(value.logicalLiveBytes) * weight;
			total_weight += weight;
		}
	}
	if (!csv)
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	auto samples_added =
		add_text_artifact(snapshot, "memory_samples", "memory/samples.csv", "text/csv",
						  "FlowUi.Agentic.MemorySamples", csv_buffer.take(), "1.0", "decimal_text");
	if (!samples_added)
		return samples_added;
	auto sources = Json::make_array();
	for (const auto& descriptor : reporting.descriptorSnapshot())
		sources.array.emplace_back(Json::make_object(
			{{"source_id", decimal(descriptor.id)},
			 {"parent_id", decimal(descriptor.parent)},
			 {"name", descriptor.name},
			 {"domain", Json::number(static_cast<std::uint8_t>(descriptor.domain))},
			 {"kind", Json::number(static_cast<std::uint8_t>(descriptor.kind))},
			 {"accuracy", Json::number(static_cast<std::uint8_t>(descriptor.accuracy))}}));
	auto sources_added =
		add_text_artifact(snapshot, "memory_sources", "memory/sources.json", "application/json",
						  "FlowUi.Agentic.MemorySources",
						  write_json(Json::make_object({{"schema", "FlowUi.Agentic.MemorySources"},
														{"schema_version", "1.0"},
														{"sources", std::move(sources)}})));
	if (!sources_added)
		return sources_added;
	BoundedOutput events_buffer(snapshot.request.limits.maximum_artifact_bytes);
	std::ostream event_output(&events_buffer);
	for (const auto& retained : events) {
		const auto& event = retained.operation;
		auto record = Json::make_object(
			{{"schema", "FlowUi.Agentic.MemoryEvent"},
			 {"schema_version", "1.0"},
			 {"run_id", snapshot.run_id},
			 {"capture_id", snapshot.capture_id},
			 {"sequence", decimal(retained.sequence)},
			 {"generation", decimal(event_read.generation)},
			 {"source_id", decimal(event.source)},
			 {"app_tick", decimal(event.appTick)},
			 {"operation", Json::number(static_cast<std::uint8_t>(event.operation))},
			 {"monotonic_ns", decimal(event.timestampNs)},
			 {"bytes_before", decimal(event.bytesBefore)},
			 {"bytes_after", decimal(event.bytesAfter)},
			 {"bytes_changed", decimal(event.bytesChanged)}});
		if (event.frame) {
			record.set("window_id", decimal(event.frame.window));
			record.set("window_frame", decimal(event.frame.frameNumber));
		}
		if (event.submissionSerial)
			record.set("submission_serial", decimal(event.submissionSerial));
		event_output << write_json(record) << '\n';
	}
	if (!event_output)
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	if (reporting.config().retainLifetimeEvents) {
		auto events_added = add_text_artifact(snapshot, "memory_events", "memory/events.jsonl",
											  "application/x-ndjson", "FlowUi.Agentic.MemoryEvent",
											  events_buffer.take());
		if (!events_added)
			return events_added;
	} else
		coverage.reasons.emplace_back("lifetime_event_retention_disabled");
	const auto summary = Json::make_object(
		{{"schema", "FlowUi.Agentic.MemorySummary"},
		 {"schema_version", "1.0"},
		 {"weighting", "source_segment_elapsed_nanoseconds"},
		 {"total_weight_ns", decimal(total_weight)},
		 {"weighted_mean_live_bytes_per_observed_source",
		  total_weight ? Json(static_cast<double>(weighted_live / total_weight)) : Json{}},
		 {"scope", "mean_over_source_segments_is_not_process_total"},
		 {"coverage", to_json(coverage)}});
	auto summary_added =
		add_text_artifact(snapshot, "memory_summary", "memory/summary.json", "application/json",
						  "FlowUi.Agentic.MemorySummary", write_json(summary));
	if (!summary_added)
		return summary_added;
	snapshot.coverage.emplace_back("memory", std::move(coverage));
	return {};
}
} // namespace

Status materialize_reporting(AgenticCaptureSnapshot& snapshot,
							 DevMonitoringAndReporting& monitoring,
							 const AgenticProducerBaseline& baseline) noexcept {
	try {
		std::optional<TimingCaptureSnapshot> timing;
		if (snapshot.request.performance != SectionRequirement::omit) {
			if (!snapshot.timing_capture_generation) {
				SectionCoverage coverage;
				coverage.status = SectionStatus::unsupported;
				coverage.reasons.emplace_back(
					"visual_or_application_harness_did_not_request_native_timing_controller");
				snapshot.coverage.emplace_back("performance", std::move(coverage));
				if (snapshot.request.performance == SectionRequirement::required)
					snapshot.partial = true;
			} else {
				auto owned = monitoring.timingReporting().owned_capture_snapshot(
					snapshot.timing_capture_generation,
					snapshot.request.limits.maximum_snapshot_bytes / 2);
				if (!owned)
					note_failure(snapshot, "performance", {}, errorName(owned.error().code));
				else {
					timing.emplace(std::move(*owned));
					const auto reporting_status = monitoring.timingReporting().status();
					const auto controller_status = monitoring.performance_capture().status();
					if (reporting_status.capture_generation != timing->generation) {
						note_failure(snapshot, "performance", {}, "timing_generation_replaced");
						timing.reset();
					} else {
						timing_coverage(snapshot, *timing, reporting_status, controller_status);
						for (auto& text : timing->descriptor_text)
							text[1] = snapshot.request.content.include_source_paths
										  ? relative_source(text[1])
										  : std::string{};
						timing->bind_descriptor_text();
						// This App has one graphics device/queue. Normalize native process-local
						// handles into the same run-scoped identities carried by frame/image
						// stamps.
						for (auto& tick : timing->reports)
							for (auto& window : tick.windows)
								for (auto& frame : window.frames)
									for (auto& gpu : frame.gpuZones) {
										gpu.device_identity = 1;
										gpu.queue_identity = 1;
									}
						snapshot.provenance.set("submission_identity_policy",
												"run_scoped_device_1_graphics_queue_1");
						BoundedOutput buffer(snapshot.request.limits.maximum_artifact_bytes);
						std::ostream output(&buffer);
						const auto serialized = write_performance_capture_csv(
							output, *timing, reporting_status, controller_status);
						if (!serialized)
							return serialized;
						auto artifact_added = add_text_artifact(
							snapshot, "performance_timing", "performance/timing.csv", "text/csv",
							"FlowUi.PerformanceCaptureCSV", buffer.take(), "1", "decimal_text");
						if (!artifact_added)
							return artifact_added;
						for (const auto& descriptor : timing->descriptors)
							snapshot.sources.array.emplace_back(
								source(descriptor.typeId, descriptor.name, descriptor.source.file,
									   descriptor.source.function, descriptor.source.line,
									   descriptor.source.column, "timing_zone"));
						for (auto& frame : snapshot.frames)
							for (const auto& tick : timing->reports)
								if (tick.appTick == frame.key.app_tick)
									for (const auto& window : tick.windows)
										if (window.window == frame.key.window_id)
											for (const auto& report : window.frames)
												if (report.key.frameNumber ==
													frame.key.window_frame)
													for (const auto& gpu : report.gpuZones)
														if (gpu.depth == 0 &&
															gpu.submissionSerial ==
																frame.submission_serial &&
															gpu.device_identity ==
																frame.device_id &&
															gpu.queue_identity == frame.queue_id) {
															frame.gpu_duration_ns = gpu.durationNs;
															frame.gpu_duration_available = true;
														}
					}
				}
			}
		} else
			snapshot.coverage.emplace_back("performance", SectionCoverage{});
		if (snapshot.request.runtime_diagnostics != SectionRequirement::omit) {
			if (FLOWUI_DEV_ERROR_LEVEL == 0) {
				SectionCoverage coverage;
				coverage.status = SectionStatus::disabled;
				coverage.reasons.emplace_back("compiled_error_producer_disabled");
				snapshot.coverage.emplace_back("runtime_diagnostics", std::move(coverage));
				if (snapshot.request.runtime_diagnostics == SectionRequirement::required)
					snapshot.partial = true;
			} else {
				auto investigation = monitoring.errorReporting().diagnostics_investigation(
					std::nullopt, snapshot.request.limits.maximum_snapshot_bytes / 4);
				if (!investigation)
					note_failure(snapshot, "runtime_diagnostics", {},
								 errorName(investigation.error().code));
				else {
					auto& dataset = *investigation;
					std::erase_if(dataset.occurrences, [&](const auto& occurrence) {
						return occurrence.lastAppTick < snapshot.first_app_tick ||
							   occurrence.firstAppTick >= snapshot.end_app_tick_exclusive;
					});
					std::erase_if(dataset.snapshot.rows, [&](const auto& row) {
						return row.last_app_tick < snapshot.first_app_tick ||
							   row.first_app_tick >= snapshot.end_app_tick_exclusive;
					});
					if (!snapshot.request.content.include_native_messages) {
						for (auto& occurrence : dataset.occurrences)
							for (auto& step : occurrence.steps) {
								step.nativeText.fill(0);
								step.nativeTextLength = 0;
							}
						dataset.fatal_capsule.reset();
						snapshot.limitations.emplace_back("native_messages_redacted");
					}
					if (!snapshot.request.content.include_stacks) {
						dataset.stacks.clear();
						for (auto& occurrence : dataset.occurrences)
							for (auto& step : occurrence.steps)
								step.stackId = 0;
						dataset.fatal_capsule.reset();
						snapshot.limitations.emplace_back("stacks_redacted");
					}
					if (!snapshot.request.content.include_source_paths) {
						for (auto& descriptor : dataset.snapshot.sources)
							descriptor.file.clear();
						snapshot.limitations.emplace_back("source_paths_redacted");
					}
					const auto& status = dataset.snapshot.status;
					SectionCoverage coverage;
					coverage.finalization_watermark = status.lastConsumedAppTick;
					coverage.generation = dataset.snapshot.revision;
					coverage.producer_records =
						counter_delta(status.consumedRecords, baseline.diagnostics.consumedRecords,
									  coverage.uncertain);
					coverage.retained_records = dataset.occurrences.size();
					coverage.dropped_records =
						counter_delta(status.quality.droppedEvents,
									  baseline.diagnostics.quality.droppedEvents,
									  coverage.uncertain) +
						counter_delta(status.consumeFailures, baseline.diagnostics.consumeFailures,
									  coverage.uncertain) +
						counter_delta(status.truncatedSteps, baseline.diagnostics.truncatedSteps,
									  coverage.uncertain);
					coverage.evicted_records =
						counter_delta(status.evictedOccurrences,
									  baseline.diagnostics.evictedOccurrences, coverage.uncertain);
					coverage.pending_records = status.pendingOccurrences;
					const auto producer_scope = monitoring.errors().producer_scope();
					const bool main_thread_scope =
						producer_scope.current_thread_only &&
						baseline.error_producers.current_thread_only &&
						producer_scope.recorder_count == baseline.error_producers.recorder_count &&
						producer_scope.fallback_events == baseline.error_producers.fallback_events;
					coverage.uncertain = coverage.uncertain || !main_thread_scope;
					coverage.status = coverage.dropped_records || coverage.evicted_records ||
											  coverage.pending_records || coverage.uncertain
										  ? SectionStatus::partial
										  : SectionStatus::complete;
					coverage.reasons.emplace_back(
						main_thread_scope ? "registered_platform_producer_consumed_at_safe_point"
										  : "background_producers_have_no_acknowledged_barrier");
					if (coverage.status == SectionStatus::partial)
						snapshot.partial = true;
					coverage.covered_ranges.emplace_back(
						TickRange{snapshot.first_app_tick, snapshot.end_app_tick_exclusive});
					DevDiagnosticsExportMetadata metadata;
					metadata.filter_description = "app ticks [" +
												  std::to_string(snapshot.first_app_tick) + "," +
												  std::to_string(snapshot.end_app_tick_exclusive) +
												  "); earlier occurrence starts and late causal "
												  "steps are retained as context";
					metadata.frozen = true;
					auto serialized = serialize_diagnostics_json(dataset, metadata);
					if (!serialized)
						return unexpectedError(serialized.error());
					auto added = add_text_artifact(snapshot, "runtime_diagnostics",
												   "diagnostics/runtime.json", "application/json",
												   "FlowUi.Diagnostics", std::move(*serialized),
												   "1", "native_numbers");
					if (!added)
						return added;
					std::string digest =
						"# Runtime diagnostics\n\nRetained occurrences touching the exercised "
						"interval: " +
						std::to_string(dataset.occurrences.size()) +
						". Coverage: " + std::string(name(coverage.status)) +
						". Full causal evidence is in [runtime.json](runtime.json).\n\n";
					for (std::size_t occurrence_index = 0;
						 occurrence_index < std::min<std::size_t>(10, dataset.occurrences.size());
						 ++occurrence_index) {
						const auto& occurrence = dataset.occurrences[occurrence_index];
						digest += "- Occurrence " + std::to_string(occurrence.id) + ": " +
								  std::string(errorName(occurrence.error.code)) + " at " +
								  std::string(errorSiteName(occurrence.error.site)) + ".\n";
					}
					added = add_text_artifact(
						snapshot, "runtime_diagnostics_digest", "diagnostics/runtime.md",
						"text/markdown", "FlowUi.Agentic.DiagnosticsDigest", std::move(digest));
					if (!added)
						return added;
					for (const auto& descriptor : dataset.snapshot.sources)
						snapshot.sources.array.emplace_back(source(
							descriptor.id, descriptor.name, descriptor.file, descriptor.function,
							descriptor.line, descriptor.column, "runtime_diagnostic_source"));
					for (const auto& occurrence : dataset.occurrences) {
						const auto& relation = occurrence.timing;
						std::string resolution(correlation_state(relation.state));
						if (relation.appTick < snapshot.first_app_tick ||
							relation.appTick >= snapshot.end_app_tick_exclusive)
							resolution = "outside_capture";
						else if (relation.state == DevErrorCorrelationState::Available) {
							bool present = false;
							if (timing && relation.capture_generation == timing->generation)
								for (const auto& tick : timing->reports) {
									const auto matches = [&](const auto& record) {
										return record.invocationId == relation.containingInvocation;
									};
									present = present ||
											  std::any_of(tick.applicationCpuZones.begin(),
														  tick.applicationCpuZones.end(), matches);
									for (const auto& window : tick.windows)
										for (const auto& frame : window.frames)
											present = present ||
													  std::any_of(frame.cpuZones.begin(),
																  frame.cpuZones.end(), matches);
								}
							resolution =
								present ? (relation.confidence ==
												   DevErrorCorrelationConfidence::TemporalNearby
											   ? "candidate"
											   : "resolved")
										: "not_retained";
						}
						snapshot.correlations.array.emplace_back(Json::make_object(
							{{"schema", "FlowUi.Agentic.Correlation"},
							 {"schema_version", "1.0"},
							 {"run_id", snapshot.run_id},
							 {"capture_id", snapshot.capture_id},
							 {"source", occurrence_selector(occurrence)},
							 {"target",
							  Json::make_object(
								  {{"artifact_id", "performance_timing"},
								   {"app_tick", decimal(relation.appTick)},
								   {"window_id", decimal(relation.frame.window)},
								   {"window_frame", decimal(relation.frame.frameNumber)},
								   {"invocation_id", decimal(relation.containingInvocation)},
								   {"capture_generation", decimal(relation.capture_generation)}})},
							 {"relationship", "occurred_within_cpu_scope"},
							 {"method", correlation_method(relation.confidence)},
							 {"confidence", correlation_confidence(relation.confidence)},
							 {"native_state", correlation_state(relation.state)},
							 {"resolution", resolution},
							 {"sealed", relation.sealed},
							 {"incomplete", relation.incomplete}}));
					}
					for (const auto& occurrence : dataset.occurrences) {
						const auto& memory = occurrence.memory;
						const bool outside =
							memory.checkpointTick < snapshot.first_app_tick ||
							memory.checkpointTick >= snapshot.end_app_tick_exclusive;
						auto resolution = outside ? std::string("outside_capture")
												  : std::string(correlation_state(memory.state));
						if (!outside && memory.state == DevErrorCorrelationState::Available)
							resolution = "not_retained";
						snapshot.correlations.array.emplace_back(Json::make_object(
							{{"schema", "FlowUi.Agentic.Correlation"},
							 {"schema_version", "1.0"},
							 {"run_id", snapshot.run_id},
							 {"capture_id", snapshot.capture_id},
							 {"source", occurrence_selector(occurrence)},
							 {"target", Json::make_object(
											{{"artifact_id", "memory_events"},
											 {"sequence", decimal(memory.eventSequence)},
											 {"source_id", decimal(memory.source)},
											 {"app_tick", decimal(memory.checkpointTick)},
											 {"generation", decimal(memory.history_generation)}})},
							 {"relationship", "shares_memory_context"},
							 {"method", correlation_method(memory.confidence)},
							 {"confidence", correlation_confidence(memory.confidence)},
							 {"native_state", correlation_state(memory.state)},
							 {"resolution", resolution},
							 {"sealed", memory.sealed},
							 {"incomplete", memory.incomplete}}));
					}
					snapshot.coverage.emplace_back("runtime_diagnostics", std::move(coverage));
				}
			}
		} else
			snapshot.coverage.emplace_back("runtime_diagnostics", SectionCoverage{});
		if (snapshot.request.memory != SectionRequirement::omit) {
			const auto memory_status = monitoring.memoryReporting().status();
			if (FLOWUI_DEV_MEMORY_LEVEL == 0 ||
				memory_status.runtimeLevel == MemoryMonitoringLevel::Disabled) {
				SectionCoverage coverage;
				coverage.status = SectionStatus::disabled;
				snapshot.coverage.emplace_back("memory", std::move(coverage));
				if (snapshot.request.memory == SectionRequirement::required)
					snapshot.partial = true;
			} else {
				auto copied = copy_memory(snapshot, monitoring.memoryReporting(), baseline.memory);
				if (!copied)
					note_failure(snapshot, "memory", {}, errorName(copied.error().code));
			}
		} else
			snapshot.coverage.emplace_back("memory", SectionCoverage{});
		for (const auto& artifact : snapshot.artifacts)
			if (artifact.artifact_id == "memory_events") {
				const std::string_view stream(reinterpret_cast<const char*>(artifact.bytes.data()),
											  artifact.bytes.size());
				std::size_t offset = 0;
				while (offset < stream.size()) {
					const auto newline = stream.find('\n', offset);
					const auto record = parse_json(stream.substr(
						offset, newline == std::string_view::npos ? stream.size() - offset
																  : newline - offset));
					if (record)
						for (auto& edge : snapshot.correlations.array) {
							auto* target = edge.find("target");
							const auto* relationship = edge.find("relationship");
							if (!target || !relationship ||
								relationship->string_or() != "shares_memory_context")
								continue;
							const auto* sequence = record->find("sequence");
							const auto* generation = record->find("generation");
							const auto* selected_sequence = target->find("sequence");
							const auto* selected_generation = target->find("generation");
							const auto* event_source = record->find("source_id");
							const auto* event_tick = record->find("app_tick");
							const auto* selected_source = target->find("source_id");
							const auto* selected_tick = target->find("app_tick");
							if (sequence && generation && selected_sequence &&
								selected_generation && event_source && event_tick &&
								selected_source && selected_tick &&
								sequence->uint64_or() == selected_sequence->uint64_or() &&
								generation->uint64_or() == selected_generation->uint64_or() &&
								event_source->uint64_or() == selected_source->uint64_or() &&
								event_tick->uint64_or() == selected_tick->uint64_or())
								edge.set("resolution", "resolved");
						}
					if (newline == std::string_view::npos)
						break;
					offset = newline + 1;
				}
			}
		if (!snapshot.request.content.include_source_paths)
			redact_paths(snapshot.sources);
		if (!snapshot.request.content.include_tree_text &&
			snapshot.request.visual != SectionRequirement::omit)
			snapshot.limitations.emplace_back("tree_text_redacted");
		SectionCoverage visual;
		if (snapshot.request.visual != SectionRequirement::omit) {
			visual.status = snapshot.images.empty() ? SectionStatus::failed
							: snapshot.partial		? SectionStatus::partial
													: SectionStatus::complete;
			visual.retained_records = snapshot.images.size();
			if (snapshot.images.empty()) {
				visual.reasons.emplace_back("no_completed_images");
				snapshot.partial = true;
			}
			visual.covered_ranges.emplace_back(
				TickRange{snapshot.first_app_tick, snapshot.end_app_tick_exclusive});
		}
		snapshot.coverage.emplace_back("visual", std::move(visual));
		snapshot.coverage.emplace_back("build_diagnostics", SectionCoverage{});
		return {};
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticSnapshot));
	}
}
} // namespace FlowUi::devSystems::agentic
