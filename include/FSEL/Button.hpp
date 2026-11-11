#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::FSEL {

/**
 * Automatic child content emitted by Button::buildElement(); constructElement() ignores this
 * setting.
 */
enum class ButtonContentMode : uint8_t {
	/** Emit no automatic text or icon children. */
	None,
	/** Emit only the text label. */
	TextOnly,
	/** Emit only the icon. */
	IconOnly,
	/** Emit icon followed by text. */
	IconThenText,
	/** Emit text followed by icon. */
	TextThenIcon,
};

/** Optional appearance overrides for one interaction state; empty values inherit the active theme. */
struct ButtonStateOverrides {
	/** Surface background color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	/** Button label color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> labelColor = std::nullopt;
	/** Icon tint color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> iconColor = std::nullopt;
	/** Border color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> borderColor = std::nullopt;
};

/**
 * Per-invocation Button configuration. Borrowed models and text must remain valid through the
 * frame; empty visual overrides inherit the active FSELTheme.
 */
struct ButtonParameters {
	/** Action invoked for an enabled activation gesture. */
	ActionCall onActivate{};
	/** Borrowed texture reference for content artwork; an empty reference draws no icon. */
	TextureRef icon{};
	/** Borrowed UTF-8 display text; keep its storage valid through the frame. */
	std::string_view text{};

	/** Appearance when enabled and idle. Empty colors inherit the theme. */
	ButtonStateOverrides idleOverrides{};
	/** Appearance while hovered. Empty colors inherit the theme. */
	ButtonStateOverrides hoveredOverrides{};
	/** Appearance during an armed pointer press. Empty colors inherit the theme. */
	ButtonStateOverrides pressedOverrides{};
	/** Appearance when interaction is disabled. Empty colors inherit the theme. */
	ButtonStateOverrides disabledOverrides{};

	/** Clay width and height sizing rules for the root element. */
	Clay_Sizing sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	/** Corner radii in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;

	/** Registered font family used for the button label. Empty inherits the active theme. */
	std::optional<FontFamilyId> labelFontFamily = std::nullopt;
	/**
	 * Font weight requested for the button label, such as 400 for normal. Empty inherits the active
	 * theme.
	 */
	std::optional<uint32_t> labelFontWeight = std::nullopt;
	/** Rendered icon side length in layout pixels. Empty inherits the active theme. */
	std::optional<float> iconSize = std::nullopt;

	// Used by buildElement and ignored by constructElement.
	/** Content emitted by draw(); construct() leaves child composition to the caller. */
	ButtonContentMode contentMode = ButtonContentMode::None;
	/** Per-edge border widths in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;

	// Optional theme overrides
	/**
	 * Inset between the root border and its children, in layout pixels. Empty inherits the active
	 * theme.
	 */
	std::optional<Clay_Padding> padding = std::nullopt;
	/** Space between adjacent content items, in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> contentGap = std::nullopt;
	/** Button label font size in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> labelFontSize = std::nullopt;
	/** Alignment of children inside the available content area. Empty inherits the active theme. */
	std::optional<Clay_ChildAlignment> childAlignment = std::nullopt;
	/** Axis and order used to arrange children. Empty inherits the active theme. */
	std::optional<Clay_LayoutDirection> layoutDirection = std::nullopt;
	/** Font style requested for the button label. Empty inherits the active theme. */
	std::optional<FontStyle> labelFontStyle = std::nullopt;
	/** Wrapping rule for the button label. Empty inherits the active theme. */
	std::optional<Clay_TextElementConfigWrapMode> labelWrapMode = std::nullopt;
	/** Horizontal alignment of the trigger or button label. Empty inherits the active theme. */
	std::optional<Clay_TextAlignment> labelAlignment = std::nullopt;

	/** Cursor requested while this enabled control is hovered. Empty inherits the active theme. */
	std::optional<CursorType> cursor = std::nullopt;
	/** Priority used to arbitrate this control's cursor request. Empty inherits the active theme. */
	std::optional<uint8_t> cursorPriority = std::nullopt;
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
	/** Tint icon artwork using the resolved state color when true. */
	bool tintIcon = true;
};

/** Per-instance interaction state maintained by the element and retained by ElementManager. */
struct ButtonState {
	/** Whether an enabled press is waiting for a matching release. */
	bool isArmed = false;
	/** Whether a mouse-up frame has been seen while the press is armed. */
	bool mouseUpObserved = false;
};

/**
 * Action-driven button supporting draw() content modes or construct() with caller-authored
 * children. Activation occurs after a completed enabled click.
 */
struct Button {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = ButtonParameters;
	/** Per-instance state owned and retained by ElementManager. */
	using State = ButtonState;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<Button>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<Button>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.button");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL Button";

	/**
	 * ElementBuilder hover hook; requests the enabled control cursor. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onHovered(InteractionContext& context) {
		if (!isEnabled(context)) {
			return;
		}

		const auto& theme = context.uiManager.theme<FSELTheme>().buttonTheme;
		context.uiManager.requestCursor(
			context.params.cursor.value_or(theme.cursor),
			context.params.cursorPriority.value_or(theme.cursorPriority));
	}

	/**
	 * ElementBuilder press hook; begins the enabled control interaction. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onPressed(InteractionContext& context) {
		auto& state = context.state();
		if (!isEnabled(context)) {
			clearInteraction(state);
			return;
		}

		state.isArmed = true;
		state.mouseUpObserved = false;
	}

	/**
	 * ElementBuilder release hook; completes or cancels the pending interaction. @param context
	 * Borrowed context valid for this callback only.
	 */
	static void onReleased(InteractionContext& context) {
		auto& state = context.state();
		if (!state.isArmed) {
			return;
		}

		const bool shouldActivate = isEnabled(context);
		clearInteraction(state);
		if (shouldActivate) {
			(void)context.invoke(context.params.onActivate);
		}
	}

	/**
	 * ElementBuilder logic hook; reconciles retained interaction state with the current input.
	 * @param context Borrowed context valid for this callback only.
	 */
	static void runLogic(InteractionContext& context) {
		auto& state = context.state();
		if (!state.isArmed) {
			return;
		}
		if (!isEnabled(context)) {
			clearInteraction(state);
			return;
		}

		const bool pointerDown =
			context.uiManager.getCurrentFrameInput().mouseDown[0];
		if (pointerDown) {
			if (state.mouseUpObserved) {
				clearInteraction(state);
			}
			return;
		}

		if (state.mouseUpObserved) {
			clearInteraction(state);
		} else {
			state.mouseUpObserved = true;
		}
	}

	/**
	 * ElementBuilder construct hook; returns the themed root declaration for caller-authored
	 * children. @param context Borrowed context valid for this callback only. @return Root Clay
	 * declaration; the builder opens it for child composition.
	 */
	static Clay_ElementDeclaration constructElement(BuildContext& context) {
		const FSELButtonStateTheme appearance = resolveAppearance(context);
		return makeRootDeclaration(context, appearance);
	}

	/**
	 * ElementBuilder draw hook; emits this control and its children for the active frame. @param
	 * context Borrowed context valid for this callback only.
	 */
	static void buildElement(BuildContext& context) {
		const auto& theme = context.uiManager.theme<FSELTheme>().buttonTheme;
		const FSELButtonStateTheme appearance = resolveAppearance(context);
		const Clay_ElementDeclaration root = makeRootDeclaration(context, appearance);
		const bool needsText =
			context.params.contentMode == ButtonContentMode::TextOnly ||
			context.params.contentMode == ButtonContentMode::IconThenText ||
			context.params.contentMode == ButtonContentMode::TextThenIcon;
		const bool needsIcon =
			context.params.contentMode == ButtonContentMode::IconOnly ||
			context.params.contentMode == ButtonContentMode::IconThenText ||
			context.params.contentMode == ButtonContentMode::TextThenIcon;

		Clay_TextElementConfig labelConfig{};
		Clay_ElementDeclaration iconDeclaration{};
		if (needsText) {
			labelConfig.textColor = appearance.labelColor;
			labelConfig.fontId = context.uiManager.resolveFont(
				context.params.labelFontFamily.value_or(theme.labelFontFamily),
				context.params.labelFontWeight.value_or(theme.labelFontWeight),
				context.params.labelFontStyle.value_or(theme.labelFontStyle));
			labelConfig.fontSize = context.params.labelFontSize.value_or(
				theme.labelFontSize);
			labelConfig.wrapMode = context.params.labelWrapMode.value_or(
				theme.labelWrapMode);
			labelConfig.textAlignment = context.params.labelAlignment.value_or(
				theme.labelAlignment);
		}
		if (needsIcon) {
			TextureRef icon = context.params.icon;
			icon.tintEnabled = context.params.tintIcon;
			const float iconSize = std::max(
				context.params.iconSize.value_or(theme.iconSize),
				1.0f);
			iconDeclaration.layout.sizing = {
				.width = CLAY_SIZING_FIXED(iconSize),
				.height = CLAY_SIZING_FIXED(iconSize),
			};
			iconDeclaration.backgroundColor = appearance.iconColor;
			iconDeclaration.image = {
				.imageData = context.uiManager.imageData(icon),
			};
		}

		auto drawText = [&]() {
			CLAY(context.clayID("label"), {}) {
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.text),
					CLAY_TEXT_CONFIG(labelConfig));
			}
		};
		auto drawIcon = [&]() {
			CLAY(context.clayID("icon"), iconDeclaration);
		};

		CLAY(context.clayID(), root) {
			switch (context.params.contentMode) {
			case ButtonContentMode::None:
				break;
			case ButtonContentMode::TextOnly:
				drawText();
				break;
			case ButtonContentMode::IconOnly:
				drawIcon();
				break;
			case ButtonContentMode::IconThenText:
				drawIcon();
				drawText();
				break;
			case ButtonContentMode::TextThenIcon:
				drawText();
				drawIcon();
				break;
			}
		}
	}

private:
	enum class VisualState {
		Idle,
		Hovered,
		Pressed,
		Disabled,
	};

	static bool isEnabled(const InteractionContext& context) {
		return context.params.enabled &&
			context.actionAvailability(context.params.onActivate).enabled;
	}

	static bool isEnabled(const BuildContext& context) {
		return context.params.enabled &&
			context.uiManager.actions()
				.availability(context.params.onActivate).enabled;
	}

	static void clearInteraction(ButtonState& state) {
		state.isArmed = false;
		state.mouseUpObserved = false;
	}

	static VisualState resolveVisualState(BuildContext& context) {
		if (!isEnabled(context)) {
			return VisualState::Disabled;
		}

		const bool hovered = context.uiManager
			.getPreviousFramesInteraction()
			.isHovered(context.clayID());
		const bool pressed = context.state().isArmed && hovered &&
			context.uiManager.getCurrentFrameInput().mouseDown[0];
		if (pressed) {
			return VisualState::Pressed;
		}
		return hovered ? VisualState::Hovered : VisualState::Idle;
	}

	static FSELButtonStateTheme applyOverrides(
		FSELButtonStateTheme appearance,
		const ButtonStateOverrides& overrides) {
		appearance.backgroundColor = overrides.backgroundColor.value_or(
			appearance.backgroundColor);
		appearance.labelColor = overrides.labelColor.value_or(
			appearance.labelColor);
		appearance.iconColor = overrides.iconColor.value_or(
			appearance.iconColor);
		appearance.borderColor = overrides.borderColor.value_or(
			appearance.borderColor);
		return appearance;
	}

	static FSELButtonStateTheme resolveAppearance(BuildContext& context) {
		const auto& theme = context.uiManager.theme<FSELTheme>().buttonTheme;
		switch (resolveVisualState(context)) {
		case VisualState::Idle:
			return applyOverrides(theme.idle, context.params.idleOverrides);
		case VisualState::Hovered:
			return applyOverrides(theme.hovered, context.params.hoveredOverrides);
		case VisualState::Pressed:
			return applyOverrides(theme.pressed, context.params.pressedOverrides);
		case VisualState::Disabled:
			return applyOverrides(theme.disabled, context.params.disabledOverrides);
		}
		return applyOverrides(theme.idle, context.params.idleOverrides);
	}

	static Clay_ElementDeclaration makeRootDeclaration(
		BuildContext& context,
		const FSELButtonStateTheme& appearance) {
		const auto& theme = context.uiManager.theme<FSELTheme>().buttonTheme;

		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing;
		root.layout.padding = context.params.padding.value_or(theme.padding);
		root.layout.childAlignment = context.params.childAlignment.value_or(
			theme.childAlignment);
		root.layout.layoutDirection = context.params.layoutDirection.value_or(
			theme.layoutDirection);
		root.layout.childGap = context.params.contentGap.value_or(theme.contentGap);
		root.backgroundColor = appearance.backgroundColor;
		root.cornerRadius = context.params.cornerRadius.value_or(theme.cornerRadius);
		root.border = {
			.color = appearance.borderColor,
			.width = context.params.borderWidth.value_or(theme.borderWidth),
		};
		return root;
	}
};

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr Button kButton{};
static_assert(validate_flow_element<Button>());
static_assert(DrawableFlowElement<Button>);
static_assert(ConstructibleFlowElement<Button>);

} // namespace FlowUi::FSEL
