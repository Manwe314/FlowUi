#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/Button.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"

namespace FlowUi::devSystems::interface_elements::action_button {

/* Use the conventional icon when available, retaining the full text fallback. */
inline void set_icon(FSEL::ButtonParameters& parameters, TextureRef icon) noexcept {
	parameters.icon = icon;
	parameters.contentMode =
		icon.handle ? FSEL::ButtonContentMode::IconOnly : FSEL::ButtonContentMode::TextOnly;
	parameters.iconSize = 14.0f;
	parameters.tintIcon = true;
	parameters.idleOverrides.iconColor = parameters.idleOverrides.labelColor;
	parameters.hoveredOverrides.iconColor = parameters.hoveredOverrides.labelColor;
	parameters.pressedOverrides.iconColor = parameters.pressedOverrides.labelColor;
	parameters.disabledOverrides.iconColor = parameters.disabledOverrides.labelColor;
}

/* Keep an icon action's name available on hover without changing its identity. */
template <class Context, class Identity>
void tooltip(Context& context, const Identity& identity, std::string_view label,
			 FlowDefinitionID definition = FSEL::Button::definitionId) {
	const auto element =
		::FlowUi::detail::element_id::resolveLocal(context.id, definition, identity.token);
	if (context.uiManager.getPreviousFramesInteraction().isHovered(
			context.uiManager.toClayEID(element))) {
		diagnostics_ui::pointer_tooltip(context, Keyed("action-tooltip", identity.token), label);
	}
}

} // namespace FlowUi::devSystems::interface_elements::action_button

#endif
