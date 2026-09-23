#include "devSystems/devInterface/Permanents/Elements/DevInspectOverlayMenu.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/App.hpp"
#include "FSEL/Button.hpp"
#include "FSEL/Checkbox.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "managers/ActionManager.hpp"
#include "managers/PopupManager.hpp"
#include "managers/UiManager.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace FlowUi::devSystems::interface_elements {
namespace {
using Controller = tooling::DevInspectInteractionController;
using Flags = tooling::DevOverlayModeFlags;
inline constexpr LocalElementName trigger_id{"trigger"};
inline constexpr LocalElementName pick_id{"pick"};
inline constexpr LocalElementName secondary_id{"secondary"};
inline constexpr LocalElementName pick_domain_flow_id{"pick-domain-flow"};
inline constexpr LocalElementName pick_domain_clay_id{"pick-domain-clay"};
inline constexpr LocalElementName pick_domain_row_id{"pick-domain-row"};
inline constexpr LocalElementName clear_id{"clear"};

struct Surface {
	std::string_view label;
	Flags flag;
};
constexpr std::array<Surface, 6> surfaces{{
	{"Box model", Flags::BoxModel},
	{"Distances and rulers", Flags::RulersAndDistance},
	{"Tree hierarchy", Flags::TreeHierarchy},
	{"Typography", Flags::Typography},
	{"Scissor and clip", Flags::ScissorAndClip},
	{"Render run diagnostics", Flags::RenderRunDiagnostics},
}};
constexpr auto toggle_menu = UiAction("flowui.dev_interface.inspect.toggle-overlay-menu",
									  [](DevInspectOverlayMenuState& state) {
										  state.open = !state.open;
										  state.keyboard_focus = false;
									  });
constexpr auto toggle_pick =
	UiAction("flowui.dev_interface.inspect.toggle-pick",
			 [](Controller& controller, DevInspectOverlayMenuState& state) {
				 state.open = false;
				 controller.toggle_primary_pick();
			 });
constexpr auto pick_secondary =
	UiAction("flowui.dev_interface.inspect.pick-secondary",
			 [](Controller& controller, DevInspectOverlayMenuState& state) {
				 state.open = false;
				 controller.begin_secondary_pick();
			 });
constexpr auto clear_targets =
	UiAction("flowui.dev_interface.inspect.clear-targets",
			 [](Controller& controller) { controller.clear_selection(); });
constexpr auto toggle_surface = UiAction("flowui.dev_interface.inspect.toggle-surface",
										 [](Controller& controller, const Surface& surface) {
											 controller.toggle_surface(surface.flag);
										 });
constexpr auto set_pick_domain_flow =
	UiAction("flowui.dev_interface.inspect.set-pick-domain-flow",
			 [](Controller& controller) {
				 controller.set_pick_domain(tooling::DevInspectPickDomain::Flow);
			 });
constexpr auto set_pick_domain_clay =
	UiAction("flowui.dev_interface.inspect.set-pick-domain-clay",
			 [](Controller& controller) {
				 controller.set_pick_domain(tooling::DevInspectPickDomain::Clay);
			 });

[[nodiscard]] FSEL::ButtonParameters button_parameters(std::string_view label, ActionCall action,
													   bool emphasized = false,
													   bool enabled = true) {
	FSEL::ButtonParameters parameters{};
	parameters.text = label;
	parameters.onActivate = action;
	parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
	parameters.enabled = enabled;
	parameters.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(28)};
	parameters.padding = {9, 9, 5, 5};
	parameters.borderWidth = {1, 1, 1, 1, 0};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.labelFontSize = 12;
	parameters.idleOverrides.backgroundColor =
		emphasized ? Flow_Color("#071f2a") : interface_theme::kDepth3Elevated;
	parameters.idleOverrides.borderColor =
		emphasized ? interface_theme::kAccentCurrent : interface_theme::kBorderVisible;
	parameters.idleOverrides.labelColor =
		emphasized ? interface_theme::kTextCanvas : interface_theme::kTextSecondary;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.hoveredOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.pressedOverrides.backgroundColor = interface_theme::kDepth0Keel;
	parameters.disabledOverrides.labelColor = interface_theme::kTextMuted;
	return parameters;
}

void text(DevInspectOverlayMenu::BuildContext& context, std::string_view label,
		  Clay_Color color = interface_theme::kTextSecondary, uint16_t size = 12) {
	Clay_TextElementConfig config{};
	config.fontSize = size;
	config.textColor = color;
	config.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(config));
}

void separator(DevInspectOverlayMenu::BuildContext& context, uint64_t index) {
	Clay_ElementDeclaration line{};
	line.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(1)};
	line.backgroundColor = interface_theme::kBorderPrimary;
	CLAY(context.clayID(Keyed("separator", index)), line);
}

[[nodiscard]] std::string target_label(App& app, tooling::DevInspectTarget target,
									   std::string_view role) {
	std::string label(role);
	label += ": ";
	if (target.kind == tooling::DevInspectTargetKind::Clay) {
		if (app.hasWindow(target.window)) {
			const auto& snapshot = app.ui(target.window).devTreeSnapshot();
			const auto resolved = tooling::resolve_inspect_target(target, snapshot);
			if (resolved.isValid() && resolved.clayNodeIndex < snapshot.clay.nodes.size()) {
				const auto& node = snapshot.clay.nodes[resolved.clayNodeIndex];
				auto name = snapshot.string(node.idString);
				if (name.empty()) {
					char fallback[40]{};
					std::snprintf(fallback, sizeof(fallback), "Clay #%u", node.clayId);
					name = fallback;
				}
				label.append(name);
			}
		}
		char identity[72]{};
		std::snprintf(identity, sizeof(identity), "\nWindow %llu · Clay #%u",
					  static_cast<unsigned long long>(target.window),
					  static_cast<unsigned int>(target.clay_id));
		label += identity;
		return label;
	}
	if (app.hasWindow(target.window)) {
		const auto& snapshot = app.ui(target.window).devTreeSnapshot();
		const auto resolved = tooling::resolve_inspect_target(target, snapshot);
		if (resolved.isValid() && resolved.flowNodeIndex < snapshot.flow.nodes.size()) {
			const auto& node = snapshot.flow.nodes[resolved.flowNodeIndex];
			auto name = snapshot.string(node.debugName);
			if (name.empty())
				name = snapshot.string(node.definitionName);
			label.append(name.empty() ? "Unnamed element" : name);
		}
	}
	char identity[72]{};
	std::snprintf(identity, sizeof(identity), "\nWindow %llu · %llx",
				  static_cast<unsigned long long>(target.window),
				  static_cast<unsigned long long>(target.instance.value));
	label += identity;
	return label;
}
} // namespace

void DevInspectOverlayMenu::buildElement(BuildContext& context) {
	if (!context.params.app)
		return;
	auto& app = *context.params.app;
	auto& controller = app.devTooling().inspect_interaction();
	auto& state = context.state();
	auto& actions = app.actions().uiActions();
	const auto popup_id = context.childID("popup");
	const bool popup_dismissed = context.uiManager.popups().consumeDismissed(popup_id);
	if (popup_dismissed)
		state.open = false;
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	const auto pressed = [&](size_t key) { return input.keyDown[key] && !previous.keyDown[key]; };
	// A local keyboard shortcut also makes the trigger reachable without pointer focus.
	if (input.ctrl && input.shift && pressed(79))
		state.open = !state.open;
	if (state.open) {
		if (pressed(264) || pressed(258)) {
			state.focused_row =
				static_cast<uint8_t>((state.focused_row + (input.shift ? 9 : 1)) % 10);
			state.keyboard_focus = true;
		}
		if (pressed(265)) {
			state.focused_row = static_cast<uint8_t>((state.focused_row + 9) % 10);
			state.keyboard_focus = true;
		}
		if (pressed(32) || pressed(257)) {
			if (state.focused_row == 0 && controller.primary_target()) {
				controller.begin_secondary_pick();
				state.open = false;
			} else if (state.focused_row > 0 && state.focused_row < 7)
				controller.toggle_surface(surfaces[state.focused_row - 1].flag);
			else if (state.focused_row == 7)
				controller.set_pick_domain(tooling::DevInspectPickDomain::Flow);
			else if (state.focused_row == 8)
				controller.set_pick_domain(tooling::DevInspectPickDomain::Clay);
			else if (state.focused_row == 9)
				controller.clear_selection();
		}
	} else if (!popup_dismissed && pressed(256))
		controller.cancel_pick();
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIXED(28)};
	root.layout.childGap = 8;
	CLAY(context.clayID(), root) {
		auto trigger =
			button_parameters("Overlay", ActionCall{actions.make(toggle_menu, state)}, state.open);
		trigger.icon = context.resources().dropdown_icon;
		trigger.contentMode = trigger.icon.handle ? FSEL::ButtonContentMode::TextThenIcon
												  : FSEL::ButtonContentMode::TextOnly;
		trigger.iconSize = 12.0f;
		context.uiManager.createElement(FSEL::kButton, trigger_id).setParameters(trigger).draw();
		const auto pick_label = controller.pick_mode() == tooling::DevInspectPickMode::Secondary
									? "Picking Secondary · Esc"
								: controller.picking() ? "Picking Element · Esc"
													   : "Pick Element";
		context.uiManager.createElement(FSEL::kButton, pick_id)
			.setParameters(button_parameters(
				pick_label, ActionCall{actions.make(toggle_pick, controller, state)},
				controller.picking(), FLOW_UI_DEV_CAPTURE_CLAY))
			.draw();
	}
	// Child buttons execute their actions during draw(). Only decide whether to submit
	// the popup afterward; PopupManager retires records that were not submitted.
	if (!state.open)
		return;
	const float width =
		std::min(320.0f, std::max(160.0f, Clay_GetLayoutDimensions().width - 16.0f));
	const float height = std::min(controller.secondary_target() ? 620.0f : 540.0f,
								  std::max(100.0f, Clay_GetLayoutDimensions().height - 100.0f));
	PopupRequest request{};
	request.anchor = PopupAnchor::element(context.id);
	request.placement.offset = {0, 5};
	request.expectedSize = Clay_Dimensions{width, height};
	request.outsidePress = PopupOutsidePressPolicy::DismissAndConsume;
	const auto frame = context.uiManager.popups().request(popup_id, request).value_or(PopupFrame{});
	if (!frame.visible && !frame.measureOnly)
		return;
	Clay_ElementDeclaration popup{};
	popup.layout.sizing = {.width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height)};
	popup.layout.padding = {12, 12, 12, 12};
	popup.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	popup.backgroundColor = interface_theme::kDepth1Panel;
	popup.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
	popup.cornerRadius = CLAY_CORNER_RADIUS(3);
	popup.floating = frame.floating;
	CLAY(context.uiManager.toClayEID(popup_id), popup) {
		const auto scroll_id = context.clayID("popup-scroll");
		const auto scroll = Clay_GetScrollContainerData(scroll_id);
		Clay_ElementDeclaration viewport{};
		viewport.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		viewport.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		viewport.layout.childGap = 8;
		viewport.clip = {.vertical = true,
						 .childOffset = scroll.found && scroll.scrollPosition
											? *scroll.scrollPosition
											: Clay_Vector2{}};
		CLAY(scroll_id, viewport) {
			text(context, "Overlay", interface_theme::kTextCanvas, 14);
			separator(context, 0);
			if (controller.primary_target())
				text(context, target_label(app, controller.primary_target(), "Primary"));
			if (controller.secondary_target())
				text(context, target_label(app, controller.secondary_target(), "Secondary"));
			if (!controller.primary_target())
				text(context, "No targets selected", interface_theme::kTextMuted);
			context.uiManager.createElement(FSEL::kButton, secondary_id)
				.setParameters(button_parameters(
					"Select Secondary Target",
					ActionCall{actions.make(pick_secondary, controller, state)},
					state.keyboard_focus && state.focused_row == 0,
					FLOW_UI_DEV_CAPTURE_CLAY && bool(controller.primary_target())))
				.draw();
			if (!controller.primary_target())
				text(context, "Pick an element or select a Flow tree row first.",
					 interface_theme::kTextMuted, 11);
#if !FLOW_UI_DEV_CAPTURE_CLAY
			text(context, "Picking requires Clay tree capture.", interface_theme::kStatusAmber);
#endif
			separator(context, 1);
			text(context, "Follow cursor while picking", interface_theme::kTextSecondary, 11);
			for (size_t index = 0; index < surfaces.size(); ++index) {
				const auto& surface = surfaces[index];
				const auto action = ActionCall{actions.make(toggle_surface, controller, surface)};
				const bool checked = tooling::hasFlag(controller.mode_flags(), surface.flag);
				Clay_ElementDeclaration row{};
				row.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
				row.layout.childGap = 8;
				row.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
				CLAY(context.clayID(Keyed("surface-row", index)), row) {
					FSEL::CheckboxParameters checkbox{};
					checkbox.isChecked = checked;
					checkbox.onToggle = action;
					checkbox.size = 16;
					checkbox.checkedIcon = context.resources().checked_icon;
					checkbox.iconSize = 12;
					checkbox.checkedOverrides.idle.backgroundColor =
						interface_theme::kAccentCurrent;
					checkbox.checkedOverrides.idle.borderColor = interface_theme::kAccentCurrent;
					checkbox.uncheckedOverrides.idle.backgroundColor = interface_theme::kDepth0Keel;
					checkbox.uncheckedOverrides.idle.borderColor = interface_theme::kBorderVisible;
					context.uiManager.createElement(FSEL::kCheckbox, Keyed("surface-check", index))
						.setParameters(checkbox)
						.draw();
					auto label =
						button_parameters(surface.label, action,
										  state.keyboard_focus && state.focused_row == index + 1);
					label.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
					label.padding = {4, 4, 3, 3};
					label.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
					label.idleOverrides.backgroundColor = interface_theme::kDepth1Panel;
					label.borderWidth = state.keyboard_focus && state.focused_row == index + 1
											? Clay_BorderWidth{1, 1, 1, 1, 0}
											: Clay_BorderWidth{};
					context.uiManager.createElement(FSEL::kButton, Keyed("surface-label", index))
						.setParameters(label)
						.draw();
				}
			}
			separator(context, 2);
			text(context, "Picker target mode", interface_theme::kTextSecondary, 11);
			Clay_ElementDeclaration domain_row{};
			domain_row.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
			domain_row.layout.childGap = 4;
			domain_row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			CLAY(context.clayID(pick_domain_row_id), domain_row) {
				const bool flow_active =
					controller.pick_domain() == tooling::DevInspectPickDomain::Flow;
				const bool clay_active =
					controller.pick_domain() == tooling::DevInspectPickDomain::Clay;

				auto flow_btn = button_parameters(
					"Flow Element", ActionCall{actions.make(set_pick_domain_flow, controller)},
					flow_active, true);
				flow_btn.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
				if (state.keyboard_focus && state.focused_row == 7) {
					flow_btn.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
				}

				auto clay_btn = button_parameters(
					"Clay Element", ActionCall{actions.make(set_pick_domain_clay, controller)},
					clay_active, true);
				clay_btn.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
				if (state.keyboard_focus && state.focused_row == 8) {
					clay_btn.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
				}

				context.uiManager.createElement(FSEL::kButton, pick_domain_flow_id)
					.setParameters(flow_btn)
					.draw();
				context.uiManager.createElement(FSEL::kButton, pick_domain_clay_id)
					.setParameters(clay_btn)
					.draw();
			}
			separator(context, 3);
			context.uiManager.createElement(FSEL::kButton, clear_id)
				.setParameters(button_parameters(
					"Clear Selection", ActionCall{actions.make(clear_targets, controller)},
					state.keyboard_focus && state.focused_row == 9))
				.draw();
		}
	}
}

DevInspectOverlayMenuResources::DevInspectOverlayMenuResources([[maybe_unused]] App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager& icons = app.icons();
	interface_icons::registerDevInterfaceIcons(icons);
	if (icons.contains(interface_icons::kTreeExpandedKey)) {
		dropdown_icon = icons.textureRef(interface_icons::kTreeExpandedKey);
	}
	constexpr std::string_view checked_key = "flowui/dev-interface/overlay/checked";
	if (!icons.contains(checked_key)) {
		const auto registered = icons.registerSvg(
			checked_key,
			R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m3 8 3 3 7-7" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg");
		if (!registered)
			return;
	}
	checked_icon = icons.textureRef(checked_key);
#endif
}

} // namespace FlowUi::devSystems::interface_elements
#endif
