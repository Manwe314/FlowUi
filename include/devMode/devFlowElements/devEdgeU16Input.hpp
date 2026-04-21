#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devNumericInput.hpp"

struct devEdgeU16InputParams {
	std::string fieldIdPrefix = "";
	std::string firstHintText = "first";
	std::string secondHintText = "second";
	std::string thirdHintText = "third";
	std::string fourthHintText = "fourth";
	std::string fifthHintText = "fifth";
	bool showFifth = false;
	int64_t firstValue = 0;
	int64_t secondValue = 0;
	int64_t thirdValue = 0;
	int64_t fourthValue = 0;
	int64_t fifthValue = 0;
	std::function<void(int64_t, int64_t, int64_t, int64_t, int64_t)> onValueChangedCallback = nullptr;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(0);
	uint16_t childGap = 10;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	double minValue = 0.0;
	double maxValue = static_cast<double>(std::numeric_limits<uint16_t>::max());
	double dragRatePerPixel = 1.0;

	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color hintTextColor = FlowUi::Flow_Color("#a8b4ccff");
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");
};

using DevEdgeU16InputDef = FlowUi::ElementDefinition<
	devEdgeU16InputParams,
	void,
	void,
	FLOW_DEF_ID("DevEdgeU16Input"),
	true>;

inline const DevEdgeU16InputDef kDevEdgeU16Input = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevEdgeU16InputDef::BuildContext& context) {
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
			const auto drawField = [&](std::string_view localId, std::string_view hintText, int64_t value) {
				context.uiManager
					.createElement(kDevNumericInput, context.createChildElementId(localId))
					.setParameters(devNumericInputParams{
						.fieldId = context.params.fieldIdPrefix.empty()
							? context.createChildElementId(std::string(localId) + "/field")
							: context.params.fieldIdPrefix + "/" + std::string(localId),
						.initialText = devNumericValueToText(
							devNumericInputValueKind::UnsignedInt,
							static_cast<double>(value)),
						.hintText = std::string(hintText),
						.valueKind = devNumericInputValueKind::UnsignedInt,
						.minValue = context.params.minValue,
						.maxValue = context.params.maxValue,
						.integerRatePerPixel = context.params.dragRatePerPixel,
						.onValueChangedCallback = [
							onValueChanged = context.params.onValueChangedCallback,
							localId = std::string(localId),
							firstValue = context.params.firstValue,
							secondValue = context.params.secondValue,
							thirdValue = context.params.thirdValue,
							fourthValue = context.params.fourthValue,
							fifthValue = context.params.fifthValue
						](double changedValue) {
							if (onValueChanged == nullptr)
							{
								return;
							}

							int64_t first = firstValue;
							int64_t second = secondValue;
							int64_t third = thirdValue;
							int64_t fourth = fourthValue;
							int64_t fifth = fifthValue;
							const int64_t changedNumeric = static_cast<int64_t>(std::llround(changedValue));

							if (localId == "first")
							{
								first = changedNumeric;
							}
							else if (localId == "second")
							{
								second = changedNumeric;
							}
							else if (localId == "third")
							{
								third = changedNumeric;
							}
							else if (localId == "fourth")
							{
								fourth = changedNumeric;
							}
							else if (localId == "fifth")
							{
								fifth = changedNumeric;
							}

							onValueChanged(first, second, third, fourth, fifth);
						},
						.fontId = context.params.fontId,
						.fontSize = context.params.fontSize,
						.hintTextColor = context.params.hintTextColor,
						.valueTextColor = context.params.valueTextColor,
					})
					.draw();
			};

			drawField("first", context.params.firstHintText, context.params.firstValue);
			drawField("second", context.params.secondHintText, context.params.secondValue);
			drawField("third", context.params.thirdHintText, context.params.thirdValue);
			drawField("fourth", context.params.fourthHintText, context.params.fourthValue);
			if (context.params.showFifth)
			{
				drawField("fifth", context.params.fifthHintText, context.params.fifthValue);
			}
		};
	},
};
