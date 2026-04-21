#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devNumericInput.hpp"

struct devFloat4InputParams {
	std::string fieldIdPrefix = "";
	std::string firstHintText = "first";
	std::string secondHintText = "second";
	std::string thirdHintText = "third";
	std::string fourthHintText = "fourth";
	double firstValue = 0.0;
	double secondValue = 0.0;
	double thirdValue = 0.0;
	double fourthValue = 0.0;
	std::function<void(double, double, double, double)> onValueQuadChangedCallback = nullptr;

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
	Clay_Sizing thirdInputSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Sizing fourthInputSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};

	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color hintTextColor = FlowUi::Flow_Color("#a8b4ccff");
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");
};

using DevFloat4InputDef = FlowUi::ElementDefinition<
	devFloat4InputParams,
	void,
	void,
	FLOW_DEF_ID("DevFloat4Input"),
	true>;

inline const DevFloat4InputDef kDevFloat4Input = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevFloat4InputDef::BuildContext& context) {
		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_TOP,
		};
		root.backgroundColor = context.params.backgroundColor;

		CLAY(root){
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
						onQuadChanged = context.params.onValueQuadChangedCallback,
						secondValue = context.params.secondValue,
						thirdValue = context.params.thirdValue,
						fourthValue = context.params.fourthValue
					](double firstValue) {
						if (onQuadChanged != nullptr)
						{
							onQuadChanged(firstValue, secondValue, thirdValue, fourthValue);
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
						onQuadChanged = context.params.onValueQuadChangedCallback,
						firstValue = context.params.firstValue,
						thirdValue = context.params.thirdValue,
						fourthValue = context.params.fourthValue
					](double secondValue) {
						if (onQuadChanged != nullptr)
						{
							onQuadChanged(firstValue, secondValue, thirdValue, fourthValue);
						}
					},
					.sizing = context.params.secondInputSizing,
					.fontId = context.params.fontId,
					.fontSize = context.params.fontSize,
					.hintTextColor = context.params.hintTextColor,
					.valueTextColor = context.params.valueTextColor,
				})
				.draw();

			context.uiManager
				.createElement(kDevNumericInput, context.createChildElementId("third"))
				.setParameters(devNumericInputParams{
					.fieldId = context.params.fieldIdPrefix.empty()
						? context.createChildElementId("third/field")
						: context.params.fieldIdPrefix + "/third",
					.initialText = devNumericValueToText(
						devNumericInputValueKind::Floating,
						context.params.thirdValue),
					.hintText = context.params.thirdHintText,
					.valueKind = devNumericInputValueKind::Floating,
					.minValue = context.params.minValue,
					.maxValue = context.params.maxValue,
					.floatRatePerPixel = context.params.dragRatePerPixel,
					.onValueChangedCallback = [
						onQuadChanged = context.params.onValueQuadChangedCallback,
						firstValue = context.params.firstValue,
						secondValue = context.params.secondValue,
						fourthValue = context.params.fourthValue
					](double thirdValue) {
						if (onQuadChanged != nullptr)
						{
							onQuadChanged(firstValue, secondValue, thirdValue, fourthValue);
						}
					},
					.sizing = context.params.thirdInputSizing,
					.fontId = context.params.fontId,
					.fontSize = context.params.fontSize,
					.hintTextColor = context.params.hintTextColor,
					.valueTextColor = context.params.valueTextColor,
				})
				.draw();

			context.uiManager
				.createElement(kDevNumericInput, context.createChildElementId("fourth"))
				.setParameters(devNumericInputParams{
					.fieldId = context.params.fieldIdPrefix.empty()
						? context.createChildElementId("fourth/field")
						: context.params.fieldIdPrefix + "/fourth",
					.initialText = devNumericValueToText(
						devNumericInputValueKind::Floating,
						context.params.fourthValue),
					.hintText = context.params.fourthHintText,
					.valueKind = devNumericInputValueKind::Floating,
					.minValue = context.params.minValue,
					.maxValue = context.params.maxValue,
					.floatRatePerPixel = context.params.dragRatePerPixel,
					.onValueChangedCallback = [
						onQuadChanged = context.params.onValueQuadChangedCallback,
						firstValue = context.params.firstValue,
						secondValue = context.params.secondValue,
						thirdValue = context.params.thirdValue
					](double fourthValue) {
						if (onQuadChanged != nullptr)
						{
							onQuadChanged(firstValue, secondValue, thirdValue, fourthValue);
						}
					},
					.sizing = context.params.fourthInputSizing,
					.fontId = context.params.fontId,
					.fontSize = context.params.fontSize,
					.hintTextColor = context.params.hintTextColor,
					.valueTextColor = context.params.valueTextColor,
				})
				.draw();
		};
	},
};
