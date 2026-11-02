#include "devSystems/devMonitoringAndReporting/reporting/DevDiagnosticsExport.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <limits>
#include <type_traits>

namespace FlowUi::devSystems {
namespace {

class JsonWriter {
public:
	std::string text{};

	void begin_object() {
		separator();
		text += '{';
		first_.emplace_back(true);
	}
	void begin_object(std::string_view key) {
		key_prefix(key);
		text += '{';
		first_.emplace_back(true);
	}
	void end_object() {
		text += '}';
		first_.pop_back();
	}
	void begin_array(std::string_view key) {
		key_prefix(key);
		text += '[';
		first_.emplace_back(true);
	}
	void end_array() {
		text += ']';
		first_.pop_back();
	}
	void field(std::string_view key, std::string_view value) {
		key_prefix(key);
		quoted(value);
	}
	void field(std::string_view key, const std::string& value) {
		field(key, std::string_view(value));
	}
	void field(std::string_view key, const char* value) { field(key, std::string_view(value)); }
	template <typename Value>
		requires(std::is_integral_v<Value> || std::is_enum_v<Value>)
	void field(std::string_view key, Value value) {
		key_prefix(key);
		integer(value);
	}
	template <typename Value>
	void element(Value value) {
		separator();
		integer(value);
	}

private:
	std::vector<bool> first_{};
	[[nodiscard]] static size_t utf8_sequence_size(std::string_view value, size_t offset) noexcept {
		const auto first_byte = static_cast<unsigned char>(value[offset]);
		const size_t length = first_byte >= 0xc2 && first_byte <= 0xdf	 ? 2
							  : first_byte >= 0xe0 && first_byte <= 0xef ? 3
							  : first_byte >= 0xf0 && first_byte <= 0xf4 ? 4
																		 : 0;
		if (!length || length > value.size() - offset)
			return 0;
		for (size_t index = 1; index < length; ++index) {
			const auto byte = static_cast<unsigned char>(value[offset + index]);
			if (byte < 0x80 || byte > 0xbf)
				return 0;
		}
		const auto second_byte = static_cast<unsigned char>(value[offset + 1]);
		if ((first_byte == 0xe0 && second_byte < 0xa0) ||
			(first_byte == 0xed && second_byte > 0x9f) ||
			(first_byte == 0xf0 && second_byte < 0x90) ||
			(first_byte == 0xf4 && second_byte > 0x8f))
			return 0;
		return length;
	}
	void separator() {
		if (first_.empty())
			return;
		if (!first_.back())
			text += ',';
		first_.back() = false;
	}
	void key_prefix(std::string_view key) {
		separator();
		quoted(key);
		text += ':';
	}
	void quoted(std::string_view value) {
		constexpr char digits[] = "0123456789abcdef";
		text += '"';
		for (size_t offset = 0; offset < value.size(); ++offset) {
			const auto character = static_cast<unsigned char>(value[offset]);
			if (character >= 0x80) {
				const auto length = utf8_sequence_size(value, offset);
				if (length) {
					text.append(value.substr(offset, length));
					offset += length - 1;
					continue;
				}
			}
			if (character == '"' || character == '\\') {
				text += '\\';
				text += static_cast<char>(character);
			} else if (character < 0x20 || character >= 0x80) {
				text += "\\u00";
				text += digits[character >> 4];
				text += digits[character & 15];
			} else
				text += static_cast<char>(character);
		}
		text += '"';
	}
	template <typename Value>
	void integer(Value value) {
		if constexpr (std::is_same_v<Value, bool>)
			text += value ? "true" : "false";
		else if constexpr (std::is_enum_v<Value>)
			integer(static_cast<std::underlying_type_t<Value>>(value));
		else {
			std::array<char, 32> digits{};
			const auto converted =
				std::to_chars(digits.data(), digits.data() + digits.size(), value);
			text.append(digits.data(), converted.ptr);
		}
	}
};

[[nodiscard]] FlowUiError export_error() noexcept {
	return makeError(ErrorCode::ResourceCreationFailed, ErrorSite::None);
}

#define DIAGNOSTICS_FIELD(value, member) writer.field(#member, (value).member)

void write_context(JsonWriter& writer, const DevErrorContext& context) {
	writer.begin_object("context");
	DIAGNOSTICS_FIELD(context, appTick);
	DIAGNOSTICS_FIELD(context, submissionSerial);
	DIAGNOSTICS_FIELD(context, primaryEntity);
	DIAGNOSTICS_FIELD(context, secondaryEntity);
	DIAGNOSTICS_FIELD(context, timingTrack);
	DIAGNOSTICS_FIELD(context, phase);
	writer.field("window", context.frame.window);
	writer.field("frame", context.frame.frameNumber);
	writer.end_object();
}

void write_error(JsonWriter& writer, const FlowUiError& error) {
	writer.begin_object("error");
	DIAGNOSTICS_FIELD(error, code);
	DIAGNOSTICS_FIELD(error, site);
	DIAGNOSTICS_FIELD(error, subject);
	DIAGNOSTICS_FIELD(error, auxiliary);
	DIAGNOSTICS_FIELD(error, nativeCode);
	const auto descriptor = error.descriptor();
	writer.field("name", descriptor.name);
	writer.field("message", descriptor.message);
	DIAGNOSTICS_FIELD(descriptor, domain);
	DIAGNOSTICS_FIELD(descriptor, category);
	DIAGNOSTICS_FIELD(descriptor, boundary);
	DIAGNOSTICS_FIELD(descriptor, impact);
	DIAGNOSTICS_FIELD(descriptor, resolvability);
	DIAGNOSTICS_FIELD(descriptor, resolutionPolicy);
	DIAGNOSTICS_FIELD(descriptor, defaultResolution);
	const auto site_descriptor = error.siteDescriptor();
	writer.field("site_name", site_descriptor.name);
	DIAGNOSTICS_FIELD(site_descriptor, expectedSubject);
	writer.end_object();
}

template <size_t Capacity>
void write_evidence(JsonWriter& writer, const std::array<DevErrorEvidenceValue, Capacity>& values,
					size_t count) {
	writer.begin_array("values");
	for (size_t index = 0; index < std::min(count, values.size()); ++index) {
		writer.begin_object();
		DIAGNOSTICS_FIELD(values[index], key);
		DIAGNOSTICS_FIELD(values[index], index);
		DIAGNOSTICS_FIELD(values[index], value);
		writer.end_object();
	}
	writer.end_array();
}

void write_immediate_evidence(JsonWriter& writer, const DevErrorEvidenceBlock& evidence) {
	writer.begin_object("evidence");
	DIAGNOSTICS_FIELD(evidence, count);
	DIAGNOSTICS_FIELD(evidence, truncated);
	write_evidence(writer, evidence.values, evidence.count);
	writer.end_object();
}

void write_quality(JsonWriter& writer, const DevErrorQualitySnapshot& quality) {
	writer.begin_object("quality");
	DIAGNOSTICS_FIELD(quality, recordedEvents);
	DIAGNOSTICS_FIELD(quality, recordedBreadcrumbs);
	DIAGNOSTICS_FIELD(quality, suppressedEvents);
	DIAGNOSTICS_FIELD(quality, droppedEvents);
	DIAGNOSTICS_FIELD(quality, droppedBreadcrumbs);
	DIAGNOSTICS_FIELD(quality, overwrittenBreadcrumbs);
	DIAGNOSTICS_FIELD(quality, recursiveEvents);
	DIAGNOSTICS_FIELD(quality, nativeTextTruncations);
	DIAGNOSTICS_FIELD(quality, droppedDescriptors);
	DIAGNOSTICS_FIELD(quality, capturedStacks);
	DIAGNOSTICS_FIELD(quality, deduplicatedStacks);
	DIAGNOSTICS_FIELD(quality, unavailableStacks);
	DIAGNOSTICS_FIELD(quality, truncatedStacks);
	DIAGNOSTICS_FIELD(quality, lostStacks);
	DIAGNOSTICS_FIELD(quality, requestedSnapshots);
	DIAGNOSTICS_FIELD(quality, capturedSnapshots);
	DIAGNOSTICS_FIELD(quality, staleSnapshots);
	DIAGNOSTICS_FIELD(quality, unavailableSnapshots);
	DIAGNOSTICS_FIELD(quality, truncatedSnapshots);
	DIAGNOSTICS_FIELD(quality, lostSnapshots);
	DIAGNOSTICS_FIELD(quality, capturedFatalCapsules);
	DIAGNOSTICS_FIELD(quality, lostFatalCapsules);
	writer.begin_object("overhead");
	const auto& overhead = quality.overhead;
	DIAGNOSTICS_FIELD(overhead, producerCalls);
	DIAGNOSTICS_FIELD(overhead, producerTimeNs);
	DIAGNOSTICS_FIELD(overhead, recordedBytes);
	DIAGNOSTICS_FIELD(overhead, sourceRegistrationCalls);
	DIAGNOSTICS_FIELD(overhead, sourceRegistrationTimeNs);
	DIAGNOSTICS_FIELD(overhead, stackCaptureCalls);
	DIAGNOSTICS_FIELD(overhead, stackCaptureTimeNs);
	DIAGNOSTICS_FIELD(overhead, capturedStackFrames);
	DIAGNOSTICS_FIELD(overhead, fatalCaptureCalls);
	DIAGNOSTICS_FIELD(overhead, fatalCaptureTimeNs);
	writer.end_object();
	writer.end_object();
}

void write_status(JsonWriter& writer, const DevErrorReportingStatus& status) {
	writer.begin_object("reporting_status");
	DIAGNOSTICS_FIELD(status, consumedRecords);
	DIAGNOSTICS_FIELD(status, consumedBreadcrumbs);
	DIAGNOSTICS_FIELD(status, consumedSnapshots);
	DIAGNOSTICS_FIELD(status, evictedOccurrences);
	DIAGNOSTICS_FIELD(status, consumeFailures);
	DIAGNOSTICS_FIELD(status, truncatedSteps);
	DIAGNOSTICS_FIELD(status, truncatedBreadcrumbRanges);
	DIAGNOSTICS_FIELD(status, triggeredCaptures);
	DIAGNOSTICS_FIELD(status, evictedCaptures);
	DIAGNOSTICS_FIELD(status, timingCorrelationEvictions);
	DIAGNOSTICS_FIELD(status, memoryCorrelationEvictions);
	DIAGNOSTICS_FIELD(status, lastConsumedAppTick);
	DIAGNOSTICS_FIELD(status, retainedOccurrences);
	DIAGNOSTICS_FIELD(status, pendingOccurrences);
	DIAGNOSTICS_FIELD(status, retainedBytes);
	DIAGNOSTICS_FIELD(status, revision);
	writer.begin_object("overhead");
	const auto& overhead = status.overhead;
	DIAGNOSTICS_FIELD(overhead, consumeCalls);
	DIAGNOSTICS_FIELD(overhead, consumeTimeNs);
	DIAGNOSTICS_FIELD(overhead, drainTimeNs);
	DIAGNOSTICS_FIELD(overhead, correlationTimeNs);
	DIAGNOSTICS_FIELD(overhead, adviceTimeNs);
	DIAGNOSTICS_FIELD(overhead, groupingCalls);
	DIAGNOSTICS_FIELD(overhead, groupingTimeNs);
	DIAGNOSTICS_FIELD(overhead, drainedBytes);
	DIAGNOSTICS_FIELD(overhead, memoryCorrelationBatches);
	DIAGNOSTICS_FIELD(overhead, memoryCorrelationQueries);
	DIAGNOSTICS_FIELD(overhead, memoryEventCandidates);
	DIAGNOSTICS_FIELD(overhead, memorySourceChecks);
	DIAGNOSTICS_FIELD(overhead, timingCorrelationQueries);
	DIAGNOSTICS_FIELD(overhead, adviceUpdates);
	DIAGNOSTICS_FIELD(overhead, sealedCorrelations);
	writer.end_object();
	write_quality(writer, status.quality);
	writer.end_object();
}

void write_configuration(JsonWriter& writer, const DevDiagnosticsSnapshot& snapshot) {
	writer.begin_object("capture_configuration");
	const auto& capture = snapshot.capture_config;
	writer.field("stack_provider_available", snapshot.stack_provider_available);
	DIAGNOSTICS_FIELD(capture, level);
	DIAGNOSTICS_FIELD(capture, producerRecordCapacity);
	DIAGNOSTICS_FIELD(capture, breadcrumbCapacity);
	DIAGNOSTICS_FIELD(capture, recentBreadcrumbCount);
	DIAGNOSTICS_FIELD(capture, nativeTextLimit);
	DIAGNOSTICS_FIELD(capture, threadRecorderCapacity);
	DIAGNOSTICS_FIELD(capture, sourceDescriptorCapacity);
	DIAGNOSTICS_FIELD(capture, breadcrumbDescriptorCapacity);
	DIAGNOSTICS_FIELD(capture, stackTraceCapacity);
	DIAGNOSTICS_FIELD(capture, maximumStackFrames);
	DIAGNOSTICS_FIELD(capture, snapshotProviderCapacity);
	DIAGNOSTICS_FIELD(capture, pendingSnapshotCapacity);
	DIAGNOSTICS_FIELD(capture, retainedSnapshotCapacity);
	writer.end_object();
	writer.begin_object("reporting_configuration");
	const auto& config = snapshot.config;
	DIAGNOSTICS_FIELD(config, retainedByteBudget);
	DIAGNOSTICS_FIELD(config, retainedOccurrenceCapacity);
	DIAGNOSTICS_FIELD(config, retainedBreadcrumbCapacity);
	DIAGNOSTICS_FIELD(config, postOccurrenceTicks);
	DIAGNOSTICS_FIELD(config, maxStepsPerOccurrence);
	DIAGNOSTICS_FIELD(config, maximumAdvicePerOccurrence);
	DIAGNOSTICS_FIELD(config, retainedCaptureCapacity);
	DIAGNOSTICS_FIELD(config, maximumPinnedTimingTicks);
	DIAGNOSTICS_FIELD(config, maximumPinnedMemoryEvents);
	writer.begin_array("triggers");
	for (const auto& trigger : config.triggers) {
		writer.begin_object();
		DIAGNOSTICS_FIELD(trigger, code);
		DIAGNOSTICS_FIELD(trigger, site);
		DIAGNOSTICS_FIELD(trigger, nativeCode);
		DIAGNOSTICS_FIELD(trigger, matchNativeCode);
		DIAGNOSTICS_FIELD(trigger, preOccurrenceTicks);
		DIAGNOSTICS_FIELD(trigger, postOccurrenceTicks);
		writer.end_object();
	}
	writer.end_array();
	writer.end_object();
}

void write_correlations(JsonWriter& writer, const DevErrorOccurrence& occurrence) {
	writer.begin_object("timing");
	const auto& timing = occurrence.timing;
	DIAGNOSTICS_FIELD(timing, state);
	DIAGNOSTICS_FIELD(timing, confidence);
	DIAGNOSTICS_FIELD(timing, appTick);
	DIAGNOSTICS_FIELD(timing, containingInvocation);
	DIAGNOSTICS_FIELD(timing, containingZone);
	DIAGNOSTICS_FIELD(timing, reportRevision);
	DIAGNOSTICS_FIELD(timing, capture_generation);
	DIAGNOSTICS_FIELD(timing, sealed);
	DIAGNOSTICS_FIELD(timing, incomplete);
	writer.field("window", timing.frame.window);
	writer.field("frame", timing.frame.frameNumber);
	writer.field("entity_kind", timing.entity.kind);
	writer.field("entity_primary", timing.entity.primaryId);
	writer.field("entity_secondary", timing.entity.secondaryId);
	writer.end_object();
	writer.begin_object("memory");
	const auto& memory = occurrence.memory;
	DIAGNOSTICS_FIELD(memory, eventSequence);
	DIAGNOSTICS_FIELD(memory, source);
	DIAGNOSTICS_FIELD(memory, checkpointTick);
	DIAGNOSTICS_FIELD(memory, storageMutationSequence);
	DIAGNOSTICS_FIELD(memory, history_generation);
	DIAGNOSTICS_FIELD(memory, state);
	DIAGNOSTICS_FIELD(memory, confidence);
	DIAGNOSTICS_FIELD(memory, operation);
	DIAGNOSTICS_FIELD(memory, sealed);
	DIAGNOSTICS_FIELD(memory, incomplete);
	writer.end_object();
}

void write_occurrence(JsonWriter& writer, const DevErrorOccurrence& occurrence) {
	writer.begin_object();
	DIAGNOSTICS_FIELD(occurrence, id);
	DIAGNOSTICS_FIELD(occurrence, firstAppTick);
	DIAGNOSTICS_FIELD(occurrence, lastAppTick);
	DIAGNOSTICS_FIELD(occurrence, firstTimestampNs);
	DIAGNOSTICS_FIELD(occurrence, lastTimestampNs);
	DIAGNOSTICS_FIELD(occurrence, productionKind);
	DIAGNOSTICS_FIELD(occurrence, resolution);
	DIAGNOSTICS_FIELD(occurrence, flags);
	DIAGNOSTICS_FIELD(occurrence, state);
	DIAGNOSTICS_FIELD(occurrence, closesAfterAppTick);
	DIAGNOSTICS_FIELD(occurrence, captureId);
	write_error(writer, occurrence.error);
	write_correlations(writer, occurrence);
	writer.begin_array("steps");
	for (const auto& step : occurrence.steps) {
		writer.begin_object();
		DIAGNOSTICS_FIELD(step, sequence);
		DIAGNOSTICS_FIELD(step, timestampNs);
		DIAGNOSTICS_FIELD(step, occurrence);
		DIAGNOSTICS_FIELD(step, cause);
		DIAGNOSTICS_FIELD(step, sourceId);
		DIAGNOSTICS_FIELD(step, stackId);
		DIAGNOSTICS_FIELD(step, breadcrumbBegin);
		DIAGNOSTICS_FIELD(step, breadcrumbEnd);
		DIAGNOSTICS_FIELD(step, threadTrack);
		DIAGNOSTICS_FIELD(step, nativeCategory);
		DIAGNOSTICS_FIELD(step, kind);
		DIAGNOSTICS_FIELD(step, productionKind);
		DIAGNOSTICS_FIELD(step, resolution);
		DIAGNOSTICS_FIELD(step, flags);
		DIAGNOSTICS_FIELD(step, nativeTextLength);
		writer.field("native_text", std::string_view(step.nativeText.data(),
													 std::min<size_t>(step.nativeTextLength,
																	  step.nativeText.size())));
		writer.begin_array("native_text_bytes");
		for (size_t index = 0;
			 index < std::min<size_t>(step.nativeTextLength, step.nativeText.size()); ++index)
			writer.element(static_cast<unsigned char>(step.nativeText[index]));
		writer.end_array();
		write_error(writer, step.error);
		write_context(writer, step.context);
		write_immediate_evidence(writer, step.evidence);
		writer.end_object();
	}
	writer.end_array();
	writer.begin_array("breadcrumbs");
	for (const auto& breadcrumb : occurrence.breadcrumbs) {
		writer.begin_object();
		DIAGNOSTICS_FIELD(breadcrumb, sequence);
		DIAGNOSTICS_FIELD(breadcrumb, timestampNs);
		DIAGNOSTICS_FIELD(breadcrumb, descriptorId);
		DIAGNOSTICS_FIELD(breadcrumb, occurrence);
		DIAGNOSTICS_FIELD(breadcrumb, primaryValue);
		DIAGNOSTICS_FIELD(breadcrumb, secondaryValue);
		DIAGNOSTICS_FIELD(breadcrumb, threadTrack);
		write_context(writer, breadcrumb.context);
		writer.end_object();
	}
	writer.end_array();
	writer.begin_array("snapshots");
	for (const auto& snapshot : occurrence.snapshots) {
		writer.begin_object();
		DIAGNOSTICS_FIELD(snapshot, occurrence);
		DIAGNOSTICS_FIELD(snapshot, source);
		DIAGNOSTICS_FIELD(snapshot, expectedRevision);
		DIAGNOSTICS_FIELD(snapshot, capturedRevision);
		DIAGNOSTICS_FIELD(snapshot, outcome);
		DIAGNOSTICS_FIELD(snapshot, valueCount);
		write_evidence(writer, snapshot.values, snapshot.valueCount);
		writer.end_object();
	}
	writer.end_array();
	writer.begin_array("advice");
	for (const auto& advice : occurrence.advice) {
		writer.begin_object();
		DIAGNOSTICS_FIELD(advice, descriptorId);
		DIAGNOSTICS_FIELD(advice, sourceId);
		DIAGNOSTICS_FIELD(advice, category);
		DIAGNOSTICS_FIELD(advice, confidence);
		DIAGNOSTICS_FIELD(advice, priority);
		DIAGNOSTICS_FIELD(advice, evidenceCount);
		DIAGNOSTICS_FIELD(advice, title);
		DIAGNOSTICS_FIELD(advice, explanation);
		DIAGNOSTICS_FIELD(advice, suggestedAction);
		DIAGNOSTICS_FIELD(advice, configurationKey);
		DIAGNOSTICS_FIELD(advice, documentation);
		DIAGNOSTICS_FIELD(advice, limitation);
		writer.begin_array("evidence");
		for (size_t index = 0;
			 index < std::min<size_t>(advice.evidenceCount, advice.evidence.size()); ++index) {
			writer.begin_object();
			const auto& evidence = advice.evidence[index];
			DIAGNOSTICS_FIELD(evidence, kind);
			DIAGNOSTICS_FIELD(evidence, primary);
			DIAGNOSTICS_FIELD(evidence, secondary);
			writer.end_object();
		}
		writer.end_array();
		writer.end_object();
	}
	writer.end_array();
	writer.end_object();
}

void write_capture(JsonWriter& writer, const DevErrorTriggeredCapture& capture) {
	writer.begin_object();
	DIAGNOSTICS_FIELD(capture, id);
	DIAGNOSTICS_FIELD(capture, triggerOccurrence);
	DIAGNOSTICS_FIELD(capture, firstAppTick);
	DIAGNOSTICS_FIELD(capture, lastAppTick);
	DIAGNOSTICS_FIELD(capture, state);
	DIAGNOSTICS_FIELD(capture, timingHistoryEvicted);
	DIAGNOSTICS_FIELD(capture, memoryHistoryEvicted);
	DIAGNOSTICS_FIELD(capture, timingTruncated);
	DIAGNOSTICS_FIELD(capture, memoryTruncated);
	DIAGNOSTICS_FIELD(capture, timing_generation);
	DIAGNOSTICS_FIELD(capture, timing_retained_ticks);
	DIAGNOSTICS_FIELD(capture, memory_generation);
	DIAGNOSTICS_FIELD(capture, memory_cursor);
	DIAGNOSTICS_FIELD(capture, memory_sealed);
	DIAGNOSTICS_FIELD(capture, timing_state);
	writer.begin_array("memory_events");
	for (const auto& event : capture.memoryEvents) {
		writer.begin_object();
		DIAGNOSTICS_FIELD(event, sequence);
		const auto& operation = event.operation;
		DIAGNOSTICS_FIELD(operation, source);
		DIAGNOSTICS_FIELD(operation, lifetime);
		DIAGNOSTICS_FIELD(operation, operation);
		DIAGNOSTICS_FIELD(operation, detailLevel);
		DIAGNOSTICS_FIELD(operation, appTick);
		DIAGNOSTICS_FIELD(operation, submissionSerial);
		DIAGNOSTICS_FIELD(operation, timestampNs);
		DIAGNOSTICS_FIELD(operation, bytesBefore);
		DIAGNOSTICS_FIELD(operation, bytesAfter);
		DIAGNOSTICS_FIELD(operation, bytesChanged);
		DIAGNOSTICS_FIELD(operation, threadTrack);
		DIAGNOSTICS_FIELD(operation, debugName);
		writer.field("window", operation.frame.window);
		writer.field("frame", operation.frame.frameNumber);
		writer.end_object();
	}
	writer.end_array();
	writer.end_object();
}

void write_fatal_capsule(JsonWriter& writer, const DevErrorFatalCapsule& capsule) {
	writer.begin_object("fatal_capsule");
	DIAGNOSTICS_FIELD(capsule, layoutVersion);
	DIAGNOSTICS_FIELD(capsule, byteSize);
	DIAGNOSTICS_FIELD(capsule, occurrence);
	DIAGNOSTICS_FIELD(capsule, timestampNs);
	DIAGNOSTICS_FIELD(capsule, sourceId);
	DIAGNOSTICS_FIELD(capsule, moduleIdentity);
	DIAGNOSTICS_FIELD(capsule, buildIdentity);
	DIAGNOSTICS_FIELD(capsule, threadTrack);
	DIAGNOSTICS_FIELD(capsule, nativeCategory);
	DIAGNOSTICS_FIELD(capsule, resolution);
	DIAGNOSTICS_FIELD(capsule, productionInspection);
	DIAGNOSTICS_FIELD(capsule, captureLevel);
	DIAGNOSTICS_FIELD(capsule, stackStatus);
	DIAGNOSTICS_FIELD(capsule, flags);
	DIAGNOSTICS_FIELD(capsule, availableInputs);
	DIAGNOSTICS_FIELD(capsule, stackFrameCount);
	DIAGNOSTICS_FIELD(capsule, nativeTextLength);
	DIAGNOSTICS_FIELD(capsule, breadcrumbCount);
	writer.field("native_text", std::string_view(capsule.nativeText.data(),
												 std::min<size_t>(capsule.nativeTextLength,
																  capsule.nativeText.size())));
	writer.begin_array("native_text_bytes");
	for (size_t index = 0;
		 index < std::min<size_t>(capsule.nativeTextLength, capsule.nativeText.size()); ++index)
		writer.element(static_cast<unsigned char>(capsule.nativeText[index]));
	writer.end_array();
	write_error(writer, capsule.error);
	write_context(writer, capsule.context);
	write_immediate_evidence(writer, capsule.evidence);
	writer.begin_object("safe_point");
	const auto& safe_point = capsule.safePoint;
	DIAGNOSTICS_FIELD(safe_point, revision);
	DIAGNOSTICS_FIELD(safe_point, appTick);
	DIAGNOSTICS_FIELD(safe_point, timingMutationSequence);
	DIAGNOSTICS_FIELD(safe_point, timingDroppedRecords);
	DIAGNOSTICS_FIELD(safe_point, memoryGeneration);
	DIAGNOSTICS_FIELD(safe_point, memoryMutationSequence);
	DIAGNOSTICS_FIELD(safe_point, memoryDroppedOperations);
	DIAGNOSTICS_FIELD(safe_point, errorRecordedEvents);
	DIAGNOSTICS_FIELD(safe_point, errorDroppedEvents);
	writer.end_object();
	writer.begin_array("stack_frames");
	for (size_t index = 0;
		 index < std::min<size_t>(capsule.stackFrameCount, capsule.stackFrames.size()); ++index)
		writer.element(capsule.stackFrames[index]);
	writer.end_array();
	writer.begin_array("breadcrumbs");
	for (size_t index = 0;
		 index < std::min<size_t>(capsule.breadcrumbCount, capsule.breadcrumbs.size()); ++index) {
		const auto& breadcrumb = capsule.breadcrumbs[index];
		writer.begin_object();
		DIAGNOSTICS_FIELD(breadcrumb, sequence);
		DIAGNOSTICS_FIELD(breadcrumb, timestampNs);
		DIAGNOSTICS_FIELD(breadcrumb, descriptorId);
		DIAGNOSTICS_FIELD(breadcrumb, primaryValue);
		DIAGNOSTICS_FIELD(breadcrumb, secondaryValue);
		DIAGNOSTICS_FIELD(breadcrumb, occurrence);
		DIAGNOSTICS_FIELD(breadcrumb, threadTrack);
		writer.end_object();
	}
	writer.end_array();
	writer.end_object();
}

#undef DIAGNOSTICS_FIELD

[[nodiscard]] std::string markdown_text(std::string_view value) {
	std::string result;
	result.reserve(value.size());
	for (const char character : value) {
		if (character == '<')
			result += "&lt;";
		else if (character == '>')
			result += "&gt;";
		else if (character == '&')
			result += "&amp;";
		else {
			if (character == '|' || character == '*' || character == '_' || character == '`' ||
				character == '\\')
				result += '\\';
			result += character;
		}
	}
	return result;
}

[[nodiscard]] std::string_view confidence_name(DevErrorAdviceConfidence confidence) noexcept {
	switch (confidence) {
	case DevErrorAdviceConfidence::Certain:
		return "Certain";
	case DevErrorAdviceConfidence::Strong:
		return "Strong";
	case DevErrorAdviceConfidence::Possible:
		return "Possible";
	case DevErrorAdviceConfidence::InsufficientEvidence:
		return "InsufficientEvidence";
	}
	return "Unknown";
}

} // namespace

Result<std::string>
serialize_diagnostics_json(const DevDiagnosticsInvestigation& investigation,
						   const DevDiagnosticsExportMetadata& metadata) noexcept {
	try {
		JsonWriter writer;
		writer.text.reserve(
			static_cast<size_t>(std::min<uint64_t>(investigation.owned_bytes, 1024 * 1024)));
		writer.begin_object();
		writer.field("schema", "FlowUi.Diagnostics");
		writer.field("schema_version", 1u);
		writer.field("integer_encoding",
					 "Exact decimal integers; consumers must preserve unsigned 64-bit values");
		writer.field("text_encoding", "UTF-8; invalid bytes escaped as U+00xx; native_text_bytes "
									  "preserves original diagnostic bytes");
		writer.field("scope",
					 investigation.selected_id ? "Selected investigation" : "Retained diagnostics");
		writer.field("frozen", metadata.frozen);
		writer.field("filter_description", metadata.filter_description);
		writer.field("group_mode", metadata.group_mode);
		writer.field("group_code", metadata.group_key.code);
		writer.field("group_site", metadata.group_key.site);
		writer.field("group_native_code", metadata.group_key.nativeCode);
		writer.field("group_source_id", metadata.group_key.sourceId);
		if (investigation.selected_id)
			writer.field("selected_occurrence_id", *investigation.selected_id);
		writer.field("revision", investigation.snapshot.revision);
		writer.field("owned_bytes", investigation.owned_bytes);
		writer.field("timestamp_coverage",
					 "Occurrence observations only; uninterrupted coverage is not established");
		write_status(writer, investigation.snapshot.status);
		write_configuration(writer, investigation.snapshot);
		writer.begin_array("sources");
		for (const auto& source : investigation.snapshot.sources) {
			writer.begin_object();
			writer.field("id", source.id);
			writer.field("name", source.name);
			writer.field("file", source.file);
			writer.field("function", source.function);
			writer.field("line", source.line);
			writer.field("column", source.column);
			writer.end_object();
		}
		writer.end_array();
		writer.begin_array("breadcrumb_descriptors");
		for (const auto& descriptor : investigation.snapshot.breadcrumb_descriptors) {
			writer.begin_object();
			writer.field("id", descriptor.id);
			writer.field("name", descriptor.name);
			writer.end_object();
		}
		writer.end_array();
		writer.begin_array("occurrences");
		for (const auto& occurrence : investigation.occurrences)
			write_occurrence(writer, occurrence);
		writer.end_array();
		writer.begin_array("stacks");
		for (const auto& stack : investigation.stacks) {
			writer.begin_object();
			writer.field("id", stack.id);
			writer.field("hash", stack.hash);
			writer.field("module_identity", stack.moduleIdentity);
			writer.field("build_identity", stack.buildIdentity);
			writer.field("status", stack.status);
			writer.field("frame_count", stack.frameCount);
			writer.begin_array("raw_instruction_addresses");
			for (size_t index = 0; index < std::min<size_t>(stack.frameCount, stack.frames.size());
				 ++index)
				writer.element(stack.frames[index]);
			writer.end_array();
			writer.end_object();
		}
		writer.end_array();
		writer.begin_array("triggered_captures");
		for (const auto& capture : investigation.captures)
			write_capture(writer, capture);
		writer.end_array();
		writer.field("fatal_capsule_available", investigation.fatal_capsule.has_value());
		if (investigation.fatal_capsule)
			write_fatal_capsule(writer, *investigation.fatal_capsule);
		writer.end_object();
		writer.text += '\n';
		return std::move(writer.text);
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::None));
	}
}

Result<std::string>
serialize_diagnostics_markdown(const DevDiagnosticsInvestigation& investigation,
							   const DevDiagnosticsExportMetadata& metadata) noexcept {
	try {
		auto json = serialize_diagnostics_json(investigation, metadata);
		if (!json)
			return unexpectedError(json.error());
		std::string report =
			"# Diagnostics investigation\n\nSchema: FlowUi.Diagnostics v1. Scope: ";
		report += investigation.selected_id ? "Selected investigation" : "Retained diagnostics";
		report += metadata.frozen ? " (frozen)" : " (live snapshot)";
		report += ". Consumed application tick: " +
				  std::to_string(investigation.snapshot.status.lastConsumedAppTick);
		report +=
			". Reporting revision: " + std::to_string(investigation.snapshot.revision) + ".\n\n";
		report += "Filters: " +
				  markdown_text(metadata.filter_description.empty() ? "All retained occurrences"
																	: metadata.filter_description) +
				  ".\n\n";
		report += "Counts describe retained observations. Collection closure does not mean the "
				  "problem is fixed. Correlation is not proof of causation. Timestamp coverage is "
				  "not established; rates are not extrapolated.\n\n";
		for (const auto& occurrence : investigation.occurrences) {
			report += "## " + markdown_text(occurrence.error.descriptor().name) + " — occurrence " +
					  std::to_string(occurrence.id) + "\n\n";
			report += markdown_text(occurrence.error.descriptor().message) +
					  "\n\nSite: " + markdown_text(occurrence.error.siteDescriptor().name) +
					  ".\n\n";
			report += "First/last application ticks: " + std::to_string(occurrence.firstAppTick) +
					  " / " + std::to_string(occurrence.lastAppTick) + ". Recorded resolution: " +
					  std::to_string(static_cast<unsigned>(occurrence.resolution)) + ".\n\n";
			if (occurrence.advice.empty())
				report += "No catalogue advice is available for this occurrence.\n\n";
			for (const auto& advice : occurrence.advice) {
				report += "### " + markdown_text(advice.title) + "\n\n" +
						  markdown_text(advice.explanation) + "\n\n";
				report += "**Recommended action:** " + markdown_text(advice.suggestedAction) +
						  "\n\nConfidence: " + std::string(confidence_name(advice.confidence)) +
						  ". Advice ID: " + std::to_string(advice.descriptorId) + ".\n\n";
				if (!advice.configurationKey.empty())
					report += "Configuration: " + markdown_text(advice.configurationKey) + ".\n\n";
				if (!advice.documentation.empty())
					report += "Documentation: " + markdown_text(advice.documentation) + ".\n\n";
				if (!advice.limitation.empty())
					report += "Limitation: " + markdown_text(advice.limitation) + "\n\n";
			}
			report += "### Recorded history\n\n| Sequence | Kind | Timestamp (ns) | Application "
					  "tick | Source ID | Resolution |\n| --- | --- | --- | --- | --- | --- |\n";
			for (const auto& step : occurrence.steps)
				report += "| " + std::to_string(step.sequence) + " | " +
						  std::to_string(static_cast<unsigned>(step.kind)) + " | " +
						  std::to_string(step.timestampNs) + " | " +
						  std::to_string(step.context.appTick) + " | " +
						  std::to_string(step.sourceId) + " | " +
						  std::to_string(static_cast<unsigned>(step.resolution)) + " |\n";
			report +=
				"\nRetained breadcrumbs: " + std::to_string(occurrence.breadcrumbs.size()) +
				"; structured snapshots: " + std::to_string(occurrence.snapshots.size()) +
				". Occurrence flags: " + std::to_string(static_cast<unsigned>(occurrence.flags)) +
				".\n\n";
		}
		const auto& status = investigation.snapshot.status;
		report +=
			"## Evidence limitations\n\nReporting-wide evicted occurrences: " +
			std::to_string(status.evictedOccurrences) +
			"; dropped events: " + std::to_string(status.quality.droppedEvents) +
			"; suppressed events: " + std::to_string(status.quality.suppressedEvents) +
			"; consume failures: " + std::to_string(status.consumeFailures) +
			". These counters cannot be attributed to one selected occurrence.\n\nUnavailable "
			"descriptors, missing stacks and snapshots remain unavailable. Raw stack addresses are "
			"captured facts; symbolization is unavailable. This export contains retained evidence "
			"and cannot recover evicted history.\n\n## Complete structured evidence\n\nThe "
			"following appendix contains the same versioned evidence bundle as the accompanying "
			"JSON file, including exact identities, contexts, advice references, configuration, "
			"raw stacks, captured state, correlations and capture quality.\n\n";
		size_t longest_fence = 0, current_fence = 0;
		for (const char character : *json) {
			current_fence = character == '`' ? current_fence + 1 : 0;
			longest_fence = std::max(longest_fence, current_fence);
		}
		const std::string fence(std::max<size_t>(3, longest_fence + 1), '`');
		report += fence + "json\n" + *json + fence + "\n";
		return report;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::None));
	}
}

Status write_diagnostics_export(const DevDiagnosticsInvestigation& investigation,
								const std::filesystem::path& path_prefix,
								const DevDiagnosticsExportMetadata& metadata) noexcept {
	std::filesystem::path json_temporary, markdown_temporary, json_path;
	bool published_json = false;
	try {
		if (path_prefix.empty() || investigation.occurrences.empty())
			return unexpectedError(export_error());
		auto json = serialize_diagnostics_json(investigation, metadata);
		if (!json)
			return unexpectedError(json.error());
		auto markdown = serialize_diagnostics_markdown(investigation, metadata);
		if (!markdown)
			return unexpectedError(markdown.error());
		json_path = path_prefix;
		json_path += ".json";
		auto markdown_path = path_prefix;
		markdown_path += ".md";
		json_temporary = json_path;
		json_temporary += ".tmp";
		markdown_temporary = markdown_path;
		markdown_temporary += ".tmp";
		if (std::filesystem::exists(json_path) || std::filesystem::exists(markdown_path) ||
			std::filesystem::exists(json_temporary) || std::filesystem::exists(markdown_temporary))
			return unexpectedError(export_error());
		const auto write_file = [](const std::filesystem::path& path, const std::string& content) {
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			output.write(content.data(), static_cast<std::streamsize>(content.size()));
			output.flush();
			output.close();
			return !output.fail();
		};
		if (!write_file(json_temporary, *json) || !write_file(markdown_temporary, *markdown)) {
			std::error_code error;
			std::filesystem::remove(json_temporary, error);
			std::filesystem::remove(markdown_temporary, error);
			return unexpectedError(export_error());
		}
		std::filesystem::rename(json_temporary, json_path);
		published_json = true;
		std::filesystem::rename(markdown_temporary, markdown_path);
		return {};
	} catch (...) {
		std::error_code error;
		if (!json_temporary.empty())
			std::filesystem::remove(json_temporary, error);
		if (!markdown_temporary.empty())
			std::filesystem::remove(markdown_temporary, error);
		if (published_json)
			std::filesystem::remove(json_path, error);
		return unexpectedError(export_error());
	}
}

} // namespace FlowUi::devSystems

#endif
