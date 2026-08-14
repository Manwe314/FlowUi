#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FSEL/internal/BooleanControlInteraction.hpp"

namespace FlowUi::FSEL {

struct CheckboxStateOverrides {
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	std::optional<Clay_Color> borderColor = std::nullopt;
	std::optional<Clay_Color> iconColor = std::nullopt;
};

struct CheckboxValueOverrides {
	CheckboxStateOverrides idle{};
	CheckboxStateOverrides hovered{};
	CheckboxStateOverrides pressed{};
	CheckboxStateOverrides disabled{};
};

struct CheckboxParameters {
	bool isChecked = false;
	bool enabled = true;
	ActionCall onToggle{};

	TextureRef uncheckedIcon{};
	TextureRef checkedIcon{};
	bool tintIcon = true;

	// Optional theme overrides
	std::optional<float> size = std::nullopt;
	std::optional<float> iconSize = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
	CheckboxValueOverrides uncheckedOverrides{};
	CheckboxValueOverrides checkedOverrides{};
	std::optional<CursorType> cursor = std::nullopt;
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

using CheckboxState = detail::boolean_control::State;

/** Build-only boolean checkbox with optional per-value overlay icons. */
struct Checkbox {
	using Parameters = CheckboxParameters;
	using State = CheckboxState;
	using BuildContext = ElementBuildContext<Checkbox>;
	using InteractionContext = ElementInteractionContext<Checkbox>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.checkbox");
	static constexpr std::string_view debugName = "FSEL Checkbox";

	static void onHovered(InteractionContext& context) {
		if (!detail::boolean_control::isEnabled(
				context,
				context.params.onToggle)) {
			return;
		}

		const auto& theme = context.uiManager.theme<FSELTheme>().checkboxTheme;
		context.uiManager.requestCursor(
			context.params.cursor.value_or(theme.cursor),
			context.params.cursorPriority.value_or(theme.cursorPriority));
	}

	static void onPressed(InteractionContext& context) {
		detail::boolean_control::onPressed(context, context.params.onToggle);
	}

	static void onReleased(InteractionContext& context) {
		detail::boolean_control::onReleased(context, context.params.onToggle);
	}

	static void runLogic(InteractionContext& context) {
		detail::boolean_control::runLogic(context, context.params.onToggle);
	}

	static void buildElement(BuildContext& context) {
		const auto& theme = context.uiManager.theme<FSELTheme>().checkboxTheme;
		const FSELCheckboxStateTheme appearance = resolveAppearance(context, theme);
		const float size = std::max(context.params.size.value_or(theme.size), 1.0f);
		const float iconSize = std::clamp(
			context.params.iconSize.value_or(theme.iconSize),
			1.0f,
			size);
		TextureRef icon = context.params.isChecked
			? context.params.checkedIcon
			: context.params.uncheckedIcon;
		const bool hasIcon = static_cast<bool>(icon.handle);

		Clay_ElementDeclaration root{};
		root.layout.sizing = {
			.width = CLAY_SIZING_FIXED(size),
			.height = CLAY_SIZING_FIXED(size),
		};
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_CENTER,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		root.backgroundColor = appearance.backgroundColor;
		root.cornerRadius = context.params.cornerRadius.value_or(
			theme.cornerRadius);
		root.border = {
			.color = appearance.borderColor,
			.width = context.params.borderWidth.value_or(theme.borderWidth),
		};

		Clay_ElementDeclaration iconDeclaration{};
		if (hasIcon) {
			icon.tintEnabled = context.params.tintIcon;
			iconDeclaration.layout.sizing = {
				.width = CLAY_SIZING_FIXED(iconSize),
				.height = CLAY_SIZING_FIXED(iconSize),
			};
			iconDeclaration.backgroundColor = appearance.iconColor;
			iconDeclaration.image = {
				.imageData = context.uiManager.imageData(icon),
			};
		}

		CLAY(context.clayID(), root) {
			if (hasIcon) {
				CLAY(context.clayID("icon"), iconDeclaration);
			}
		}
	}

private:
	static FSELCheckboxStateTheme applyOverrides(
		FSELCheckboxStateTheme appearance,
		const CheckboxStateOverrides& overrides) {
		appearance.backgroundColor = overrides.backgroundColor.value_or(
			appearance.backgroundColor);
		appearance.borderColor = overrides.borderColor.value_or(
			appearance.borderColor);
		appearance.iconColor = overrides.iconColor.value_or(
			appearance.iconColor);
		return appearance;
	}

	static FSELCheckboxStateTheme resolveAppearance(
		BuildContext& context,
		const FSELCheckboxTheme& theme) {
		const FSELCheckboxValueTheme& valueTheme = context.params.isChecked
			? theme.checked
			: theme.unchecked;
		const CheckboxValueOverrides& overrides = context.params.isChecked
			? context.params.checkedOverrides
			: context.params.uncheckedOverrides;

		switch (detail::boolean_control::resolveVisualState(
			context,
			context.params.onToggle)) {
		case detail::boolean_control::VisualState::Idle:
			return applyOverrides(valueTheme.idle, overrides.idle);
		case detail::boolean_control::VisualState::Hovered:
			return applyOverrides(valueTheme.hovered, overrides.hovered);
		case detail::boolean_control::VisualState::Pressed:
			return applyOverrides(valueTheme.pressed, overrides.pressed);
		case detail::boolean_control::VisualState::Disabled:
			return applyOverrides(valueTheme.disabled, overrides.disabled);
		}
		return applyOverrides(valueTheme.idle, overrides.idle);
	}
};

inline constexpr Checkbox kCheckbox{};
static_assert(FlowElement<Checkbox>);
static_assert(DrawableFlowElement<Checkbox>);

} // namespace FlowUi::FSEL
