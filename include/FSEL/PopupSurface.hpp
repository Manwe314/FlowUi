#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
#include "FlowUi/Public/managers/PopupManager.hpp"

namespace FlowUi::FSEL {

/**
 * Per-invocation PopupSurface configuration. Borrowed models and text must remain valid through
 * the frame; empty visual overrides inherit the active FSELTheme.
 */
struct PopupSurfaceParameters {
	/** Anchor, placement, layer and dismissal configuration passed to PopupManager. */
	PopupRequest popupRequest{};
	/** Optional notification consumed after PopupManager dismisses this surface. */
	ActionCall onDismissed{};

	/** Clay width and height sizing rules for the root element. */
	Clay_Sizing sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	/** Clay clipping and scroll offset configuration for the surface. */
	Clay_ClipElementConfig clipConfig{};

	/**
	 * Inset between the root border and its children, in layout pixels. Empty inherits the active
	 * theme.
	 */
	std::optional<Clay_Padding> padding = std::nullopt;
	/** Space between consecutive children, in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> childGap = std::nullopt;
	/** Alignment of children inside the available content area. Empty inherits the active theme. */
	std::optional<Clay_ChildAlignment> childAlignment = std::nullopt;
	/** Axis and order used to arrange children. Empty inherits the active theme. */
	std::optional<Clay_LayoutDirection> layoutDirection = std::nullopt;
	/** Surface background color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	/** Border color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> borderColor = std::nullopt;
	/** Per-edge border widths in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	/** Corner radii in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
};

/**
 * Construct-only popup root. The caller controls presence and authors arbitrary
 * children; PopupManager supplies placement, overflow correction, stacking,
 * measurement, and dismissal behavior.
 */
struct PopupSurface {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = PopupSurfaceParameters;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<PopupSurface>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<PopupSurface>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.popup-surface");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL PopupSurface";

	/**
	 * ElementBuilder logic hook; reconciles retained interaction state with the current input.
	 * @param context Borrowed context valid for this callback only.
	 */
	static void runLogic(InteractionContext& context) {
		if (context.uiManager.popups().consumeDismissed(context.id)) {
			(void)context.invoke(context.params.onDismissed);
		}
	}

	/**
	 * ElementBuilder construct hook; returns the themed root declaration for caller-authored
	 * children. @param context Borrowed context valid for this callback only. @return Root Clay
	 * declaration; the builder opens it for child composition.
	 */
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

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr PopupSurface kPopupSurface{};
static_assert(FlowElement<PopupSurface>);
static_assert(ConstructibleFlowElement<PopupSurface>);
static_assert(!DrawableFlowElement<PopupSurface>);

} // namespace FlowUi::FSEL
