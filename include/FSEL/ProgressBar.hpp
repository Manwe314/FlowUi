#pragma once

#include <algorithm>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FSEL/internal/BoundedValueMath.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::FSEL {

enum class ProgressBarAxis {
	Horizontal,
	Vertical,
};

struct ProgressBarParameters {
	ProgressBarAxis axis = ProgressBarAxis::Horizontal;
	bool inverted = false;

	/** Current read-only value snapshot supplied by the caller for this frame. */
	double value = 0.0;
	double minimum = 0.0;
	double maximum = 1.0;

	/** Empty uses the theme's fixed length and thickness for the selected axis. */
	std::optional<Clay_Sizing> sizing = std::nullopt;

	// Optional theme overrides
	std::optional<Clay_Color> baseColor = std::nullopt;
	std::optional<Clay_Color> borderColor = std::nullopt;
	std::optional<Clay_Color> fillColor = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
};

/** Stateless, build-only display of a caller-owned bounded value snapshot. */
struct ProgressBar {
	using Parameters = ProgressBarParameters;
	using BuildContext = ElementBuildContext<ProgressBar>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.progress-bar");
	static constexpr std::string_view debugName = "FSEL ProgressBar";

	static void buildElement(BuildContext& context) {
		const FSELProgressBarTheme& theme =
			context.uiManager.theme<FSELTheme>().progressBarTheme;
		const detail::bounded_value::Bounds bounds =
			detail::bounded_value::normalizeBounds(
				context.params.minimum,
				context.params.maximum);
		const float ratio = static_cast<float>(
			detail::bounded_value::normalizedRatio(
				context.params.value,
				bounds));
		const Clay_BorderWidth borderWidth =
			context.params.borderWidth.value_or(theme.borderWidth);
		const Clay_CornerRadius cornerRadius =
			context.params.cornerRadius.value_or(theme.cornerRadius);

		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing.value_or(
			defaultSizing(context.params.axis, theme));
		root.layout.padding = {
			.left = borderWidth.left,
			.right = borderWidth.right,
			.top = borderWidth.top,
			.bottom = borderWidth.bottom,
		};
		root.layout.childAlignment = childAlignment(
			context.params.axis,
			context.params.inverted);
		root.layout.layoutDirection =
			context.params.axis == ProgressBarAxis::Horizontal
				? CLAY_LEFT_TO_RIGHT
				: CLAY_TOP_TO_BOTTOM;
		root.backgroundColor = context.params.baseColor.value_or(
			theme.baseColor);
		root.cornerRadius = cornerRadius;
		root.clip = {
			.horizontal = true,
			.vertical = true,
		};
		root.border = {
			.color = context.params.borderColor.value_or(theme.borderColor),
			.width = borderWidth,
		};

		Clay_ElementDeclaration fill{};
		fill.layout.sizing = fillSizing(context.params.axis, ratio);
		fill.backgroundColor = context.params.fillColor.value_or(
			theme.fillColor);
		fill.cornerRadius = cornerRadius;

		CLAY(context.clayID(), root) {
			if (ratio > 0.0f) {
				CLAY(context.clayID("fill"), fill);
			}
		}
	}

private:
	static Clay_Sizing defaultSizing(
		ProgressBarAxis axis,
		const FSELProgressBarTheme& theme) {
		const float length = std::max(theme.length, 1.0f);
		const float thickness = std::max(theme.thickness, 1.0f);
		return {
			.width = axis == ProgressBarAxis::Horizontal
				? CLAY_SIZING_FIXED(length)
				: CLAY_SIZING_FIXED(thickness),
			.height = axis == ProgressBarAxis::Vertical
				? CLAY_SIZING_FIXED(length)
				: CLAY_SIZING_FIXED(thickness),
		};
	}

	static Clay_ChildAlignment childAlignment(
		ProgressBarAxis axis,
		bool inverted) {
		if (axis == ProgressBarAxis::Horizontal) {
			return {
				.x = inverted ? CLAY_ALIGN_X_RIGHT : CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_CENTER,
			};
		}
		return {
			.x = CLAY_ALIGN_X_CENTER,
			.y = inverted ? CLAY_ALIGN_Y_TOP : CLAY_ALIGN_Y_BOTTOM,
		};
	}

	static Clay_Sizing fillSizing(ProgressBarAxis axis, float ratio) {
		if (axis == ProgressBarAxis::Horizontal) {
			return {
				.width = CLAY_SIZING_PERCENT(ratio),
				.height = CLAY_SIZING_GROW(0),
			};
		}
		return {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_PERCENT(ratio),
		};
	}
};

inline constexpr ProgressBar kProgressBar{};
static_assert(FlowElement<ProgressBar>);
static_assert(DrawableFlowElement<ProgressBar>);

} // namespace FlowUi::FSEL
