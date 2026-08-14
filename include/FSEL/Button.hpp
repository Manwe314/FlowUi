#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::FSEL {

enum class ButtonContentMode : uint8_t {
	None,
	TextOnly,
	IconOnly,
	IconThenText,
	TextThenIcon,
};

struct ButtonStateOverrides {
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	std::optional<Clay_Color> labelColor = std::nullopt;
	std::optional<Clay_Color> iconColor = std::nullopt;
	std::optional<Clay_Color> borderColor = std::nullopt;
};

struct ButtonParameters {
	ActionCall onActivate{};
	bool enabled = true;

	// Used by buildElement and ignored by constructElement.
	ButtonContentMode contentMode = ButtonContentMode::None;
	std::string_view text{};
	TextureRef icon{};
	bool tintIcon = true;

	Clay_Sizing sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};

	// Optional theme overrides
	std::optional<Clay_Padding> padding = std::nullopt;
	std::optional<Clay_ChildAlignment> childAlignment = std::nullopt;
	std::optional<Clay_LayoutDirection> layoutDirection = std::nullopt;
	std::optional<uint16_t> contentGap = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;

	ButtonStateOverrides idleOverrides{};
	ButtonStateOverrides hoveredOverrides{};
	ButtonStateOverrides pressedOverrides{};
	ButtonStateOverrides disabledOverrides{};

	std::optional<FontFamilyId> labelFontFamily = std::nullopt;
	std::optional<uint32_t> labelFontWeight = std::nullopt;
	std::optional<FontStyle> labelFontStyle = std::nullopt;
	std::optional<uint16_t> labelFontSize = std::nullopt;
	std::optional<Clay_TextElementConfigWrapMode> labelWrapMode = std::nullopt;
	std::optional<Clay_TextAlignment> labelAlignment = std::nullopt;
	std::optional<float> iconSize = std::nullopt;

	std::optional<CursorType> cursor = std::nullopt;
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

struct ButtonState {
	bool isArmed = false;
	bool mouseUpObserved = false;
};

struct Button {
	using Parameters = ButtonParameters;
	using State = ButtonState;
	using BuildContext = ElementBuildContext<Button>;
	using InteractionContext = ElementInteractionContext<Button>;

	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.button");
	static constexpr std::string_view debugName = "FSEL Button";

	static void onHovered(InteractionContext& context) {
		if (!isEnabled(context)) {
			return;
		}

		const auto& theme = context.uiManager.theme<FSELTheme>().buttonTheme;
		context.uiManager.requestCursor(
			context.params.cursor.value_or(theme.cursor),
			context.params.cursorPriority.value_or(theme.cursorPriority));
	}

	static void onPressed(InteractionContext& context) {
		auto& state = context.state();
		if (!isEnabled(context)) {
			clearInteraction(state);
			return;
		}

		state.isArmed = true;
		state.mouseUpObserved = false;
	}

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

	static Clay_ElementDeclaration constructElement(BuildContext& context) {
		const FSELButtonStateTheme appearance = resolveAppearance(context);
		return makeRootDeclaration(context, appearance);
	}

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

inline constexpr Button kButton{};
static_assert(FlowElement<Button>);
static_assert(DrawableFlowElement<Button>);
static_assert(ConstructibleFlowElement<Button>);

} // namespace FlowUi::FSEL
