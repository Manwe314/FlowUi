#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FSEL/internal/BooleanControlInteraction.hpp"

namespace FlowUi::FSEL {

/** Optional appearance overrides for one interaction state; empty values inherit the active theme. */
struct CheckboxStateOverrides {
	/** Surface background color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	/** Border color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> borderColor = std::nullopt;
	/** Icon tint color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> iconColor = std::nullopt;
};

/** Per-interaction-state overrides for one boolean value. */
struct CheckboxValueOverrides {
	/** Appearance when enabled and idle. */
	CheckboxStateOverrides idle{};
	/** Appearance while hovered. */
	CheckboxStateOverrides hovered{};
	/** Appearance during an armed pointer press. */
	CheckboxStateOverrides pressed{};
	/** Appearance when interaction is disabled. */
	CheckboxStateOverrides disabled{};
};

/**
 * Per-invocation Checkbox configuration. Borrowed models and text must remain valid through the
 * frame; empty visual overrides inherit the active FSELTheme.
 */
struct CheckboxParameters {
	/** Action invoked on a completed click; the application updates the boolean value. */
	ActionCall onToggle{};

	/** Borrowed artwork for the unchecked value; empty draws no overlay icon. */
	TextureRef uncheckedIcon{};
	/** Borrowed artwork for the checked value; empty draws no overlay icon. */
	TextureRef checkedIcon{};
	/** Appearance for the unchecked value. Empty colors inherit the theme. */
	CheckboxValueOverrides uncheckedOverrides{};
	/** Appearance for the checked value. Empty colors inherit the theme. */
	CheckboxValueOverrides checkedOverrides{};
	/** Corner radii in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;

	// Optional theme overrides
	/** Checkbox side length in layout pixels. Empty inherits the active theme. */
	std::optional<float> size = std::nullopt;
	/** Rendered icon side length in layout pixels. Empty inherits the active theme. */
	std::optional<float> iconSize = std::nullopt;
	/** Per-edge border widths in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	/** Cursor requested while this enabled control is hovered. Empty inherits the active theme. */
	std::optional<CursorType> cursor = std::nullopt;
	/** Priority used to arbitrate this control's cursor request. Empty inherits the active theme. */
	std::optional<uint8_t> cursorPriority = std::nullopt;
	/** Application-provided checked value; onToggle does not mutate it. */
	bool isChecked = false;
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
	/** Tint icon artwork using the resolved state color when true. */
	bool tintIcon = true;
};

/** Retained interaction state used by this standard element. */
using CheckboxState = detail::boolean_control::State;

/** Build-only boolean checkbox with optional per-value overlay icons. */
struct Checkbox {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = CheckboxParameters;
	/** Per-instance state owned and retained by ElementManager. */
	using State = CheckboxState;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<Checkbox>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<Checkbox>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.checkbox");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL Checkbox";

	/**
	 * ElementBuilder hover hook; requests the enabled control cursor. @param context Borrowed
	 * context valid for this callback only.
	 */
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

	/**
	 * ElementBuilder press hook; begins the enabled control interaction. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onPressed(InteractionContext& context) {
		detail::boolean_control::onPressed(context, context.params.onToggle);
	}

	/**
	 * ElementBuilder release hook; completes or cancels the pending interaction. @param context
	 * Borrowed context valid for this callback only.
	 */
	static void onReleased(InteractionContext& context) {
		detail::boolean_control::onReleased(context, context.params.onToggle);
	}

	/**
	 * ElementBuilder logic hook; reconciles retained interaction state with the current input.
	 * @param context Borrowed context valid for this callback only.
	 */
	static void runLogic(InteractionContext& context) {
		detail::boolean_control::runLogic(context, context.params.onToggle);
	}

	/**
	 * ElementBuilder draw hook; emits this control and its children for the active frame. @param
	 * context Borrowed context valid for this callback only.
	 */
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

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr Checkbox kCheckbox{};
static_assert(validate_flow_element<Checkbox>());
static_assert(DrawableFlowElement<Checkbox>);

} // namespace FlowUi::FSEL
