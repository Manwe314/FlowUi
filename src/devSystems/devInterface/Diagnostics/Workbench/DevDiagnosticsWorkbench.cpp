#include "devSystems/devInterface/Diagnostics/Workbench/DevDiagnosticsWorkbench.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <tuple>
#include <vector>

namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = diagnostics_ui;
using Context = DevDiagnosticsWorkbench::BuildContext;
enum Action : uint64_t {
	Reset = 1,
	Latest,
	First,
	Previous,
	Next,
	Follow,
	Specific,
	Pin,
	Bin,
	AllBins,
	HistoryPrevious,
	HistoryNext,
	Step,
	Breadcrumb,
	Cause,
	Advice,
	Performance,
	Memory,
	TimelinePrevious,
	TimelineNext,
	DefaultFocus,
	Retry,
	ClearSelection
};
constexpr uint64_t history_page_size = 10, timeline_page_size = 40;

void activate(void* target, uint64_t action, uint64_t value) {
	auto& local = *static_cast<DevDiagnosticsWorkbench::State*>(target);
	auto& state = *local.diagnostics;
	const auto* group = selected_diagnostics_group(state);
	if (action == Reset)
		reset_diagnostics_filters(state);
	if (action == Retry) {
		state.pending = DevDiagnosticsCommand::Refresh;
	}
	if (action == ClearSelection)
		state.pending = DevDiagnosticsCommand::ClearSelection;
	if (action == Latest && group)
		select_diagnostics_occurrence(state, group->latest);
	if (action == First && group)
		select_diagnostics_occurrence(state, group->first);
	if (action == Follow) {
		state.follow_latest = !state.follow_latest;
		state.dirty = true;
	}
	if (action == Specific)
		local.history_open = !local.history_open;
	if (action == Pin || action == Cause)
		select_diagnostics_occurrence(state, value);
	if (action == Previous || action == Next) {
		const auto found =
			std::ranges::find(state.history, state.selected_id, &DevDiagnosticsHistoryRow::id);
		if (found != state.history.end()) {
			const auto index = static_cast<std::size_t>(found - state.history.begin());
			if (action == Previous && index + 1 < state.history.size())
				select_diagnostics_occurrence(state, state.history[index + 1].id);
			if (action == Next && index > 0)
				select_diagnostics_occurrence(state, state.history[index - 1].id);
		}
	}
	if (action == Bin) {
		state.selected_bin = value;
		state.history_page = 0;
		local.history_open = true;
	}
	if (action == AllBins) {
		state.selected_bin = UINT64_MAX;
		state.history_page = 0;
		local.history_open = true;
	}
	if (action == HistoryPrevious && state.history_page)
		--state.history_page;
	if (action == HistoryNext)
		++state.history_page;
	if (action == Step || action == Breadcrumb)
		focus_diagnostics_event(state, action == Step ? 1 : 2, value);
	if (action == DefaultFocus)
		focus_diagnostics_event(state, 0, 0);
	if (action == Advice && value < local.evidence_actions.size()) {
		const auto evidence = local.evidence_actions[value];
		diagnostics_focus_advice(state, evidence);
		if (evidence.kind == DevErrorAdviceEvidenceKind::Frame ||
			evidence.kind == DevErrorAdviceEvidenceKind::TimingZone)
			local.scroll_to_section = 4;
		if (evidence.kind == DevErrorAdviceEvidenceKind::MemoryEvent ||
			evidence.kind == DevErrorAdviceEvidenceKind::MemoryCheckpoint)
			local.scroll_to_section = 4;
		if (evidence.kind == DevErrorAdviceEvidenceKind::CaptureQuality)
			local.scroll_to_section = 5;
	}
	if (action == Performance)
		state.pending = DevDiagnosticsCommand::OpenPerformance;
	if (action == Memory)
		state.pending = DevDiagnosticsCommand::OpenMemory;
	if (action == TimelinePrevious && local.timeline_page)
		--local.timeline_page;
	if (action == TimelineNext)
		++local.timeline_page;
}

[[nodiscard]] diagnostics_icons::Icon action_icon(uint64_t action) noexcept {
	using Icon = diagnostics_icons::Icon;
	switch (action) {
	case Reset:
	case Retry:
		return Icon::Refresh;
	case ClearSelection:
		return Icon::Close;
	case Latest:
		return Icon::Latest;
	case First:
		return Icon::First;
	case Previous:
	case HistoryPrevious:
	case TimelinePrevious:
		return Icon::Previous;
	case Next:
	case HistoryNext:
	case TimelineNext:
		return Icon::Next;
	case Follow:
		return Icon::Follow;
	case Specific:
	case AllBins:
		return Icon::History;
	case Pin:
	case DefaultFocus:
		return Icon::Pin;
	case Step:
		return Icon::Timeline;
	case Breadcrumb:
		return Icon::Info;
	case Cause:
		return Icon::Link;
	case Advice:
		return Icon::Advice;
	case Performance:
		return Icon::Performance;
	case Memory:
		return Icon::Memory;
	default:
		return Icon::None;
	}
}

void button(Context& context, LocalElementName identity, std::string label, uint64_t action,
			uint64_t value = 0, bool enabled = true, std::string tooltip = {}) {
	ui::button(context, identity, std::move(label), &context.state(), activate, action, value,
			   enabled, std::move(tooltip), action_icon(action));
}

void button(Context& context, IndexedElementName identity, std::string label, uint64_t action,
			uint64_t value = 0, bool enabled = true, std::string tooltip = {}) {
	ui::button(context, identity, std::move(label), &context.state(), activate, action, value,
			   enabled, std::move(tooltip), action_icon(action));
}

[[nodiscard]] diagnostics_icons::Icon section_icon(uint64_t identity) noexcept {
	using Icon = diagnostics_icons::Icon;
	switch (identity) {
	case 0:
		return Icon::Issue;
	case 1:
		return Icon::Advice;
	case 2:
		return Icon::History;
	case 3:
		return Icon::Timeline;
	case 4:
		return Icon::Link;
	default:
		return Icon::Warning;
	}
}

[[nodiscard]] diagnostics_icons::Icon step_icon(DevErrorStepKind kind) noexcept {
	using Icon = diagnostics_icons::Icon;
	switch (kind) {
	case DevErrorStepKind::Raised:
		return Icon::Issue;
	case DevErrorStepKind::Boundary:
		return Icon::Link;
	case DevErrorStepKind::Attempt:
		return Icon::Refresh;
	case DevErrorStepKind::Resolution:
		return Icon::Check;
	case DevErrorStepKind::Consequence:
		return Icon::Warning;
	case DevErrorStepKind::BackendDiagnostic:
		return Icon::Source;
	case DevErrorStepKind::FatalCapture:
		return Icon::Warning;
	}
	return Icon::Timeline;
}

void section(Context& context, uint64_t identity, std::string_view title) {
	auto heading = ui::row(34);
	heading.layout.padding = {12, 12, 0, 0};
	heading.layout.childGap = 8;
	heading.backgroundColor = interface_theme::kDepth2Ink;
	heading.border = {.color = interface_theme::kBorderPrimary, .width = {0, 0, 0, 1, 0}};
	CLAY(context.clayID(Keyed("section", identity)), heading) {
		ui::icon(context, Keyed("section-icon", identity), section_icon(identity),
				 interface_theme::kAccentSeaGlass, 14);
		ui::text(context, title, interface_theme::kTextCanvas, 14);
	}
}

template <class Contents>
void section_surface(Context& context, uint64_t identity, std::string_view title,
					 Contents&& contents) {
	auto surface = ui::column(0);
	surface.backgroundColor = interface_theme::kDepth1Panel;
	surface.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
	surface.cornerRadius = CLAY_CORNER_RADIUS(4);
	CLAY(context.clayID(Keyed("section-surface", identity)), surface) {
		section(context, identity, title);
		auto body = ui::column(10);
		body.layout.padding = {12, 12, 12, 12};
		CLAY(context.clayID(Keyed("section-body", identity)), body) {
			contents();
		}
	}
}

void notice(Context& context, uint64_t identity, std::string_view label, bool warning) {
	auto surface = ui::row(0);
	surface.layout.sizing.height = CLAY_SIZING_FIT(0);
	surface.layout.padding = {9, 9, 8, 8};
	surface.layout.childGap = 7;
	surface.backgroundColor = interface_theme::kDepth2Ink;
	surface.border = {.color =
						  warning ? interface_theme::kStatusAmber : interface_theme::kBorderVisible,
					  .width = {2, 0, 0, 0, 0}};
	CLAY(context.clayID(Keyed("notice", identity)), surface) {
		ui::icon(context, Keyed("notice-icon", identity),
				 warning ? diagnostics_icons::Icon::Warning : diagnostics_icons::Icon::Info,
				 warning ? interface_theme::kStatusAmber : interface_theme::kAccentSeaGlass, 14);
		ui::text(context, label,
				 warning ? interface_theme::kStatusAmber : interface_theme::kAccentSeaGlass, 12);
	}
}

[[nodiscard]] std::string grouping_name(uint64_t grouping) {
	if (grouping == 1)
		return "Native code (code + site + native code)";
	if (grouping == 2)
		return "Source location (first retained step)";
	return "Contract (code + site)";
}
[[nodiscard]] Clay_Color impact_color(ErrorImpact impact) noexcept {
	if (impact >= ErrorImpact::FrameCanceled)
		return interface_theme::kStatusRed;
	return impact >= ErrorImpact::Degraded ? interface_theme::kStatusAmber
										   : interface_theme::kTextSecondary;
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
	return "Unknown confidence";
}
[[nodiscard]] std::string_view correlation_state(DevErrorCorrelationState state) noexcept {
	switch (state) {
	case DevErrorCorrelationState::Available:
		return "Available";
	case DevErrorCorrelationState::Evicted:
		return "Evicted";
	case DevErrorCorrelationState::NotCaptured:
		return "NotCaptured";
	case DevErrorCorrelationState::Ambiguous:
		return "Ambiguous";
	}
	return "Unknown state";
}
[[nodiscard]] std::string_view
correlation_confidence(DevErrorCorrelationConfidence confidence) noexcept {
	switch (confidence) {
	case DevErrorCorrelationConfidence::Explicit:
		return "Explicit";
	case DevErrorCorrelationConfidence::SharedContext:
		return "SharedContext";
	case DevErrorCorrelationConfidence::TemporalNearby:
		return "TemporalNearby";
	case DevErrorCorrelationConfidence::None:
		return "None";
	}
	return "Unknown confidence";
}
[[nodiscard]] std::string_view evidence_name(DevErrorAdviceEvidenceKind kind) noexcept {
	switch (kind) {
	case DevErrorAdviceEvidenceKind::Contract:
		return "Contract";
	case DevErrorAdviceEvidenceKind::Subject:
		return "Subject";
	case DevErrorAdviceEvidenceKind::Auxiliary:
		return "Auxiliary";
	case DevErrorAdviceEvidenceKind::Resolution:
		return "Resolution";
	case DevErrorAdviceEvidenceKind::Frame:
		return "Frame";
	case DevErrorAdviceEvidenceKind::TimingZone:
		return "Timing zone";
	case DevErrorAdviceEvidenceKind::MemoryEvent:
		return "Memory event";
	case DevErrorAdviceEvidenceKind::MemoryCheckpoint:
		return "Memory checkpoint";
	case DevErrorAdviceEvidenceKind::NativeCode:
		return "Native code";
	case DevErrorAdviceEvidenceKind::Snapshot:
		return "Snapshot";
	case DevErrorAdviceEvidenceKind::CaptureQuality:
		return "Capture quality";
	}
	return "Unknown evidence";
}
[[nodiscard]] std::string_view step_name(DevErrorStepKind kind) noexcept {
	switch (kind) {
	case DevErrorStepKind::Raised:
		return "Raised";
	case DevErrorStepKind::Boundary:
		return "Boundary / propagation";
	case DevErrorStepKind::Attempt:
		return "Attempt";
	case DevErrorStepKind::Resolution:
		return "Recorded resolution";
	case DevErrorStepKind::Consequence:
		return "Consequence";
	case DevErrorStepKind::BackendDiagnostic:
		return "Backend diagnostic";
	case DevErrorStepKind::FatalCapture:
		return "Fatal capture";
	}
	return "Unknown recorded step";
}
[[nodiscard]] bool has_flag(DevErrorRecordFlag flags, DevErrorRecordFlag flag) noexcept {
	return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(flag)) != 0;
}
[[nodiscard]] std::vector<std::string> limitations(const DevDiagnosticsState& state,
												   const DevErrorOccurrence& occurrence) {
	std::vector<std::string> result{};
	result.reserve(24);
	if (state.frozen_details_limited)
		result.emplace_back("Frozen view: the list and available selected details are held. Other "
							"details exceeded the capture budget; resume live to inspect them.");
	auto flags = occurrence.flags;
	for (const auto& step : occurrence.steps)
		flags |= step.flags;
	const std::array<std::pair<DevErrorRecordFlag, std::string_view>, 12> descriptions = {
		{{DevErrorRecordFlag::NativeTextTruncated,
		  "Occurrence: native diagnostic text was truncated."},
		 {DevErrorRecordFlag::SourceUnavailable, "Occurrence: a captured source descriptor is "
												 "unavailable; source paths cannot be inferred."},
		 {DevErrorRecordFlag::CausalHistoryUnavailable,
		  "Occurrence: causal history was unavailable."},
		 {DevErrorRecordFlag::Recursive, "Occurrence: recursive reporting was observed."},
		 {DevErrorRecordFlag::StepHistoryTruncated,
		  "Occurrence: step history was truncated; omitted stages cannot be reconstructed."},
		 {DevErrorRecordFlag::BreadcrumbHistoryTruncated,
		  "Occurrence: breadcrumb history was truncated."},
		 {DevErrorRecordFlag::StackUnavailable, "Occurrence: a requested stack was unavailable."},
		 {DevErrorRecordFlag::StackTruncated, "Occurrence: captured stack frames were truncated."},
		 {DevErrorRecordFlag::StackStorageLost, "Occurrence: retained stack storage was lost."},
		 {DevErrorRecordFlag::SnapshotUnavailable,
		  "Occurrence: requested captured state was unavailable."},
		 {DevErrorRecordFlag::SnapshotStale, "Occurrence: captured state has a stale revision."},
		 {DevErrorRecordFlag::SnapshotTruncated,
		  "Occurrence: captured state values were truncated."}}};
	for (const auto& [flag, description] : descriptions)
		if (has_flag(flags, flag))
			result.emplace_back(description);
	if (std::ranges::any_of(occurrence.steps, [&](const auto& step) {
			return step.sourceId && !diagnostics_source(state, step.sourceId);
		}))
		result.emplace_back(
			"Occurrence: at least one nonzero source ID has no retained descriptor.");
	for (const auto& snapshot : occurrence.snapshots) {
		if (snapshot.outcome == DevErrorSnapshotOutcome::StaleRevision)
			result.emplace_back("Occurrence snapshot " + std::to_string(snapshot.source) +
								": expected and captured revisions differ.");
		if (snapshot.outcome == DevErrorSnapshotOutcome::StorageLost)
			result.emplace_back("Occurrence snapshot " + std::to_string(snapshot.source) +
								": captured storage was lost.");
		if (snapshot.outcome == DevErrorSnapshotOutcome::Unavailable)
			result.emplace_back("Occurrence snapshot " + std::to_string(snapshot.source) +
								": provider data was unavailable.");
		if (snapshot.outcome == DevErrorSnapshotOutcome::Truncated)
			result.emplace_back("Occurrence snapshot " + std::to_string(snapshot.source) +
								": value storage was truncated.");
	}
	if (occurrence.timing.state == DevErrorCorrelationState::Evicted ||
		occurrence.memory.state == DevErrorCorrelationState::Evicted)
		result.emplace_back("Occurrence: correlated evidence was evicted. The captured identity "
							"remains available for inspection.");
	if (occurrence.timing.incomplete || occurrence.memory.incomplete)
		result.emplace_back("Occurrence: a correlation search window is incomplete; sealing does "
							"not guarantee evidence availability.");
	if (const auto* capture = diagnostics_capture(state, occurrence.captureId);
		capture && (capture->timingTruncated || capture->memoryTruncated ||
					capture->timingHistoryEvicted || capture->memoryHistoryEvicted))
		result.emplace_back("Occurrence triggered capture: timing or memory evidence was truncated "
							"or evicted; the requested window is incomplete.");
	const auto& status = state.snapshot.status;
	if (status.evictedOccurrences)
		result.emplace_back("Reporting-wide: " + std::to_string(status.evictedOccurrences) +
							" occurrences were evicted. This is not a loss count for this issue.");
	if (status.quality.droppedEvents || status.quality.suppressedEvents)
		result.emplace_back("Reporting-wide: " + std::to_string(status.quality.droppedEvents) +
							" dropped and " + std::to_string(status.quality.suppressedEvents) +
							" suppressed events; observation coverage is incomplete.");
	if (status.consumeFailures)
		result.emplace_back("Reporting-wide: " + std::to_string(status.consumeFailures) +
							" consume failures; the last coherent report is retained.");
	return result;
}

void metric(Context& context, uint64_t identity, std::string_view label, std::string_view value) {
	auto surface = ui::card(4);
	surface.layout.padding = {9, 9, 7, 7};
	surface.backgroundColor = interface_theme::kDepth0Keel;
	surface.border.color = interface_theme::kBorderPrimary;
	CLAY(context.clayID(Keyed("metric", identity)), surface) {
		ui::text(context, label, interface_theme::kTextSecondary, 11);
		ui::text(context, value, interface_theme::kTextCanvas, 14);
	}
}

void overview(Context& context, DevDiagnosticsState& state, const DevErrorOccurrence& occurrence,
			  const std::vector<std::string>& capture_limitations, bool compact) {
	const auto descriptor = occurrence.error.descriptor();
	auto identity = ui::column(5);
	CLAY(context.clayID("issue-identity"), identity) {
		ui::text(context, descriptor.name, interface_theme::kTextCanvas, 20);
		ui::text(context, occurrence.error.siteDescriptor().name, interface_theme::kTextSecondary,
				 13);
		ui::text(context,
				 diagnostics_domain_name(occurrence.error.domain()) +
					 " · Contract impact: " + diagnostics_impact_name(descriptor.impact),
				 impact_color(descriptor.impact), 12);
		ui::text(context, occurrence.error.message(), interface_theme::kTextCanvas, 14);
	}
	const auto* group = selected_diagnostics_group(state);
	button(context, "deselect", "Clear selection", ClearSelection, 0, true,
		   "Clear this investigation without changing the error capture or retained evidence.");
	ui::text(context,
			 "Selected occurrence #" + std::to_string(occurrence.id) +
				 (state.follow_latest ? " · Following latest" : " · Pinned") + " · " +
				 std::string(diagnostics_collection_name(occurrence.state)),
			 interface_theme::kTextSecondary, 12);
	if (state.frozen)
		ui::text(context,
				 "Frozen at tick " + std::to_string(state.anchor_tick) +
					 (state.selected_only_freeze	 ? " · selected investigation only"
					  : state.frozen_details_limited ? " · list and selected details"
													 : " · owned retained set"),
				 interface_theme::kAccentSeaGlass, 12);
	if (state.selected_evicted) {
		ui::text(context,
				 "No longer retained. This owned historical investigation remains inspectable.",
				 interface_theme::kStatusAmber, 12);
		button(context, "evicted-latest", "Choose latest retained", Latest, 0,
			   group != nullptr && !state.selected_only_freeze);
	}
	if (state.selected_outside_filters) {
		ui::text(context, "Selection outside current filters", interface_theme::kStatusAmber, 12);
		button(context, "outside-reset", "Clear filters", Reset);
	}
	if (!capture_limitations.empty())
		notice(context, 1,
			   "Evidence limitations affect this investigation or reporting coverage. See the "
			   "final section.",
			   true);
	if (group) {
		ui::text(context,
				 group->ongoing ? "Seen within " + std::to_string(state.filters.ongoing_ticks) +
									  " ticks · Observation incomplete"
								: "Quiet in this window · Observation incomplete",
				 interface_theme::kTextSecondary, 12);
		const std::string first_last = "Ticks " + std::to_string(group->first_tick) + " to " +
									   std::to_string(group->last_tick);
		const std::string interval =
			group->count == 1
				? "Single retained observation"
				: std::to_string(group->last_tick - group->first_tick) + " application ticks";
		auto metrics = compact ? ui::column(6) : ui::row(0);
		metrics.layout.sizing.height = CLAY_SIZING_FIT(0);
		CLAY(context.clayID("metrics-first"), metrics) {
			metric(context, 0, "Retained occurrences", std::to_string(group->count));
			metric(context, 1, "Recent recurrence (partial coverage)",
				   std::to_string(group->recent_count) + " / " +
					   std::to_string(state.filters.ongoing_ticks) + " ticks");
		}
		CLAY(context.clayID("metrics-second"), metrics) {
			metric(context, 2, "First / last retained observation", first_last);
			metric(context, 3, "Observed interval", interval);
		}
		auto scope = ui::card(5);
		scope.layout.padding = {9, 9, 8, 8};
		scope.backgroundColor = interface_theme::kDepth0Keel;
		scope.border.color = interface_theme::kBorderPrimary;
		CLAY(context.clayID("retained-scope"), scope) {
			ui::text(context, "Scope & retained outcomes", interface_theme::kTextCanvas, 12);
			ui::text(context,
					 grouping_name(state.selected_mode) + " · " +
						 std::to_string(diagnostics_filter_count(state.filters)) +
						 " advanced filters" +
						 (state.filters.search.empty() ? "" : " · search active") + " · across " +
						 std::to_string(group->window_count) + " known windows / " +
						 std::to_string(group->subject_count) + " subjects",
					 interface_theme::kTextSecondary, 11);
			std::string outcomes = "Retained outcomes: ";
			bool first = true;
			for (uint64_t outcome = 0; outcome < group->outcomes.size(); ++outcome)
				if (group->outcomes[outcome]) {
					if (!first)
						outcomes += " · ";
					first = false;
					outcomes += diagnostics_resolution_name(static_cast<ErrorResolution>(outcome)) +
								" x" + std::to_string(group->outcomes[outcome]);
				}
			ui::text(context, outcomes, interface_theme::kTextSecondary, 11);
		}
	} else {
		ui::text(context,
				 "Filtered group metrics are unavailable for this historical or hidden selection.",
				 interface_theme::kTextMuted, 12);
	}
	ui::text(context,
			 "Selected occurrence outcome: " + diagnostics_resolution_name(occurrence.resolution),
			 interface_theme::kTextCanvas, 12);
}

void advice(Context& context, DevDiagnosticsState& state, const DevErrorOccurrence& occurrence) {
	context.state().evidence_actions.clear();
	context.state().evidence_actions.reserve(occurrence.advice.size() *
											 kDevErrorAdviceEvidenceCapacity);
	if (occurrence.advice.empty())
		ui::text(context, "No catalogue advice is available for this occurrence.",
				 interface_theme::kTextSecondary, 14);
	for (std::size_t advice_index = 0; advice_index < occurrence.advice.size(); ++advice_index) {
		const auto& recommendation = occurrence.advice[advice_index];
		auto surface = ui::card();
		if (advice_index == 0) {
			surface.border.color = interface_theme::kAccentCurrent;
			surface.border.width.left = 3;
		}
		CLAY(context.clayID(Keyed("advice", recommendation.descriptorId)), surface) {
			ui::text(context, recommendation.title, interface_theme::kTextCanvas, 14);
			ui::text(context,
					 std::string("Evidence confidence: ") +
						 std::string(confidence_name(recommendation.confidence)),
					 recommendation.confidence == DevErrorAdviceConfidence::InsufficientEvidence
						 ? interface_theme::kStatusAmber
						 : interface_theme::kAccentSeaGlass,
					 12);
			ui::text(context, recommendation.explanation, interface_theme::kTextSecondary, 14);
			ui::text(context, "Recommended action", interface_theme::kTextCanvas, 12);
			ui::text(context, recommendation.suggestedAction, interface_theme::kTextCanvas, 14);
			if (!recommendation.configurationKey.empty()) {
				ui::text(context,
						 "Configuration guidance: " + std::string(recommendation.configurationKey),
						 interface_theme::kTextSecondary, 12);
				context.uiManager
					.createElement(ui::kControl, Keyed("copy-key", recommendation.descriptorId))
					.setParameters({.label = "Copy configuration key",
									.tooltip = "Guidance for application configuration. Production "
											   "resolution policies are chosen at App creation.",
									.clipboard_text = std::string(recommendation.configurationKey),
									.enabled = context.uiManager.hasClipboardAccess()})
					.setDevInternalCapture(true)
					.draw();
			}
			if (!recommendation.documentation.empty())
				ui::text(context,
						 "Documentation: " + std::string(recommendation.documentation) +
							 " (external link resolver unavailable)",
						 interface_theme::kTextMuted, 12);
			if (!recommendation.limitation.empty())
				ui::text(context, recommendation.limitation, interface_theme::kStatusAmber, 12);
			for (std::size_t evidence_index = 0;
				 evidence_index < std::min<std::size_t>(recommendation.evidenceCount,
														recommendation.evidence.size());
				 ++evidence_index) {
				const auto evidence = recommendation.evidence[evidence_index];
				const auto action_identity = context.state().evidence_actions.size();
				context.state().evidence_actions.emplace_back(evidence);
				bool available = true;
				if (evidence.kind == DevErrorAdviceEvidenceKind::Snapshot)
					available =
						std::ranges::any_of(occurrence.snapshots, [&](const auto& snapshot) {
							return snapshot.source == evidence.primary &&
								   snapshot.capturedRevision == evidence.secondary;
						});
				button(context,
					   Keyed("advice-evidence", recommendation.descriptorId * 16 + evidence_index),
					   std::string(evidence_name(evidence.kind)) + " · " +
						   std::to_string(evidence.primary) + " / " +
						   std::to_string(evidence.secondary),
					   Advice, action_identity, available,
					   "Opens captured evidence with these exact primary and secondary identities. "
					   "Confidence describes support for catalogue advice, not a probability or "
					   "proof of causation.");
				if (!available)
					ui::text(context, "Referenced snapshot is unavailable in retained evidence.",
							 interface_theme::kStatusAmber, 11);
			}
		}
	}
}

void history(Context& context, DevDiagnosticsState& state, const DevErrorOccurrence& occurrence,
			 bool compact) {
	ui::text(context,
			 "Counts occurrence starts. Empty bins mean no retained occurrence observed. Coverage "
			 "is not established; reporting-wide losses are listed below.",
			 interface_theme::kTextSecondary, 12);
	const auto maximum = std::max_element(state.bins.begin(), state.bins.end(),
										  [](const auto& left, const auto& right) {
											  return left.count < right.count;
										  })
							 ->count;
	auto strip = ui::row(72);
	strip.layout.childGap = 1;
	strip.layout.padding = {5, 5, 5, 5};
	strip.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID("history-strip"), strip) {
		for (uint64_t bin_index = 0; bin_index < state.bins.size(); ++bin_index) {
			const auto& bin = state.bins[bin_index];
			context.uiManager.createElement(ui::kControl, Keyed("history-bin", bin_index))
				.setParameters({.label = "",
								.tooltip = "Ticks " + std::to_string(bin.first_tick) + ".." +
										   std::to_string(bin.last_tick) + ": " +
										   std::to_string(bin.count) +
										   " retained occurrence starts" +
										   (bin.incomplete ? ". Observation coverage incomplete."
														   : ". Coverage is not guaranteed."),
								.activate = activate,
								.target = &context.state(),
								.action = Bin,
								.value = bin_index,
								.selected = state.selected_bin == bin_index,
								.histogram_bin = true})
				.setDevInternalCapture(true)
				.construct();
			auto bar = ui::column(0);
			bar.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIXED(maximum ? 4 + 48 * static_cast<float>(bin.count) /
															  static_cast<float>(maximum)
													: 4)};
			bar.backgroundColor = bin.selected ? interface_theme::kAccentCurrent
								  : bin.count  ? interface_theme::kAccentSignalBlue
											   : interface_theme::kBorderPrimary;
			bar.border = {.color = bin.incomplete ? interface_theme::kStatusAmber
												  : interface_theme::kBorderSubtle,
						  .width = {0, 0, 0, 2, 0}};
			CLAY(context.clayID(Keyed("bar", bin_index)), bar) {}
			context.uiManager.drawConstructed();
		}
	}
	ui::text(context,
			 "Application ticks " + std::to_string(state.bins.front().first_tick) + " to " +
				 std::to_string(state.bins.back().last_tick),
			 interface_theme::kTextMuted, 11);
	auto transport = compact ? ui::column(6) : ui::row();
	transport.layout.sizing.height = CLAY_SIZING_FIT(0);
	CLAY(context.clayID("history-transport"), transport) {
		button(context, "latest", "Latest retained", Latest, 0,
			   !state.history.empty() && !state.selected_only_freeze);
		button(context, "first", "First retained", First, 0,
			   !state.history.empty() && !state.selected_only_freeze);
		button(context, "specific", "Specific occurrence...", Specific, 0,
			   !state.selected_only_freeze);
	}
	CLAY(context.clayID("history-transport-next"), transport) {
		const auto found =
			std::ranges::find(state.history, occurrence.id, &DevDiagnosticsHistoryRow::id);
		const auto index = found == state.history.end()
							   ? state.history.size()
							   : static_cast<std::size_t>(found - state.history.begin());
		button(context, "previous", "Previous", Previous, 0,
			   index < state.history.size() && index + 1 < state.history.size() &&
				   !state.selected_only_freeze);
		button(context, "next", "Next", Next, 0,
			   index > 0 && index < state.history.size() && !state.selected_only_freeze);
		button(context, "follow", state.follow_latest ? "Follow latest: on" : "Follow latest: off",
			   Follow, 0, !state.selected_only_freeze);
	}
	if (state.newer_available && !state.follow_latest)
		ui::text(context, "Newer occurrences available · investigation remains pinned",
				 interface_theme::kAccentSeaGlass, 12);
	if (context.state().history_open) {
		auto browser = ui::card();
		CLAY(context.clayID("occurrence-browser"), browser) {
			ui::text(context,
					 state.selected_bin < state.bins.size()
						 ? "Retained starts in selected tick bin"
						 : "Specific retained occurrences (newest first)",
					 interface_theme::kTextCanvas, 13);
			std::vector<const DevDiagnosticsHistoryRow*> rows{};
			rows.reserve(state.history.size());
			for (const auto& row : state.history) {
				if (state.selected_bin < state.bins.size()) {
					const auto& bin = state.bins[state.selected_bin];
					if (!bin.count || row.first_tick < bin.first_tick ||
						row.first_tick > bin.last_tick)
						continue;
				}
				rows.emplace_back(&row);
			}
			const auto pages =
				std::max(uint64_t{1}, (rows.size() + history_page_size - 1) / history_page_size);
			state.history_page = std::min(state.history_page, pages - 1);
			const auto begin = state.history_page * history_page_size;
			for (auto row_index = begin;
				 row_index < std::min<uint64_t>(rows.size(), begin + history_page_size);
				 ++row_index) {
				const auto& row = *rows[row_index];
				context.uiManager.createElement(ui::kControl, Keyed("occurrence", row.id))
					.setParameters({.label = "#" + std::to_string(row.id) + " · tick " +
											 std::to_string(row.first_tick),
									.secondary = "Window " + std::to_string(row.window) +
												 " / frame " + std::to_string(row.frame) + " · " +
												 diagnostics_resolution_name(row.resolution) +
												 (row.evidence_available ? " · recorded evidence"
																		 : " · no steps retained"),
									.icon = diagnostics_icons::Icon::Pin,
									.activate = activate,
									.target = &context.state(),
									.action = Pin,
									.value = row.id,
									.selected = row.id == occurrence.id,
									.issue_row = true,
									.flexible = true})
					.setDevInternalCapture(true)
					.draw();
			}
			if (rows.empty())
				ui::text(context, "No retained occurrence starts in this bin.",
						 interface_theme::kTextMuted, 12);
			ui::text(context,
					 "Page " + std::to_string(state.history_page + 1) + " / " +
						 std::to_string(pages),
					 interface_theme::kTextMuted, 11);
			button(context, "history-page-previous", "Previous page", HistoryPrevious, 0,
				   state.history_page > 0);
			button(context, "history-page-next", "Next page", HistoryNext, 0,
				   state.history_page + 1 < pages);
			if (state.selected_bin < state.bins.size())
				button(context, "all-history", "Show all retained starts", AllBins);
		}
	}
}

[[nodiscard]] std::string offset_label(uint64_t timestamp, uint64_t origin) {
	if (!timestamp || !origin)
		return "Time unavailable";
	const bool negative = timestamp < origin;
	const auto distance = negative ? origin - timestamp : timestamp - origin;
	std::array<char, 64> buffer{};
	std::snprintf(buffer.data(), buffer.size(), "%s%.3f ms", negative ? "-" : "+",
				  static_cast<double>(distance) / 1'000'000.0);
	return buffer.data();
}
void timeline(Context& context, DevDiagnosticsState& state, const DevErrorOccurrence& occurrence) {
	ui::text(context,
			 "Observed records use solid markers. Equal timestamps across streams do not establish "
			 "causal order.",
			 interface_theme::kTextSecondary, 12);
	struct Event {
		uint64_t timestamp = 0, sequence = 0, kind = 0;
		std::size_t index = 0;
	};
	std::vector<Event> events{};
	events.reserve(occurrence.steps.size() + occurrence.breadcrumbs.size());
	for (std::size_t index = 0; index < occurrence.steps.size(); ++index)
		events.emplace_back(
			Event{occurrence.steps[index].timestampNs, occurrence.steps[index].sequence, 1, index});
	std::vector<std::pair<uint64_t, uint32_t>> breadcrumb_identities{};
	breadcrumb_identities.reserve(occurrence.breadcrumbs.size());
	for (std::size_t index = 0; index < occurrence.breadcrumbs.size(); ++index) {
		const auto& breadcrumb = occurrence.breadcrumbs[index];
		const auto identity = std::pair{breadcrumb.sequence, breadcrumb.threadTrack};
		if (std::ranges::find(breadcrumb_identities, identity) == breadcrumb_identities.end()) {
			breadcrumb_identities.emplace_back(identity);
			events.emplace_back(Event{breadcrumb.timestampNs, breadcrumb.sequence, 2, index});
		}
	}
	std::ranges::sort(events, [](const auto& left, const auto& right) {
		return std::tie(left.timestamp, left.kind, left.sequence) <
			   std::tie(right.timestamp, right.kind, right.sequence);
	});
	if (!std::ranges::any_of(occurrence.steps, [](const auto& step) {
			return step.kind == DevErrorStepKind::Raised;
		}))
		ui::text(context, "Raised step not retained", interface_theme::kStatusAmber, 12);
	if (events.empty())
		ui::text(context, "No steps or breadcrumbs were captured for this occurrence.",
				 interface_theme::kTextMuted, 12);
	const auto pages =
		std::max(uint64_t{1}, (events.size() + timeline_page_size - 1) / timeline_page_size);
	auto& local = context.state();
	local.timeline_page = std::min(local.timeline_page, pages - 1);
	const auto begin = local.timeline_page * timeline_page_size;
	for (auto event_index = begin;
		 event_index < std::min<uint64_t>(events.size(), begin + timeline_page_size);
		 ++event_index) {
		const auto& event = events[event_index];
		std::string label, summary;
		uint64_t tick = 0;
		Clay_Color marker = interface_theme::kTextSecondary;
		if (event.kind == 1) {
			const auto& step = occurrence.steps[event.index];
			tick = step.context.appTick;
			label = std::string(step_name(step.kind));
			summary = step.kind == DevErrorStepKind::Resolution
						  ? diagnostics_resolution_name(step.resolution)
						  : std::string(step.error.message());
			if (step.kind == DevErrorStepKind::BackendDiagnostic) {
				summary = "Native category " + std::to_string(step.nativeCategory) + ": " +
						  std::string(
							  step.nativeText.data(),
							  std::min<std::size_t>(step.nativeTextLength, step.nativeText.size()));
				if (has_flag(step.flags, DevErrorRecordFlag::NativeTextTruncated))
					summary += " [truncated]";
			}
			if (const auto* source = diagnostics_source(state, step.sourceId))
				summary += " · " + source->name;
			else if (step.sourceId)
				summary += " · descriptor unavailable #" + std::to_string(step.sourceId);
			if (step.kind == DevErrorStepKind::Raised ||
				step.kind == DevErrorStepKind::FatalCapture)
				marker = impact_color(step.error.descriptor().impact);
		} else {
			const auto& breadcrumb = occurrence.breadcrumbs[event.index];
			tick = breadcrumb.context.appTick;
			label = std::string(breadcrumb.occurrence == occurrence.id ? "Breadcrumb · "
																	   : "Context breadcrumb · ") +
					std::string(diagnostics_breadcrumb_name(state, breadcrumb.descriptorId));
			summary = "Values " + std::to_string(breadcrumb.primaryValue) + " / " +
					  std::to_string(breadcrumb.secondaryValue) + " · thread " +
					  std::to_string(breadcrumb.threadTrack);
		}
		label = offset_label(event.timestamp, occurrence.firstTimestampNs) + " · " + label;
		summary +=
			" · tick " + std::to_string(tick) + " · sequence " + std::to_string(event.sequence);
		context.uiManager
			.createElement(ui::kControl, Keyed(event.kind == 1 ? LocalElementName{"step"}
															   : LocalElementName{"breadcrumb"},
											   event.sequence))
			.setParameters({.label = std::move(label),
							.secondary = std::move(summary),
							.tooltip = "Recorded evidence focus updates the Inspector while "
									   "preserving this occurrence.",
							.icon = event.kind == 1 ? step_icon(occurrence.steps[event.index].kind)
													: diagnostics_icons::Icon::Info,
							.activate = activate,
							.target = &local,
							.action = event.kind == 1 ? Step : Breadcrumb,
							.value = event.sequence,
							.marker = marker,
							.selected = state.focus_kind == event.kind &&
										state.focus_sequence == event.sequence,
							.issue_row = true,
							.flexible = true})
			.setDevInternalCapture(true)
			.draw();
		if (event.kind == 1 && occurrence.steps[event.index].cause) {
			const auto cause_id = occurrence.steps[event.index].cause;
			const bool retained = std::ranges::any_of(
				state.snapshot.rows, [&](const auto& row) { return row.id == cause_id; });
			button(context, Keyed("cause", event.sequence),
				   "Explicit cause: occurrence #" + std::to_string(cause_id), Cause, cause_id,
				   retained && !state.selected_only_freeze);
			if (!retained)
				ui::text(context, "Cause occurrence is not retained; identity preserved.",
						 interface_theme::kStatusAmber, 11);
		}
	}
	if (!std::ranges::any_of(occurrence.steps, [](const auto& step) {
			return step.kind == DevErrorStepKind::Resolution;
		}))
		ui::text(context, "No resolution step captured", interface_theme::kTextMuted, 12);
	if (pages > 1) {
		ui::text(context,
				 "Timeline page " + std::to_string(local.timeline_page + 1) + " / " +
					 std::to_string(pages),
				 interface_theme::kTextMuted, 11);
		button(context, "timeline-previous", "Previous timeline page", TimelinePrevious, 0,
			   local.timeline_page > 0);
		button(context, "timeline-next", "Next timeline page", TimelineNext, 0,
			   local.timeline_page + 1 < pages);
	}
	if (state.focus_kind)
		button(context, "reset-focus", "Use occurrence context", DefaultFocus);
}

void correlations(Context& context, DevDiagnosticsState& state,
				  const DevErrorOccurrence& occurrence) {
	ui::text(context,
			 "Context links are correlations. Shared context or temporal proximity does not "
			 "establish cause.",
			 interface_theme::kTextSecondary, 12);
	const auto surface = ui::card();
	CLAY(context.clayID("performance-evidence"), surface) {
		const auto& timing = occurrence.timing;
		auto performance_heading = ui::row(22);
		CLAY(context.clayID("performance-heading"), performance_heading) {
			ui::icon(context, "performance-heading-icon", diagnostics_icons::Icon::Performance,
					 interface_theme::kAccentSeaGlass, 15);
			ui::text(context, "Performance", interface_theme::kTextCanvas, 14);
		}
		ui::text(context,
				 std::string(correlation_state(timing.state)) +
					 " · Confidence: " + std::string(correlation_confidence(timing.confidence)) +
					 (timing.sealed ? " · sealed" : " · search open") +
					 (timing.incomplete ? " · incomplete" : ""),
				 timing.incomplete || timing.state == DevErrorCorrelationState::Evicted
					 ? interface_theme::kStatusAmber
					 : interface_theme::kTextSecondary,
				 12);
		ui::text(context,
				 "Tick " + std::to_string(timing.appTick) + " · window " +
					 std::to_string(timing.frame.window) + " / frame " +
					 std::to_string(timing.frame.frameNumber) + " · entity kind " +
					 std::to_string(static_cast<uint64_t>(timing.entity.kind)) + " / " +
					 std::to_string(timing.entity.primaryId) + " / " +
					 std::to_string(timing.entity.secondaryId),
				 interface_theme::kTextSecondary, 12);
		ui::text(context,
				 "Invocation " + std::to_string(timing.containingInvocation) + " · zone " +
					 std::to_string(timing.containingZone) + " · report revision " +
					 std::to_string(timing.reportRevision) + " · capture generation " +
					 std::to_string(timing.capture_generation),
				 interface_theme::kTextMuted, 11);
		button(context, "open-performance", "Open in Performance", Performance, 0,
			   timing.state == DevErrorCorrelationState::Available &&
				   timing.capture_generation != 0,
			   "Validates the exact retained context and capture generation before changing tabs.");
		if (timing.state != DevErrorCorrelationState::Available)
			ui::text(context,
					 timing.state == DevErrorCorrelationState::Evicted
						 ? "Evidence evicted; captured identity remains inspectable."
						 : "Matching timing evidence is unavailable or ambiguous.",
					 interface_theme::kTextMuted, 12);
	}
	CLAY(context.clayID("memory-evidence"), surface) {
		const auto& memory = occurrence.memory;
		auto memory_heading = ui::row(22);
		CLAY(context.clayID("memory-heading"), memory_heading) {
			ui::icon(context, "memory-heading-icon", diagnostics_icons::Icon::Memory,
					 interface_theme::kAccentSeaGlass, 15);
			ui::text(context, "Memory", interface_theme::kTextCanvas, 14);
		}
		ui::text(context,
				 std::string(correlation_state(memory.state)) +
					 " · Confidence: " + std::string(correlation_confidence(memory.confidence)) +
					 (memory.sealed ? " · sealed" : " · search open") +
					 (memory.incomplete ? " · incomplete" : ""),
				 memory.incomplete || memory.state == DevErrorCorrelationState::Evicted
					 ? interface_theme::kStatusAmber
					 : interface_theme::kTextSecondary,
				 12);
		ui::text(context,
				 "Operation " + std::to_string(static_cast<uint64_t>(memory.operation)) +
					 " · source " + std::to_string(memory.source) + " · event sequence " +
					 std::to_string(memory.eventSequence) + " · checkpoint tick " +
					 std::to_string(memory.checkpointTick),
				 interface_theme::kTextSecondary, 12);
		ui::text(context,
				 "Storage mutation " + std::to_string(memory.storageMutationSequence) +
					 " · history generation " + std::to_string(memory.history_generation),
				 interface_theme::kTextMuted, 11);
		button(context, "open-memory", "Open in Memory", Memory, 0,
			   memory.state == DevErrorCorrelationState::Available,
			   "Validates the exact event/checkpoint identity and history generation before "
			   "changing tabs.");
		if (memory.state != DevErrorCorrelationState::Available)
			ui::text(context,
					 memory.state == DevErrorCorrelationState::Evicted
						 ? "Evidence evicted; captured identity remains inspectable."
						 : "Matching memory evidence is unavailable or ambiguous.",
					 interface_theme::kTextMuted, 12);
	}
	CLAY(context.clayID("capture-evidence"), surface) {
		auto capture_heading = ui::row(22);
		CLAY(context.clayID("capture-heading"), capture_heading) {
			ui::icon(context, "capture-heading-icon", diagnostics_icons::Icon::Capture,
					 interface_theme::kAccentSeaGlass, 15);
			ui::text(context, "Triggered capture", interface_theme::kTextCanvas, 14);
		}
		if (const auto* capture = diagnostics_capture(state, occurrence.captureId)) {
			ui::text(context,
					 "Capture #" + std::to_string(capture->id) +
						 (capture->state == DevErrorTriggeredCaptureState::Collecting
							  ? " · Collecting"
							  : " · Complete") +
						 " · requested ticks " + std::to_string(capture->firstAppTick) + ".." +
						 std::to_string(capture->lastAppTick),
					 interface_theme::kTextSecondary, 12);
			ui::text(context,
					 std::to_string(capture->timing_retained_ticks) + " retained timing ticks · " +
						 std::to_string(capture->memoryEvents.size()) + " pinned memory events",
					 interface_theme::kTextSecondary, 12);
			ui::text(
				context,
				"Timing generation " + std::to_string(capture->timing_generation) +
					" · memory generation " + std::to_string(capture->memory_generation) +
					(capture->memory_sealed ? " · memory search sealed" : " · memory search open"),
				interface_theme::kTextMuted, 11);
			if (capture->timingTruncated || capture->memoryTruncated ||
				capture->timingHistoryEvicted || capture->memoryHistoryEvicted)
				ui::text(context,
						 "Requested capture window is incomplete: " +
							 std::string(capture->timingTruncated ? "timing truncated; " : "") +
							 (capture->memoryTruncated ? "memory truncated; " : "") +
							 (capture->timingHistoryEvicted ? "timing evicted; " : "") +
							 (capture->memoryHistoryEvicted ? "memory evicted" : ""),
						 interface_theme::kStatusAmber, 12);
		} else
			ui::text(context,
					 occurrence.captureId
						 ? "Capture #" + std::to_string(occurrence.captureId) +
							   " is no longer retained."
						 : "No triggered capture was admitted for this occurrence.",
					 interface_theme::kTextMuted, 12);
	}
	if (!state.navigation_message.empty())
		ui::text(context, state.navigation_message, interface_theme::kStatusAmber, 12);
}
} // namespace

void DevDiagnosticsWorkbench::buildElement(BuildContext& context) {
	auto root = ui::column(0);
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), root) {
		if (!context.params.app || !context.params.interfaceState) {
			ui::text(context, "Diagnostics reporting unavailable", interface_theme::kTextMuted);
		} else {
			auto& state = context.params.interfaceState->diagnostics;
			auto& local = context.state();
			local.diagnostics = &state;
			if (local.selected_occurrence != state.selected_id) {
				local.selected_occurrence = state.selected_id;
				local.timeline_page = 0;
			}
			const auto* occurrence = diagnostics_selected_occurrence(state);
			const auto previous_width = Clay_GetElementData(context.clayID()).boundingBox.width;
			const bool compact = previous_width <= 0 || previous_width < 540;
			auto scroll = ui::column(0);
			scroll.layout.sizing.height = CLAY_SIZING_GROW(0);
			const auto scroll_id = context.clayID("scroll");
			const auto scroll_data = Clay_GetScrollContainerData(scroll_id);
			if (scroll_data.found && scroll_data.scrollPosition) {
				if (local.scroll_to_section) {
					const auto target =
						Clay_GetElementData(
							context.clayID(Keyed("section", local.scroll_to_section)))
							.boundingBox;
					const auto viewport = Clay_GetElementData(scroll_id).boundingBox;
					scroll_data.scrollPosition->y -= target.y - viewport.y;
					local.scroll_to_section = 0;
				}
				state.workbench_scroll = scroll_data.scrollPosition->y;
			}
			scroll.clip = {.vertical = true,
						   .childOffset = scroll_data.found && scroll_data.scrollPosition
											  ? *scroll_data.scrollPosition
											  : Clay_Vector2{0, state.workbench_scroll}};
			CLAY(scroll_id, scroll) {
				auto report = ui::column(16);
				report.layout.padding = {15, 15, 14, 20};
				CLAY(context.clayID("report"), report) {
					if (!state.message.empty()) {
						const bool informational = !state.message_is_error;
						notice(context, 0, state.message, !informational);
						if (!informational)
							button(context, "retry", "Refresh report", Retry);
					}
					if (!occurrence) {
						ui::text(context, "Investigation report", interface_theme::kTextCanvas, 20);
						ui::text(context,
								 state.selected_id
									 ? (state.frozen
											? "Details were not included in this frozen view. "
											  "Resume live to inspect this occurrence."
											: "Loading the selected retained investigation...")
									 : "Select an issue to inspect its retained occurrences, "
									   "catalogue advice and captured evidence.",
								 interface_theme::kTextSecondary, 14);
						if (state.selected_id)
							button(context, "deselect", "Clear selection", ClearSelection);
						section_surface(context, 1, "Diagnosis & advice", [&] {
							ui::text(context, "No occurrence selected",
									 interface_theme::kTextSecondary);
						});
						section_surface(context, 2, "Occurrence history", [&] {
							ui::text(context, "Select an issue to see retained starts.",
									 interface_theme::kTextSecondary);
						});
						section_surface(context, 3, "Causal timeline", [&] {
							ui::text(context, "No occurrence selected",
									 interface_theme::kTextSecondary);
						});
						section_surface(context, 4, "Correlated evidence", [&] {
							ui::text(context, "Performance: No occurrence selected",
									 interface_theme::kTextSecondary);
							ui::text(context, "Memory: No occurrence selected",
									 interface_theme::kTextSecondary);
							ui::text(context, "Triggered capture: No occurrence selected",
									 interface_theme::kTextSecondary);
						});
						section_surface(context, 5, "Evidence limitations", [&] {
							ui::text(context,
									 "No occurrence selected. Retention and loss counters remain "
									 "visible in the selector.",
									 interface_theme::kTextSecondary);
						});
					} else {
						const auto capture_limitations = limitations(state, *occurrence);
						section_surface(context, 0, "Issue overview", [&] {
							overview(context, state, *occurrence, capture_limitations, compact);
						});
						section_surface(context, 1, "Diagnosis & advice",
										[&] { advice(context, state, *occurrence); });
						section_surface(context, 2, "Occurrence history",
										[&] { history(context, state, *occurrence, compact); });
						section_surface(context, 3, "Causal timeline",
										[&] { timeline(context, state, *occurrence); });
						section_surface(context, 4, "Correlated evidence",
										[&] { correlations(context, state, *occurrence); });
						section_surface(context, 5, "Evidence limitations", [&] {
							if (capture_limitations.empty())
								ui::text(
									context,
									"No known capture limitations for this retained occurrence. "
									"This does not establish completeness beyond configured "
									"instrumentation.",
									interface_theme::kTextSecondary, 13);
							for (std::size_t limitation_index = 0;
								 limitation_index < capture_limitations.size(); ++limitation_index)
								notice(context, 100 + limitation_index,
									   capture_limitations[limitation_index], true);
							ui::text(
								context,
								"Rates use application ticks because uninterrupted timestamp "
								"coverage is not established. Retained history is bounded and is "
								"not a lifetime count.",
								interface_theme::kTextMuted, 11);
						});
					}
				}
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements

#endif
