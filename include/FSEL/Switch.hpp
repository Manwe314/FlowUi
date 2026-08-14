#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FSEL/internal/BooleanControlInteraction.hpp"

namespace FlowUi::FSEL {

struct SwitchStateOverrides {
	std::optional<Clay_Color> trackColor = std::nullopt;
	std::optional<Clay_Color> trackBorderColor = std::nullopt;
	std::optional<Clay_Color> knobColor = std::nullopt;
	std::optional<Clay_Color> knobBorderColor = std::nullopt;
};

struct SwitchValueOverrides {
	SwitchStateOverrides idle{};
	SwitchStateOverrides hovered{};
	SwitchStateOverrides pressed{};
	SwitchStateOverrides disabled{};
};

struct SwitchParameters {
	bool isOn = false;
	bool enabled = true;
	ActionCall onToggle{};

	// Optional theme overrides
	std::optional<float> trackWidth = std::nullopt;
	std::optional<float> trackHeight = std::nullopt;
	std::optional<uint16_t> knobInset = std::nullopt;
	std::optional<float> roundness = std::nullopt;
	std::optional<Clay_BorderWidth> trackBorderWidth = std::nullopt;
	std::optional<Clay_BorderWidth> knobBorderWidth = std::nullopt;
	SwitchValueOverrides offOverrides{};
	SwitchValueOverrides onOverrides{};
	std::optional<CursorType> cursor = std::nullopt;
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

using SwitchState = detail::boolean_control::State;

/** Build-only boolean switch rendered as a track and movable knob. */
struct Switch {
	using Parameters = SwitchParameters;
	using State = SwitchState;
	using BuildContext = ElementBuildContext<Switch>;
	using InteractionContext = ElementInteractionContext<Switch>;

	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.switch");
	static constexpr std::string_view debugName = "FSEL Switch";

	static void onHovered(InteractionContext& context) {
		if (!detail::boolean_control::isEnabled(
				context,
				context.params.onToggle)) {
			return;
		}

		const auto& theme = context.uiManager.theme<FSELTheme>().switchTheme;
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
		const auto& theme = context.uiManager.theme<FSELTheme>().switchTheme;
		const FSELSwitchStateTheme appearance = resolveAppearance(context, theme);
		const float trackWidth = std::max(
			context.params.trackWidth.value_or(theme.trackWidth),
			1.0f);
		const float trackHeight = std::max(
			context.params.trackHeight.value_or(theme.trackHeight),
			1.0f);
		const float maximumInset = std::max(
			(std::min(trackWidth, trackHeight) - 1.0f) * 0.5f,
			0.0f);
		const uint16_t knobInset = static_cast<uint16_t>(std::min(
			static_cast<float>(context.params.knobInset.value_or(theme.knobInset)),
			maximumInset));
		const float knobSize = std::max(
			std::min(trackWidth, trackHeight) - 2.0f * knobInset,
			1.0f);
		const float roundness = detail::boolean_control::clampRoundness(
			context.params.roundness.value_or(theme.roundness));

		Clay_ElementDeclaration track{};
		track.layout.sizing = {
			.width = CLAY_SIZING_FIXED(trackWidth),
			.height = CLAY_SIZING_FIXED(trackHeight),
		};
		track.layout.padding = {
			.left = knobInset,
			.right = knobInset,
			.top = knobInset,
			.bottom = knobInset,
		};
		track.layout.childAlignment = {
			.x = context.params.isOn ? CLAY_ALIGN_X_RIGHT : CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		track.backgroundColor = appearance.trackColor;
		track.cornerRadius = CLAY_CORNER_RADIUS(
			std::min(trackWidth, trackHeight) * 0.5f * roundness);
		track.border = {
			.color = appearance.trackBorderColor,
			.width = context.params.trackBorderWidth.value_or(
				theme.trackBorderWidth),
		};

		Clay_ElementDeclaration knob{};
		knob.layout.sizing = {
			.width = CLAY_SIZING_FIXED(knobSize),
			.height = CLAY_SIZING_FIXED(knobSize),
		};
		knob.backgroundColor = appearance.knobColor;
		knob.cornerRadius = CLAY_CORNER_RADIUS(
			knobSize * 0.5f * roundness);
		knob.border = {
			.color = appearance.knobBorderColor,
			.width = context.params.knobBorderWidth.value_or(
				theme.knobBorderWidth),
		};

		CLAY(context.clayID(), track) {
			CLAY(context.clayID("knob"), knob);
		}
	}

private:
	static FSELSwitchStateTheme applyOverrides(
		FSELSwitchStateTheme appearance,
		const SwitchStateOverrides& overrides) {
		appearance.trackColor = overrides.trackColor.value_or(
			appearance.trackColor);
		appearance.trackBorderColor = overrides.trackBorderColor.value_or(
			appearance.trackBorderColor);
		appearance.knobColor = overrides.knobColor.value_or(
			appearance.knobColor);
		appearance.knobBorderColor = overrides.knobBorderColor.value_or(
			appearance.knobBorderColor);
		return appearance;
	}

	static FSELSwitchStateTheme resolveAppearance(
		BuildContext& context,
		const FSELSwitchTheme& theme) {
		const FSELSwitchValueTheme& valueTheme = context.params.isOn
			? theme.on
			: theme.off;
		const SwitchValueOverrides& overrides = context.params.isOn
			? context.params.onOverrides
			: context.params.offOverrides;

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

inline constexpr Switch kSwitch{};
static_assert(FlowElement<Switch>);
static_assert(DrawableFlowElement<Switch>);

} // namespace FlowUi::FSEL
