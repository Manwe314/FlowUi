#pragma once

#include "FSEL/SelectableSurfaceStyle.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::FSEL::detail::selectable_surface {

struct State {
	bool isArmed = false;
	bool mouseUpObserved = false;
};

enum class VisualState {
	Idle,
	Hovered,
	Pressed,
	Selected,
	Disabled,
	SelectedDisabled,
};

inline void clearInteraction(State& state) {
	state.isArmed = false;
	state.mouseUpObserved = false;
}

template <typename Context>
void onPressed(Context& context, bool canSelect) {
	auto& state = context.state();
	if (!canSelect) {
		clearInteraction(state);
		return;
	}

	state.isArmed = true;
	state.mouseUpObserved = false;
}

template <typename Context>
[[nodiscard]] bool onReleased(Context& context, bool canSelect) {
	auto& state = context.state();
	if (!state.isArmed) {
		return false;
	}

	clearInteraction(state);
	return canSelect;
}

template <typename Context>
void runLogic(Context& context, bool canSelect) {
	auto& state = context.state();
	if (!state.isArmed) {
		return;
	}
	if (!canSelect) {
		clearInteraction(state);
		return;
	}

	const FrameInput& input = context.uiManager.getCurrentFrameInput();
	if (input.mouseDown[0]) {
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

template <typename Context>
[[nodiscard]] VisualState resolveVisualState(
	Context& context,
	bool selected,
	bool enabled) {
	if (!enabled) {
		return selected ? VisualState::SelectedDisabled : VisualState::Disabled;
	}
	if (selected) {
		return VisualState::Selected;
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

inline FSELSelectableSurfaceStateTheme applyOverrides(
	FSELSelectableSurfaceStateTheme appearance,
	const SelectableSurfaceStateOverrides& overrides) {
	appearance.backgroundColor = overrides.backgroundColor.value_or(
		appearance.backgroundColor);
	appearance.borderColor = overrides.borderColor.value_or(
		appearance.borderColor);
	return appearance;
}

template <typename Context>
[[nodiscard]] FSELSelectableSurfaceStateTheme resolveAppearance(
	Context& context,
	bool selected,
	bool enabled,
	const SelectableSurfaceStyle& style,
	const FSELSelectableSurfaceTheme& theme) {
	switch (resolveVisualState(context, selected, enabled)) {
	case VisualState::Idle:
		return applyOverrides(theme.idle, style.idleOverrides);
	case VisualState::Hovered:
		return applyOverrides(theme.hovered, style.hoveredOverrides);
	case VisualState::Pressed:
		return applyOverrides(theme.pressed, style.pressedOverrides);
	case VisualState::Selected:
		return applyOverrides(theme.selected, style.selectedOverrides);
	case VisualState::Disabled:
		return applyOverrides(theme.disabled, style.disabledOverrides);
	case VisualState::SelectedDisabled:
		return applyOverrides(
			theme.selectedDisabled,
			style.selectedDisabledOverrides);
	}
	return applyOverrides(theme.idle, style.idleOverrides);
}

template <typename Context>
[[nodiscard]] Clay_ElementDeclaration makeDeclaration(
	Context& context,
	bool selected,
	bool enabled,
	const SelectableSurfaceStyle& style) {
	const auto& theme = context.uiManager
		.template theme<FSELTheme>()
		.selectableSurfaceTheme;
	const FSELSelectableSurfaceStateTheme appearance = resolveAppearance(
		context,
		selected,
		enabled,
		style,
		theme);

	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = style.sizing;
	declaration.layout.padding = style.padding.value_or(theme.padding);
	declaration.layout.childAlignment =
		style.childAlignment.value_or(theme.childAlignment);
	declaration.layout.layoutDirection =
		style.layoutDirection.value_or(theme.layoutDirection);
	declaration.layout.childGap = style.contentGap.value_or(theme.contentGap);
	declaration.backgroundColor = appearance.backgroundColor;
	declaration.cornerRadius = style.cornerRadius.value_or(theme.cornerRadius);
	declaration.border = {
		.color = appearance.borderColor,
		.width = style.borderWidth.value_or(theme.borderWidth),
	};
	return declaration;
}

} // namespace FlowUi::FSEL::detail::selectable_surface
