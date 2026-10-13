#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchControls.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "managers/UiManager.hpp"
namespace FlowUi::devSystems::interface_elements {
namespace interaction = FSEL::detail::selectable_surface;
namespace {
[[nodiscard]] bool track_can_select(DevMemoryControl::InteractionContext& context) {
	if (!context.params.enabled)
		return false;
	if (!context.params.whole_track)
		return true;
	const auto scope_control = ::FlowUi::detail::element_id::resolveLocal(
		context.id, DevMemoryControl::definitionId,
		Keyed("memory-control",
			  (context.params.identity | (uint64_t{1} << 63)) ^ (uint64_t{9} << 56))
			.token);
	return !context.previousInteraction.isHovered(context.uiManager.toClayEID(scope_control));
}
} // namespace
void DevMemoryControl::onPressed(InteractionContext& context) {
	interaction::onPressed(context, track_can_select(context));
}
void DevMemoryControl::runLogic(InteractionContext& context) {
	interaction::runLogic(context, true);
}
void DevMemoryControl::onReleased(InteractionContext& context) {
	if (interaction::onReleased(context, track_can_select(context)) && context.params.state) {
		if (context.params.action >= 100) {
			context.params.state->analysis.ui_action = context.params.action;
		} else if (context.params.action == 9 && context.params.selection) {
			context.params.selection->selected_node = context.params.identity;
			context.params.state->next_live_ns = 0;
			context.params.state->next_history_ns = 0;
		} else if (context.params.identity) {
			context.params.state->focus = context.params.identity;
			context.params.state->next_history_ns = 0;
		} else
			context.params.state->pending_action = context.params.action;
	}
}
void DevMemoryControl::onHovered(InteractionContext& context) {
	if (context.params.enabled)
		context.uiManager.requestCursor(CursorType::PointingHand, 4);
}
Clay_ElementDeclaration DevMemoryControl::constructElement(BuildContext& context) {
	if (context.params.whole_track) {
		auto surface = context.params.surface;
		if (context.params.state && context.params.state->focus == context.params.identity)
			surface.backgroundColor = interface_theme::kSelectedRow;
		else if (context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID()))
			surface.backgroundColor = interface_theme::kHoverSurface;
		return surface;
	}
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(24)};
	root.layout.padding = {6, 6, 2, 2};
	root.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	root.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
	root.backgroundColor =
		context.params.enabled ? interface_theme::kDepth3Elevated : interface_theme::kDepth2Ink;
	root.cornerRadius = CLAY_CORNER_RADIUS(3);
	if (context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID()))
		root.backgroundColor = interface_theme::kHoverSurface;
	return root;
}
void DevMemoryControl::buildElement(BuildContext& context) {
	const auto root = constructElement(context);
	Clay_TextElementConfig text{};
	text.fontSize = 11;
	text.textColor =
		context.params.enabled ? interface_theme::kTextCanvas : interface_theme::kTextMuted;
	text.wrapMode = CLAY_TEXT_WRAP_WORDS;
	text.textAlignment = CLAY_TEXT_ALIGN_CENTER;
	CLAY(context.clayID(), root) {
		CLAY_TEXT(context.uiManager.toClayString(context.params.label), CLAY_TEXT_CONFIG(text));
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
