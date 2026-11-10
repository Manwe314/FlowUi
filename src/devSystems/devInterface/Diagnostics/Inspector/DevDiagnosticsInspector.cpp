#include "devSystems/devInterface/Diagnostics/Inspector/DevDiagnosticsInspector.hpp"

#if FLOW_UI_DEV_MODE

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <sstream>

#include "FSEL/Button.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsModel.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {
using Context = DevDiagnosticsInspector::BuildContext;
constexpr auto toggle_section = UiAction("flowui.diagnostics.inspector.disclosure",
										 [](bool& expanded) { expanded = !expanded; });

[[nodiscard]] Clay_ElementDeclaration column(uint16_t gap = 6) noexcept {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_FIT(0)};
	declaration.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	declaration.layout.childGap = gap;
	return declaration;
}
[[nodiscard]] Clay_ElementDeclaration row(uint16_t gap = 6) noexcept {
	auto declaration = column(gap);
	declaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	declaration.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	return declaration;
}
template <class BuildContext>
void text(BuildContext& context, std::string_view value,
		  Clay_Color color = interface_theme::kTextCanvas, uint16_t size = 12,
		  float maximum_width = 0, bool break_words = true) {
	Clay_TextElementConfig config{};
	config.fontSize = size;
	config.textColor = color;
	config.wrapMode = CLAY_TEXT_WRAP_WORDS;
	if (!break_words) {
		CLAY_TEXT(context.uiManager.toClayString(value), CLAY_TEXT_CONFIG(config));
		return;
	}
	const auto parent = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const float available_width = maximum_width > 0
									  ? maximum_width - 12
									  : (parent.found ? parent.boundingBox.width - 12 : 180);
	const size_t maximum_word = size_t(std::max(8.0f, available_width / (size * 1.05f)));
	std::string display;
	display.reserve(value.size() + value.size() / maximum_word);
	size_t word_length = 0;
	for (const unsigned char character : value) {
		if (character == ' ' || character == '\n' || character == '\t')
			word_length = 0;
		else if ((character & 0xc0u) != 0x80u) {
			if (word_length == maximum_word) {
				display += '\n';
				word_length = 0;
			}
			++word_length;
		}
		display += static_cast<char>(character);
	}
	CLAY_TEXT(context.uiManager.toClayString(display), CLAY_TEXT_CONFIG(config));
}
[[nodiscard]] std::string hex(uint64_t value) {
	char buffer[24]{};
	std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
	return buffer;
}
[[nodiscard]] std::string identity(uint64_t value) {
	return std::to_string(value) + " (" + hex(value) + ")";
}
template <class Enum, size_t Count>
[[nodiscard]] std::string vocabulary(Enum value, const std::array<std::string_view, Count>& names) {
	const auto numeric = static_cast<uint64_t>(value);
	return numeric < names.size() ? std::string(names[numeric])
								  : "Unknown (" + std::to_string(numeric) + ")";
}
[[nodiscard]] std::string category(ErrorCategory value) {
	return vocabulary(value, std::array<std::string_view, 4>{"Static", "Local", "Unwind", "Fatal"});
}
[[nodiscard]] std::string boundary(ErrorBoundary value) {
	return vocabulary(value, std::array<std::string_view, 8>{"CompileExpression", "DirectCaller",
															 "ManagerOperation", "WindowFrame",
															 "WindowLifecycle", "AppPoll",
															 "AppLifecycle", "Process"});
}
[[nodiscard]] std::string resolvability(ErrorResolvability value) {
	return vocabulary(value, std::array<std::string_view, 5>{"AutomaticallyResolved",
															 "CallerResolvable", "RecreateWindow",
															 "RecreateApp", "NotResolvable"});
}
[[nodiscard]] std::string policy(ErrorResolutionPolicy value) {
	return vocabulary(value, std::array<std::string_view, 4>{"HardCoded", "AppConfigured",
															 "CallerDefined", "None"});
}
[[nodiscard]] std::string production_kind(ErrorEventKind value) {
	return vocabulary(value, std::array<std::string_view, 4>{"Resolved", "Reported", "Fatal",
															 "BackendDiagnostic"});
}
[[nodiscard]] std::string step_kind(DevErrorStepKind value) {
	return vocabulary(value, std::array<std::string_view, 7>{"Raised", "Boundary", "Attempt",
															 "Resolution", "Consequence",
															 "BackendDiagnostic", "FatalCapture"});
}
[[nodiscard]] std::string subject_kind(ErrorSubjectKind value) {
	return vocabulary(value,
					  std::array<std::string_view, 18>{
						  "None", "App", "Window", "Frame", "Submission", "Resource", "ResourceKey",
						  "ElementDefinition", "ElementInstance", "Action", "Theme", "Font",
						  "Image", "Icon", "Popup", "Shortcut", "Viewport", "MemoryTarget"});
}
[[nodiscard]] std::string stack_status(DevErrorStackStatus value) {
	return vocabulary(value,
					  std::array<std::string_view, 5>{"Not requested", "Available", "Unavailable",
													  "Truncated", "Storage lost"});
}
[[nodiscard]] std::string snapshot_status(DevErrorSnapshotOutcome value) {
	return vocabulary(value,
					  std::array<std::string_view, 5>{"Available", "Unavailable", "Stale revision",
													  "Truncated", "Storage lost"});
}
[[nodiscard]] std::string evidence_key(DevErrorEvidenceKey value) {
	return vocabulary(value,
					  std::array<std::string_view, 12>{"None", "Subject", "Auxiliary", "NativeCode",
													   "Resolution", "LifecyclePhase",
													   "ConfiguredPolicy", "Capacity", "Count",
													   "Revision", "OwnershipFlags", "Custom"}) +
		   " [" + std::to_string(static_cast<uint16_t>(value)) + "]";
}
[[nodiscard]] bool has_flag(DevErrorRecordFlag flags, DevErrorRecordFlag flag) noexcept {
	return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(flag)) != 0;
}
[[nodiscard]] std::string flags_text(DevErrorRecordFlag flags) {
	constexpr std::array<std::string_view, 15> names{
		"ExternallyReported",  "DevOnlyDiagnostic",	   "NativeTextPresent",
		"NativeTextTruncated", "SourceUnavailable",	   "CausalHistoryUnavailable",
		"Recursive",		   "StepHistoryTruncated", "BreadcrumbHistoryTruncated",
		"StackUnavailable",	   "StackTruncated",	   "StackStorageLost",
		"SnapshotUnavailable", "SnapshotStale",		   "SnapshotTruncated"};
	std::string result = hex(static_cast<uint16_t>(flags));
	for (size_t flag_index = 0; flag_index < names.size(); ++flag_index)
		if ((static_cast<uint16_t>(flags) & (1u << flag_index)) != 0)
			result += " · " + std::string(names[flag_index]);
	return result + (flags == DevErrorRecordFlag::None ? " · None recorded" : "");
}
void copy(Context& context, uint64_t key, std::string value, std::string label = "Copy",
		  float width = 0, bool compact = false) {
	const auto icon = context.resources().texture(diagnostics_icons::Icon::Copy);
	context.uiManager.createElement(kDevDiagnosticsCopy, Keyed("copy", key))
		.setParameters(DevDiagnosticsCopyParameters{std::move(value), std::move(label), icon,
													context.params.app, width, compact})
		.setDevInternalCapture(true)
		.draw();
}
void property(Context& context, uint64_t key, std::string_view label, std::string_view value,
			  bool narrow, Clay_Color color = interface_theme::kTextCanvas) {
	const auto panel = Clay_GetElementData(context.clayID());
	const float panel_width =
		panel.found ? panel.boundingBox.width : context.params.interfaceState->inspectorWidth;
	const float inner_width = std::max(80.0f, panel_width - 24);
	constexpr float copy_width = FLOWUI_INCLUDE_ICON_MANAGER ? 22.0f : 44.0f;
	const float data_width = std::max(8.0f, inner_width - copy_width - 8);
	const float content_width = narrow ? data_width : data_width * .58f;
	auto declaration = row(8);
	declaration.layout.padding = {0, 0, 6, 6};
	declaration.layout.childAlignment.y = CLAY_ALIGN_Y_TOP;
	declaration.border = {.color = interface_theme::kBorderPrimary, .width = {.bottom = 1}};
	CLAY(context.clayID(Keyed("property", key)), declaration) {
		auto data = narrow ? column(3) : row(8);
		data.layout.sizing.width = CLAY_SIZING_FIXED(data_width);
		data.layout.childAlignment.y = CLAY_ALIGN_Y_TOP;
		CLAY(context.clayID(Keyed("property-data", key)), data) {
			auto label_cell = column(0);
			if (!narrow)
				label_cell.layout.sizing.width = CLAY_SIZING_PERCENT(.38f);
			CLAY(context.clayID(Keyed("property-label", key)), label_cell)
			text(context, label, interface_theme::kTextSecondary, 11, 0, false);
			auto value_cell = column(0);
			value_cell.layout.sizing.width = CLAY_SIZING_FIXED(content_width);
			CLAY(context.clayID(Keyed("property-value", key)), value_cell) {
				CLAY(context.clayID(Keyed("property-text", key)), column(0))
				text(context, value, color, 12, content_width);
			}
		}
		copy(context, key, std::string(value), "Copy " + std::string(label), copy_width, true);
	}
}
void number(Context& context, uint64_t key, std::string_view label, uint64_t value, bool narrow) {
	property(context, key, label, std::to_string(value), narrow);
}
void subtitle(Context& context, uint64_t key, std::string_view title) {
	auto declaration = column();
	declaration.layout.padding = {0, 0, 8, 3};
	declaration.border = {.color = interface_theme::kBorderPrimary, .width = {.top = 1}};
	CLAY(context.clayID(Keyed("subtitle", key)), declaration)
	text(context, title, interface_theme::kTextSecondary, 12);
}
void disclosure(Context& context, uint64_t key, std::string_view label, bool& expanded,
				bool header = true) {
	FSEL::ButtonParameters parameters{};
	const std::string owned_label = std::string(expanded ? "-  " : "+  ") + std::string(label);
	parameters.text = owned_label;
	parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
	parameters.labelFontSize = 12;
	parameters.labelAlignment = CLAY_TEXT_ALIGN_LEFT;
	parameters.labelWrapMode = CLAY_TEXT_WRAP_WORDS;
	parameters.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
	parameters.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(32)};
	parameters.padding = header ? Clay_Padding{12, 12, 8, 8} : Clay_Padding{6, 6, 5, 5};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(0);
	parameters.idleOverrides.backgroundColor =
		header ? interface_theme::kDepth2Ink : interface_theme::kDepth3Elevated;
	parameters.idleOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
#if FLOWUI_INCLUDE_ICON_MANAGER
	if (context.params.app) {
		auto& icons = context.params.app->icons();
		interface_icons::registerDevInterfaceIcons(icons);
		parameters.icon = icons.textureRef(expanded ? interface_icons::kCollapseKey
													: interface_icons::kExpandKey);
		parameters.contentMode = FSEL::ButtonContentMode::IconThenText;
		parameters.iconSize = 12;
		parameters.text = label;
	}
#endif
	if (context.params.app)
		parameters.onActivate =
			ActionCall{context.params.app->actions().uiActions().make(toggle_section, expanded)};
	context.uiManager.createElement(FSEL::kButton, Keyed("section-disclosure", key))
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
}
[[nodiscard]] const DevErrorRecord* focused_step(const DevDiagnosticsState& state,
												 const DevErrorOccurrence& occurrence) noexcept {
	if (state.focus_kind == 2)
		return nullptr;
	if (state.focus_kind == 1)
		for (const auto& step : occurrence.steps)
			if (step.sequence == state.focus_sequence)
				return &step;
	return occurrence.steps.empty() ? nullptr : &occurrence.steps.front();
}
[[nodiscard]] const DevErrorBreadcrumb*
focused_breadcrumb(const DevDiagnosticsState& state,
				   const DevErrorOccurrence& occurrence) noexcept {
	if (state.focus_kind == 2)
		for (const auto& breadcrumb : occurrence.breadcrumbs)
			if (breadcrumb.sequence == state.focus_sequence)
				return &breadcrumb;
	return nullptr;
}
void context_properties(Context& context, const DevErrorContext& captured, uint64_t timestamp,
						std::optional<uint64_t> sequence, uint32_t thread, bool narrow,
						uint64_t base = 200, std::string_view sequence_label = "Record sequence") {
	number(context, base, "Application tick", captured.appTick, narrow);
	property(context, base + 1, "Window ID",
			 captured.frame.window == InvalidWindowId ? "No window context"
													  : identity(captured.frame.window),
			 narrow);
	property(context, base + 2, "Window frame",
			 captured.frame.frameNumber ? std::to_string(captured.frame.frameNumber)
										: "Not recorded",
			 narrow);
	number(context, base + 3, "Phase (captured numeric)", captured.phase, narrow);
	number(context, base + 4, "Submission serial", captured.submissionSerial, narrow);
	property(context, base + 5, "Primary entity", identity(captured.primaryEntity), narrow);
	property(context, base + 6, "Secondary entity", identity(captured.secondaryEntity), narrow);
	number(context, base + 7, "Timing track", captured.timingTrack, narrow);
	number(context, base + 8, "Thread track", thread, narrow);
	number(context, base + 9, "Timestamp (ns)", timestamp, narrow);
	property(context, base + 10, sequence_label,
			 sequence ? std::to_string(*sequence) : "Not present in fatal capsule layout", narrow);
	text(context,
		 "Track identities and phase are captured numeric values. No retained name mapping is "
		 "available.",
		 interface_theme::kTextMuted, 11);
}
void evidence(Context& context, uint64_t key, std::span<const DevErrorEvidenceValue> values,
			  bool truncated, bool narrow) {
	if (truncated)
		text(context, "Immediate evidence truncated at its configured capacity.",
			 interface_theme::kStatusAmber, 11);
	if (values.empty()) {
		text(context, "No structured numeric values captured.", interface_theme::kTextSecondary);
		return;
	}
	for (size_t value_index = 0; value_index < values.size(); ++value_index) {
		const auto& value = values[value_index];
		property(context, key + value_index,
				 evidence_key(value.key) + " / index " + std::to_string(value.index),
				 identity(value.value), narrow);
	}
}
void contract(Context& context, const DevErrorOccurrence& occurrence, const DevErrorRecord* step,
			  bool narrow) {
	const auto descriptor = occurrence.error.descriptor();
	property(context, 1, "Error code", identity(static_cast<uint16_t>(occurrence.error.code)),
			 narrow);
	property(context, 2, "Error name", descriptor.name, narrow);
	property(context, 3, "Domain", diagnostics_domain_name(descriptor.domain), narrow);
	property(context, 4, "Transport category", category(descriptor.category), narrow);
	property(context, 5, "Intended boundary", boundary(descriptor.boundary), narrow);
	property(context, 6, "Contract impact", diagnostics_impact_name(descriptor.impact), narrow);
	property(context, 7, "Resolvability", resolvability(descriptor.resolvability), narrow);
	property(context, 8, "Resolution policy", policy(descriptor.resolutionPolicy), narrow);
	property(context, 9, "Default resolution",
			 diagnostics_resolution_name(descriptor.defaultResolution), narrow);
	property(context, 10, "Recorded occurrence resolution",
			 diagnostics_resolution_name(occurrence.resolution), narrow);
	property(context, 11, "Production event kind", production_kind(occurrence.productionKind),
			 narrow);
	property(context, 12, "Collection state",
			 std::string(diagnostics_collection_name(occurrence.state)) + " [" +
				 std::to_string(static_cast<uint8_t>(occurrence.state)) + "]",
			 narrow);
	property(context, 13, "Production site",
			 std::string(occurrence.error.siteDescriptor().name) + " · " +
				 identity(static_cast<uint16_t>(occurrence.error.site)),
			 narrow);
	property(context, 14, "Descriptor message", descriptor.message, narrow);
	if (step &&
		(step->error.code != occurrence.error.code || step->error.site != occurrence.error.site ||
		 step->resolution != occurrence.resolution)) {
		subtitle(context, 15, "Focused step contract");
		property(context, 16, "Focused step error",
				 std::string(step->error.descriptor().name) + " · " +
					 identity(static_cast<uint16_t>(step->error.code)),
				 narrow);
		property(context, 17, "Focused step site",
				 std::string(step->error.siteDescriptor().name) + " · " +
					 identity(static_cast<uint16_t>(step->error.site)),
				 narrow);
		property(context, 18, "Focused step resolution",
				 diagnostics_resolution_name(step->resolution), narrow);
		property(context, 19, "Focused production kind", production_kind(step->productionKind),
				 narrow);
	}
}
void source(Context& context, const DevDiagnosticsState& state, const DevErrorRecord* step,
			const DevErrorBreadcrumb* breadcrumb, bool narrow) {
	if (breadcrumb) {
		property(context, 100, "Breadcrumb descriptor",
				 diagnostics_breadcrumb_name(state, breadcrumb->descriptorId), narrow);
		property(context, 101, "Descriptor ID", identity(breadcrumb->descriptorId), narrow);
		text(context,
			 "Breadcrumb descriptors do not contain a source file, function, line or column.",
			 interface_theme::kTextSecondary);
		return;
	}
	if (!step) {
		text(context, "No retained step is available for source inspection.",
			 interface_theme::kTextSecondary);
		return;
	}
	property(context, 102, "Source ID", identity(step->sourceId), narrow);
	const auto* descriptor = diagnostics_source(state, step->sourceId);
	if (!descriptor) {
		text(context,
			 step->sourceId ? "Descriptor unavailable for this captured source ID."
							: "Source not recorded (source ID zero).",
			 interface_theme::kStatusAmber);
		return;
	}
	property(context, 103, "Descriptor name",
			 descriptor->name.empty() ? "Name not registered" : descriptor->name, narrow);
	property(context, 104, "File",
			 descriptor->file.empty() ? "File not registered" : descriptor->file, narrow);
	property(context, 105, "Function",
			 descriptor->function.empty() ? "Function not registered" : descriptor->function,
			 narrow);
	property(context, 106, "Line",
			 descriptor->line ? std::to_string(descriptor->line) : "Not recorded", narrow);
	property(context, 107, "Column",
			 descriptor->column ? std::to_string(descriptor->column) : "Not recorded", narrow);
	copy(context, 108,
		 descriptor->file + ":" + std::to_string(descriptor->line) +
			 (descriptor->column ? ":" + std::to_string(descriptor->column) : ""),
		 "Copy location");
	text(context,
		 "Source editor integration unavailable. Copy location preserves the complete captured "
		 "path.",
		 interface_theme::kTextMuted, 11);
}
void subject(Context& context, const DevErrorOccurrence& occurrence, const DevErrorRecord* step,
			 const DevErrorBreadcrumb* breadcrumb, bool narrow) {
	const auto& error = step ? step->error : occurrence.error;
	property(context, 300, "Subject kind", subject_kind(error.subjectKind()), narrow);
	property(context, 301, "Subject", identity(error.subject), narrow);
	property(context, 302, "Auxiliary", identity(error.auxiliary), narrow);
	property(context, 303, "Native code (decimal / hex)", identity(error.nativeCode), narrow);
	if (breadcrumb) {
		number(context, 304, "Breadcrumb primary value", breadcrumb->primaryValue, narrow);
		number(context, 305, "Breadcrumb secondary value", breadcrumb->secondaryValue, narrow);
		property(context, 306, "Associated occurrence",
				 breadcrumb->occurrence ? identity(breadcrumb->occurrence)
										: "Context breadcrumb · no explicit occurrence association",
				 narrow);
		text(context,
			 "Native category/text, cause and immediate evidence are step-level fields; they are "
			 "unavailable for breadcrumb focus.",
			 interface_theme::kTextSecondary);
		return;
	}
	if (!step) {
		text(context, "Step-level native text, cause and immediate evidence were not captured.",
			 interface_theme::kTextSecondary);
		return;
	}
	number(context, 307, "Native category", step->nativeCategory, narrow);
	const std::string_view native_text{
		step->nativeText.data(), std::min<size_t>(step->nativeTextLength, step->nativeText.size())};
	property(context, 308, "Captured native text",
			 native_text.empty() ? "Not captured" : native_text, narrow);
	if (has_flag(step->flags, DevErrorRecordFlag::NativeTextTruncated))
		text(context, "Native text truncated in this captured record.",
			 interface_theme::kStatusAmber, 11);
	property(context, 309, "Cause occurrence ID",
			 step->cause ? identity(step->cause) : "No explicit cause recorded", narrow);
	property(context, 310, "Focused record flags", flags_text(step->flags), narrow);
	property(context, 311, "Step kind", step_kind(step->kind), narrow);
	subtitle(context, 312, "Immediate evidence · Key / Index / Value");
	evidence(context, 320,
			 std::span{step->evidence.values}.first(
				 std::min<size_t>(step->evidence.count, step->evidence.values.size())),
			 step->evidence.truncated, narrow);
}
void snapshots(Context& context, const DevErrorOccurrence& occurrence, bool narrow) {
	if (occurrence.snapshots.empty()) {
		text(context,
			 "No captured snapshots retained. Opening this section never requests current "
			 "application state.",
			 interface_theme::kTextSecondary);
		return;
	}
	for (size_t snapshot_index = 0; snapshot_index < occurrence.snapshots.size();
		 ++snapshot_index) {
		const auto& snapshot = occurrence.snapshots[snapshot_index];
		const uint64_t key = 1000 + snapshot_index * 40;
		auto& entries = context.state().snapshots;
		auto entry = std::ranges::find_if(entries, [&](const auto& candidate) {
			return candidate.identity == snapshot.source &&
				   candidate.revision == snapshot.capturedRevision;
		});
		if (entry == entries.end()) {
			entries.emplace_back(DevDiagnosticsInspector::Disclosure{
				snapshot.source, snapshot.capturedRevision, false});
			entry = std::prev(entries.end());
		}
		const auto& state = context.params.interfaceState->diagnostics;
		if (state.focus_kind == 3 && state.focused_advice &&
			state.focused_advice->primary == snapshot.source &&
			state.focused_advice->secondary == snapshot.capturedRevision)
			entry->expanded = true;
		disclosure(context, key,
				   "Source " + std::to_string(snapshot.source) + " · revision " +
					   std::to_string(snapshot.capturedRevision) + " · " +
					   snapshot_status(snapshot.outcome),
				   entry->expanded, false);
		if (!entry->expanded)
			continue;
		number(context, key + 1, "Occurrence ID", snapshot.occurrence, narrow);
		property(context, key + 2, "Snapshot source ID", identity(snapshot.source), narrow);
		number(context, key + 3, "Expected revision", snapshot.expectedRevision, narrow);
		number(context, key + 4, "Captured revision", snapshot.capturedRevision, narrow);
		property(context, key + 5, "Capture outcome", snapshot_status(snapshot.outcome), narrow,
				 snapshot.outcome == DevErrorSnapshotOutcome::Available
					 ? interface_theme::kTextCanvas
					 : interface_theme::kStatusAmber);
		if (snapshot.outcome == DevErrorSnapshotOutcome::StaleRevision)
			text(context, "Historical state: captured and expected revisions differ.",
				 interface_theme::kStatusAmber);
		evidence(context, key + 10,
				 std::span{snapshot.values}.first(
					 std::min<size_t>(snapshot.valueCount, snapshot.values.size())),
				 snapshot.outcome == DevErrorSnapshotOutcome::Truncated, narrow);
	}
}
void stack(Context& context, const DevDiagnosticsState& state, const DevErrorRecord* step,
		   bool narrow) {
	if (!step) {
		text(context,
			 "No focused retained step has a stack identity. Breadcrumbs do not carry stacks.",
			 interface_theme::kTextSecondary);
		return;
	}
	property(context, 400, "Stack ID",
			 step->stackId ? identity(step->stackId) : "Not requested / no stack identity captured",
			 narrow);
	const auto* trace = diagnostics_stack(state, step->stackId);
	if (!trace) {
		const auto status =
			has_flag(step->flags, DevErrorRecordFlag::StackStorageLost)	  ? "Storage lost"
			: has_flag(step->flags, DevErrorRecordFlag::StackUnavailable) ? "Unavailable"
			: has_flag(step->flags, DevErrorRecordFlag::StackTruncated)
				? "Truncated; retained stack payload unavailable"
			: step->stackId ? "Storage lost / stack not retained"
							: "Not requested";
		property(context, 401, "Capture status", status, narrow, interface_theme::kStatusAmber);
		text(context, "Function/file/line symbolization unavailable; no resolver is registered.",
			 interface_theme::kTextMuted, 11);
		return;
	}
	property(context, 402, "Capture status", stack_status(trace->status), narrow);
	property(context, 403, "Stack hash", identity(trace->hash), narrow);
	property(context, 404, "Module identity", identity(trace->moduleIdentity), narrow);
	property(context, 405, "Build identity", identity(trace->buildIdentity), narrow);
	number(context, 406, "Frame count", trace->frameCount, narrow);
	text(context, "Raw instruction addresses · symbolization unavailable",
		 interface_theme::kTextSecondary, 11);
	std::string raw;
	for (size_t frame_index = 0;
		 frame_index < std::min<size_t>(trace->frameCount, trace->frames.size()); ++frame_index) {
		const auto address = hex(trace->frames[frame_index]);
		raw += "#" + std::to_string(frame_index) + " " + address + "\n";
		property(context, 420 + frame_index, "Frame #" + std::to_string(frame_index), address,
				 narrow);
	}
	if (!raw.empty())
		copy(context, 470, std::move(raw), "Copy stack");
}
void other_evidence(Context& context, const DevErrorOccurrence& occurrence,
					const DevErrorRecord* focused, bool narrow) {
	bool found = false;
	for (const auto& step : occurrence.steps) {
		if (&step == focused)
			continue;
		found = true;
		const uint64_t key = 10000 + step.sequence * 16;
		auto& entries = context.state().steps;
		auto entry = std::ranges::find(entries, step.sequence,
									   &DevDiagnosticsInspector::Disclosure::identity);
		if (entry == entries.end()) {
			entries.emplace_back(DevDiagnosticsInspector::Disclosure{step.sequence, 0, false});
			entry = std::prev(entries.end());
		}
		disclosure(context, key,
				   "Step #" + std::to_string(step.sequence) + " · " + step_kind(step.kind),
				   entry->expanded, false);
		if (!entry->expanded)
			continue;
		evidence(context, key + 1,
				 std::span{step.evidence.values}.first(
					 std::min<size_t>(step.evidence.count, step.evidence.values.size())),
				 step.evidence.truncated, narrow);
	}
	if (!found)
		text(context, "No other retained step evidence.", interface_theme::kTextSecondary);
}
void integrity(Context& context, const DevDiagnosticsState& state,
			   const DevErrorOccurrence& occurrence, const DevErrorRecord* step, bool narrow) {
	property(context, 500, "Occurrence flags", flags_text(occurrence.flags), narrow);
	property(context, 501, "Focused record flags", step ? flags_text(step->flags) : "No step focus",
			 narrow);
	number(context, 502, "Retained steps", occurrence.steps.size(), narrow);
	number(context, 503, "Retained breadcrumbs", occurrence.breadcrumbs.size(), narrow);
	number(context, 504, "Retained snapshots", occurrence.snapshots.size(), narrow);
	number(context, 505, "Timing capture generation", occurrence.timing.capture_generation, narrow);
	number(context, 506, "Timing report revision", occurrence.timing.reportRevision, narrow);
	number(context, 507, "Memory history generation", occurrence.memory.history_generation, narrow);
	property(context, 508, "Timing search",
			 std::string(occurrence.timing.sealed ? "Sealed" : "Open") +
				 (occurrence.timing.incomplete ? " · Incomplete" : " · No incompleteness reported"),
			 narrow);
	property(context, 509, "Memory search",
			 std::string(occurrence.memory.sealed ? "Sealed" : "Open") +
				 (occurrence.memory.incomplete ? " · Incomplete" : " · No incompleteness reported"),
			 narrow);
	if (step) {
		property(context, 510, "Source descriptor",
				 diagnostics_source(state, step->sourceId) ? "Available"
				 : step->sourceId						   ? "Descriptor unavailable"
														   : "Source ID not recorded",
				 narrow);
		property(context, 511, "Immediate evidence",
				 step->evidence.truncated ? "Truncated" : "No truncation reported", narrow);
		const auto* trace = diagnostics_stack(state, step->stackId);
		property(context, 515, "Focused stack outcome",
				 trace ? stack_status(trace->status)
				 : has_flag(step->flags, DevErrorRecordFlag::StackStorageLost) ? "Storage lost"
				 : has_flag(step->flags, DevErrorRecordFlag::StackUnavailable) ? "Unavailable"
				 : step->stackId ? "Stack payload no longer retained"
								 : "Not requested",
				 narrow);
	}
	const auto missing_sources = std::ranges::count_if(occurrence.steps, [&](const auto& record) {
		return !diagnostics_source(state, record.sourceId);
	});
	number(context, 516, "Steps without source descriptors", static_cast<uint64_t>(missing_sources),
		   narrow);
	for (size_t snapshot_index = 0; snapshot_index < occurrence.snapshots.size();
		 ++snapshot_index) {
		const auto& snapshot = occurrence.snapshots[snapshot_index];
		property(context, 60000 + snapshot_index,
				 "Snapshot source " + std::to_string(snapshot.source),
				 snapshot_status(snapshot.outcome), narrow);
	}
	const auto* capture = diagnostics_capture(state, occurrence.captureId);
	property(context, 512, "Triggered capture ID",
			 occurrence.captureId ? identity(occurrence.captureId) : "No triggered capture",
			 narrow);
	if (capture) {
		property(context, 513, "Timing capture loss",
				 capture->timingHistoryEvicted ? "History evicted"
				 : capture->timingTruncated	   ? "Window truncated"
											   : "No loss reported",
				 narrow);
		property(context, 514, "Memory capture loss",
				 capture->memoryHistoryEvicted ? "History evicted"
				 : capture->memoryTruncated	   ? "Window truncated"
											   : "No loss reported",
				 narrow);
	} else if (occurrence.captureId)
		text(context, "Triggered capture payload no longer retained.",
			 interface_theme::kStatusAmber);
	text(context,
		 "Sealed means the search window closed. Collection closed does not mean the underlying "
		 "defect is fixed.",
		 interface_theme::kTextMuted, 11);
}
void reporting_status(Context& context, const DevErrorReportingStatus& status, bool narrow) {
	text(context,
		 "Reporting-wide counters; these are not losses attributed to the selected occurrence.",
		 interface_theme::kTextSecondary, 11);
	uint64_t key = 600;
	const auto scalar = [&](std::string_view name, uint64_t value) {
		number(context, key++, name, value, narrow);
	};
	scalar("Consumed records", status.consumedRecords);
	scalar("Consumed breadcrumbs", status.consumedBreadcrumbs);
	scalar("Consumed snapshots", status.consumedSnapshots);
	scalar("Retained occurrences", status.retainedOccurrences);
	scalar("Pending occurrences", status.pendingOccurrences);
	scalar("Retained bytes", status.retainedBytes);
	scalar("Evicted occurrences", status.evictedOccurrences);
	scalar("Evicted captures", status.evictedCaptures);
	scalar("Consume failures", status.consumeFailures);
	scalar("Last consumed tick", status.lastConsumedAppTick);
	scalar("Truncated steps", status.truncatedSteps);
	scalar("Truncated breadcrumb ranges", status.truncatedBreadcrumbRanges);
	const auto& quality = status.quality;
	scalar("Recorded events", quality.recordedEvents);
	scalar("Recorded breadcrumbs", quality.recordedBreadcrumbs);
	scalar("Suppressed events", quality.suppressedEvents);
	scalar("Dropped events", quality.droppedEvents);
	scalar("Dropped breadcrumbs", quality.droppedBreadcrumbs);
	scalar("Overwritten breadcrumbs", quality.overwrittenBreadcrumbs);
	scalar("Dropped descriptors", quality.droppedDescriptors);
	scalar("Recursive events", quality.recursiveEvents);
	scalar("Native text truncations", quality.nativeTextTruncations);
	scalar("Unavailable stacks", quality.unavailableStacks);
	scalar("Truncated stacks", quality.truncatedStacks);
	scalar("Lost stacks", quality.lostStacks);
	scalar("Stale snapshots", quality.staleSnapshots);
	scalar("Unavailable snapshots", quality.unavailableSnapshots);
	scalar("Truncated snapshots", quality.truncatedSnapshots);
	scalar("Lost snapshots", quality.lostSnapshots);
	scalar("Lost fatal capsules", quality.lostFatalCapsules);
	subtitle(context, 650, "Diagnostics overhead (cumulative)");
	scalar("Producer calls", quality.overhead.producerCalls);
	scalar("Producer time (ns)", quality.overhead.producerTimeNs);
	scalar("Producer recorded bytes", quality.overhead.recordedBytes);
	scalar("Source registration time (ns)", quality.overhead.sourceRegistrationTimeNs);
	scalar("Stack capture time (ns)", quality.overhead.stackCaptureTimeNs);
	scalar("Fatal capture time (ns)", quality.overhead.fatalCaptureTimeNs);
	scalar("Reporting consume calls", status.overhead.consumeCalls);
	scalar("Reporting consume time (ns)", status.overhead.consumeTimeNs);
	scalar("Reporting drain time (ns)", status.overhead.drainTimeNs);
	scalar("Reporting correlation time (ns)", status.overhead.correlationTimeNs);
	scalar("Reporting advice time (ns)", status.overhead.adviceTimeNs);
}
void fatal(Context& context, const DevDiagnosticsState& state, bool narrow) {
	if (!state.owned.fatal_capsule) {
		text(context,
			 "No fatal capsule retained. Termination can prevent this running UI from displaying a "
			 "capsule.",
			 interface_theme::kTextSecondary);
		return;
	}
	const auto& capsule = *state.owned.fatal_capsule;
	number(context, 700, "Layout version", capsule.layoutVersion, narrow);
	number(context, 701, "Byte size", capsule.byteSize, narrow);
	property(context, 702, "Production inspection",
			 vocabulary(capsule.productionInspection,
						std::array<std::string_view, 4>{"None", "TerminalOnly", "ImmutableSnapshot",
														"ReadOnlyInterface"}),
			 narrow);
	property(context, 703, "Available input bits",
			 hex(static_cast<uint32_t>(capsule.availableInputs)), narrow);
	property(context, 704, "Capture level",
			 vocabulary(capsule.captureLevel,
						std::array<std::string_view, 5>{"Disabled", "Summary", "Causal",
														"StackAndState", "Deep"}),
			 narrow);
	property(context, 705, "Occurrence ID", identity(capsule.occurrence), narrow);
	property(context, 706, "Source ID", identity(capsule.sourceId), narrow);
	property(context, 707, "Capsule flags", flags_text(capsule.flags), narrow);
	property(context, 708, "Error",
			 std::string(capsule.error.descriptor().name) + " · " +
				 identity(static_cast<uint16_t>(capsule.error.code)),
			 narrow);
	property(context, 709, "Resolution", diagnostics_resolution_name(capsule.resolution), narrow);
	property(context, 710, "Native text",
			 capsule.nativeTextLength
				 ? std::string_view(
					   capsule.nativeText.data(),
					   std::min<size_t>(capsule.nativeTextLength, capsule.nativeText.size()))
				 : "Not captured",
			 narrow);
	if (has_flag(capsule.flags, DevErrorRecordFlag::NativeTextTruncated))
		text(context, "Capsule native text truncated.", interface_theme::kStatusAmber);
	subtitle(context, 711, "Fatal captured context");
	context_properties(context, capsule.context, capsule.timestampNs, std::nullopt,
					   capsule.threadTrack, narrow, 720);
	subtitle(context, 735, "Safe-point summary");
	const auto& safe = capsule.safePoint;
	number(context, 736, "Safe-point revision", safe.revision, narrow);
	number(context, 737, "Safe-point tick", safe.appTick, narrow);
	number(context, 738, "Timing mutation sequence", safe.timingMutationSequence, narrow);
	number(context, 739, "Timing dropped records", safe.timingDroppedRecords, narrow);
	number(context, 740, "Memory generation", safe.memoryGeneration, narrow);
	number(context, 741, "Memory mutation sequence", safe.memoryMutationSequence, narrow);
	number(context, 742, "Memory dropped operations", safe.memoryDroppedOperations, narrow);
	number(context, 743, "Error recorded events", safe.errorRecordedEvents, narrow);
	number(context, 744, "Error dropped events", safe.errorDroppedEvents, narrow);
	subtitle(context, 745, "Capsule immediate evidence");
	evidence(context, 750,
			 std::span{capsule.evidence.values}.first(
				 std::min<size_t>(capsule.evidence.count, capsule.evidence.values.size())),
			 capsule.evidence.truncated, narrow);
	subtitle(context, 760, "Capsule breadcrumbs (separate bounded evidence)");
	for (size_t breadcrumb_index = 0;
		 breadcrumb_index < std::min<size_t>(capsule.breadcrumbCount, capsule.breadcrumbs.size());
		 ++breadcrumb_index) {
		const auto& breadcrumb = capsule.breadcrumbs[breadcrumb_index];
		property(context, 770 + breadcrumb_index,
				 "Breadcrumb #" + std::to_string(breadcrumb.sequence),
				 "descriptor=" + identity(breadcrumb.descriptorId) +
					 "; ns=" + std::to_string(breadcrumb.timestampNs) +
					 "; occurrence=" + std::to_string(breadcrumb.occurrence) +
					 "; thread=" + std::to_string(breadcrumb.threadTrack) +
					 "; primary=" + std::to_string(breadcrumb.primaryValue) +
					 "; secondary=" + std::to_string(breadcrumb.secondaryValue),
				 narrow);
	}
	if (!capsule.breadcrumbCount)
		text(context, "No emergency breadcrumbs captured.", interface_theme::kTextSecondary);
	subtitle(context, 780, "Emergency stack (24-frame capacity)");
	property(context, 781, "Stack capture status", stack_status(capsule.stackStatus), narrow);
	property(context, 782, "Module identity", identity(capsule.moduleIdentity), narrow);
	property(context, 783, "Build identity", identity(capsule.buildIdentity), narrow);
	for (size_t frame_index = 0;
		 frame_index < std::min<size_t>(capsule.stackFrameCount, capsule.stackFrames.size());
		 ++frame_index)
		property(context, 790 + frame_index, "Emergency frame #" + std::to_string(frame_index),
				 hex(capsule.stackFrames[frame_index]), narrow);
	text(context,
		 "This is separately bounded in-process evidence. Durable postmortem persistence/import "
		 "and stack symbolization are unavailable.",
		 interface_theme::kTextMuted, 11);
}
[[nodiscard]] std::string details_text(const DevDiagnosticsState& state,
									   const DevErrorOccurrence& occurrence) {
	std::ostringstream output;
	const auto descriptor = occurrence.error.descriptor();
	output << "Occurrence " << occurrence.id << '\n'
		   << descriptor.name << " (" << identity(static_cast<uint16_t>(occurrence.error.code))
		   << ")\n"
		   << "Site: " << occurrence.error.siteDescriptor().name << " ("
		   << identity(static_cast<uint16_t>(occurrence.error.site)) << ")\n"
		   << "Domain: " << diagnostics_domain_name(descriptor.domain)
		   << "\nCategory: " << category(descriptor.category)
		   << "\nBoundary: " << boundary(descriptor.boundary)
		   << "\nImpact: " << diagnostics_impact_name(descriptor.impact)
		   << "\nResolvability: " << resolvability(descriptor.resolvability)
		   << "\nPolicy: " << policy(descriptor.resolutionPolicy)
		   << "\nDefault resolution: " << diagnostics_resolution_name(descriptor.defaultResolution)
		   << "\nRecorded resolution: " << diagnostics_resolution_name(occurrence.resolution)
		   << "\nProduction event kind: " << production_kind(occurrence.productionKind)
		   << "\nFirst app tick: " << occurrence.firstAppTick
		   << "\nLast app tick: " << occurrence.lastAppTick
		   << "\nFirst timestamp ns: " << occurrence.firstTimestampNs
		   << "\nLast timestamp ns: " << occurrence.lastTimestampNs
		   << "\nSubject: " << identity(occurrence.error.subject)
		   << "\nAuxiliary: " << identity(occurrence.error.auxiliary)
		   << "\nNative code: " << identity(occurrence.error.nativeCode)
		   << "\nCapture ID: " << occurrence.captureId
		   << "\nTiming capture generation: " << occurrence.timing.capture_generation
		   << "\nTiming report revision: " << occurrence.timing.reportRevision
		   << "\nMemory history generation: " << occurrence.memory.history_generation
		   << "\nCollection: " << diagnostics_collection_name(occurrence.state)
		   << "\nFlags: " << flags_text(occurrence.flags) << '\n';
	for (const auto& step : occurrence.steps) {
		output << "\nStep #" << step.sequence << " " << step_kind(step.kind)
			   << "\nTimestamp ns: " << step.timestampNs << "\nApp tick: " << step.context.appTick
			   << "\nWindow: " << step.context.frame.window
			   << "\nFrame: " << step.context.frame.frameNumber << "\nPhase: " << step.context.phase
			   << "\nSubmission: " << step.context.submissionSerial
			   << "\nPrimary entity: " << step.context.primaryEntity
			   << "\nSecondary entity: " << step.context.secondaryEntity
			   << "\nTiming track: " << step.context.timingTrack
			   << "\nThread track: " << step.threadTrack
			   << "\nSource ID: " << identity(step.sourceId)
			   << "\nSubject: " << identity(step.error.subject)
			   << "\nAuxiliary: " << identity(step.error.auxiliary)
			   << "\nNative code: " << identity(step.error.nativeCode)
			   << "\nNative category: " << step.nativeCategory << "\nCause: " << step.cause
			   << "\nFlags: " << flags_text(step.flags) << "\nNative text: "
			   << std::string_view(step.nativeText.data(),
								   std::min<size_t>(step.nativeTextLength, step.nativeText.size()))
			   << '\n';
		if (const auto* captured = diagnostics_source(state, step.sourceId))
			output << captured->name << '\n'
				   << captured->file << ':' << captured->line << ':' << captured->column << '\n'
				   << captured->function << '\n';
		for (size_t value_index = 0;
			 value_index < std::min<size_t>(step.evidence.count, step.evidence.values.size());
			 ++value_index) {
			const auto& value = step.evidence.values[value_index];
			output << evidence_key(value.key) << " / " << value.index << " / "
				   << identity(value.value) << '\n';
		}
		if (const auto* trace = diagnostics_stack(state, step.stackId)) {
			output << "Stack " << trace->id << " " << stack_status(trace->status)
				   << " hash=" << trace->hash << " module=" << trace->moduleIdentity
				   << " build=" << trace->buildIdentity << '\n';
			for (size_t frame_index = 0;
				 frame_index < std::min<size_t>(trace->frameCount, trace->frames.size());
				 ++frame_index)
				output << '#' << frame_index << ' ' << hex(trace->frames[frame_index]) << '\n';
		}
	}
	for (const auto& breadcrumb : occurrence.breadcrumbs) {
		output << "\nBreadcrumb #" << breadcrumb.sequence << " "
			   << diagnostics_breadcrumb_name(state, breadcrumb.descriptorId)
			   << "\nDescriptor ID: " << identity(breadcrumb.descriptorId)
			   << "\nTimestamp ns: " << breadcrumb.timestampNs
			   << "\nApp tick: " << breadcrumb.context.appTick
			   << "\nWindow: " << breadcrumb.context.frame.window
			   << "\nFrame: " << breadcrumb.context.frame.frameNumber
			   << "\nSubmission: " << breadcrumb.context.submissionSerial
			   << "\nPrimary entity: " << breadcrumb.context.primaryEntity
			   << "\nSecondary entity: " << breadcrumb.context.secondaryEntity
			   << "\nPhase: " << breadcrumb.context.phase
			   << "\nTiming track: " << breadcrumb.context.timingTrack
			   << "\nThread track: " << breadcrumb.threadTrack
			   << "\nPrimary value: " << breadcrumb.primaryValue
			   << "\nSecondary value: " << breadcrumb.secondaryValue
			   << "\nAssociated occurrence: " << breadcrumb.occurrence << '\n';
	}
	for (const auto& snapshot : occurrence.snapshots) {
		output << "\nSnapshot source " << snapshot.source
			   << " expected=" << snapshot.expectedRevision
			   << " captured=" << snapshot.capturedRevision << " "
			   << snapshot_status(snapshot.outcome) << '\n';
		for (size_t value_index = 0;
			 value_index < std::min<size_t>(snapshot.valueCount, snapshot.values.size());
			 ++value_index) {
			const auto& value = snapshot.values[value_index];
			output << evidence_key(value.key) << " / " << value.index << " / "
				   << identity(value.value) << '\n';
		}
	}
	return output.str();
}
} // namespace

void DevDiagnosticsCopy::onPressed(InteractionContext& context) {
	FSEL::detail::selectable_surface::onPressed(context, context.params.app != nullptr);
}
void DevDiagnosticsCopy::runLogic(InteractionContext& context) {
	FSEL::detail::selectable_surface::runLogic(context, context.params.app != nullptr);
	const auto& current = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	if (context.params.app &&
		context.uiManager.shortcuts().focusedElement().id == context.clayID().id &&
		!context.uiManager.inputFields().hasPrimaryFieldFocus() &&
		((current.keyDown[GLFW_KEY_ENTER] && !previous.keyDown[GLFW_KEY_ENTER]) ||
		 (current.keyDown[GLFW_KEY_SPACE] && !previous.keyDown[GLFW_KEY_SPACE])))
		context.params.app->setClipboardText(context.params.value);
}
void DevDiagnosticsCopy::onHovered(InteractionContext& context) {
	if (context.params.app)
		context.uiManager.requestCursor(CursorType::PointingHand, 4);
}
void DevDiagnosticsCopy::onReleased(InteractionContext& context) {
	if (FSEL::detail::selectable_surface::onReleased(context, context.params.app != nullptr)) {
		context.uiManager.shortcuts().setFocusedElement(context.clayID());
		context.params.app->setClipboardText(context.params.value);
	}
}
void DevDiagnosticsCopy::buildElement(BuildContext& context) {
	const bool hovered =
		context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID());
	auto declaration = row(0);
	declaration.layout.sizing = {
		.width = context.params.width > 0 ? CLAY_SIZING_FIXED(context.params.width)
										  : CLAY_SIZING_FIT(28),
		.height = context.params.compact ? CLAY_SIZING_FIXED(22) : CLAY_SIZING_FIT(24)};
	declaration.layout.padding =
		context.params.compact ? Clay_Padding{3, 3, 3, 3} : Clay_Padding{5, 5, 4, 4};
	declaration.layout.childGap = 4;
	declaration.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
	declaration.cornerRadius = CLAY_CORNER_RADIUS(3);
	if (context.uiManager.shortcuts().focusedElement().id == context.clayID().id)
		declaration.border = {.color = interface_theme::kAccentCurrent, .width = {1, 1, 1, 1, 0}};
	declaration.backgroundColor = hovered				   ? interface_theme::kHoverSurface
								  : context.params.compact ? Clay_Color{}
														   : interface_theme::kDepth3Elevated;
	CLAY(context.clayID(), declaration) {
#if FLOWUI_INCLUDE_ICON_MANAGER
		auto icon = context.params.icon;
		icon.tintEnabled = true;
		Clay_ElementDeclaration image{};
		image.layout.sizing = {.width = CLAY_SIZING_FIXED(14), .height = CLAY_SIZING_FIXED(14)};
		image.backgroundColor = context.params.app ? (hovered ? interface_theme::kAccentSeaGlass
															  : interface_theme::kTextSecondary)
												   : interface_theme::kTextMuted;
		image.image = {.imageData = context.uiManager.imageData(icon)};
		CLAY(context.clayID("icon"), image) {}
#else
		text(context, context.params.compact ? "Copy" : context.params.label,
			 interface_theme::kTextSecondary, 10, 0, false);
#endif
		if (hovered) {
			auto tooltip = column(0);
			tooltip.layout.sizing.width = CLAY_SIZING_FIXED(180);
			tooltip.layout.padding = {8, 8, 6, 6};
			tooltip.backgroundColor = interface_theme::kDepth3Elevated;
			tooltip.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
			tooltip.floating.attachTo = CLAY_ATTACH_TO_PARENT;
			tooltip.floating.attachPoints = {CLAY_ATTACH_POINT_RIGHT_TOP,
											 CLAY_ATTACH_POINT_RIGHT_BOTTOM};
			tooltip.floating.offset.y = 4;
			tooltip.floating.zIndex = 120;
			tooltip.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
			CLAY(context.clayID("tooltip"), tooltip)
			text(context, context.params.label, interface_theme::kTextCanvas, 11);
		}
	}
}
void DevDiagnosticsInspector::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->diagnostics;
	const auto* occurrence = diagnostics_selected_occurrence(state);
	if (context.state().occurrence_id != state.selected_id) {
		context.state().snapshots.clear();
		context.state().steps.clear();
		context.state().occurrence_id = state.selected_id;
	}
	if (occurrence) {
		context.state().snapshots.reserve(context.state().snapshots.size() +
										  occurrence->snapshots.size());
		context.state().steps.reserve(context.state().steps.size() + occurrence->steps.size());
	}
	const auto* step = occurrence ? focused_step(state, *occurrence) : nullptr;
	const auto* breadcrumb = occurrence ? focused_breadcrumb(state, *occurrence) : nullptr;
	const auto previous = Clay_GetElementData(context.clayID());
	const float width =
		previous.found ? previous.boundingBox.width : context.params.interfaceState->inspectorWidth;
	const bool narrow = width < 340;
	auto root = column(0);
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.backgroundColor = interface_theme::kDepth1Panel;
	CLAY(context.clayID(), root) {
		auto title = column(5);
		title.layout.padding = {12, 12, 10, 10};
		title.backgroundColor = interface_theme::kDepth2Ink;
		CLAY(context.clayID("title"), title) {
			text(context, "Occurrence details", interface_theme::kTextCanvas, 14);
			if (occurrence) {
				text(context,
					 "#" + std::to_string(occurrence->id) +
						 (state.follow_latest ? " · Following latest" : " · Pinned"),
					 interface_theme::kTextSecondary);
				if (state.selected_evicted)
					text(context, "No longer retained · preserved investigation",
						 interface_theme::kStatusAmber, 11);
				if (state.frozen)
					text(context, "Frozen at tick " + std::to_string(state.anchor_tick),
						 interface_theme::kAccentSeaGlass, 11);
				if (breadcrumb)
					text(context, "Breadcrumb #" + std::to_string(breadcrumb->sequence),
						 interface_theme::kTextSecondary, 11);
				else if (step)
					text(context,
						 "Step #" + std::to_string(step->sequence) +
							 (state.focus_kind == 1
								  ? " · Selected step"
								  : " · First retained step (context/source focus)"),
						 interface_theme::kTextSecondary, 11);
				else
					text(context, "No retained step for context/source focus",
						 interface_theme::kTextSecondary, 11);
				CLAY(context.clayID("copy-actions"), row()) {
					copy(context, 90000, std::to_string(occurrence->id), "Copy occurrence ID");
					copy(context, 90001, details_text(state, *occurrence), "Copy details");
				}
			} else
				text(context,
					 context.params.app ? "No occurrence selected"
										: "Application reporting unavailable",
					 interface_theme::kTextSecondary);
		}
		auto body = column(0);
		body.layout.sizing.height = CLAY_SIZING_GROW(0);
		body.clip = {.vertical = true};
		const auto scroll = Clay_GetScrollContainerData(context.clayID("scroll-body"));
		if (scroll.found && scroll.scrollPosition) {
			if (state.inspector_scroll_to < state.inspector_expanded.size()) {
				const auto heading = Clay_GetElementData(
					context.clayID(Keyed("section", state.inspector_scroll_to)));
				const auto viewport = Clay_GetElementData(context.clayID("scroll-body"));
				if (heading.found && viewport.found) {
					scroll.scrollPosition->y += viewport.boundingBox.y - heading.boundingBox.y;
					state.inspector_scroll_to = UINT64_MAX;
				}
			}
			body.clip.childOffset = *scroll.scrollPosition;
			state.inspector_scroll = scroll.scrollPosition->y;
		} else
			body.clip.childOffset = {0, state.inspector_scroll};
		CLAY(context.clayID("scroll-body"), body) {
			const std::array<std::string, 10> headings{
				"Error contract",
				"Source location",
				"Execution context",
				"Subject & immediate evidence",
				"Captured state (" + std::to_string(occurrence ? occurrence->snapshots.size() : 0) +
					")",
				"Stack trace",
				"Other step evidence (" +
					std::to_string(occurrence ? occurrence->steps.size() : 0) + ")",
				"Capture integrity",
				"Reporting status · global",
				"Fatal capsule"};
			for (size_t section_index = 0; section_index < headings.size(); ++section_index) {
				CLAY(context.clayID(Keyed("section", section_index)), column(0)) {
					disclosure(context, section_index, headings[section_index],
							   state.inspector_expanded[section_index]);
					if (state.inspector_expanded[section_index]) {
						auto content = column(8);
						content.layout.padding = {12, 12, 10, 12};
						CLAY(context.clayID(Keyed("section-content", section_index)), content) {
							if (section_index == 8)
								reporting_status(context, state.snapshot.status, narrow);
							else if (section_index == 9)
								fatal(context, state, narrow);
							else if (!occurrence)
								text(context,
									 "Select a retained issue and occurrence to inspect captured "
									 "evidence.",
									 interface_theme::kTextSecondary);
							else
								switch (section_index) {
								case 0:
									contract(context, *occurrence, step, narrow);
									break;
								case 1:
									source(context, state, step, breadcrumb, narrow);
									break;
								case 2:
									if (breadcrumb)
										context_properties(
											context, breadcrumb->context, breadcrumb->timestampNs,
											breadcrumb->sequence, breadcrumb->threadTrack, narrow,
											200, "Breadcrumb sequence");
									else if (step)
										context_properties(context, step->context,
														   step->timestampNs, step->sequence,
														   step->threadTrack, narrow);
									else
										text(context,
											 "Execution context not captured in a retained step.",
											 interface_theme::kTextSecondary);
									break;
								case 3:
									subject(context, *occurrence, step, breadcrumb, narrow);
									break;
								case 4:
									snapshots(context, *occurrence, narrow);
									break;
								case 5:
									stack(context, state, step, narrow);
									break;
								case 6:
									other_evidence(context, *occurrence, step, narrow);
									break;
								case 7:
									integrity(context, state, *occurrence, step, narrow);
									break;
								default:
									break;
								}
						}
					}
				}
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements

#endif
