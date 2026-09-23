#include "devSystems/devInterface/Performance/Selector/DevPerformanceSelectorElements.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace interaction = FSEL::detail::selectable_surface;

DevPerformanceRowResources::DevPerformanceRowResources([[maybe_unused]] App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	auto& icons = app.icons();
	interface_icons::registerDevInterfaceIcons(icons);
	expanded_icon = icons.textureRef(interface_icons::kTreeExpandedKey);
	collapsed_icon = icons.textureRef(interface_icons::kTreeCollapsedKey);
#endif
}

void DevPerformanceRow::onPressed(InteractionContext& context) {
	interaction::onPressed(context, true);
}
void DevPerformanceRow::runLogic(InteractionContext& context) {
	interaction::runLogic(context, true);
}
void DevPerformanceRow::onHovered(InteractionContext& context) {
	context.uiManager.requestCursor(CursorType::PointingHand, 4);
}
void DevPerformanceRow::onReleased(InteractionContext& context) {
	if (!interaction::onReleased(context, true))
		return;
	auto& parameters = context.params;
	if (parameters.expanded &&
		((!parameters.selection && !parameters.scope_selection) ||
		 context.previousInteraction.isReleased(context.clayID("disclosure")))) {
		*parameters.expanded = !*parameters.expanded;
	} else if (parameters.scope_selection) {
		*parameters.scope_selection = {.id = parameters.value, .kind = parameters.scope_kind};
	} else if (parameters.selection) {
		*parameters.selection = parameters.value;
	} else if (parameters.category_mask) {
		*parameters.category_mask ^= parameters.category_bit;
	}
}

void DevPerformanceRow::buildElement(BuildContext& context) {
	const auto& parameters = context.params;
	const bool selected = parameters.scope_selection
							  ? *parameters.scope_selection ==
									DevPerformanceScope{parameters.value, parameters.scope_kind}
						  : parameters.selection
							  ? *parameters.selection == parameters.value
							  : parameters.category_mask &&
									(*parameters.category_mask & parameters.category_bit) != 0;
	const bool hovered =
		context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID());
	const Clay_Color row_color = selected  ? interface_theme::kSelectedRow
								 : hovered ? interface_theme::kHoverSurface
										   : interface_theme::kDepth0Keel;
	Clay_ElementDeclaration row{};
	row.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(25)};
	row.layout.padding = Clay_Padding{4, 4, 3, 3};
	row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	row.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
	row.backgroundColor = row_color;
	Clay_TextElementConfig text{};
	text.fontSize = 11;
	text.textColor =
		(selected || hovered) ? interface_theme::kTextCanvas : interface_theme::kTextSecondary;
	text.wrapMode = CLAY_TEXT_WRAP_NONE;
	CLAY(context.clayID(), row) {
		Clay_ElementDeclaration indicator{};
		indicator.layout.sizing = {.width = CLAY_SIZING_FIXED(3), .height = CLAY_SIZING_GROW(0)};
		indicator.backgroundColor = selected ? interface_theme::kAccentCurrent : row_color;
		CLAY(context.clayID("selection-indicator"), indicator);
		Clay_ElementDeclaration depth{};
		depth.layout.sizing = {.width = CLAY_SIZING_FIXED(parameters.child ? 18.0f : 4.0f),
							   .height = CLAY_SIZING_GROW(0)};
		CLAY(context.clayID("depth"), depth);
		if (parameters.expanded) {
			Clay_ElementDeclaration disclosure{};
			disclosure.layout.sizing = {.width = CLAY_SIZING_FIXED(18),
										.height = CLAY_SIZING_GROW(0)};
			disclosure.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
			CLAY(context.clayID("disclosure"), disclosure) {
				auto icon = *parameters.expanded ? context.resources().expanded_icon
												 : context.resources().collapsed_icon;
				if (icon.handle) {
					icon.tintEnabled = true;
					Clay_ElementDeclaration image{};
					image.layout.sizing = {.width = CLAY_SIZING_FIXED(12),
										   .height = CLAY_SIZING_FIXED(12)};
					image.backgroundColor = interface_theme::kTextSecondary;
					image.image = {.imageData = context.uiManager.imageData(icon)};
					CLAY(context.clayID("icon"), image);
				} else {
					CLAY_TEXT(context.uiManager.toClayString(*parameters.expanded ? "v" : ">"),
							  CLAY_TEXT_CONFIG(text));
				}
			}
		} else {
			Clay_ElementDeclaration spacer{};
			spacer.layout.sizing = {.width = CLAY_SIZING_FIXED(18), .height = CLAY_SIZING_GROW(0)};
			CLAY(context.clayID("disclosure"), spacer);
		}
		Clay_ElementDeclaration label{};
		label.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		label.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
		label.clip.horizontal = true;
		label.clip.scrollInputDisabled = true;
		CLAY(context.clayID("label"), label) {
			CLAY_TEXT(context.uiManager.toClayString(parameters.label), CLAY_TEXT_CONFIG(text));
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
