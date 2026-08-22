#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "managers/FlowUiElementBuilder.hpp"
#include "managers/PopupManager.hpp"

namespace FlowUi::FSEL {

struct PopupSurfaceParameters {
	PopupRequest popupRequest{};
	ActionCall onDismissed{};

	Clay_Sizing sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_ClipElementConfig clipConfig{};

	std::optional<Clay_Padding> padding = std::nullopt;
	std::optional<uint16_t> childGap = std::nullopt;
	std::optional<Clay_ChildAlignment> childAlignment = std::nullopt;
	std::optional<Clay_LayoutDirection> layoutDirection = std::nullopt;
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	std::optional<Clay_Color> borderColor = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
};

/**
 * Construct-only popup root. The caller controls presence and authors arbitrary
 * children; PopupManager supplies placement, overflow correction, stacking,
 * measurement, and dismissal behavior.
 */
struct PopupSurface {
	using Parameters = PopupSurfaceParameters;
	using BuildContext = ElementBuildContext<PopupSurface>;
	using InteractionContext = ElementInteractionContext<PopupSurface>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.popup-surface");
	static constexpr std::string_view debugName = "FSEL PopupSurface";

	static void runLogic(InteractionContext& context) {
		if (context.uiManager.popups().consumeDismissed(context.id)) {
			(void)context.invoke(context.params.onDismissed);
		}
	}

	static Clay_ElementDeclaration constructElement(BuildContext& context) {
		const FSELPopupSurfaceTheme& theme =
			context.uiManager.theme<FSELTheme>().popupSurfaceTheme;
		const PopupFrame frame = context.uiManager.popups().request(
			context.id, context.params.popupRequest).value_or(PopupFrame{});

		Clay_ElementDeclaration declaration{};
		declaration.layout = {
			.sizing = context.params.sizing,
			.padding = context.params.padding.value_or(theme.padding),
			.childGap = context.params.childGap.value_or(theme.childGap),
			.childAlignment = context.params.childAlignment.value_or(
				theme.childAlignment),
			.layoutDirection = context.params.layoutDirection.value_or(
				theme.layoutDirection),
		};
		declaration.backgroundColor = context.params.backgroundColor.value_or(
			theme.backgroundColor);
		declaration.cornerRadius = context.params.cornerRadius.value_or(
			theme.cornerRadius);
		declaration.clip = context.params.clipConfig;
		declaration.border = {
			.color = context.params.borderColor.value_or(theme.borderColor),
			.width = context.params.borderWidth.value_or(theme.borderWidth),
		};

		if (frame.visible || frame.measureOnly) {
			declaration.floating = frame.floating;
		} else {
			declaration.floating = inertFloatingRoot();
		}
		return declaration;
	}

private:
	[[nodiscard]] static constexpr Clay_FloatingElementConfig
	inertFloatingRoot() noexcept {
		Clay_FloatingElementConfig floating{};
		floating.offset = Clay_Vector2{1048576.0f, 1048576.0f};
		floating.attachPoints = Clay_FloatingAttachPoints{
			.element = CLAY_ATTACH_POINT_LEFT_TOP,
			.parent = CLAY_ATTACH_POINT_LEFT_TOP,
		};
		floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
		floating.attachTo = CLAY_ATTACH_TO_ROOT;
		floating.clipTo = CLAY_CLIP_TO_NONE;
		return floating;
	}
};

inline constexpr PopupSurface kPopupSurface{};
static_assert(FlowElement<PopupSurface>);
static_assert(ConstructibleFlowElement<PopupSurface>);
static_assert(!DrawableFlowElement<PopupSurface>);

} // namespace FlowUi::FSEL
