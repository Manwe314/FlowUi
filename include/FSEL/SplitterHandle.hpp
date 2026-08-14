#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::FSEL {

/** Axis along which dragging changes the target extent. */
enum class SplitterAxis {
	Horizontal,
	Vertical
};

/** Position of the handle relative to the element whose extent it changes. */
enum class SplitterPosition {
	Leading,
	Trailing
};

struct SplitterHandleParameters {
	SplitterAxis axis = SplitterAxis::Horizontal;
	SplitterPosition position = SplitterPosition::Trailing;

	/** Borrowed value owned by the caller. Null disables interaction. */
	float* targetExtent = nullptr;
	float minExtent = 0.0f;
	float maxExtent = 1080.0f;
	float thickness = 4.0f;
	float hitThickness = 10.0f;

	// Optional theme overrides
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	std::optional<Clay_Color> hoverColor = std::nullopt;
	std::optional<Clay_Color> draggingColor = std::nullopt;
	std::optional<Clay_Color> borderColor = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
	std::optional<CursorType> horizontalCursor = std::nullopt;
	std::optional<CursorType> verticalCursor = std::nullopt;
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

struct SplitterHandleState {
	bool isDragging = false;
	float pointerAtPress = 0.0f;
	float extentAtPress = 0.0f;
};

struct SplitterHandle {
	using Parameters = SplitterHandleParameters;
	using State = SplitterHandleState;
	using BuildContext = ElementBuildContext<SplitterHandle>;
	using InteractionContext = ElementInteractionContext<SplitterHandle>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.splitter-handle");
	static constexpr std::string_view debugName = "FSEL SplitterHandle";

	static void onHovered(InteractionContext& context) {
		if (context.params.targetExtent) {
			requestResizeCursor(context);
		}
	}

	static void onPressed(InteractionContext& context) {
		auto& state = context.state();
		if (!context.params.targetExtent) {
			state.isDragging = false;
			return;
		}

		const FrameInput& input = context.uiManager.getCurrentFrameInput();
		state.extentAtPress = *context.params.targetExtent;
		state.pointerAtPress = pointerPosition(context.params.axis, input);
		state.isDragging = true;
	}

	static void runLogic(InteractionContext& context) {
		auto& state = context.state();
		if (!state.isDragging) {
			return;
		}
		if (!context.params.targetExtent) {
			state.isDragging = false;
			return;
		}

		const FrameInput& input = context.uiManager.getCurrentFrameInput();
		if (!input.mouseDown[0]) {
			state.isDragging = false;
			return;
		}

		requestResizeCursor(context);
		const float delta =
			pointerPosition(context.params.axis, input) - state.pointerAtPress;
		const float signedDelta =
			context.params.position == SplitterPosition::Trailing ? delta : -delta;
		const auto [minimumExtent, maximumExtent] = std::minmax(
			context.params.minExtent,
			context.params.maxExtent);
		*context.params.targetExtent = std::clamp(
			state.extentAtPress + signedDelta,
			minimumExtent,
			maximumExtent);
	}

	static void buildElement(BuildContext& context) {
		const auto& theme = context.uiManager.theme<FSELTheme>();
		const auto& splitterTheme = theme.splitterHandleTheme;
		const auto& state = context.state();
		const float thickness = std::max(context.params.thickness, 1.0f);
		const float hitThickness = std::max(context.params.hitThickness, thickness);
		const Clay_ElementId rootId = context.clayID();
		const bool isHovered = context.params.targetExtent &&
			context.uiManager.getPreviousFramesInteraction().isHovered(rootId);

		Clay_ElementDeclaration root{};
		root.layout.sizing = {
			.width = context.params.axis == SplitterAxis::Horizontal
				? CLAY_SIZING_FIXED(hitThickness)
				: CLAY_SIZING_GROW(0),
			.height = context.params.axis == SplitterAxis::Vertical
				? CLAY_SIZING_FIXED(hitThickness)
				: CLAY_SIZING_GROW(0),
		};
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_CENTER,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		if (state.isDragging) {
			root.backgroundColor = context.params.draggingColor.value_or(
				splitterTheme.draggingColor);
		} else if (isHovered) {
			root.backgroundColor = context.params.hoverColor.value_or(
				splitterTheme.hoverColor);
		}

		Clay_ElementDeclaration visual{};
		visual.layout.sizing = {
			.width = context.params.axis == SplitterAxis::Horizontal
				? CLAY_SIZING_FIXED(thickness)
				: CLAY_SIZING_GROW(0),
			.height = context.params.axis == SplitterAxis::Vertical
				? CLAY_SIZING_FIXED(thickness)
				: CLAY_SIZING_GROW(0),
		};
		visual.backgroundColor = context.params.backgroundColor.value_or(
			splitterTheme.backgroundColor);
		visual.cornerRadius = context.params.cornerRadius.value_or(
			splitterTheme.cornerRadius);
		visual.border = {
			.color = context.params.borderColor.value_or(
				splitterTheme.borderColor),
			.width = context.params.borderWidth.value_or(
				splitterTheme.borderWidth)
		};

		CLAY(rootId, root) {
			CLAY(context.clayID("visual"), visual);
		}
	}

private:
	static float pointerPosition(SplitterAxis axis, const FrameInput& input) {
		return axis == SplitterAxis::Horizontal ? input.mouseX : input.mouseY;
	}

	static void requestResizeCursor(InteractionContext& context) {
		const auto& theme = context.uiManager.theme<FSELTheme>();
		const auto& splitterTheme = theme.splitterHandleTheme;
		const CursorType cursor = context.params.axis == SplitterAxis::Horizontal
			? context.params.horizontalCursor.value_or(
				splitterTheme.horizontalResizeCursor)
			: context.params.verticalCursor.value_or(
				splitterTheme.verticalResizeCursor);
		context.uiManager.requestCursor(
			cursor,
			context.params.cursorPriority.value_or(splitterTheme.cursorPriority));
	}
};

inline constexpr SplitterHandle kSplitterHandle{};
static_assert(FlowElement<SplitterHandle>);

} // namespace FlowUi::FSEL
