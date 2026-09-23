#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/SelectableSurfaceStyle.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
namespace FlowUi::devSystems::interface_elements {
/** Appearance shared by the Inspect and Performance segmented choices. */
[[nodiscard]] inline FSEL::SelectableSurfaceStyle selector_choice_style() noexcept {
	FSEL::SelectableSurfaceStyle style{};
	style.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	style.padding = Clay_Padding{7, 7, 0, 0};
	style.childAlignment = Clay_ChildAlignment{
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	style.borderWidth = Clay_BorderWidth{0, 0, 0, 0, 0};
	style.cornerRadius = CLAY_CORNER_RADIUS(2);
	style.idleOverrides.backgroundColor = interface_theme::kDepth0Keel;
	style.idleOverrides.borderColor = interface_theme::kDepth0Keel;
	style.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	style.hoveredOverrides.borderColor = interface_theme::kHoverSurface;
	style.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	style.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
	style.selectedOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	style.selectedOverrides.borderColor = interface_theme::kAccentCurrent;
	style.disabledOverrides.backgroundColor = interface_theme::kDepth0Keel;
	style.disabledOverrides.borderColor = interface_theme::kDepth0Keel;
	style.selectedDisabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
	style.selectedDisabledOverrides.borderColor = interface_theme::kBorderPrimary;
	return style;
}
/** Bordered, inset track around a group of segmented choices. */
[[nodiscard]] inline Clay_ElementDeclaration selector_choice_track() noexcept {
	Clay_ElementDeclaration track{};
	track.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
	track.layout.padding = Clay_Padding{2, 2, 2, 2};
	track.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	track.backgroundColor = interface_theme::kDepth0Keel;
	track.cornerRadius = CLAY_CORNER_RADIUS(3);
	track.border = {.color = interface_theme::kBorderVisible,
					.width = Clay_BorderWidth{1, 1, 1, 1, 0}};
	return track;
}
} // namespace FlowUi::devSystems::interface_elements
#endif
