#pragma once

#include <array>
#include <cstdio>

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicInputField.hpp"
#include "devMode/devFlowElements/devNineSplit.hpp"
#include "devMode/devFlowElements/devNumericInput.hpp"
#include "devMode/devFlowElements/devSlider.hpp"

inline double devFloat4Clamp(double value, double minValue, double maxValue) {
	if (maxValue < minValue)
	{
		maxValue = minValue;
	}
	return std::clamp(value, minValue, maxValue);
}

inline double devFloat4RoundToDecimals(double value, int decimals) {
	if (decimals <= 0)
	{
		return std::round(value);
	}
	double scale = 1.0;
	for (int i = 0; i < decimals; ++i)
	{
		scale *= 10.0;
	}
	return std::round(value * scale) / scale;
}

inline std::array<double, 4> devFloat4DisplayFromInternalColor(
	const std::array<double, 4>& internalValues) {
	return {
		devFloat4Clamp(std::round(devFloat4Clamp(internalValues[0], 0.0, 255.0)), 0.0, 255.0),
		devFloat4Clamp(std::round(devFloat4Clamp(internalValues[1], 0.0, 255.0)), 0.0, 255.0),
		devFloat4Clamp(std::round(devFloat4Clamp(internalValues[2], 0.0, 255.0)), 0.0, 255.0),
		devFloat4Clamp(
			std::round((devFloat4Clamp(internalValues[3], 0.0, 255.0) / 255.0) * 100.0),
			0.0,
			100.0),
	};
}

inline std::array<double, 4> devFloat4InternalFromDisplayColor(
	const std::array<double, 4>& displayValues) {
	const double displayR = devFloat4Clamp(std::round(displayValues[0]), 0.0, 255.0);
	const double displayG = devFloat4Clamp(std::round(displayValues[1]), 0.0, 255.0);
	const double displayB = devFloat4Clamp(std::round(displayValues[2]), 0.0, 255.0);
	const double displayAlphaPercent = devFloat4Clamp(std::round(displayValues[3]), 0.0, 100.0);
	return {
		displayR,
		displayG,
		displayB,
		devFloat4RoundToDecimals(
			devFloat4Clamp((displayAlphaPercent / 100.0) * 255.0, 0.0, 255.0),
			2),
	};
}

inline std::string devFloat4HexFromInternalColor(
	const std::array<double, 4>& internalValues) {
	const int r = static_cast<int>(std::lround(devFloat4Clamp(internalValues[0], 0.0, 255.0)));
	const int g = static_cast<int>(std::lround(devFloat4Clamp(internalValues[1], 0.0, 255.0)));
	const int b = static_cast<int>(std::lround(devFloat4Clamp(internalValues[2], 0.0, 255.0)));
	const int a = static_cast<int>(std::lround(devFloat4Clamp(internalValues[3], 0.0, 255.0)));
	char buffer[10]{};
	std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X", r, g, b, a);
	return std::string(buffer);
}

inline bool devFloat4TryParseHexToInternalColor(
	std::string_view hexText,
	std::array<double, 4>& outInternalValues) {
	try
	{
		const Clay_Color color = FlowUi::Flow_Color(hexText);
		outInternalValues = {
			devFloat4Clamp(static_cast<double>(color.r), 0.0, 255.0),
			devFloat4Clamp(static_cast<double>(color.g), 0.0, 255.0),
			devFloat4Clamp(static_cast<double>(color.b), 0.0, 255.0),
			devFloat4Clamp(static_cast<double>(color.a), 0.0, 255.0),
		};
		return true;
	}
	catch (...)
	{
		return false;
	}
}

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
	bool useColorEditor = false;
	std::string colorHexHintText = "Hex code:";
	bool useNineSplitCorners = false;
	std::string nineSplitHintText = "Corner radii:";

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

	Clay_Sizing colorChannelSizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Sizing colorNumericSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(48),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Sizing colorHexInputSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIT(120.0f, 220.0f),
		.height = CLAY_SIZING_FIT(0),
	};
	uint16_t colorSliderFunctionalWidthPx = 140;
	uint16_t colorSliderHandleWidthPx = 8;
	uint16_t colorSliderTrackHeightPx = 8;
	uint16_t colorSliderHandleHeightPx = 14;

	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color hintTextColor = FlowUi::Flow_Color("#a8b4ccff");
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");
};

struct devFloat4InputState {
	bool initialized = false;
	bool modifiedThisFrame = false;
	std::array<double, 4> internalValues{};
	std::array<double, 4> displayValues{};
	std::array<std::string, 4> numericElementIds{};
	std::string hexFieldId{};
	std::string hexText{};
	bool pendingHexFieldReset = false;
};

inline void devFloat4SyncNumericVisual(devFloat4InputState* state, uint8_t channelIndex, bool forceFieldReset) {
	if (state == nullptr || channelIndex >= 4u)
	{
		return;
	}

	const std::string& elementId = state->numericElementIds[channelIndex];
	if (elementId.empty())
	{
		return;
	}

	devNumericInputState* numericState = DevNumericInputDef::tryGetState(FlowUi::toFlowId(elementId));
	if (numericState == nullptr)
	{
		return;
	}

	const devNumericInputValueKind valueKind = devNumericInputValueKind::UnsignedInt;
	const std::string nextText =
		devNumericValueToText(valueKind, state->displayValues[channelIndex]);
	const bool textChanged = numericState->normalizedText != nextText;
	numericState->hasValue = true;
	numericState->value = state->displayValues[channelIndex];
	numericState->normalizedText = nextText;
	if (forceFieldReset && textChanged)
	{
		numericState->pendingFieldReset = true;
	}
}

using DevFloat4InputDef = FlowUi::ElementDefinition<
	devFloat4InputParams,
	devFloat4InputState,
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
		const uint64_t elementFlowId = FlowUi::toFlowId(context.elementID);
		devFloat4InputState& state = DevFloat4InputDef::getOrCreateState(elementFlowId);

		if (context.params.useColorEditor)
		{
			const bool modifiedLastFrame = state.modifiedThisFrame;
			state.modifiedThisFrame = false;
			const bool hasPrimaryInputFocus = context.uiManager.inputFields().hasPrimaryFieldFocus();

			const std::array<double, 4> incomingInternalValues{
				devFloat4RoundToDecimals(devFloat4Clamp(context.params.firstValue, 0.0, 255.0), 2),
				devFloat4RoundToDecimals(devFloat4Clamp(context.params.secondValue, 0.0, 255.0), 2),
				devFloat4RoundToDecimals(devFloat4Clamp(context.params.thirdValue, 0.0, 255.0), 2),
				devFloat4RoundToDecimals(devFloat4Clamp(context.params.fourthValue, 0.0, 255.0), 2),
			};

			std::array<bool, 4> channelSyncedFromParams{};
			channelSyncedFromParams.fill(false);
			if (!state.initialized)
			{
				state.initialized = true;
				state.internalValues = incomingInternalValues;
				state.displayValues = devFloat4DisplayFromInternalColor(state.internalValues);
				state.hexText = devFloat4HexFromInternalColor(state.internalValues);
			}
			else if (!modifiedLastFrame && !hasPrimaryInputFocus)
			{
				bool anyChannelSynced = false;
				const std::array<double, 4> incomingDisplayValues =
					devFloat4DisplayFromInternalColor(incomingInternalValues);
				for (uint8_t i = 0u; i < 4u; ++i)
				{
					// Keep the editor surface stable for small upstream feedback differences,
					// especially alpha percent <-> 0..255 conversion noise.
					const double displayDiff =
						std::fabs(state.displayValues[i] - incomingDisplayValues[i]);
					if (displayDiff <= 0.5)
					{
						continue;
					}
					state.internalValues[i] = incomingInternalValues[i];
					state.displayValues[i] = incomingDisplayValues[i];
					channelSyncedFromParams[i] = true;
					anyChannelSynced = true;
				}

				if (anyChannelSynced)
				{
					const std::string nextHex = devFloat4HexFromInternalColor(state.internalValues);
					if (state.hexText != nextHex)
					{
						state.hexText = nextHex;
						state.pendingHexFieldReset = true;
					}
				}
			}

			for (uint8_t i = 0u; i < 4u; ++i)
			{
				state.numericElementIds[i] =
					context.createChildElementId("color/channel/" + std::to_string(i) + "/numeric");
			}
			state.hexFieldId = context.params.fieldIdPrefix.empty()
				? context.createChildElementId("color/hex/field")
				: context.params.fieldIdPrefix + "/color/hex";

			if (state.pendingHexFieldReset && !state.hexFieldId.empty())
			{
				(void)context.uiManager.inputFields().removeField(state.hexFieldId);
				state.pendingHexFieldReset = false;
			}

			for (uint8_t i = 0u; i < 4u; ++i)
			{
				if (channelSyncedFromParams[i])
				{
					devFloat4SyncNumericVisual(&state, i, true);
				}
			}

			Clay_ElementDeclaration root{};
			root.id = context.uiManager.toClayEID(context.elementID);
			root.layout.sizing = context.params.sizing;
			root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			root.layout.padding = context.params.padding;
			root.layout.childGap = 11;
			root.layout.childAlignment = {
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_TOP,
			};
			root.backgroundColor = context.params.backgroundColor;

			Clay_TextElementConfig hintTextConfig{};
			hintTextConfig.textColor = context.params.hintTextColor;
			hintTextConfig.fontId = context.params.fontId;
			hintTextConfig.fontSize = context.params.fontSize;
			hintTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
			hintTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

			const std::array<std::string, 4> channelLabels{
				context.params.firstHintText,
				context.params.secondHintText,
				context.params.thirdHintText,
				context.params.fourthHintText,
			};

			const auto emitValueChanged = [onQuadChanged = context.params.onValueQuadChangedCallback](
				const std::array<double, 4>& internalValues) {
				if (onQuadChanged != nullptr)
				{
					onQuadChanged(
						internalValues[0],
						internalValues[1],
						internalValues[2],
						internalValues[3]);
				}
			};

			CLAY(root){
				for (uint8_t channelIndex = 0u; channelIndex < 4u; ++channelIndex)
				{
					Clay_ElementDeclaration channel{};
					channel.id = context.uiManager.toClayEID(
						context.createChildElementId("color/channel/" + std::to_string(channelIndex)));
					channel.layout.sizing = context.params.colorChannelSizing;
					channel.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
					channel.layout.childGap = 8;
					channel.layout.childAlignment = {
						.x = CLAY_ALIGN_X_LEFT,
						.y = CLAY_ALIGN_Y_CENTER,
					};
					channel.backgroundColor = FlowUi::Flow_Color("#00000000");

					CLAY(channel){
						const std::string labelWithColon =
							(channelLabels[channelIndex].empty()
								? std::string("value")
								: channelLabels[channelIndex]) + ":";
						Clay_ElementDeclaration labelSlot{};
						labelSlot.id = context.uiManager.toClayEID(
							context.createChildElementId("color/channel/" + std::to_string(channelIndex) + "/hint"));
						labelSlot.layout.sizing = {
							.width = CLAY_SIZING_FIXED(54),
							.height = CLAY_SIZING_FIT(0),
						};
						labelSlot.layout.childAlignment = {
							.x = CLAY_ALIGN_X_LEFT,
							.y = CLAY_ALIGN_Y_CENTER,
						};
						labelSlot.backgroundColor = FlowUi::Flow_Color("#00000000");

						CLAY(labelSlot){
							CLAY_TEXT(
								context.uiManager.toClayString(labelWithColon),
								CLAY_TEXT_CONFIG(hintTextConfig));
						};

						const double sliderMinValue = 0.0;
						const double sliderMaxValue = channelIndex == 3u ? 100.0 : 255.0;

						Clay_Color fillColor = FlowUi::Flow_Color("#5d6777ff");
						if (channelIndex == 0u)
						{
							fillColor = FlowUi::Flow_Color("#c35757ff");
						}
						else if (channelIndex == 1u)
						{
							fillColor = FlowUi::Flow_Color("#5aa15fff");
						}
						else if (channelIndex == 2u)
						{
							fillColor = FlowUi::Flow_Color("#5a83cfff");
						}

						context.uiManager
							.createElement(
								kDevSlider,
								context.createChildElementId("color/channel/" + std::to_string(channelIndex) + "/slider"))
							.setParameters(devSliderParams{
								.minValue = sliderMinValue,
								.maxValue = sliderMaxValue,
								.value = state.displayValues[channelIndex],
								.onValueChangedCallback = [
									elementFlowId,
									channelIndex,
									onValueChanged = emitValueChanged
								](double changedValue) {
									devFloat4InputState* latestState = DevFloat4InputDef::tryGetState(elementFlowId);
									if (latestState == nullptr)
									{
										return;
									}

									const double clampedDisplay = devFloat4RoundToDecimals(
										devFloat4Clamp(
											std::round(changedValue),
											0.0,
											channelIndex == 3u ? 100.0 : 255.0),
										0);
									if (std::fabs(latestState->displayValues[channelIndex] - clampedDisplay) <= 1.0e-12)
									{
										return;
									}

									latestState->displayValues[channelIndex] = clampedDisplay;
									latestState->internalValues = devFloat4InternalFromDisplayColor(latestState->displayValues);
									latestState->hexText = devFloat4HexFromInternalColor(latestState->internalValues);
									latestState->pendingHexFieldReset = true;
									latestState->modifiedThisFrame = true;

									devFloat4SyncNumericVisual(latestState, channelIndex, true);
									onValueChanged(latestState->internalValues);
								},
								.functionalWidthPx = context.params.colorSliderFunctionalWidthPx,
								.handleWidthPx = context.params.colorSliderHandleWidthPx,
								.trackHeightPx = context.params.colorSliderTrackHeightPx,
								.handleHeightPx = context.params.colorSliderHandleHeightPx,
								.fillColor = fillColor,
								.unfillColor = FlowUi::Flow_Color("#2b3039ff"),
								.handleColor = FlowUi::Flow_Color("#8f8d8dff"),
								.handleHoverColor = FlowUi::Flow_Color("#adb3beff"),
								.handleActiveColor = FlowUi::Flow_Color("#d5d9e0ff"),
							})
							.draw();

						context.uiManager
							.createElement(kDevNumericInput, state.numericElementIds[channelIndex])
							.setParameters(devNumericInputParams{
								.fieldId = context.params.fieldIdPrefix.empty()
									? context.createChildElementId(
										"color/channel/" + std::to_string(channelIndex) + "/numeric/field")
									: context.params.fieldIdPrefix + "/color/channel/" + std::to_string(channelIndex) +
										"/numeric",
								.initialText = devNumericValueToText(
									devNumericInputValueKind::UnsignedInt,
									state.displayValues[channelIndex]),
								.hintText = "",
								.showHint = false,
								.valueKind = devNumericInputValueKind::UnsignedInt,
								.minValue = sliderMinValue,
								.maxValue = sliderMaxValue,
								.integerRatePerPixel = 1.0,
								.onValueChangedCallback = [
									elementFlowId,
									channelIndex,
									onValueChanged = emitValueChanged
								](double changedValue) {
									devFloat4InputState* latestState = DevFloat4InputDef::tryGetState(elementFlowId);
									if (latestState == nullptr)
									{
										return;
									}

									const double clampedDisplay = devFloat4RoundToDecimals(
										devFloat4Clamp(
											std::round(changedValue),
											0.0,
											channelIndex == 3u ? 100.0 : 255.0),
										0);
									if (std::fabs(latestState->displayValues[channelIndex] - clampedDisplay) <= 1.0e-12)
									{
										return;
									}

									latestState->displayValues[channelIndex] = clampedDisplay;
									latestState->internalValues = devFloat4InternalFromDisplayColor(latestState->displayValues);
									latestState->hexText = devFloat4HexFromInternalColor(latestState->internalValues);
									latestState->pendingHexFieldReset = true;
									latestState->modifiedThisFrame = true;
									onValueChanged(latestState->internalValues);
								},
								.sizing = context.params.colorNumericSizing,
								.inputSizing = context.params.colorNumericSizing,
								.inputPadding = CLAY_PADDING_ALL(5),
								.fontId = context.params.fontId,
								.fontSize = context.params.fontSize,
								.valueTextColor = context.params.valueTextColor,
							})
							.draw();
					};
				}

				Clay_ElementDeclaration hexSection{};
				hexSection.id = context.uiManager.toClayEID(context.createChildElementId("color/hex-section"));
				hexSection.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				hexSection.layout.childGap = 8;
				hexSection.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT,
					.y = CLAY_ALIGN_Y_CENTER,
				};
				hexSection.backgroundColor = FlowUi::Flow_Color("#00000000");

				CLAY(hexSection){
					const std::string hexHintWithColon =
						(context.params.colorHexHintText.empty()
							? std::string("Hex code")
							: context.params.colorHexHintText) + ":";
					CLAY({.id = context.uiManager.toClayEID(context.createChildElementId("color/hex/hint"))}){
						CLAY_TEXT(
							context.uiManager.toClayString(hexHintWithColon),
							CLAY_TEXT_CONFIG(hintTextConfig));
					};

					context.uiManager
						.createElement(kDevBasicInputField, context.createChildElementId("color/hex/input"))
						.setParameters(devBasicInputFieldParams{
							.fieldId = state.hexFieldId,
							.initialText = state.hexText,
							.onTextChangedCallback = [
								elementFlowId,
								onValueChanged = emitValueChanged
							](std::string_view changedText) {
								devFloat4InputState* latestState = DevFloat4InputDef::tryGetState(elementFlowId);
								if (latestState == nullptr)
								{
									return;
								}

								latestState->hexText = std::string(changedText);
								std::array<double, 4> parsedInternalValues{};
								if (!devFloat4TryParseHexToInternalColor(changedText, parsedInternalValues))
								{
									return;
								}

								latestState->internalValues = parsedInternalValues;
								latestState->displayValues =
									devFloat4DisplayFromInternalColor(latestState->internalValues);

								const std::string normalizedHex =
									devFloat4HexFromInternalColor(latestState->internalValues);
								if (latestState->hexText != normalizedHex)
								{
									latestState->hexText = normalizedHex;
									latestState->pendingHexFieldReset = true;
								}

								for (uint8_t i = 0u; i < 4u; ++i)
								{
									devFloat4SyncNumericVisual(latestState, i, true);
								}

								latestState->modifiedThisFrame = true;
								onValueChanged(latestState->internalValues);
							},
							.padding = CLAY_PADDING_ALL(6),
							.sizing = context.params.colorHexInputSizing,
							.borderColor = FlowUi::Flow_Color("#8f8d8dff"),
							.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0},
							.backgroundColor = FlowUi::Flow_Color("#252932ff"),
							.cornerRadius = CLAY_CORNER_RADIUS(6),
							.clipHorizontal = false,
							.clipVertical = false,
							.childTextAlignment = {
								.x = CLAY_ALIGN_X_LEFT,
								.y = CLAY_ALIGN_Y_CENTER,
							},
							.textWrapMode = CLAY_TEXT_WRAP_NONE,
							.textAlignment = CLAY_TEXT_ALIGN_LEFT,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.textColor = context.params.valueTextColor,
						})
						.draw();
				};
			};
			return;
		}

		if (context.params.useNineSplitCorners)
		{
			std::array<double, 9> slotValues{};
			slotValues.fill(0.0);
			slotValues[0] = context.params.firstValue;
			slotValues[2] = context.params.secondValue;
			slotValues[6] = context.params.thirdValue;
			slotValues[8] = context.params.fourthValue;

			Clay_ElementDeclaration root{};
			root.id = context.uiManager.toClayEID(context.elementID);
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

			CLAY(root){
				CLAY({.id = context.uiManager.toClayEID(context.createChildElementId("nine-split-hint"))}){
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.nineSplitHintText),
						CLAY_TEXT_CONFIG(hintTextConfig));
				};

				context.uiManager
					.createElement(kDevNineSplit, context.createChildElementId("nine-split"))
					.setParameters(devNineSplitParams{
						.fieldIdPrefix = context.params.fieldIdPrefix.empty()
							? context.createChildElementId("nine-split/fields")
							: context.params.fieldIdPrefix + "/nine-split",
						.numericSlots = std::vector<uint8_t>{0, 2, 6, 8},
						.slotValues = slotValues,
						.valueKind = devNumericInputValueKind::Floating,
						.minValue = context.params.minValue,
						.maxValue = context.params.maxValue,
						.floatRatePerPixel = context.params.dragRatePerPixel,
						.onValuesChangedCallback = [onQuadChanged = context.params.onValueQuadChangedCallback](
							const std::array<double, 9>& values) {
							if (onQuadChanged != nullptr)
							{
								onQuadChanged(values[0], values[2], values[6], values[8]);
							}
						},
						.sizing = context.params.nineSplitSizing,
						.rowGap = 1,
						.columnGap = 1,
						.slotSizing = context.params.nineSplitSlotSizing,
						.numericSizing = context.params.nineSplitNumericSizing,
						.fontId = context.params.fontId,
						.fontSize = context.params.fontSize,
						.valueTextColor = context.params.valueTextColor,
					})
					.draw();
			};
			return;
		}

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
