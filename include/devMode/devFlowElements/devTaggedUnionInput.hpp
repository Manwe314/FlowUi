#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devEnum1Input.hpp"
#include "devMode/devFlowElements/devFloat2Input.hpp"
#include "devMode/devFlowElements/devNumericInput.hpp"

struct devTaggedUnionInputParams {
	std::string fieldIdPrefix = "";
	std::string tagHintText = "type";
	std::string selectedTagValue = "";
	std::string selectedTagLabel = "";
	std::vector<devEnum1InputOption> tagOptions{};
	std::string emptyTagText = "<type>";
	std::function<void(std::string_view)> onTagChangedCallback = nullptr;

	bool usePercentEditor = false;
	std::string minHintText = "min";
	std::string maxHintText = "max";
	double minValue = 0.0;
	double maxValue = 0.0;
	std::function<void(double, double)> onMinMaxChangedCallback = nullptr;

	std::string percentHintText = "precentage";
	double percentValue = 0.0;
	std::function<void(double)> onPercentChangedCallback = nullptr;

	double minMaxEditorMinValue = static_cast<double>(-std::numeric_limits<float>::max());
	double minMaxEditorMaxValue = static_cast<double>(std::numeric_limits<float>::max());
	double percentEditorMinValue = static_cast<double>(-std::numeric_limits<float>::max());
	double percentEditorMaxValue = static_cast<double>(std::numeric_limits<float>::max());
	double dragRatePerPixel = 0.01;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(0);
	uint16_t childGap = 6;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	Clay_Sizing tagInputSizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Sizing minMaxInputSizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Sizing percentInputSizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};

	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color hintTextColor = FlowUi::Flow_Color("#a8b4ccff");
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");
};

using DevTaggedUnionInputDef = FlowUi::ElementDefinition<
	devTaggedUnionInputParams,
	void,
	void,
	FLOW_DEF_ID("DevTaggedUnionInput"),
	true>;

inline const DevTaggedUnionInputDef kDevTaggedUnionInput = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevTaggedUnionInputDef::BuildContext& context) {
		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		root.backgroundColor = context.params.backgroundColor;

		CLAY(rootId, root){
			context.uiManager
				.createElement(kDevEnum1Input, context.createChildElementId("tag"))
				.setParameters(devEnum1InputParams{
					.hintText = context.params.tagHintText,
					.selectedValue = context.params.selectedTagValue,
					.selectedLabel = context.params.selectedTagLabel,
					.options = context.params.tagOptions,
					.emptyValueText = context.params.emptyTagText,
					.onValueChangedCallback = context.params.onTagChangedCallback,
					.sizing = context.params.tagInputSizing,
					.fontId = context.params.fontId,
					.fontSize = context.params.fontSize,
					.hintTextColor = context.params.hintTextColor,
					.valueTextColor = context.params.valueTextColor,
				})
				.draw();

			if (context.params.usePercentEditor)
			{
				context.uiManager
					.createElement(kDevNumericInput, context.createChildElementId("percent"))
					.setParameters(devNumericInputParams{
						.fieldId = context.params.fieldIdPrefix.empty()
							? context.createChildElementId("percent/field")
							: context.params.fieldIdPrefix + "/percent",
						.initialText = devNumericValueToText(
							devNumericInputValueKind::Floating,
							context.params.percentValue),
						.hintText = context.params.percentHintText,
						.valueKind = devNumericInputValueKind::Floating,
						.minValue = context.params.percentEditorMinValue,
						.maxValue = context.params.percentEditorMaxValue,
						.floatRatePerPixel = context.params.dragRatePerPixel,
						.onValueChangedCallback = context.params.onPercentChangedCallback,
						.sizing = context.params.percentInputSizing,
						.fontId = context.params.fontId,
						.fontSize = context.params.fontSize,
						.hintTextColor = context.params.hintTextColor,
						.valueTextColor = context.params.valueTextColor,
					})
					.draw();
			}
			else
			{
				context.uiManager
					.createElement(kDevFloat2Input, context.createChildElementId("minmax"))
					.setParameters(devFloat2InputParams{
						.fieldIdPrefix = context.params.fieldIdPrefix.empty()
							? context.createChildElementId("minmax/field")
							: context.params.fieldIdPrefix + "/minmax",
						.firstHintText = context.params.minHintText,
						.secondHintText = context.params.maxHintText,
						.firstValue = context.params.minValue,
						.secondValue = context.params.maxValue,
						.onValuePairChangedCallback = context.params.onMinMaxChangedCallback,
						.sizing = context.params.minMaxInputSizing,
						.minValue = context.params.minMaxEditorMinValue,
						.maxValue = context.params.minMaxEditorMaxValue,
						.dragRatePerPixel = context.params.dragRatePerPixel,
						.fontId = context.params.fontId,
						.fontSize = context.params.fontSize,
						.hintTextColor = context.params.hintTextColor,
						.valueTextColor = context.params.valueTextColor,
					})
					.draw();
			}
		};
	},
};
