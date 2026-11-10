#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FSEL/internal/BooleanControlInteraction.hpp"

namespace FlowUi::FSEL {

/** Optional appearance overrides for one interaction state; empty values inherit the active theme. */
struct SwitchStateOverrides {
	/** Track background color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> trackColor = std::nullopt;
	/** Track border color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> trackBorderColor = std::nullopt;
	/** Switch knob color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> knobColor = std::nullopt;
	/** Switch knob border color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> knobBorderColor = std::nullopt;
};

/** Per-interaction-state overrides for one boolean value. */
struct SwitchValueOverrides {
	/** Appearance when enabled and idle. */
	SwitchStateOverrides idle{};
	/** Appearance while hovered. */
	SwitchStateOverrides hovered{};
	/** Appearance during an armed pointer press. */
	SwitchStateOverrides pressed{};
	/** Appearance when interaction is disabled. */
	SwitchStateOverrides disabled{};
};

/**
 * Per-invocation Switch configuration. Borrowed models and text must remain valid through the
 * frame; empty visual overrides inherit the active FSELTheme.
 */
struct SwitchParameters {
	/** Application-provided switch value; onToggle does not mutate it. */
	bool isOn = false;
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
	/** Action invoked on a completed click; the application updates the boolean value. */
	ActionCall onToggle{};

	// Optional theme overrides
	/** Switch track width in layout pixels. Empty inherits the active theme. */
	std::optional<float> trackWidth = std::nullopt;
	/** Switch track height in layout pixels. Empty inherits the active theme. */
	std::optional<float> trackHeight = std::nullopt;
	/** Space between the track edge and knob, in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> knobInset = std::nullopt;
	/** Switch roundness factor; 0 is square and 1 is fully rounded. Empty inherits the active theme. */
	std::optional<float> roundness = std::nullopt;
	/** Per-edge track border widths in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_BorderWidth> trackBorderWidth = std::nullopt;
	/** Per-edge knob border widths in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_BorderWidth> knobBorderWidth = std::nullopt;
	/** Appearance for the off value. Empty colors inherit the theme. */
	SwitchValueOverrides offOverrides{};
	/** Appearance for the on value. Empty colors inherit the theme. */
	SwitchValueOverrides onOverrides{};
	/** Cursor requested while this enabled control is hovered. Empty inherits the active theme. */
	std::optional<CursorType> cursor = std::nullopt;
	/** Priority used to arbitrate this control's cursor request. Empty inherits the active theme. */
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

/** Retained interaction state used by this standard element. */
using SwitchState = detail::boolean_control::State;

/** Build-only boolean switch rendered as a track and movable knob. */
struct Switch {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = SwitchParameters;
	/** Per-instance state owned and retained by ElementManager. */
	using State = SwitchState;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<Switch>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<Switch>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.switch");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL Switch";

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

		const auto& theme = context.uiManager.theme<FSELTheme>().switchTheme;
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

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr Switch kSwitch{};
static_assert(FlowElement<Switch>);
static_assert(DrawableFlowElement<Switch>);

} // namespace FlowUi::FSEL
