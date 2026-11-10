#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::FSEL {

/** Axis along which dragging changes the target extent. */
enum class SplitterAxis {
	/** Represent values or resizing along the horizontal axis. */
	Horizontal,
	/** Represent values or resizing along the vertical axis. */
	Vertical
};

/** Position of the handle relative to the element whose extent it changes. */
enum class SplitterPosition {
	/** Handle precedes the target; positive-axis pointer movement reduces the extent. */
	Leading,
	/** Handle follows the target; positive-axis pointer movement increases the extent. */
	Trailing
};

/**
 * Per-invocation SplitterHandle configuration. Borrowed models and text must remain valid through
 * the frame; empty visual overrides inherit the active FSELTheme.
 */
struct SplitterHandleParameters {
	/** Axis along which the control represents its value or extent. */
	SplitterAxis axis = SplitterAxis::Horizontal;
	/** Side of the resized target occupied by the handle; determines drag direction. */
	SplitterPosition position = SplitterPosition::Trailing;

	/** Borrowed value owned by the caller. Null disables interaction. */
	float* targetExtent = nullptr;
	/** Minimum permitted target extent in layout pixels. */
	float minExtent = 0.0f;
	/** Maximum permitted target extent in layout pixels. */
	float maxExtent = 1080.0f;
	/** Visible bar thickness in layout pixels. */
	float thickness = 4.0f;
	/** Interactive thickness perpendicular to the main axis, in layout pixels. */
	float hitThickness = 10.0f;

	// Optional theme overrides
	/** Surface background color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	/** Adjacent surfaces continuing beneath the resize target on either side of the line. */
	std::optional<Clay_Color> leadingSurfaceColor = std::nullopt;
	/** Color of the adjacent surface after the resize line. Empty inherits the active theme. */
	std::optional<Clay_Color> trailingSurfaceColor = std::nullopt;
	/** Hovered handle background color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> hoverColor = std::nullopt;
	/** Dragged handle background color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> draggingColor = std::nullopt;
	/** Border color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> borderColor = std::nullopt;
	/** Per-edge border widths in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	/** Corner radii in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
	/** Cursor requested for a horizontal resize. Empty inherits the active theme. */
	std::optional<CursorType> horizontalCursor = std::nullopt;
	/** Cursor requested for a vertical resize. Empty inherits the active theme. */
	std::optional<CursorType> verticalCursor = std::nullopt;
	/** Priority used to arbitrate this control's cursor request. Empty inherits the active theme. */
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

/** Per-instance interaction state maintained by the element and retained by ElementManager. */
struct SplitterHandleState {
	/** Whether a pointer drag is currently active. */
	bool isDragging = false;
	/** Pointer position along the active axis at press, in layout pixels. */
	float pointerAtPress = 0.0f;
	/** Target extent captured at press, in layout pixels. */
	float extentAtPress = 0.0f;
};

/** Build-only draggable separator that updates a borrowed application extent. */
struct SplitterHandle {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = SplitterHandleParameters;
	/** Per-instance state owned and retained by ElementManager. */
	using State = SplitterHandleState;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<SplitterHandle>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<SplitterHandle>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.splitter-handle");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL SplitterHandle";

	/**
	 * ElementBuilder hover hook; requests the enabled control cursor. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onHovered(InteractionContext& context) {
		if (context.params.targetExtent) {
			requestResizeCursor(context);
		}
	}

	/**
	 * ElementBuilder press hook; begins the enabled control interaction. @param context Borrowed
	 * context valid for this callback only.
	 */
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

	/**
	 * ElementBuilder logic hook; reconciles retained interaction state with the current input.
	 * @param context Borrowed context valid for this callback only.
	 */
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

	/**
	 * ElementBuilder draw hook; emits this control and its children for the active frame. @param
	 * context Borrowed context valid for this callback only.
	 */
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
		const bool separate_surfaces = context.params.leadingSurfaceColor.has_value() ||
									   context.params.trailingSurfaceColor.has_value();
		root.layout.layoutDirection = context.params.axis == SplitterAxis::Horizontal
										  ? CLAY_LEFT_TO_RIGHT
										  : CLAY_TOP_TO_BOTTOM;
		if (state.isDragging && !separate_surfaces) {
			root.backgroundColor =
				context.params.draggingColor.value_or(splitterTheme.draggingColor);
		} else if (isHovered && !separate_surfaces) {
			root.backgroundColor = context.params.hoverColor.value_or(splitterTheme.hoverColor);
		}

		Clay_ElementDeclaration visual{};
		visual.layout.sizing = {
			.width = context.params.axis == SplitterAxis::Horizontal ? CLAY_SIZING_FIXED(thickness)
																	 : CLAY_SIZING_GROW(0),
			.height = context.params.axis == SplitterAxis::Vertical ? CLAY_SIZING_FIXED(thickness)
																	: CLAY_SIZING_GROW(0),
		};
		visual.backgroundColor =
			context.params.backgroundColor.value_or(splitterTheme.backgroundColor);
		if (separate_surfaces && state.isDragging)
			visual.backgroundColor =
				context.params.draggingColor.value_or(splitterTheme.draggingColor);
		else if (separate_surfaces && isHovered)
			visual.backgroundColor = context.params.hoverColor.value_or(splitterTheme.hoverColor);
		visual.cornerRadius = context.params.cornerRadius.value_or(splitterTheme.cornerRadius);
		visual.border = {.color = context.params.borderColor.value_or(splitterTheme.borderColor),
						 .width = context.params.borderWidth.value_or(splitterTheme.borderWidth)};

		const auto surface = [&](Clay_Color color) {
			Clay_ElementDeclaration declaration{};
			const float extent = (hitThickness - thickness) * 0.5f;
			declaration.layout.sizing = {
				.width = context.params.axis == SplitterAxis::Horizontal ? CLAY_SIZING_FIXED(extent)
																		 : CLAY_SIZING_GROW(0),
				.height = context.params.axis == SplitterAxis::Vertical ? CLAY_SIZING_FIXED(extent)
																		: CLAY_SIZING_GROW(0)};
			declaration.backgroundColor = color;
			return declaration;
		};
		CLAY(rootId, root) {
			if (separate_surfaces) {
				CLAY(context.clayID("leading-surface"),
					 surface(context.params.leadingSurfaceColor.value_or(Clay_Color{})));
			}
			CLAY(context.clayID("visual"), visual);
			if (separate_surfaces) {
				CLAY(context.clayID("trailing-surface"),
					 surface(context.params.trailingSurfaceColor.value_or(Clay_Color{})));
			}
		}
	}

private:
	static float pointerPosition(SplitterAxis axis, const FrameInput& input) {
		return axis == SplitterAxis::Horizontal ? input.mouseX : input.mouseY;
	}

	static void requestResizeCursor(InteractionContext& context) {
		const auto& theme = context.uiManager.theme<FSELTheme>();
		const auto& splitterTheme = theme.splitterHandleTheme;
		const CursorType cursor =
			context.params.axis == SplitterAxis::Horizontal
				? context.params.horizontalCursor.value_or(splitterTheme.horizontalResizeCursor)
				: context.params.verticalCursor.value_or(splitterTheme.verticalResizeCursor);
		context.uiManager.requestCursor(
			cursor, context.params.cursorPriority.value_or(splitterTheme.cursorPriority));
	}
};

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr SplitterHandle kSplitterHandle{};
static_assert(FlowElement<SplitterHandle>);

} // namespace FlowUi::FSEL
