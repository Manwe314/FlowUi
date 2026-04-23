#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devNineSplit.hpp"
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
	bool useNineSplitEdges = false;
	bool showFifthAfterNineSplit = false;
	std::string nineSplitHintText = "";

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
	Clay_Sizing nineSplitSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Sizing nineSplitSlotSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(50),
		.height = CLAY_SIZING_FIXED(50),
	};
	Clay_Sizing nineSplitNumericSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(44),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Sizing fifthAfterSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};

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
		if (context.params.useNineSplitEdges)
		{
			std::array<double, 9> slotValues{};
			slotValues.fill(0.0);
			slotValues[1] = static_cast<double>(context.params.firstValue);
			slotValues[3] = static_cast<double>(context.params.secondValue);
			slotValues[5] = static_cast<double>(context.params.thirdValue);
			slotValues[7] = static_cast<double>(context.params.fourthValue);

			Clay_ElementDeclaration root{};
			const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
			root.layout.sizing = context.params.sizing;
			root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			root.layout.padding = context.params.padding;
			root.layout.childGap = 8;
			root.layout.childAlignment = {
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_CENTER,
			};
			root.backgroundColor = context.params.backgroundColor;

			Clay_TextElementConfig hintTextConfig{};
			hintTextConfig.textColor = context.params.hintTextColor;
			hintTextConfig.fontId = context.params.fontId;
			hintTextConfig.fontSize = context.params.fontSize;
			hintTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
			hintTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

			CLAY(rootId, root){
				CLAY(context.uiManager.toClayEID(context.createChildElementId("nine-split-hint")), {}){
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.nineSplitHintText),
						CLAY_TEXT_CONFIG(hintTextConfig));
				};

				Clay_ElementDeclaration row{};
				const Clay_ElementId rowId = context.uiManager.toClayEID(context.createChildElementId("nine-split-row"));
				row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				row.layout.childGap = 8;
				row.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT,
					.y = CLAY_ALIGN_Y_CENTER,
				};
				row.backgroundColor = FlowUi::Flow_Color("#00000000");

				CLAY(rowId, row){
					context.uiManager
						.createElement(kDevNineSplit, context.createChildElementId("nine-split"))
						.setParameters(devNineSplitParams{
							.fieldIdPrefix = context.params.fieldIdPrefix.empty()
								? context.createChildElementId("nine-split/fields")
								: context.params.fieldIdPrefix + "/nine-split",
							.numericSlots = std::vector<uint8_t>{1, 3, 5, 7},
							.slotValues = slotValues,
							.valueKind = devNumericInputValueKind::UnsignedInt,
							.minValue = context.params.minValue,
							.maxValue = context.params.maxValue,
							.integerRatePerPixel = context.params.dragRatePerPixel,
							.onValuesChangedCallback = [
								onValueChanged = context.params.onValueChangedCallback,
								fifthValue = context.params.fifthValue
							](const std::array<double, 9>& values) {
								if (onValueChanged != nullptr)
								{
									onValueChanged(
										static_cast<int64_t>(std::llround(values[1])),
										static_cast<int64_t>(std::llround(values[3])),
										static_cast<int64_t>(std::llround(values[5])),
										static_cast<int64_t>(std::llround(values[7])),
										fifthValue);
								}
							},
							.sizing = context.params.nineSplitSizing,
							.rowGap = 2,
							.columnGap = 2,
							.slotSizing = context.params.nineSplitSlotSizing,
							.numericSizing = context.params.nineSplitNumericSizing,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.valueTextColor = context.params.valueTextColor,
						})
						.draw();

					if (context.params.showFifthAfterNineSplit)
					{
						context.uiManager
							.createElement(kDevNumericInput, context.createChildElementId("fifth"))
							.setParameters(devNumericInputParams{
								.fieldId = context.params.fieldIdPrefix.empty()
									? context.createChildElementId("fifth/field")
									: context.params.fieldIdPrefix + "/fifth",
								.initialText = devNumericValueToText(
									devNumericInputValueKind::UnsignedInt,
									static_cast<double>(context.params.fifthValue)),
								.hintText = context.params.fifthHintText,
								.valueKind = devNumericInputValueKind::UnsignedInt,
								.minValue = context.params.minValue,
								.maxValue = context.params.maxValue,
								.integerRatePerPixel = context.params.dragRatePerPixel,
								.onValueChangedCallback = [
									onValueChanged = context.params.onValueChangedCallback,
									firstValue = context.params.firstValue,
									secondValue = context.params.secondValue,
									thirdValue = context.params.thirdValue,
									fourthValue = context.params.fourthValue
								](double changedValue) {
									if (onValueChanged != nullptr)
									{
										onValueChanged(
											firstValue,
											secondValue,
											thirdValue,
											fourthValue,
											static_cast<int64_t>(std::llround(changedValue)));
									}
								},
								.sizing = context.params.fifthAfterSizing,
								.fontId = context.params.fontId,
								.fontSize = context.params.fontSize,
								.hintTextColor = context.params.hintTextColor,
								.valueTextColor = context.params.valueTextColor,
							})
							.draw();
					}
				};
			};
			return;
		}

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
