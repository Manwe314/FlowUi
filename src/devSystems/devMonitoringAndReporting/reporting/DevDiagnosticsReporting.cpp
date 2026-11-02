#include "devSystems/devMonitoringAndReporting/reporting/DevDiagnosticsReporting.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <utility>

namespace FlowUi::devSystems {

DevDiagnosticsInvestigation::DevDiagnosticsInvestigation(const DevDiagnosticsInvestigation& other)
	: snapshot(other.snapshot), occurrences(other.occurrences), stacks(other.stacks),
	  captures(other.captures), advice_text(other.advice_text), fatal_capsule(other.fatal_capsule),
	  selected_id(other.selected_id), owned_bytes(other.owned_bytes) {
	bind_advice_text();
}

DevDiagnosticsInvestigation::DevDiagnosticsInvestigation(
	DevDiagnosticsInvestigation&& other) noexcept
	: snapshot(std::move(other.snapshot)), occurrences(std::move(other.occurrences)),
	  stacks(std::move(other.stacks)), captures(std::move(other.captures)),
	  advice_text(std::move(other.advice_text)), fatal_capsule(std::move(other.fatal_capsule)),
	  selected_id(other.selected_id), owned_bytes(other.owned_bytes) {
	bind_advice_text();
}

DevDiagnosticsInvestigation&
DevDiagnosticsInvestigation::operator=(const DevDiagnosticsInvestigation& other) {
	if (this != &other) {
		DevDiagnosticsInvestigation copy(other);
		*this = std::move(copy);
	}
	return *this;
}

DevDiagnosticsInvestigation&
DevDiagnosticsInvestigation::operator=(DevDiagnosticsInvestigation&& other) noexcept {
	if (this != &other) {
		snapshot = std::move(other.snapshot);
		occurrences = std::move(other.occurrences);
		stacks = std::move(other.stacks);
		captures = std::move(other.captures);
		advice_text = std::move(other.advice_text);
		fatal_capsule = std::move(other.fatal_capsule);
		selected_id = other.selected_id;
		owned_bytes = other.owned_bytes;
		bind_advice_text();
	}
	return *this;
}

void DevDiagnosticsInvestigation::bind_advice_text() noexcept {
	size_t text_index = 0;
	for (auto& occurrence : occurrences) {
		for (auto& advice : occurrence.advice) {
			if (text_index >= advice_text.size())
				return;
			const auto& text = advice_text[text_index++];
			advice.title = text.title;
			advice.explanation = text.explanation;
			advice.suggestedAction = text.suggested_action;
			advice.configurationKey = text.configuration_key;
			advice.documentation = text.documentation;
			advice.limitation = text.limitation;
		}
	}
}

uint64_t diagnostics_owned_bytes(const DevDiagnosticsInvestigation& investigation) noexcept {
	const auto& snapshot = investigation.snapshot;
	uint64_t bytes =
		sizeof(DevDiagnosticsInvestigation) +
		snapshot.rows.capacity() * sizeof(DevDiagnosticsOccurrenceRow) +
		snapshot.sources.capacity() * sizeof(DevDiagnosticsSource) +
		snapshot.breadcrumb_descriptors.capacity() * sizeof(DevDiagnosticsBreadcrumbDescriptor) +
		snapshot.config.triggers.capacity() * sizeof(DevErrorTrigger) +
		investigation.occurrences.capacity() * sizeof(DevErrorOccurrence) +
		investigation.stacks.capacity() * sizeof(DevErrorStackTrace) +
		investigation.captures.capacity() * sizeof(DevErrorTriggeredCapture) +
		investigation.advice_text.capacity() * sizeof(DevDiagnosticsAdviceText);
	for (const auto& row : snapshot.rows)
		bytes += row.steps.capacity() * sizeof(DevDiagnosticsStepReference);
	for (const auto& source : snapshot.sources)
		bytes += source.name.capacity() + source.file.capacity() + source.function.capacity() + 3;
	for (const auto& descriptor : snapshot.breadcrumb_descriptors)
		bytes += descriptor.name.capacity() + 1;
	for (const auto& occurrence : investigation.occurrences)
		bytes += occurrence.steps.capacity() * sizeof(DevErrorRecord) +
				 occurrence.breadcrumbs.capacity() * sizeof(DevErrorBreadcrumb) +
				 occurrence.snapshots.capacity() * sizeof(DevErrorSnapshot) +
				 occurrence.advice.capacity() * sizeof(DevErrorAdviceResult);
	for (const auto& capture : investigation.captures)
		bytes += capture.memoryEvents.capacity() * sizeof(RetainedMemoryEvent);
	for (const auto& text : investigation.advice_text)
		bytes += text.title.capacity() + text.explanation.capacity() +
				 text.suggested_action.capacity() + text.configuration_key.capacity() +
				 text.documentation.capacity() + text.limitation.capacity() + 6;
	return bytes;
}

Result<DevDiagnosticsInvestigation>
select_diagnostics_investigation(const DevDiagnosticsInvestigation& investigation,
								 DevErrorOccurrenceId occurrence_id) noexcept {
	try {
		const auto found =
			std::find_if(investigation.occurrences.begin(), investigation.occurrences.end(),
						 [&](const auto& occurrence) { return occurrence.id == occurrence_id; });
		if (found == investigation.occurrences.end())
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::None, occurrence_id));
		DevDiagnosticsInvestigation result;
		result.snapshot.status = investigation.snapshot.status;
		result.snapshot.config = investigation.snapshot.config;
		result.snapshot.capture_config = investigation.snapshot.capture_config;
		result.snapshot.revision = investigation.snapshot.revision;
		result.snapshot.stack_provider_available = investigation.snapshot.stack_provider_available;
		result.snapshot.rows.reserve(1);
		for (const auto& row : investigation.snapshot.rows)
			if (row.id == occurrence_id)
				result.snapshot.rows.emplace_back(row);
		if (investigation.fatal_capsule && investigation.fatal_capsule->occurrence == occurrence_id)
			result.fatal_capsule = investigation.fatal_capsule;
		const auto source_referenced = [&](uint64_t source_id) {
			return std::any_of(found->steps.begin(), found->steps.end(),
							   [&](const auto& step) { return step.sourceId == source_id; }) ||
				   std::any_of(
					   found->snapshots.begin(), found->snapshots.end(),
					   [&](const auto& snapshot) { return snapshot.source == source_id; }) ||
				   (result.fatal_capsule && result.fatal_capsule->sourceId == source_id);
		};
		const auto breadcrumb_referenced = [&](uint64_t descriptor_id) {
			if (std::any_of(found->breadcrumbs.begin(), found->breadcrumbs.end(),
							[&](const auto& breadcrumb) {
								return breadcrumb.descriptorId == descriptor_id;
							}))
				return true;
			if (result.fatal_capsule)
				for (size_t index = 0;
					 index < std::min<size_t>(result.fatal_capsule->breadcrumbCount,
											  result.fatal_capsule->breadcrumbs.size());
					 ++index)
					if (result.fatal_capsule->breadcrumbs[index].descriptorId == descriptor_id)
						return true;
			return false;
		};
		result.snapshot.sources.reserve(static_cast<size_t>(std::count_if(
			investigation.snapshot.sources.begin(), investigation.snapshot.sources.end(),
			[&](const auto& source) { return source_referenced(source.id); })));
		for (const auto& source : investigation.snapshot.sources)
			if (source_referenced(source.id))
				result.snapshot.sources.emplace_back(source);
		result.snapshot.breadcrumb_descriptors.reserve(static_cast<size_t>(std::count_if(
			investigation.snapshot.breadcrumb_descriptors.begin(),
			investigation.snapshot.breadcrumb_descriptors.end(),
			[&](const auto& descriptor) { return breadcrumb_referenced(descriptor.id); })));
		for (const auto& descriptor : investigation.snapshot.breadcrumb_descriptors)
			if (breadcrumb_referenced(descriptor.id))
				result.snapshot.breadcrumb_descriptors.emplace_back(descriptor);
		result.selected_id = occurrence_id;
		result.occurrences.reserve(1);
		result.occurrences.emplace_back(*found);
		result.advice_text.reserve(found->advice.size());
		for (const auto& advice : found->advice)
			result.advice_text.emplace_back(
				DevDiagnosticsAdviceText{.title = std::string(advice.title),
										 .explanation = std::string(advice.explanation),
										 .suggested_action = std::string(advice.suggestedAction),
										 .configuration_key = std::string(advice.configurationKey),
										 .documentation = std::string(advice.documentation),
										 .limitation = std::string(advice.limitation)});
		result.stacks.reserve(static_cast<size_t>(std::count_if(
			investigation.stacks.begin(), investigation.stacks.end(), [&](const auto& stack) {
				return std::any_of(found->steps.begin(), found->steps.end(),
								   [&](const auto& step) { return step.stackId == stack.id; });
			})));
		for (const auto& stack : investigation.stacks)
			if (std::any_of(found->steps.begin(), found->steps.end(),
							[&](const auto& step) { return step.stackId == stack.id; }))
				result.stacks.emplace_back(stack);
		result.captures.reserve(1);
		for (const auto& capture : investigation.captures)
			if (capture.id == found->captureId)
				result.captures.emplace_back(capture);
		result.bind_advice_text();
		result.owned_bytes = diagnostics_owned_bytes(result);
		return result;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::None));
	}
}

} // namespace FlowUi::devSystems

#endif
