#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devNumericInput.hpp"

struct devFloat2InputParams {
	std::string fieldIdPrefix = "";
	std::string firstHintText = "first";
	std::string secondHintText = "second";
	double firstValue = 0.0;
	double secondValue = 0.0;
	std::function<void(double, double)> onValuePairChangedCallback = nullptr;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(0);
	uint16_t childGap = 10;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	double minValue = static_cast<double>(-std::numeric_limits<float>::max());
	double maxValue = static_cast<double>(std::numeric_limits<float>::max());
	double dragRatePerPixel = 0.01;

	Clay_Sizing firstInputSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Sizing secondInputSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};

	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color hintTextColor = FlowUi::Flow_Color("#a8b4ccff");
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");
};

using DevFloat2InputDef = FlowUi::ElementDefinition<
	devFloat2InputParams,
	void,
	void,
	FLOW_DEF_ID("DevFloat2Input"),
	true>;

inline const DevFloat2InputDef kDevFloat2Input = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevFloat2InputDef::BuildContext& context) {
		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_TOP,
		};
		root.backgroundColor = context.params.backgroundColor;

		CLAY(rootId, root){
			context.uiManager
				.createElement(kDevNumericInput, context.createChildElementId("first"))
				.setParameters(devNumericInputParams{
					.fieldId = context.params.fieldIdPrefix.empty()
						? context.createChildElementId("first/field")
						: context.params.fieldIdPrefix + "/first",
					.initialText = devNumericValueToText(
						devNumericInputValueKind::Floating,
						context.params.firstValue),
					.hintText = context.params.firstHintText,
					.valueKind = devNumericInputValueKind::Floating,
					.minValue = context.params.minValue,
					.maxValue = context.params.maxValue,
					.floatRatePerPixel = context.params.dragRatePerPixel,
					.onValueChangedCallback = [
						onPairChanged = context.params.onValuePairChangedCallback,
						secondValue = context.params.secondValue
					](double firstValue) {
						if (onPairChanged != nullptr)
						{
							onPairChanged(firstValue, secondValue);
						}
					},
					.sizing = context.params.firstInputSizing,
					.fontId = context.params.fontId,
					.fontSize = context.params.fontSize,
					.hintTextColor = context.params.hintTextColor,
					.valueTextColor = context.params.valueTextColor,
				})
				.draw();

			context.uiManager
				.createElement(kDevNumericInput, context.createChildElementId("second"))
				.setParameters(devNumericInputParams{
					.fieldId = context.params.fieldIdPrefix.empty()
						? context.createChildElementId("second/field")
						: context.params.fieldIdPrefix + "/second",
					.initialText = devNumericValueToText(
						devNumericInputValueKind::Floating,
						context.params.secondValue),
					.hintText = context.params.secondHintText,
					.valueKind = devNumericInputValueKind::Floating,
					.minValue = context.params.minValue,
					.maxValue = context.params.maxValue,
					.floatRatePerPixel = context.params.dragRatePerPixel,
					.onValueChangedCallback = [
						onPairChanged = context.params.onValuePairChangedCallback,
						firstValue = context.params.firstValue
					](double secondValue) {
						if (onPairChanged != nullptr)
						{
							onPairChanged(firstValue, secondValue);
						}
					},
					.sizing = context.params.secondInputSizing,
					.fontId = context.params.fontId,
					.fontSize = context.params.fontSize,
					.hintTextColor = context.params.hintTextColor,
					.valueTextColor = context.params.valueTextColor,
				})
				.draw();
		};
	},
};
