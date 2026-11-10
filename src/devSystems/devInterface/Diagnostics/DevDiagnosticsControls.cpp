#include "devSystems/devInterface/Diagnostics/DevDiagnosticsControls.hpp"

#if FLOW_UI_DEV_MODE

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>

#include "FSEL/Button.hpp"
#include "FSEL/NumberInput.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsModel.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {
using Context = DevDiagnosticsControls::BuildContext;
using Icon = diagnostics_icons::Icon;
constexpr auto queue_command =
	UiAction("flowui.diagnostics.controls.command",
			 [](DevDiagnosticsState& state, DevDiagnosticsCommand command, uint32_t index) {
				 state.command_index = index;
				 state.pending = command;
			 });
constexpr auto change_capture =
	UiAction("flowui.diagnostics.controls.capture", [](DevDiagnosticsState& state) {
		state.pending = DevDiagnosticsCommand::ApplyCaptureLevel;
	});
constexpr auto toggle_trigger =
	UiAction("flowui.diagnostics.controls.trigger",
			 [](DevDiagnosticsState& state, DevDiagnosticsControlsState& controls) {
				 state.show_trigger = !state.show_trigger;
				 state.show_export = false;
				 if (!state.show_trigger)
					 return;
				 if (const auto* selected = diagnostics_selected_occurrence(state)) {
					 state.trigger.code = selected->error.code;
					 state.trigger.site = selected->error.site;
					 state.trigger.nativeCode = selected->error.nativeCode;
				 }
				 controls.trigger_code = static_cast<uint64_t>(state.trigger.code);
				 controls.trigger_site = static_cast<uint64_t>(state.trigger.site);
			 });
constexpr auto arm_trigger =
	UiAction("flowui.diagnostics.controls.arm",
			 [](DevDiagnosticsState& state, DevDiagnosticsControlsState& controls) {
				 state.trigger.code = static_cast<ErrorCode>(controls.trigger_code);
				 state.trigger.site = static_cast<ErrorSite>(controls.trigger_site);
				 state.pending = DevDiagnosticsCommand::ArmTrigger;
			 });
constexpr auto toggle_native =
	UiAction("flowui.diagnostics.controls.native", [](bool& value) { value = !value; });
constexpr auto toggle_export =
	UiAction("flowui.diagnostics.controls.export", [](DevDiagnosticsState& state) {
		state.show_export = !state.show_export;
		state.show_trigger = false;
	});
constexpr auto close_popovers =
	UiAction("flowui.diagnostics.controls.close",
			 [](DevDiagnosticsState& state, DevDiagnosticsControlsState& controls) {
				 state.show_export = false;
				 state.show_trigger = false;
				 controls.compact_open = false;
			 });
constexpr auto toggle_compact =
	UiAction("flowui.diagnostics.controls.menu",
			 [](DevDiagnosticsState& state, DevDiagnosticsControlsState& controls) {
				 controls.compact_open = !controls.compact_open;
				 state.show_export = false;
				 state.show_trigger = false;
			 });

[[nodiscard]] Clay_ElementDeclaration column(uint16_t gap = 6) noexcept {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
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
void text(Context& context, std::string_view value,
		  Clay_Color color = interface_theme::kTextSecondary, uint16_t size = 11) {
	Clay_TextElementConfig config{};
	config.fontSize = size;
	config.textColor = color;
	config.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY_TEXT(context.uiManager.toClayString(value), CLAY_TEXT_CONFIG(config));
}
void help_text(Context& context, uint64_t key, std::string_view label, std::string_view description,
			   Clay_Color color = interface_theme::kTextSecondary, uint16_t size = 11) {
	auto heading = row(0);
	heading.layout.sizing.height = CLAY_SIZING_FIT(0);
	heading.layout.sizing.width = CLAY_SIZING_FIT(0);
	CLAY(context.clayID(Keyed("help", key)), heading) {
		text(context, label, color, size);
		diagnostics_ui::icon(context, Keyed("help-icon", key), Icon::Info,
							 interface_theme::kTextMuted, 12);
		if (context.uiManager.getPreviousFramesInteraction().isHovered(
				context.clayID(Keyed("help", key))))
			diagnostics_ui::pointer_tooltip(context, Keyed("help-tooltip", key), description);
	}
}
[[nodiscard]] FSEL::ButtonParameters button_style(std::string_view label, bool enabled = true) {
	FSEL::ButtonParameters parameters{};
	parameters.text = label;
	parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
	parameters.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(28)};
	parameters.padding = Clay_Padding{8, 8, 5, 5};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.labelFontSize = 11;
	parameters.labelWrapMode = CLAY_TEXT_WRAP_WORDS;
	parameters.labelAlignment = CLAY_TEXT_ALIGN_CENTER;
	parameters.childAlignment =
		Clay_ChildAlignment{.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	parameters.iconSize = 13;
	parameters.contentGap = 5;
	parameters.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	parameters.idleOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.disabledOverrides.labelColor = interface_theme::kTextMuted;
	parameters.enabled = enabled;
	return parameters;
}
void button(Context& context, uint64_t key, std::string_view label, ActionCall action,
			bool enabled = true, Icon icon = Icon::None) {
	auto parameters = button_style(label, enabled && context.params.app != nullptr);
	if (icon == Icon::None) {
		switch (key) {
		case 1:
		case 21:
			icon = Icon::Capture;
			break;
		case 3:
			icon = Icon::Export;
			break;
		case 4:
		case 20:
			icon = Icon::Settings;
			break;
		case 5:
		case 22:
		case 32:
			icon = Icon::Close;
			break;
		default:
			break;
		}
	}
	parameters.icon = context.resources().texture(icon);
	const bool icon_only =
		key == 2 || key == 3 || key == 4 || key == 5 || key == 22 || key == 32 || key >= 1000;
	if (parameters.icon.handle)
		parameters.contentMode =
			icon_only ? FSEL::ButtonContentMode::IconOnly : FSEL::ButtonContentMode::IconThenText;
	parameters.onActivate = action;
	context.uiManager.createElement(FSEL::kButton, Keyed("diagnostics-control", key))
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
	const auto element = ::FlowUi::detail::element_id::resolveLocal(
		context.id, FSEL::Button::definitionId, Keyed("diagnostics-control", key).token);
	if (icon_only && context.uiManager.getPreviousFramesInteraction().isHovered(
						 context.uiManager.toClayEID(element)))
		diagnostics_ui::pointer_tooltip(context, Keyed("control-tooltip", key), label);
}
void command(Context& context, uint64_t key, std::string_view label, DevDiagnosticsCommand action,
			 bool enabled = true, uint32_t index = 0) {
	ActionCall call{};
	if (context.params.app)
		call = ActionCall{context.params.app->actions().uiActions().make(
			queue_command, context.params.interfaceState->diagnostics, action, index)};
	const auto icon =
		action == DevDiagnosticsCommand::Resume ? Icon::Resume
		: action == DevDiagnosticsCommand::Freeze || action == DevDiagnosticsCommand::FreezeSelected
			? Icon::Freeze
		: action == DevDiagnosticsCommand::ExportSelected ||
				action == DevDiagnosticsCommand::ExportRetained
			? Icon::Export
		: action == DevDiagnosticsCommand::MoveTriggerUp   ? Icon::ChevronUp
		: action == DevDiagnosticsCommand::MoveTriggerDown ? Icon::ChevronDown
		: action == DevDiagnosticsCommand::RemoveTrigger   ? Icon::Remove
														   : Icon::None;
	button(context, key, label, call, enabled, icon);
}
void number(Context& context, uint64_t key, std::string_view label, uint32_t& value,
			uint32_t maximum = 1000000) {
	CLAY(context.clayID(Keyed("number", key)), column(3)) {
		help_text(
			context, 10 + key, label,
			key == 3
				? "Compare the captured native error code exactly, as an unsigned decimal value."
				: "Requested application ticks before or after the occurrence. Available timing "
				  "and memory history may clip this window.");
		FSEL::NumberInputParameters<unsigned int> parameters{};
		parameters.value = &value;
		parameters.minimum = 0;
		parameters.maximum = maximum;
		parameters.fontSize = 12;
		parameters.centerText = true;
		parameters.floatingZIndex = 111;
		parameters.sizing =
			Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
		parameters.idleOverrides.backgroundColor = interface_theme::kDepth0Keel;
		parameters.idleOverrides.textColor = interface_theme::kTextCanvas;
		context.uiManager.createElement(FSEL::kNumberInputUInt, Keyed("number-input", key))
			.setParameters(parameters)
			.setDevInternalCapture(true)
			.draw();
	}
}
[[nodiscard]] bool matching_scope_overlaps(const DevErrorTrigger& left,
										   const DevErrorTrigger& right) noexcept {
	return (left.code == ErrorCode::None || right.code == ErrorCode::None ||
			left.code == right.code) &&
		   (left.site == ErrorSite::None || right.site == ErrorSite::None ||
			left.site == right.site) &&
		   (!left.matchNativeCode || !right.matchNativeCode || left.nativeCode == right.nativeCode);
}
[[nodiscard]] bool duplicate(const DevErrorTrigger& left, const DevErrorTrigger& right) noexcept {
	return left.code == right.code && left.site == right.site &&
		   left.matchNativeCode == right.matchNativeCode &&
		   (!left.matchNativeCode || left.nativeCode == right.nativeCode);
}
void rebuild_options(Context& context) {
	auto& controls = context.state();
	const auto& state = context.params.interfaceState->diagnostics;
	std::vector<uint64_t> codes{}, sites{};
	codes.reserve(state.snapshot.rows.size() + 2);
	sites.reserve(state.snapshot.rows.size() + 2);
	codes.emplace_back(0);
	sites.emplace_back(0);
	for (const auto& occurrence : state.snapshot.rows) {
		codes.emplace_back(static_cast<uint64_t>(occurrence.error.code));
		sites.emplace_back(static_cast<uint64_t>(occurrence.error.site));
	}
	codes.emplace_back(controls.trigger_code);
	sites.emplace_back(controls.trigger_site);
	for (const auto& trigger : state.live_reporting_config.triggers) {
		codes.emplace_back(static_cast<uint64_t>(trigger.code));
		sites.emplace_back(static_cast<uint64_t>(trigger.site));
	}
	std::ranges::sort(codes);
	codes.erase(std::unique(codes.begin(), codes.end()), codes.end());
	std::ranges::sort(sites);
	sites.erase(std::unique(sites.begin(), sites.end()), sites.end());
	controls.code_labels.clear();
	controls.site_labels.clear();
	controls.code_options.clear();
	controls.site_options.clear();
	controls.code_labels.reserve(codes.size());
	controls.site_labels.reserve(sites.size());
	controls.code_options.reserve(codes.size());
	controls.site_options.reserve(sites.size());
	for (const auto code : codes) {
		controls.code_labels.emplace_back(
			code ? std::string(describeError(static_cast<ErrorCode>(code)).name) + " (" +
					   std::to_string(code) + ")"
				 : "Any error code (wildcard)");
		controls.code_options.emplace_back(FSEL::ComboBoxOption{code, controls.code_labels.back()});
	}
	for (const auto site : sites) {
		controls.site_labels.emplace_back(
			site ? std::string(describeErrorSite(static_cast<ErrorSite>(site)).name) + " (" +
					   std::to_string(site) + ")"
				 : "Any production site (wildcard)");
		controls.site_options.emplace_back(FSEL::ComboBoxOption{site, controls.site_labels.back()});
	}
}
void trigger_choice(Context& context, uint64_t key, std::string_view label, uint64_t& value,
					std::span<const FSEL::ComboBoxOption> options) {
	CLAY(context.clayID(Keyed("trigger-choice-field", key)), column(3)) {
		help_text(context, 20 + key, label,
				  "Any is a wildcard. Match the error code and production site; source, subject "
				  "and window do not narrow this trigger.");
		FSEL::ComboBoxParameters parameters{};
		parameters.options = options;
		parameters.selectedValue = &value;
		parameters.fontSize = 11;
		parameters.labelAlignment = CLAY_TEXT_ALIGN_CENTER;
		parameters.sizing =
			Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
		parameters.popupWidthPolicy = FSEL::ComboBoxPopupWidthPolicy::ContentAtLeastTrigger;
		context.uiManager.createElement(FSEL::kComboBox, Keyed("trigger-choice", key))
			.setParameters(parameters)
			.setDevInternalCapture(true)
			.draw();
	}
}
void trigger_editor(Context& context) {
	auto& state = context.params.interfaceState->diagnostics;
	auto& controls = context.state();
	const auto& config = state.live_reporting_config;
	rebuild_options(context);
	help_text(context, 1, "Capture trigger",
			  "Arm a bounded timing/memory capture for future admitted occurrences. Arming does "
			  "not raise the error capture level or recover prehistory. Enable timing/memory "
			  "monitoring separately.",
			  interface_theme::kTextCanvas, 13);
	trigger_choice(context, 1, "Error code", controls.trigger_code, controls.code_options);
	trigger_choice(context, 2, "Production site", controls.trigger_site, controls.site_options);
	button(context, 20,
		   state.trigger.matchNativeCode ? "Native code matching: on" : "Native code matching: off",
		   ActionCall{context.params.app->actions().uiActions().make(
			   toggle_native, state.trigger.matchNativeCode)});
	if (state.trigger.matchNativeCode)
		number(context, 3, "Native code (unsigned decimal)", state.trigger.nativeCode, UINT32_MAX);
	CLAY(context.clayID("trigger-windows"), row()) {
		number(context, 4, "Pre-occurrence ticks", state.trigger.preOccurrenceTicks);
		number(context, 5, "Post-occurrence ticks", state.trigger.postOccurrenceTicks);
	}
	help_text(
		context, 2, "First matching rule wins",
		"Up to 64 rules can be armed. Reorder rules below to change precedence. A source-group "
		"selection arms the wider code/site contract, rather than matching a particular source.");
	DevErrorTrigger proposed = state.trigger;
	proposed.code = static_cast<ErrorCode>(controls.trigger_code);
	proposed.site = static_cast<ErrorSite>(controls.trigger_site);
	bool duplicate_found = false;
	size_t overlap_index = config.triggers.size();
	for (size_t trigger_index = 0; trigger_index < config.triggers.size(); ++trigger_index) {
		const auto& armed = config.triggers[trigger_index];
		duplicate_found = duplicate_found || duplicate(armed, proposed);
		if (overlap_index == config.triggers.size() && matching_scope_overlaps(armed, proposed))
			overlap_index = trigger_index;
	}
	if (duplicate_found)
		text(context,
			 "Duplicate rule already armed. Remove or edit that rule before adding another copy.",
			 interface_theme::kStatusAmber);
	else if (overlap_index != config.triggers.size())
		text(context,
			 "Overlaps armed rule #" + std::to_string(overlap_index + 1) +
				 ". That earlier rule wins for matching occurrences.",
			 interface_theme::kStatusAmber);
	button(context, 21, "Arm trigger",
		   ActionCall{context.params.app->actions().uiActions().make(arm_trigger, state, controls)},
		   !duplicate_found && config.triggers.size() < 64);
	if (config.triggers.size() >= 64)
		text(context, "Trigger capacity reached (64 rules).", interface_theme::kStatusAmber);
	help_text(context, 3, "Capture limits",
			  "Effective limits: " + std::to_string(config.retainedCaptureCapacity) +
				  " retained captures · " + std::to_string(config.maximumPinnedTimingTicks) +
				  " timing ticks · " + std::to_string(config.maximumPinnedMemoryEvents) +
				  " memory events",
			  interface_theme::kTextSecondary);
	help_text(context, 4, "Armed rules",
			  "Rules are evaluated in this order. Move an earlier broad rule below a specific rule "
			  "when you want the specific match to win.",
			  interface_theme::kTextCanvas, 12);
	if (config.triggers.empty())
		text(context, "No capture triggers armed.");
	for (size_t trigger_index = 0; trigger_index < config.triggers.size(); ++trigger_index) {
		const auto& armed = config.triggers[trigger_index];
		auto card = column(4);
		card.layout.padding = {8, 8, 6, 6};
		card.backgroundColor = interface_theme::kDepth1Panel;
		CLAY(context.clayID(Keyed("armed-rule", trigger_index)), card) {
			text(context,
				 "#" + std::to_string(trigger_index + 1) + " " +
					 (armed.code == ErrorCode::None ? "Any code"
													: std::string(describeError(armed.code).name)) +
					 " · " +
					 (armed.site == ErrorSite::None
						  ? "Any site"
						  : std::string(describeErrorSite(armed.site).name)),
				 interface_theme::kTextCanvas);
			text(context,
				 std::string(armed.matchNativeCode ? "Native " + std::to_string(armed.nativeCode)
												   : "Any native code") +
					 " · pre " + std::to_string(armed.preOccurrenceTicks) + " / post " +
					 std::to_string(armed.postOccurrenceTicks) + " ticks");
			CLAY(context.clayID(Keyed("armed-actions", trigger_index)), row()) {
				command(context, 1000 + trigger_index * 3, "Move up",
						DevDiagnosticsCommand::MoveTriggerUp, trigger_index > 0, trigger_index);
				command(context, 1001 + trigger_index * 3, "Move down",
						DevDiagnosticsCommand::MoveTriggerDown,
						trigger_index + 1 < config.triggers.size(), trigger_index);
				command(context, 1002 + trigger_index * 3, "Remove",
						DevDiagnosticsCommand::RemoveTrigger, true, trigger_index);
			}
		}
	}
	button(context, 22, "Close",
		   ActionCall{
			   context.params.app->actions().uiActions().make(close_popovers, state, controls)});
}
void export_editor(Context& context) {
	auto& state = context.params.interfaceState->diagnostics;
	text(context, "Export diagnostics", interface_theme::kTextCanvas, 13);
	text(context, state.frozen
					  ? (state.selected_only_freeze		? "Scope: frozen selected investigation"
						 : state.frozen_details_limited ? "Scope: frozen list and selected details"
														: "Scope: frozen retained diagnostics")
					  : "Scope: live retained diagnostics");
	text(context, "Destination base path (writes .json and .md)");
	FSEL::TextInputParameters path{};
	path.value = &state.export_path;
	path.placeholder = "flowui-diagnostics";
	path.maxBytes = 4096;
	path.fontSize = 12;
	path.centerText = true;
	path.floatingZIndex = 111;
	path.sizing = Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(30)};
	path.padding = Clay_Padding{8, 8, 5, 5};
	path.idleOverrides.backgroundColor = interface_theme::kDepth0Keel;
	context.uiManager.createElement(FSEL::kTextInput, "export-path")
		.setParameters(path)
		.setDevInternalCapture(true)
		.draw();
	command(context, 30, "Selected investigation", DevDiagnosticsCommand::ExportSelected,
			state.selected.has_value() && !state.export_path.empty());
	command(context, 31, "Retained diagnostics", DevDiagnosticsCommand::ExportRetained,
			!state.selected_only_freeze && !state.frozen_details_limited &&
				!state.snapshot.rows.empty() && !state.export_path.empty());
	if (state.selected_only_freeze)
		text(context,
			 "This freeze owns only the selected investigation. Resume live to export the retained "
			 "set.",
			 interface_theme::kTextSecondary);
	if (state.frozen_details_limited)
		text(context, "The frozen list holds available selected details. Resume live for a "
					  "complete retained export.");
	if (!state.frozen)
		command(context, 33, "Freeze selected investigation only",
				DevDiagnosticsCommand::FreezeSelected, state.selected.has_value());
	if (!state.selected && state.snapshot.rows.empty())
		text(context, "No retained evidence available for export.",
			 interface_theme::kTextSecondary);
	text(context,
		 "Versioned structured evidence and a readable Markdown report use the same owned "
		 "snapshot. Export cannot recover evicted history or manufacture stack symbols.",
		 interface_theme::kTextMuted);
	if (!state.message.empty())
		text(context, state.message, interface_theme::kTextSecondary);
	button(context, 32, "Close",
		   ActionCall{context.params.app->actions().uiActions().make(close_popovers, state,
																	 context.state())});
}
} // namespace

void DevDiagnosticsControls::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->diagnostics;
	const bool available = context.params.app != nullptr;
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	if (input.keyDown[GLFW_KEY_ESCAPE] && !previous.keyDown[GLFW_KEY_ESCAPE]) {
		state.show_trigger = false;
		state.show_export = false;
		context.state().compact_open = false;
	}
	const auto previous_bounds = Clay_GetElementData(context.clayID());
	const auto viewport = Clay_GetLayoutDimensions();
	const bool compact =
		viewport.width - (previous_bounds.found ? previous_bounds.boundingBox.x : 650) < 490;
	auto root = row(5);
	root.layout.sizing.width = CLAY_SIZING_FIT(0);
	root.layout.sizing.height = CLAY_SIZING_FIXED(28);
	CLAY(context.clayID(), root) {
		static constexpr auto options = [] {
			std::array<FSEL::ComboBoxOption, 5> result{{{0, "Capture: Disabled"},
														{1, "Capture: Summary"},
														{2, "Capture: Causal"},
														{3, "Capture: StackAndState"},
														{4, "Capture: Deep"}}};
			for (auto& option : result)
				option.enabled = option.value <= static_cast<uint64_t>(compiledDevErrorLevel());
			return result;
		}();
		const auto draw_capture = [&] {
			FSEL::ComboBoxParameters capture{};
			capture.options = options;
			capture.selectedValue = &state.capture_level;
			capture.fontSize = 11;
			capture.labelAlignment = CLAY_TEXT_ALIGN_CENTER;
			capture.enabled = available;
			capture.popupWidthPolicy = FSEL::ComboBoxPopupWidthPolicy::ContentAtLeastTrigger;
			capture.sizing =
				Clay_Sizing{.width = CLAY_SIZING_FIXED(163), .height = CLAY_SIZING_FIXED(28)};
			if (available)
				capture.onChanged = ActionCall{
					context.params.app->actions().uiActions().make(change_capture, state)};
			context.uiManager.createElement(FSEL::kComboBox, "capture-level")
				.setParameters(capture)
				.setDevInternalCapture(true)
				.draw();
		};
		if (!compact)
			draw_capture();
		if (available && compact) {
			button(context, 4, "Diagnostics…",
				   ActionCall{context.params.app->actions().uiActions().make(toggle_compact, state,
																			 context.state())});
		} else if (available) {
			button(context, 1, "Capture trigger…",
				   ActionCall{context.params.app->actions().uiActions().make(toggle_trigger, state,
																			 context.state())});
			command(context, 2, state.frozen ? "Resume live" : "Freeze view",
					state.frozen ? DevDiagnosticsCommand::Resume : DevDiagnosticsCommand::Freeze);
			button(context, 3, "Export…",
				   ActionCall{context.params.app->actions().uiActions().make(toggle_export, state)},
				   state.selected.has_value() || !state.snapshot.rows.empty());
		} else
			text(context, "Reporting unavailable");
		if (available &&
			(state.show_trigger || state.show_export || context.state().compact_open)) {
			const auto anchor = Clay_GetElementData(context.clayID());
			const auto dimensions = Clay_GetLayoutDimensions();
			const float available_width = std::max(160.0f, dimensions.width - 24);
			const float panel_width = std::min(380.0f, available_width);
			auto panel = column(8);
			panel.layout.sizing.width = CLAY_SIZING_FIXED(panel_width);
			panel.layout.sizing.height = CLAY_SIZING_FIT(0);
			panel.layout.padding = {12, 12, 12, 12};
			panel.backgroundColor = interface_theme::kDepth3Elevated;
			panel.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
			panel.cornerRadius = CLAY_CORNER_RADIUS(4);
			panel.floating.attachTo = CLAY_ATTACH_TO_PARENT;
			panel.floating.attachPoints = {CLAY_ATTACH_POINT_RIGHT_TOP,
										   CLAY_ATTACH_POINT_RIGHT_BOTTOM};
			panel.floating.offset = {anchor.found ? std::min(0.0f, dimensions.width - 12 -
																	   anchor.boundingBox.x -
																	   anchor.boundingBox.width)
												  : 0,
									 5};
			panel.floating.zIndex = 110;
			CLAY(context.clayID("control-popover"), panel) {
				auto scroll_body = column(8);
				scroll_body.layout.sizing.height = CLAY_SIZING_FIT(
					0, std::max(120.0f, dimensions.height -
											(anchor.found ? anchor.boundingBox.y + 40 : 80) - 12));
				scroll_body.clip = {.vertical = true};
				const auto scroll = Clay_GetScrollContainerData(context.clayID("popover-body"));
				if (scroll.found && scroll.scrollPosition)
					scroll_body.clip.childOffset = *scroll.scrollPosition;
				CLAY(context.clayID("popover-body"), scroll_body) {
					if (state.show_trigger)
						trigger_editor(context);
					else if (state.show_export)
						export_editor(context);
					else {
						text(context, "Diagnostics controls", interface_theme::kTextCanvas, 13);
						draw_capture();
						button(context, 1, "Capture trigger…",
							   ActionCall{context.params.app->actions().uiActions().make(
								   toggle_trigger, state, context.state())});
						command(context, 2, state.frozen ? "Resume live" : "Freeze view",
								state.frozen ? DevDiagnosticsCommand::Resume
											 : DevDiagnosticsCommand::Freeze);
						button(context, 3, "Export…",
							   ActionCall{context.params.app->actions().uiActions().make(
								   toggle_export, state)},
							   state.selected.has_value() || !state.snapshot.rows.empty());
						if (!state.message.empty())
							text(context, state.message);
						button(context, 5, "Close",
							   ActionCall{context.params.app->actions().uiActions().make(
								   close_popovers, state, context.state())});
					}
					constexpr std::array<std::string_view, 5> level_names{
						"Disabled", "Summary", "Causal", "StackAndState", "Deep"};
					const auto effective = static_cast<size_t>(state.live_capture_config.level);
					text(context,
						 "Effective future capture: " + std::string(effective < level_names.size()
																		? level_names[effective]
																		: "Unknown"),
						 interface_theme::kAccentSeaGlass);
					help_text(
						context, 5,
						"Compiled ceiling: " +
							std::string(level_names[static_cast<size_t>(compiledDevErrorLevel())]),
						"Higher choices are unavailable in this build. Capture changes affect "
						"future evidence only. Disabled suppresses future development evidence; "
						"providers and instrumentation determine stack/state availability. "
						"Producer capacities require App recreation/configuration.",
						interface_theme::kTextMuted);
				}
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements

#endif
