#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FSEL/internal/SliderMath.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::FSEL {

enum class SliderAxis {
	Horizontal,
	Vertical,
};

enum class SliderPressBehavior {
	/** Pressing anywhere moves the thumb to the pointer before dragging. */
	JumpToPointer,
	/** Pressing anywhere starts a relative drag from the current value. */
	DragFromCurrent,
};

enum class SliderVisualParts : uint8_t {
	None = 0,
	Track = 1u << 0u,
	Fill = 1u << 1u,
	Thumb = 1u << 2u,
	All = Track | Fill | Thumb,
};

[[nodiscard]] constexpr SliderVisualParts operator|(
	SliderVisualParts left,
	SliderVisualParts right) noexcept {
	return static_cast<SliderVisualParts>(
		static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
}

[[nodiscard]] constexpr SliderVisualParts operator&(
	SliderVisualParts left,
	SliderVisualParts right) noexcept {
	return static_cast<SliderVisualParts>(
		static_cast<uint8_t>(left) & static_cast<uint8_t>(right));
}

[[nodiscard]] constexpr bool hasSliderVisualPart(
	SliderVisualParts parts,
	SliderVisualParts part) noexcept {
	return (parts & part) != SliderVisualParts::None;
}

struct SliderStateOverrides {
	std::optional<Clay_Color> trackColor = std::nullopt;
	std::optional<Clay_Color> trackBorderColor = std::nullopt;
	std::optional<Clay_Color> fillColor = std::nullopt;
	std::optional<Clay_Color> fillBorderColor = std::nullopt;
	std::optional<Clay_Color> thumbColor = std::nullopt;
	std::optional<Clay_Color> thumbBorderColor = std::nullopt;
};

struct SliderParameters {
	SliderAxis axis = SliderAxis::Horizontal;
	SliderPressBehavior pressBehavior = SliderPressBehavior::JumpToPointer;
	bool inverted = false;

	/** Borrowed authoritative value. Null disables interaction. */
	double* value = nullptr;
	double minimum = 0.0;
	double maximum = 1.0;
	/** Optional snapping interval. For example, 0.01 produces hundredths. */
	std::optional<double> roundingStep = std::nullopt;
	bool enabled = true;
	ActionCall onBegin{};
	ActionCall onChanged{};
	ActionCall onCommit{};

	SliderVisualParts visualParts = SliderVisualParts::All;

	// Optional theme overrides
	std::optional<float> length = std::nullopt;
	std::optional<float> hitThickness = std::nullopt;
	std::optional<float> trackThickness = std::nullopt;
	std::optional<float> thumbLength = std::nullopt;
	std::optional<float> thumbThickness = std::nullopt;
	std::optional<float> trackRoundness = std::nullopt;
	std::optional<float> thumbRoundness = std::nullopt;
	std::optional<Clay_BorderWidth> trackBorderWidth = std::nullopt;
	std::optional<Clay_BorderWidth> fillBorderWidth = std::nullopt;
	std::optional<Clay_BorderWidth> thumbBorderWidth = std::nullopt;
	SliderStateOverrides idleOverrides{};
	SliderStateOverrides hoveredOverrides{};
	SliderStateOverrides draggingOverrides{};
	SliderStateOverrides disabledOverrides{};
	std::optional<CursorType> cursor = std::nullopt;
	std::optional<CursorType> draggingCursor = std::nullopt;
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

struct SliderState {
	bool isDragging = false;
	float pointerAtPress = 0.0f;
	double valueAtPress = 0.0;
};

/** Build-only controlled slider with independently optional track, fill, and thumb. */
struct Slider {
	using Parameters = SliderParameters;
	using State = SliderState;
	using BuildContext = ElementBuildContext<Slider>;
	using InteractionContext = ElementInteractionContext<Slider>;

	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.slider");
	static constexpr std::string_view debugName = "FSEL Slider";

	static void onHovered(InteractionContext& context) {
		if (isEnabled(context.params)) {
			requestCursor(context, false);
		}
	}

	static void onPressed(InteractionContext& context) {
		auto& state = context.state();
		if (!isEnabled(context.params)) {
			state.isDragging = false;
			return;
		}

		const detail::slider::Bounds bounds = resolveBounds(context.params);
		const FrameInput& input = context.uiManager.getCurrentFrameInput();
		state.isDragging = true;
		state.pointerAtPress = pointerPosition(context.params.axis, input);
		state.valueAtPress = detail::slider::snapValue(
			*context.params.value,
			bounds,
			context.params.roundingStep);

		invokeIfBound(context, context.params.onBegin);
		if (context.params.pressBehavior == SliderPressBehavior::JumpToPointer) {
			updateFromPointer(context, bounds);
		}
	}

	static void runLogic(InteractionContext& context) {
		auto& state = context.state();
		if (!state.isDragging) {
			return;
		}
		if (!isEnabled(context.params)) {
			state.isDragging = false;
			return;
		}

		const FrameInput& input = context.uiManager.getCurrentFrameInput();
		if (!input.mouseDown[0]) {
			state.isDragging = false;
			invokeIfBound(context, context.params.onCommit);
			return;
		}

		requestCursor(context, true);
		updateFromPointer(context, resolveBounds(context.params));
	}

	static void buildElement(BuildContext& context) {
		const auto& theme = context.uiManager.theme<FSELTheme>().sliderTheme;
		const Geometry geometry = resolveGeometry(context.params, theme);
		const FSELSliderStateTheme appearance = resolveAppearance(context, theme);
		const detail::slider::Bounds bounds = resolveBounds(context.params);
		const double ratio = context.params.value
			? detail::slider::normalizedRatio(*context.params.value, bounds)
			: 0.0;
		const bool minimumAtLeading = valueDirection(context.params) > 0.0;
		const float leadingRatio = minimumAtLeading
			? static_cast<float>(ratio)
			: 1.0f - static_cast<float>(ratio);
		const float travel = geometry.length - geometry.thumbLength;
		const float thumbLeadingOffset = geometry.thumbLength * 0.5f +
			leadingRatio * travel;
		const float fillLength = static_cast<float>(ratio) * travel;

		Clay_ElementDeclaration root{};
		root.layout.sizing = {
			.width = context.params.axis == SliderAxis::Horizontal
				? CLAY_SIZING_FIXED(geometry.length)
				: CLAY_SIZING_FIXED(geometry.crossSize),
			.height = context.params.axis == SliderAxis::Vertical
				? CLAY_SIZING_FIXED(geometry.length)
				: CLAY_SIZING_FIXED(geometry.crossSize),
		};

		CLAY(context.clayID(), root) {
			if (hasSliderVisualPart(
				context.params.visualParts,
				SliderVisualParts::Track)) {
				emitTrack(context, geometry, appearance, theme);
			}
			if (fillLength > 0.0f && hasSliderVisualPart(
				context.params.visualParts,
				SliderVisualParts::Fill)) {
				emitFill(
					context,
					geometry,
					appearance,
					theme,
					fillLength,
					minimumAtLeading);
			}
			if (hasSliderVisualPart(
				context.params.visualParts,
				SliderVisualParts::Thumb)) {
				emitThumb(
					context,
					geometry,
					appearance,
					theme,
					thumbLeadingOffset);
			}
		}
	}

private:
	struct Geometry {
		float length = 1.0f;
		float crossSize = 1.0f;
		float trackThickness = 1.0f;
		float thumbLength = 1.0f;
		float thumbThickness = 1.0f;
		float trackRoundness = 0.0f;
		float thumbRoundness = 0.0f;
	};

	static bool isEnabled(const SliderParameters& params) {
		return params.enabled && params.value != nullptr;
	}

	static detail::slider::Bounds resolveBounds(const SliderParameters& params) {
		return detail::slider::normalizeBounds(params.minimum, params.maximum);
	}

	static Geometry resolveGeometry(
		const SliderParameters& params,
		const FSELSliderTheme& theme) {
		Geometry geometry{};
		geometry.trackThickness = std::max(
			params.trackThickness.value_or(theme.trackThickness),
			1.0f);
		geometry.thumbLength = std::max(
			params.thumbLength.value_or(theme.thumbLength),
			1.0f);
		geometry.thumbThickness = std::max(
			params.thumbThickness.value_or(theme.thumbThickness),
			1.0f);
		geometry.length = std::max(
			params.length.value_or(theme.length),
			geometry.thumbLength + 1.0f);
		geometry.crossSize = std::max({
			params.hitThickness.value_or(theme.hitThickness),
			geometry.trackThickness,
			geometry.thumbThickness,
			1.0f,
		});
		geometry.trackRoundness = detail::slider::clampRoundness(
			params.trackRoundness.value_or(theme.trackRoundness));
		geometry.thumbRoundness = detail::slider::clampRoundness(
			params.thumbRoundness.value_or(theme.thumbRoundness));
		return geometry;
	}

	static double valueDirection(const SliderParameters& params) {
		double direction = params.axis == SliderAxis::Horizontal ? 1.0 : -1.0;
		return params.inverted ? -direction : direction;
	}

	static float pointerPosition(SliderAxis axis, const FrameInput& input) {
		return axis == SliderAxis::Horizontal ? input.mouseX : input.mouseY;
	}

	static double pointerRatio(
		InteractionContext& context,
		const Geometry& geometry) {
		const Clay_ElementData rootData = Clay_GetElementData(context.clayID());
		if (!rootData.found) {
			return detail::slider::normalizedRatio(
				*context.params.value,
				resolveBounds(context.params));
		}

		const bool horizontal = context.params.axis == SliderAxis::Horizontal;
		const float origin = horizontal
			? rootData.boundingBox.x
			: rootData.boundingBox.y;
		const float actualLength = horizontal
			? rootData.boundingBox.width
			: rootData.boundingBox.height;
		const float thumbLength = std::min(geometry.thumbLength, actualLength);
		const float travel = std::max(actualLength - thumbLength, 1.0f);
		const float pointer = pointerPosition(
			context.params.axis,
			context.uiManager.getCurrentFrameInput());
		return std::clamp(
			static_cast<double>((pointer - origin - thumbLength * 0.5f) / travel),
			0.0,
			1.0);
	}

	static void updateFromPointer(
		InteractionContext& context,
		detail::slider::Bounds bounds) {
		const auto& theme = context.uiManager.theme<FSELTheme>().sliderTheme;
		const Geometry geometry = resolveGeometry(context.params, theme);
		double ratio = 0.0;
		if (context.params.pressBehavior == SliderPressBehavior::JumpToPointer) {
			ratio = pointerRatio(context, geometry);
			if (valueDirection(context.params) < 0.0) {
				ratio = 1.0 - ratio;
			}
		} else {
			const Clay_ElementData rootData = Clay_GetElementData(context.clayID());
			const bool horizontal = context.params.axis == SliderAxis::Horizontal;
			const float actualLength = rootData.found
				? (horizontal
					? rootData.boundingBox.width
					: rootData.boundingBox.height)
				: geometry.length;
			const double travel = std::max(
				static_cast<double>(actualLength - geometry.thumbLength),
				1.0);
			const float pointer = pointerPosition(
				context.params.axis,
				context.uiManager.getCurrentFrameInput());
			const double deltaRatio =
				static_cast<double>(pointer - context.state().pointerAtPress) /
				travel * valueDirection(context.params);
			ratio = detail::slider::normalizedRatio(
				context.state().valueAtPress,
				bounds) + deltaRatio;
		}

		const double nextValue = detail::slider::snapValue(
			detail::slider::valueFromRatio(ratio, bounds),
			bounds,
			context.params.roundingStep);
		if (nextValue == *context.params.value) {
			return;
		}
		*context.params.value = nextValue;
		invokeIfBound(context, context.params.onChanged);
	}

	static void invokeIfBound(InteractionContext& context, ActionCall action) {
		if (action) {
			(void)context.invoke(action);
		}
	}

	static void requestCursor(InteractionContext& context, bool dragging) {
		const auto& theme = context.uiManager.theme<FSELTheme>().sliderTheme;
		const CursorType cursor = dragging
			? context.params.draggingCursor.value_or(theme.draggingCursor)
			: context.params.cursor.value_or(theme.cursor);
		context.uiManager.requestCursor(
			cursor,
			context.params.cursorPriority.value_or(theme.cursorPriority));
	}

	static FSELSliderStateTheme applyOverrides(
		FSELSliderStateTheme appearance,
		const SliderStateOverrides& overrides) {
		appearance.trackColor = overrides.trackColor.value_or(
			appearance.trackColor);
		appearance.trackBorderColor = overrides.trackBorderColor.value_or(
			appearance.trackBorderColor);
		appearance.fillColor = overrides.fillColor.value_or(
			appearance.fillColor);
		appearance.fillBorderColor = overrides.fillBorderColor.value_or(
			appearance.fillBorderColor);
		appearance.thumbColor = overrides.thumbColor.value_or(
			appearance.thumbColor);
		appearance.thumbBorderColor = overrides.thumbBorderColor.value_or(
			appearance.thumbBorderColor);
		return appearance;
	}

	static FSELSliderStateTheme resolveAppearance(
		BuildContext& context,
		const FSELSliderTheme& theme) {
		if (!isEnabled(context.params)) {
			return applyOverrides(theme.disabled, context.params.disabledOverrides);
		}
		if (context.state().isDragging) {
			return applyOverrides(theme.dragging, context.params.draggingOverrides);
		}
		if (context.uiManager.getPreviousFramesInteraction().isHovered(
			context.clayID())) {
			return applyOverrides(theme.hovered, context.params.hoveredOverrides);
		}
		return applyOverrides(theme.idle, context.params.idleOverrides);
	}

	static Clay_FloatingElementConfig centeredFloating() {
		return {
			.zIndex = 0,
			.attachPoints = {
				.element = CLAY_ATTACH_POINT_CENTER_CENTER,
				.parent = CLAY_ATTACH_POINT_CENTER_CENTER,
			},
			.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
			.attachTo = CLAY_ATTACH_TO_PARENT,
		};
	}

	static void emitTrack(
		BuildContext& context,
		const Geometry& geometry,
		const FSELSliderStateTheme& appearance,
		const FSELSliderTheme& theme) {
		const float travel = geometry.length - geometry.thumbLength;
		Clay_ElementDeclaration track{};
		track.layout.sizing = {
			.width = context.params.axis == SliderAxis::Horizontal
				? CLAY_SIZING_FIXED(travel)
				: CLAY_SIZING_FIXED(geometry.trackThickness),
			.height = context.params.axis == SliderAxis::Vertical
				? CLAY_SIZING_FIXED(travel)
				: CLAY_SIZING_FIXED(geometry.trackThickness),
		};
		track.backgroundColor = appearance.trackColor;
		track.cornerRadius = CLAY_CORNER_RADIUS(
			geometry.trackThickness * 0.5f * geometry.trackRoundness);
		track.border = {
			.color = appearance.trackBorderColor,
			.width = context.params.trackBorderWidth.value_or(
				theme.trackBorderWidth),
		};
		track.floating = centeredFloating();
		CLAY(context.clayID("track"), track);
	}

	static void emitFill(
		BuildContext& context,
		const Geometry& geometry,
		const FSELSliderStateTheme& appearance,
		const FSELSliderTheme& theme,
		float fillLength,
		bool minimumAtLeading) {
		Clay_ElementDeclaration fill{};
		fill.layout.sizing = {
			.width = context.params.axis == SliderAxis::Horizontal
				? CLAY_SIZING_FIXED(fillLength)
				: CLAY_SIZING_FIXED(geometry.trackThickness),
			.height = context.params.axis == SliderAxis::Vertical
				? CLAY_SIZING_FIXED(fillLength)
				: CLAY_SIZING_FIXED(geometry.trackThickness),
		};
		fill.backgroundColor = appearance.fillColor;
		fill.cornerRadius = CLAY_CORNER_RADIUS(
			geometry.trackThickness * 0.5f * geometry.trackRoundness);
		fill.border = {
			.color = appearance.fillBorderColor,
			.width = context.params.fillBorderWidth.value_or(
				theme.fillBorderWidth),
		};
		fill.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
		fill.floating.attachTo = CLAY_ATTACH_TO_PARENT;

		const float endpointOffset = geometry.thumbLength * 0.5f;
		if (context.params.axis == SliderAxis::Horizontal) {
			fill.floating.offset.x = minimumAtLeading
				? endpointOffset
				: -endpointOffset;
			fill.floating.attachPoints = minimumAtLeading
				? Clay_FloatingAttachPoints{
					.element = CLAY_ATTACH_POINT_LEFT_CENTER,
					.parent = CLAY_ATTACH_POINT_LEFT_CENTER,
				}
				: Clay_FloatingAttachPoints{
					.element = CLAY_ATTACH_POINT_RIGHT_CENTER,
					.parent = CLAY_ATTACH_POINT_RIGHT_CENTER,
				};
		} else {
			fill.floating.offset.y = minimumAtLeading
				? endpointOffset
				: -endpointOffset;
			fill.floating.attachPoints = minimumAtLeading
				? Clay_FloatingAttachPoints{
					.element = CLAY_ATTACH_POINT_CENTER_TOP,
					.parent = CLAY_ATTACH_POINT_CENTER_TOP,
				}
				: Clay_FloatingAttachPoints{
					.element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
					.parent = CLAY_ATTACH_POINT_CENTER_BOTTOM,
				};
		}
		CLAY(context.clayID("fill"), fill);
	}

	static void emitThumb(
		BuildContext& context,
		const Geometry& geometry,
		const FSELSliderStateTheme& appearance,
		const FSELSliderTheme& theme,
		float leadingOffset) {
		Clay_ElementDeclaration thumb{};
		thumb.layout.sizing = {
			.width = context.params.axis == SliderAxis::Horizontal
				? CLAY_SIZING_FIXED(geometry.thumbLength)
				: CLAY_SIZING_FIXED(geometry.thumbThickness),
			.height = context.params.axis == SliderAxis::Vertical
				? CLAY_SIZING_FIXED(geometry.thumbLength)
				: CLAY_SIZING_FIXED(geometry.thumbThickness),
		};
		thumb.backgroundColor = appearance.thumbColor;
		thumb.cornerRadius = CLAY_CORNER_RADIUS(
			std::min(geometry.thumbLength, geometry.thumbThickness) *
			0.5f * geometry.thumbRoundness);
		thumb.border = {
			.color = appearance.thumbBorderColor,
			.width = context.params.thumbBorderWidth.value_or(
				theme.thumbBorderWidth),
		};
		thumb.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
		thumb.floating.attachTo = CLAY_ATTACH_TO_PARENT;
		if (context.params.axis == SliderAxis::Horizontal) {
			thumb.floating.offset.x = leadingOffset;
			thumb.floating.attachPoints = {
				.element = CLAY_ATTACH_POINT_CENTER_CENTER,
				.parent = CLAY_ATTACH_POINT_LEFT_CENTER,
			};
		} else {
			thumb.floating.offset.y = leadingOffset;
			thumb.floating.attachPoints = {
				.element = CLAY_ATTACH_POINT_CENTER_CENTER,
				.parent = CLAY_ATTACH_POINT_CENTER_TOP,
			};
		}
		CLAY(context.clayID("thumb"), thumb);
	}
};

inline constexpr Slider kSlider{};
static_assert(FlowElement<Slider>);
static_assert(DrawableFlowElement<Slider>);

} // namespace FlowUi::FSEL
