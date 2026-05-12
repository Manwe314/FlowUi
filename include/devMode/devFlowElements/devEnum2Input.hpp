#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devEnum1Input.hpp"

struct devEnum2InputParams {
	std::string firstHintText = "first";
	std::string secondHintText = "second";
	std::string firstSelectedValue = "";
	std::string firstSelectedLabel = "";
	std::string secondSelectedValue = "";
	std::string secondSelectedLabel = "";
	std::vector<devEnum1InputOption> firstOptions{};
	std::vector<devEnum1InputOption> secondOptions{};
	std::function<void(std::string_view, std::string_view)> onValuePairChangedCallback = nullptr;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(0);
	uint16_t childGap = 10;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

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

using DevEnum2InputDef = FlowUi::ElementDefinition<
	devEnum2InputParams,
	void,
	void,
	FLOW_DEF_ID("DevEnum2Input"),
	true>;

inline const DevEnum2InputDef kDevEnum2Input = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevEnum2InputDef::BuildContext& context) {
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
				.createElement(kDevEnum1Input, context.createChildElementId("first"))
				.setParameters(devEnum1InputParams{
					.hintText = context.params.firstHintText,
					.selectedValue = context.params.firstSelectedValue,
					.selectedLabel = context.params.firstSelectedLabel,
					.options = context.params.firstOptions,
					.onValueChangedCallback = [
						onPairChanged = context.params.onValuePairChangedCallback,
						secondValue = context.params.secondSelectedValue
					](std::string_view firstValue) {
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
					.optionTextColor = context.params.valueTextColor,
				})
				.draw();

			context.uiManager
				.createElement(kDevEnum1Input, context.createChildElementId("second"))
				.setParameters(devEnum1InputParams{
					.hintText = context.params.secondHintText,
					.selectedValue = context.params.secondSelectedValue,
					.selectedLabel = context.params.secondSelectedLabel,
					.options = context.params.secondOptions,
					.onValueChangedCallback = [
						onPairChanged = context.params.onValuePairChangedCallback,
						firstValue = context.params.firstSelectedValue
					](std::string_view secondValue) {
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
					.optionTextColor = context.params.valueTextColor,
				})
				.draw();
		};
	},
};
