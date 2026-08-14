#pragma once

#include <algorithm>
#include <cmath>

#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::FSEL::detail::boolean_control {

struct State {
	bool isArmed = false;
	bool mouseUpObserved = false;
};

enum class VisualState {
	Idle,
	Hovered,
	Pressed,
	Disabled,
};

template <typename Element>
[[nodiscard]] bool isEnabled(
	const ElementInteractionContext<Element>& context,
	ActionCall onToggle) {
	return context.params.enabled &&
		context.actionAvailability(onToggle).enabled;
}

template <typename Element>
[[nodiscard]] bool isEnabled(
	const ElementBuildContext<Element>& context,
	ActionCall onToggle) {
	return context.params.enabled &&
		context.uiManager.actions().availability(onToggle).enabled;
}

inline void clearInteraction(State& state) {
	state.isArmed = false;
	state.mouseUpObserved = false;
}

template <typename Element>
void onPressed(
	ElementInteractionContext<Element>& context,
	ActionCall onToggle) {
	auto& state = context.state();
	if (!isEnabled(context, onToggle)) {
		clearInteraction(state);
		return;
	}

	state.isArmed = true;
	state.mouseUpObserved = false;
}

template <typename Element>
void onReleased(
	ElementInteractionContext<Element>& context,
	ActionCall onToggle) {
	auto& state = context.state();
	if (!state.isArmed) {
		return;
	}

	const bool shouldToggle = isEnabled(context, onToggle);
	clearInteraction(state);
	if (shouldToggle) {
		(void)context.invoke(onToggle);
	}
}

template <typename Element>
void runLogic(
	ElementInteractionContext<Element>& context,
	ActionCall onToggle) {
	auto& state = context.state();
	if (!state.isArmed) {
		return;
	}
	if (!isEnabled(context, onToggle)) {
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

template <typename Element>
[[nodiscard]] VisualState resolveVisualState(
	ElementBuildContext<Element>& context,
	ActionCall onToggle) {
	if (!isEnabled(context, onToggle)) {
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

[[nodiscard]] inline float clampRoundness(float roundness) {
	if (!std::isfinite(roundness)) {
		return 0.0f;
	}
	return std::clamp(roundness, 0.0f, 1.0f);
}

} // namespace FlowUi::FSEL::detail::boolean_control
