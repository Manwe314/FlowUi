#pragma once

#include "devMode/devFlowElements/common.hpp"

inline double devSliderRoundTo2Decimals(double value) {
	double rounded = std::round(value * 100.0) / 100.0;
	if (std::fabs(rounded) < 0.005)
	{
		rounded = 0.0;
	}
	return rounded;
}

struct devSliderParams {
	double minValue = 0.0;
	double maxValue = 1.0;
	double value = 0.0;
	std::function<void(double)> onValueChangedCallback = nullptr;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(0);
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	uint16_t functionalWidthPx = 140;
	uint16_t handleWidthPx = 8;
	uint16_t trackHeightPx = 8;
	uint16_t handleHeightPx = 14;
	float trackCornerRadius = 5.0f;
	float handleCornerRadius = 4.0f;

	Clay_Color fillColor = FlowUi::Flow_Color("#4b8c5aff");
	Clay_Color unfillColor = FlowUi::Flow_Color("#2b3039ff");
	Clay_Color handleColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_Color handleHoverColor = FlowUi::Flow_Color("#adb3beff");
	Clay_Color handleActiveColor = FlowUi::Flow_Color("#d5d9e0ff");
};

struct devSliderState {
	bool initialized = false;
	bool dragging = false;
	float pressMouseX = 0.0f;
	double pressValue = 0.0;
	double currentValue = 0.0;
};

using DevSliderDef = FlowUi::ElementDefinition<
	devSliderParams,
	devSliderState,
	void,
	FLOW_DEF_ID("DevSlider"),
	true>;

inline const DevSliderDef kDevSlider = {
	+[](DevSliderDef::InteractionContext& context) {
		const Clay_ElementId barId = context.uiManager.toClayEID(context.createChildElementId("bar"));
		if (context.previousInteraction.isHovered(barId))
		{
			context.uiManager.requestCursor(FlowUi::CursorType::PointingHand);
		}
	},
	+[](DevSliderDef::InteractionContext& context) {
		const Clay_ElementId barId = context.uiManager.toClayEID(context.createChildElementId("bar"));
		if (!context.previousInteraction.isPressed(barId))
		{
			return;
		}

		devSliderState& state = DevSliderDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const double lower = std::min(context.params.minValue, context.params.maxValue);
		const double upper = std::max(context.params.minValue, context.params.maxValue);
		state.dragging = true;
		state.pressMouseX = context.uiManager.getCurrentFrameInput().mouseX;
		state.pressValue = std::clamp(state.currentValue, lower, upper);
	},
	nullptr,
	+[](DevSliderDef::InteractionContext& context) {
		devSliderState& state = DevSliderDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		state.dragging = false;
	},
	+[](DevSliderDef::InteractionContext& context) {
		devSliderState& state = DevSliderDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const FrameInput& input = context.uiManager.getCurrentFrameInput();
		if (!input.mouseDown[0])
		{
			state.dragging = false;
			return;
		}
		if (!state.dragging)
		{
			return;
		}

		const double lower = std::min(context.params.minValue, context.params.maxValue);
		const double upper = std::max(context.params.minValue, context.params.maxValue);
		const double range = upper - lower;
		if (range <= 0.0)
		{
			if (state.currentValue != lower)
			{
				state.currentValue = lower;
				if (context.params.onValueChangedCallback != nullptr)
				{
					context.params.onValueChangedCallback(state.currentValue);
				}
			}
			return;
		}

		const double functionalWidth = static_cast<double>(std::max<uint16_t>(1u, context.params.functionalWidthPx));
		const double valuePerPixel = range / functionalWidth;
		const double pressRatio = std::clamp((state.pressValue - lower) / range, 0.0, 1.0);
		const double pressFillPx = std::round(pressRatio * functionalWidth);
		const double deltaPx = std::round(static_cast<double>(input.mouseX - state.pressMouseX));
		const double nextFillPx = std::clamp(pressFillPx + deltaPx, 0.0, functionalWidth);
		const double nextValue = devSliderRoundTo2Decimals(
			std::clamp(lower + nextFillPx * valuePerPixel, lower, upper));

		if (std::fabs(nextValue - state.currentValue) <= 1.0e-12)
		{
			return;
		}

		state.currentValue = nextValue;
		if (context.params.onValueChangedCallback != nullptr)
		{
			context.params.onValueChangedCallback(state.currentValue);
		}
	},
	nullptr,
	+[](DevSliderDef::BuildContext& context) {
		devSliderState& state = DevSliderDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const double lower = std::min(context.params.minValue, context.params.maxValue);
		const double upper = std::max(context.params.minValue, context.params.maxValue);
		const double normalizedIncoming = devSliderRoundTo2Decimals(
			std::clamp(context.params.value, lower, upper));

		if (!state.initialized)
		{
			state.currentValue = normalizedIncoming;
			state.initialized = true;
		}
		else if (!state.dragging)
		{
			state.currentValue = normalizedIncoming;
		}

		const float functionalWidthPx = static_cast<float>(std::max<uint16_t>(1u, context.params.functionalWidthPx));
		const float handleWidthPx = static_cast<float>(std::max<uint16_t>(1u, context.params.handleWidthPx));
		const float trackHeightPx = static_cast<float>(std::max<uint16_t>(1u, context.params.trackHeightPx));
		const float handleHeightPx = static_cast<float>(
			std::max<uint16_t>(context.params.trackHeightPx, context.params.handleHeightPx));

		float fillWidthPx = 0.0f;
		if (upper > lower)
		{
			const double ratio = std::clamp((state.currentValue - lower) / (upper - lower), 0.0, 1.0);
			fillWidthPx = static_cast<float>(std::round(ratio * static_cast<double>(functionalWidthPx)));
		}
		fillWidthPx = std::clamp(fillWidthPx, 0.0f, functionalWidthPx);
		const float unfillWidthPx = std::max(0.0f, functionalWidthPx - fillWidthPx);

		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		const Clay_ElementId fillId = context.uiManager.toClayEID(context.createChildElementId("fill"));
		const Clay_ElementId barId = context.uiManager.toClayEID(context.createChildElementId("bar"));
		const Clay_ElementId unfillId = context.uiManager.toClayEID(context.createChildElementId("unfill"));

		const bool barHovered = context.uiManager.getPreviousFramesInteraction().isHovered(barId);
		Clay_Color barColor = context.params.handleColor;
		if (state.dragging)
		{
			barColor = context.params.handleActiveColor;
		}
		else if (barHovered)
		{
			barColor = context.params.handleHoverColor;
		}

		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing;
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.padding = context.params.padding;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		root.backgroundColor = context.params.backgroundColor;

		CLAY(rootId, root){
			if (fillWidthPx > 0.0f)
			{
				Clay_ElementDeclaration fill{};
				fill.layout.sizing = {
					.width = CLAY_SIZING_FIXED(fillWidthPx),
					.height = CLAY_SIZING_FIXED(trackHeightPx),
				};
				fill.backgroundColor = context.params.fillColor;
				fill.cornerRadius = Clay_CornerRadius{
					.topLeft = context.params.trackCornerRadius,
					.topRight = 0.0f,
					.bottomLeft = context.params.trackCornerRadius,
					.bottomRight = 0.0f,
				};
				CLAY(fillId, fill){};
			}

			Clay_ElementDeclaration bar{};
			bar.layout.sizing = {
				.width = CLAY_SIZING_FIXED(handleWidthPx),
				.height = CLAY_SIZING_FIXED(handleHeightPx),
			};
			bar.backgroundColor = barColor;
			bar.cornerRadius = CLAY_CORNER_RADIUS(context.params.handleCornerRadius);
			CLAY(barId, bar){};

			if (unfillWidthPx > 0.0f)
			{
				Clay_ElementDeclaration unfill{};
				unfill.layout.sizing = {
					.width = CLAY_SIZING_FIXED(unfillWidthPx),
					.height = CLAY_SIZING_FIXED(trackHeightPx),
				};
				unfill.backgroundColor = context.params.unfillColor;
				unfill.cornerRadius = Clay_CornerRadius{
					.topLeft = 0.0f,
					.topRight = context.params.trackCornerRadius,
					.bottomLeft = 0.0f,
					.bottomRight = context.params.trackCornerRadius,
				};
				CLAY(unfillId, unfill){};
			}
		};
	},
};
