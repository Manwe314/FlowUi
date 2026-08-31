#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicInputField.hpp"
#include "devMode/devFlowElements/devBasicToggle.hpp"
#include "devMode/devFlowElements/devEdgeU16Input.hpp"
#include "devMode/devFlowElements/devEnum1Input.hpp"
#include "devMode/devFlowElements/devEnum2Input.hpp"
#include "devMode/devFlowElements/devFloat2Input.hpp"
#include "devMode/devFlowElements/devFloat4Input.hpp"
#include "devMode/devFlowElements/devNumericInput.hpp"
#include "devMode/devFlowElements/devTaggedUnionInput.hpp"
#include "devMode/devTypes/devCompositeStruct.hpp"
#include "devMode/devTypes/devEnum2.hpp"
#include "devMode/devTypes/devTaggedUnion.hpp"

inline std::string devCompositeStructEnumUserFacingTailName(std::string_view value) {
	const std::size_t scopeSeparator = value.rfind("::");
	if (scopeSeparator == std::string_view::npos || (scopeSeparator + 2u) >= value.size())
	{
		return std::string(value);
	}
	return std::string(value.substr(scopeSeparator + 2u));
}

inline std::string devCompositeStructHumanizeClayValue(std::string_view value) {
	std::string normalized(value);
	constexpr std::string_view kClayPrefix = "CLAY_";
	if (normalized.rfind(kClayPrefix, 0u) == 0u)
	{
		normalized.erase(0u, kClayPrefix.size());
	}
	while (!normalized.empty() && normalized.front() == '_')
	{
		normalized.erase(normalized.begin());
	}

	std::string humanized{};
	humanized.reserve(normalized.size());
	bool startOfWord = true;
	for (char raw : normalized)
	{
		if (raw == '_')
		{
			if (!humanized.empty() && humanized.back() != ' ')
			{
				humanized.push_back(' ');
			}
			startOfWord = true;
			continue;
		}

		const char lowered = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
		if (startOfWord)
		{
			humanized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(lowered))));
			startOfWord = false;
		}
		else
		{
			humanized.push_back(lowered);
		}
	}

	while (!humanized.empty() && humanized.back() == ' ')
	{
		humanized.pop_back();
	}
	return humanized.empty() ? std::string(value) : humanized;
}

inline std::string devCompositeStructEnumLabelForValue(
	uint64_t enumTypeHash,
	std::string_view value) {
	if (value.empty())
	{
		return {};
	}
	if (FlowUi::devMode::findDevEnum1BuiltinTypeInfo(enumTypeHash) != nullptr)
	{
		return devCompositeStructHumanizeClayValue(value);
	}
	return devCompositeStructEnumUserFacingTailName(value);
}

inline std::vector<devEnum1InputOption> devCompositeStructEnumOptionsForType(uint64_t enumTypeHash) {
	std::vector<devEnum1InputOption> options{};
	if (const FlowUi::devMode::DevEnum1TypeInfo* builtinInfo =
		FlowUi::devMode::findDevEnum1BuiltinTypeInfo(enumTypeHash))
	{
		options.reserve(builtinInfo->valueCount);
		for (std::size_t i = 0; i < builtinInfo->valueCount; ++i)
		{
			const std::string rawName(builtinInfo->values[i].name);
			options.push_back(devEnum1InputOption{
				.value = rawName,
				.label = devCompositeStructEnumLabelForValue(enumTypeHash, rawName),
			});
		}
		return options;
	}

	const FlowUi::devMode::DevRegistry& registry = FlowUi::devMode::DevRegistry::instance();
	const FlowUi::devMode::EnumDescriptor* descriptor = registry.findEnumByTypeHash(enumTypeHash);
	if (descriptor == nullptr)
	{
		return options;
	}

	options.reserve(descriptor->values.size());
	for (const FlowUi::devMode::EnumValueDescriptor& valueDescriptor : descriptor->values)
	{
		options.push_back(devEnum1InputOption{
			.value = valueDescriptor.name,
			.label = devCompositeStructEnumLabelForValue(enumTypeHash, valueDescriptor.name),
		});
	}
	return options;
}

inline std::string devCompositeStructEnumValueFromNumeric(
	uint64_t enumTypeHash,
	uint8_t numeric) {
	std::string_view enumName{};
	if (FlowUi::devMode::tryDevEnum1ValueToName(enumTypeHash, numeric, enumName))
	{
		return std::string(enumName);
	}
	return {};
}

inline void devCompositeStructEnum2HintTexts(
	uint64_t fieldTypeHash,
	std::string& outFirstHintText,
	std::string& outSecondHintText) {
	if (fieldTypeHash == FlowUi::devMode::typeHash<Clay_ChildAlignment>())
	{
		outFirstHintText = "X direction";
		outSecondHintText = "Y direction";
		return;
	}
	if (fieldTypeHash == FlowUi::devMode::typeHash<Clay_FloatingAttachPoints>())
	{
		outFirstHintText = "element";
		outSecondHintText = "parent";
		return;
	}

	const FlowUi::devMode::DevEnum2TypeInfo* info =
		FlowUi::devMode::findDevEnum2TypeInfo(fieldTypeHash);
	if (info == nullptr)
	{
		outFirstHintText = "first";
		outSecondHintText = "second";
		return;
	}

	outFirstHintText = std::string(info->firstFieldName);
	outSecondHintText = std::string(info->secondFieldName);
}

inline FlowUi::devMode::DevCompositeStructValue devCompositeStructNormalizeOrDefault(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevCompositeStructValue& source) {
	FlowUi::devMode::DevCompositeStructValue normalized{};
	if (FlowUi::devMode::tryMakeDevCompositeStructValue(fieldTypeHash, source, normalized))
	{
		return normalized;
	}

	FlowUi::devMode::DevCompositeStructValue fallback{};
	fallback.typeHash = fieldTypeHash;
	if (FlowUi::devMode::tryMakeDevCompositeStructValue(fieldTypeHash, fallback, normalized))
	{
		return normalized;
	}
	return fallback;
}

struct devCompositeStructInputParams {
	std::string fieldIdPrefix = "";
	uint64_t fieldTypeHash = 0u;
	FlowUi::devMode::DevCompositeStructValue value{};
	std::function<void(const FlowUi::devMode::DevCompositeStructValue&)> onValueChangedCallback = nullptr;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(0);
	uint16_t childGap = 8;
	uint16_t fieldGap = 4;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	Clay_Sizing inputSizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};

	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color hintTextColor = FlowUi::Flow_Color("#a8b4ccff");
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");
};

using DevCompositeStructInputDef = FlowUi::ElementDefinition<
	devCompositeStructInputParams,
	void,
	void,
	FLOW_DEF_ID("DevCompositeStructInput"),
	true>;

inline const DevCompositeStructInputDef kDevCompositeStructInput = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevCompositeStructInputDef::BuildContext& context) {
		using FlowUi::devMode::DevCompositeStructValue;
		using FlowUi::devMode::DevEnum2Value;
		using FlowUi::devMode::DevFloat2Value;
		using FlowUi::devMode::DevFloat4Value;
		using FlowUi::devMode::DevTaggedUnionValue;
		using FlowUi::devMode::typeHash;

		const double kFloatMin = static_cast<double>(-std::numeric_limits<float>::max());
		const double kFloatMax = static_cast<double>(std::numeric_limits<float>::max());

		const DevCompositeStructValue normalizedValue =
			devCompositeStructNormalizeOrDefault(context.params.fieldTypeHash, context.params.value);

		const std::string fieldIdPrefix =
			context.params.fieldIdPrefix.empty()
			? context.createChildElementId("composite")
			: context.params.fieldIdPrefix;
		auto makeFieldIdPrefix = [&](std::string_view suffix) {
			return fieldIdPrefix + "/" + std::string(suffix);
		};

		auto emitChanged = [
			onValueChanged = context.params.onValueChangedCallback,
			fieldTypeHash = context.params.fieldTypeHash
		](const DevCompositeStructValue& candidate) {
			if (onValueChanged == nullptr)
			{
				return;
			}
			DevCompositeStructValue normalized{};
			if (!FlowUi::devMode::tryMakeDevCompositeStructValue(fieldTypeHash, candidate, normalized))
			{
				return;
			}
			onValueChanged(normalized);
		};

		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
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

		auto drawLabeledField = [&](std::string_view localId, std::string_view label, auto&& drawInput) {
			Clay_ElementDeclaration fieldRoot{};
			const Clay_ElementId fieldRootId = context.uiManager.toClayEID(context.createChildElementId(std::string(localId)));
			fieldRoot.layout.sizing = context.params.inputSizing;
			fieldRoot.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			fieldRoot.layout.childGap = context.params.fieldGap;
			fieldRoot.layout.childAlignment = {
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_TOP,
			};
			fieldRoot.backgroundColor = FlowUi::Flow_Color("#00000000");

				CLAY(fieldRootId, fieldRoot){
					if (!label.empty())
					{
						CLAY(context.uiManager.toClayEID(
							context.createChildElementId(std::string(localId) + "/hint")), {}){
							CLAY_TEXT(
								context.uiManager.toClayString(std::string(label)),
								CLAY_TEXT_CONFIG(hintTextConfig));
					};
				}
				drawInput();
			};
		};

		CLAY(rootId, root){
			if (context.params.fieldTypeHash == typeHash<Clay_Sizing>())
			{
				auto drawSizingAxis = [&](std::string_view axisId, std::string_view axisLabel, const DevTaggedUnionValue& axisValue, bool isWidth) {
					drawLabeledField(axisId, axisLabel, [&](){
						const uint64_t tagEnumTypeHash = typeHash<Clay__SizingType>();
						const std::vector<devEnum1InputOption> options =
							devCompositeStructEnumOptionsForType(tagEnumTypeHash);
						const std::string selectedTagValue =
							devCompositeStructEnumValueFromNumeric(tagEnumTypeHash, axisValue.tag.numeric);
						const std::string selectedTagLabel =
							devCompositeStructEnumLabelForValue(tagEnumTypeHash, selectedTagValue);

						devTaggedUnionInputParams taggedParams{};
						taggedParams.fieldIdPrefix = makeFieldIdPrefix(std::string(axisId));
						taggedParams.selectedTagValue = selectedTagValue;
						taggedParams.selectedTagLabel = selectedTagLabel;
						taggedParams.tagOptions = options;
						taggedParams.usePercentEditor = FlowUi::devMode::devSizingAxisTagUsesPercent(axisValue.tag.numeric);
						taggedParams.minValue = axisValue.minMax.first;
						taggedParams.maxValue = axisValue.minMax.second;
						taggedParams.percentValue = axisValue.percent;
						taggedParams.sizing = context.params.inputSizing;
						taggedParams.fontId = context.params.fontId;
						taggedParams.fontSize = context.params.fontSize;
						taggedParams.hintTextColor = context.params.hintTextColor;
						taggedParams.valueTextColor = context.params.valueTextColor;
						taggedParams.onTagChangedCallback = [
							emitChanged,
							normalizedValue,
							isWidth,
							tagEnumTypeHash
						](std::string_view changedTagName) {
							uint8_t tagNumeric = 0u;
							if (!FlowUi::devMode::tryDevEnum1NameToValue(tagEnumTypeHash, changedTagName, tagNumeric))
							{
								return;
							}
							DevCompositeStructValue updated = normalizedValue;
							DevTaggedUnionValue& axis = isWidth ? updated.sizing.width : updated.sizing.height;
							axis.tag.numeric = tagNumeric;
							if (FlowUi::devMode::devSizingAxisTagUsesPercent(tagNumeric))
							{
								axis.minMax = DevFloat2Value{};
							}
							else
							{
								axis.percent = 0.0;
							}
							emitChanged(updated);
						};
						taggedParams.onMinMaxChangedCallback = [
							emitChanged,
							normalizedValue,
							isWidth
						](double minValue, double maxValue) {
							DevCompositeStructValue updated = normalizedValue;
							DevTaggedUnionValue& axis = isWidth ? updated.sizing.width : updated.sizing.height;
							axis.minMax.first = minValue;
							axis.minMax.second = maxValue;
							emitChanged(updated);
						};
						taggedParams.onPercentChangedCallback = [
							emitChanged,
							normalizedValue,
							isWidth
						](double percentValue) {
							DevCompositeStructValue updated = normalizedValue;
							DevTaggedUnionValue& axis = isWidth ? updated.sizing.width : updated.sizing.height;
							axis.percent = percentValue;
							emitChanged(updated);
						};

						context.uiManager
							.createElement(kDevTaggedUnionInput, context.createChildElementId(std::string(axisId) + "/input"))
							.setParameters(taggedParams)
							.draw();
					});
				};

				drawSizingAxis("width", "Width:", normalizedValue.sizing.width, true);
				drawSizingAxis("height", "Height:", normalizedValue.sizing.height, false);
			}
			else if (context.params.fieldTypeHash == typeHash<Clay_LayoutConfig>())
			{
				drawLabeledField("sizing", "Sizing:", [&](){
					DevCompositeStructValue nested{};
					nested.typeHash = typeHash<Clay_Sizing>();
					nested.sizing = normalizedValue.layoutConfig.sizing;

					devCompositeStructInputParams nestedParams{};
					nestedParams.fieldIdPrefix = makeFieldIdPrefix("sizing");
					nestedParams.fieldTypeHash = typeHash<Clay_Sizing>();
					nestedParams.value = nested;
					nestedParams.sizing = context.params.inputSizing;
					nestedParams.fontId = context.params.fontId;
					nestedParams.fontSize = context.params.fontSize;
					nestedParams.hintTextColor = context.params.hintTextColor;
					nestedParams.valueTextColor = context.params.valueTextColor;
					nestedParams.onValueChangedCallback = [emitChanged, normalizedValue](const DevCompositeStructValue& changed) {
						DevCompositeStructValue updated = normalizedValue;
						updated.layoutConfig.sizing = changed.sizing;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevCompositeStructInput, context.createChildElementId("sizing/input"))
						.setParameters(nestedParams)
						.draw();
				});

				drawLabeledField("padding", "Padding:", [&](){
					devEdgeU16InputParams edgeParams{};
					edgeParams.fieldIdPrefix = makeFieldIdPrefix("padding");
					edgeParams.firstHintText = "left";
					edgeParams.secondHintText = "right";
					edgeParams.thirdHintText = "top";
					edgeParams.fourthHintText = "bottom";
					edgeParams.firstValue = static_cast<int64_t>(normalizedValue.layoutConfig.padding.first);
					edgeParams.secondValue = static_cast<int64_t>(normalizedValue.layoutConfig.padding.second);
					edgeParams.thirdValue = static_cast<int64_t>(normalizedValue.layoutConfig.padding.third);
					edgeParams.fourthValue = static_cast<int64_t>(normalizedValue.layoutConfig.padding.fourth);
					edgeParams.useNineSplitEdges = true;
					edgeParams.nineSplitHintText = "";
					edgeParams.sizing = context.params.inputSizing;
					edgeParams.fontId = context.params.fontId;
					edgeParams.fontSize = context.params.fontSize;
					edgeParams.hintTextColor = context.params.hintTextColor;
					edgeParams.valueTextColor = context.params.valueTextColor;
					edgeParams.onValueChangedCallback = [emitChanged, normalizedValue](
						int64_t first,
						int64_t second,
						int64_t third,
						int64_t fourth,
						int64_t) {
						DevCompositeStructValue updated = normalizedValue;
						updated.layoutConfig.padding.first = static_cast<uint16_t>(std::max<int64_t>(0, first));
						updated.layoutConfig.padding.second = static_cast<uint16_t>(std::max<int64_t>(0, second));
						updated.layoutConfig.padding.third = static_cast<uint16_t>(std::max<int64_t>(0, third));
						updated.layoutConfig.padding.fourth = static_cast<uint16_t>(std::max<int64_t>(0, fourth));
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevEdgeU16Input, context.createChildElementId("padding/input"))
						.setParameters(edgeParams)
						.draw();
				});

				drawLabeledField("child-gap", "Child Gap:", [&](){
					devNumericInputParams numericParams{};
					numericParams.fieldId = makeFieldIdPrefix("child-gap");
					numericParams.initialText = devNumericValueToText(
						devNumericInputValueKind::UnsignedInt,
						static_cast<double>(normalizedValue.layoutConfig.childGap));
					numericParams.showHint = false;
					numericParams.valueKind = devNumericInputValueKind::UnsignedInt;
					numericParams.minValue = 0.0;
					numericParams.maxValue = static_cast<double>(std::numeric_limits<uint16_t>::max());
					numericParams.sizing = context.params.inputSizing;
					numericParams.fontId = context.params.fontId;
					numericParams.fontSize = context.params.fontSize;
					numericParams.valueTextColor = context.params.valueTextColor;
					numericParams.onValueChangedCallback = [emitChanged, normalizedValue](double changedValue) {
						DevCompositeStructValue updated = normalizedValue;
						updated.layoutConfig.childGap = static_cast<uint16_t>(std::llround(changedValue));
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevNumericInput, context.createChildElementId("child-gap/input"))
						.setParameters(numericParams)
						.draw();
				});

				drawLabeledField("child-alignment", "Child Alignment:", [&](){
					const uint64_t enum2TypeHash = typeHash<Clay_ChildAlignment>();
					const FlowUi::devMode::DevEnum2TypeInfo* enum2Info =
						FlowUi::devMode::findDevEnum2TypeInfo(enum2TypeHash);
					if (enum2Info == nullptr)
					{
						return;
					}

					std::string firstHint = "first";
					std::string secondHint = "second";
					devCompositeStructEnum2HintTexts(enum2TypeHash, firstHint, secondHint);

					const std::string firstValue = devCompositeStructEnumValueFromNumeric(
						enum2Info->firstEnumTypeHash,
						normalizedValue.layoutConfig.childAlignment.first.numeric);
					const std::string secondValue = devCompositeStructEnumValueFromNumeric(
						enum2Info->secondEnumTypeHash,
						normalizedValue.layoutConfig.childAlignment.second.numeric);

					devEnum2InputParams enum2Params{};
					enum2Params.firstHintText = firstHint;
					enum2Params.secondHintText = secondHint;
					enum2Params.firstSelectedValue = firstValue;
					enum2Params.firstSelectedLabel =
						devCompositeStructEnumLabelForValue(enum2Info->firstEnumTypeHash, firstValue);
					enum2Params.secondSelectedValue = secondValue;
					enum2Params.secondSelectedLabel =
						devCompositeStructEnumLabelForValue(enum2Info->secondEnumTypeHash, secondValue);
					enum2Params.firstOptions =
						devCompositeStructEnumOptionsForType(enum2Info->firstEnumTypeHash);
					enum2Params.secondOptions =
						devCompositeStructEnumOptionsForType(enum2Info->secondEnumTypeHash);
					enum2Params.sizing = context.params.inputSizing;
					enum2Params.fontId = context.params.fontId;
					enum2Params.fontSize = context.params.fontSize;
					enum2Params.hintTextColor = context.params.hintTextColor;
					enum2Params.valueTextColor = context.params.valueTextColor;
					enum2Params.onValuePairChangedCallback = [
						emitChanged,
						normalizedValue,
						enum2TypeHash
					](std::string_view firstName, std::string_view secondName) {
						DevEnum2Value parsed{};
						if (!FlowUi::devMode::tryDevEnum2NamesToValue(enum2TypeHash, firstName, secondName, parsed))
						{
							return;
						}
						DevCompositeStructValue updated = normalizedValue;
						updated.layoutConfig.childAlignment = parsed;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevEnum2Input, context.createChildElementId("child-alignment/input"))
						.setParameters(enum2Params)
						.draw();
				});

				drawLabeledField("layout-direction", "Layout Direction:", [&](){
					const uint64_t enumTypeHash = typeHash<Clay_LayoutDirection>();
					const std::string selectedValue = devCompositeStructEnumValueFromNumeric(
						enumTypeHash,
						normalizedValue.layoutConfig.layoutDirection.numeric);

					devEnum1InputParams enumParams{};
					enumParams.hintText = "";
					enumParams.selectedValue = selectedValue;
					enumParams.selectedLabel = devCompositeStructEnumLabelForValue(enumTypeHash, selectedValue);
					enumParams.options = devCompositeStructEnumOptionsForType(enumTypeHash);
					enumParams.sizing = context.params.inputSizing;
					enumParams.fontId = context.params.fontId;
					enumParams.fontSize = context.params.fontSize;
					enumParams.hintTextColor = context.params.hintTextColor;
					enumParams.valueTextColor = context.params.valueTextColor;
					enumParams.onValueChangedCallback = [emitChanged, normalizedValue, enumTypeHash](std::string_view changedName) {
						uint8_t numeric = 0u;
						if (!FlowUi::devMode::tryDevEnum1NameToValue(enumTypeHash, changedName, numeric))
						{
							return;
						}
						DevCompositeStructValue updated = normalizedValue;
						updated.layoutConfig.layoutDirection.numeric = numeric;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevEnum1Input, context.createChildElementId("layout-direction/input"))
						.setParameters(enumParams)
						.draw();
				});
			}
			else if (context.params.fieldTypeHash == typeHash<Clay_TextElementConfig>())
			{
				drawLabeledField("user-data", "User Data:", [&](){
					devNumericInputParams numericParams{};
					numericParams.fieldId = makeFieldIdPrefix("user-data");
					numericParams.initialText = devNumericValueToText(
						devNumericInputValueKind::UnsignedInt,
						static_cast<double>(normalizedValue.textElementConfig.userData.bits));
					numericParams.showHint = false;
					numericParams.valueKind = devNumericInputValueKind::UnsignedInt;
					numericParams.minValue = 0.0;
					numericParams.maxValue = static_cast<double>(std::numeric_limits<int64_t>::max());
					numericParams.sizing = context.params.inputSizing;
					numericParams.fontId = context.params.fontId;
					numericParams.fontSize = context.params.fontSize;
					numericParams.valueTextColor = context.params.valueTextColor;
					numericParams.onValueChangedCallback = [emitChanged, normalizedValue](double changedValue) {
						DevCompositeStructValue updated = normalizedValue;
						updated.textElementConfig.userData.bits = static_cast<uint64_t>(std::llround(changedValue));
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevNumericInput, context.createChildElementId("user-data/input"))
						.setParameters(numericParams)
						.draw();
				});

				drawLabeledField("text-color", "Text Color:", [&](){
					devFloat4InputParams float4Params{};
					float4Params.fieldIdPrefix = makeFieldIdPrefix("text-color");
					float4Params.firstHintText = "Red";
					float4Params.secondHintText = "Green";
					float4Params.thirdHintText = "Blue";
					float4Params.fourthHintText = "Alpha";
					float4Params.firstValue = normalizedValue.textElementConfig.textColor.first;
					float4Params.secondValue = normalizedValue.textElementConfig.textColor.second;
					float4Params.thirdValue = normalizedValue.textElementConfig.textColor.third;
					float4Params.fourthValue = normalizedValue.textElementConfig.textColor.fourth;
					float4Params.useColorEditor = true;
					float4Params.sizing = context.params.inputSizing;
					float4Params.fontId = context.params.fontId;
					float4Params.fontSize = context.params.fontSize;
					float4Params.hintTextColor = context.params.hintTextColor;
					float4Params.valueTextColor = context.params.valueTextColor;
					float4Params.onValueQuadChangedCallback = [
						emitChanged,
						normalizedValue
					](double first, double second, double third, double fourth) {
						DevFloat4Value parsed{};
						if (!FlowUi::devMode::tryMakeDevFloat4Value(typeHash<Clay_Color>(), first, second, third, fourth, parsed))
						{
							return;
						}
						DevCompositeStructValue updated = normalizedValue;
						updated.textElementConfig.textColor = parsed;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevFloat4Input, context.createChildElementId("text-color/input"))
						.setParameters(float4Params)
						.draw();
				});

				auto drawUnsignedTextNumeric = [&](std::string_view fieldId, std::string_view label, uint16_t value, auto applyValue) {
					drawLabeledField(fieldId, label, [&](){
						devNumericInputParams numericParams{};
						numericParams.fieldId = makeFieldIdPrefix(std::string(fieldId));
						numericParams.initialText = devNumericValueToText(
							devNumericInputValueKind::UnsignedInt,
							static_cast<double>(value));
						numericParams.showHint = false;
						numericParams.valueKind = devNumericInputValueKind::UnsignedInt;
						numericParams.minValue = 0.0;
						numericParams.maxValue = static_cast<double>(std::numeric_limits<uint16_t>::max());
						numericParams.sizing = context.params.inputSizing;
						numericParams.fontId = context.params.fontId;
						numericParams.fontSize = context.params.fontSize;
						numericParams.valueTextColor = context.params.valueTextColor;
						numericParams.onValueChangedCallback = [emitChanged, normalizedValue, applyValue](double changedValue) {
							DevCompositeStructValue updated = normalizedValue;
							applyValue(updated, static_cast<uint16_t>(std::llround(changedValue)));
							emitChanged(updated);
						};

						context.uiManager
							.createElement(kDevNumericInput, context.createChildElementId(std::string(fieldId) + "/input"))
							.setParameters(numericParams)
							.draw();
					});
				};

				drawUnsignedTextNumeric(
					"font-id",
					"Font Id:",
					normalizedValue.textElementConfig.fontId,
					[](DevCompositeStructValue& updated, uint16_t changed) {
						updated.textElementConfig.fontId = changed;
					});
				drawUnsignedTextNumeric(
					"font-size",
					"Font Size:",
					normalizedValue.textElementConfig.fontSize,
					[](DevCompositeStructValue& updated, uint16_t changed) {
						updated.textElementConfig.fontSize = changed;
					});
				drawUnsignedTextNumeric(
					"letter-spacing",
					"Letter Spacing:",
					normalizedValue.textElementConfig.letterSpacing,
					[](DevCompositeStructValue& updated, uint16_t changed) {
						updated.textElementConfig.letterSpacing = changed;
					});
				drawUnsignedTextNumeric(
					"line-height",
					"Line Height:",
					normalizedValue.textElementConfig.lineHeight,
					[](DevCompositeStructValue& updated, uint16_t changed) {
						updated.textElementConfig.lineHeight = changed;
					});

				auto drawTextEnum = [&](std::string_view fieldId, std::string_view label, uint64_t enumTypeHash, uint8_t currentNumeric, auto applyNumeric) {
					drawLabeledField(fieldId, label, [&](){
						const std::string selectedValue =
							devCompositeStructEnumValueFromNumeric(enumTypeHash, currentNumeric);

						devEnum1InputParams enumParams{};
						enumParams.hintText = "";
						enumParams.selectedValue = selectedValue;
						enumParams.selectedLabel =
							devCompositeStructEnumLabelForValue(enumTypeHash, selectedValue);
						enumParams.options = devCompositeStructEnumOptionsForType(enumTypeHash);
						enumParams.sizing = context.params.inputSizing;
						enumParams.fontId = context.params.fontId;
						enumParams.fontSize = context.params.fontSize;
						enumParams.hintTextColor = context.params.hintTextColor;
						enumParams.valueTextColor = context.params.valueTextColor;
						enumParams.onValueChangedCallback = [
							emitChanged,
							normalizedValue,
							enumTypeHash,
							applyNumeric
						](std::string_view changedName) {
							uint8_t numeric = 0u;
							if (!FlowUi::devMode::tryDevEnum1NameToValue(enumTypeHash, changedName, numeric))
							{
								return;
							}
							DevCompositeStructValue updated = normalizedValue;
							applyNumeric(updated, numeric);
							emitChanged(updated);
						};

						context.uiManager
							.createElement(kDevEnum1Input, context.createChildElementId(std::string(fieldId) + "/input"))
							.setParameters(enumParams)
							.draw();
					});
				};

				drawTextEnum(
					"wrap-mode",
					"Wrap Mode:",
					typeHash<Clay_TextElementConfigWrapMode>(),
					normalizedValue.textElementConfig.wrapMode.numeric,
					[](DevCompositeStructValue& updated, uint8_t changed) {
						updated.textElementConfig.wrapMode.numeric = changed;
					});
				drawTextEnum(
					"text-alignment",
					"Text Alignment:",
					typeHash<Clay_TextAlignment>(),
					normalizedValue.textElementConfig.textAlignment.numeric,
					[](DevCompositeStructValue& updated, uint8_t changed) {
						updated.textElementConfig.textAlignment.numeric = changed;
					});
			}
			else if (context.params.fieldTypeHash == typeHash<Clay_FloatingElementConfig>())
			{
				auto drawFloat2Field = [&](std::string_view fieldId, std::string_view label, const DevFloat2Value& value, auto applyValue, std::string_view firstHint, std::string_view secondHint) {
					drawLabeledField(fieldId, label, [&](){
						devFloat2InputParams float2Params{};
						float2Params.fieldIdPrefix = makeFieldIdPrefix(std::string(fieldId));
						float2Params.firstHintText = std::string(firstHint);
						float2Params.secondHintText = std::string(secondHint);
						float2Params.firstValue = value.first;
						float2Params.secondValue = value.second;
						float2Params.minValue = kFloatMin;
						float2Params.maxValue = kFloatMax;
						float2Params.sizing = context.params.inputSizing;
						float2Params.fontId = context.params.fontId;
						float2Params.fontSize = context.params.fontSize;
						float2Params.hintTextColor = context.params.hintTextColor;
						float2Params.valueTextColor = context.params.valueTextColor;
						float2Params.onValuePairChangedCallback = [emitChanged, normalizedValue, applyValue](double first, double second) {
							DevCompositeStructValue updated = normalizedValue;
							applyValue(updated, DevFloat2Value{.first = first, .second = second});
							emitChanged(updated);
						};

						context.uiManager
							.createElement(kDevFloat2Input, context.createChildElementId(std::string(fieldId) + "/input"))
							.setParameters(float2Params)
							.draw();
					});
				};

				drawFloat2Field(
					"offset",
					"Offset:",
					normalizedValue.floatingElementConfig.offset,
					[](DevCompositeStructValue& updated, const DevFloat2Value& changed) {
						updated.floatingElementConfig.offset = changed;
					},
					"X",
					"Y");

				drawFloat2Field(
					"expand",
					"Expand:",
					normalizedValue.floatingElementConfig.expand,
					[](DevCompositeStructValue& updated, const DevFloat2Value& changed) {
						updated.floatingElementConfig.expand = changed;
					},
					"Width",
					"Height");

				drawLabeledField("parent-id", "Parent Id:", [&](){
					devNumericInputParams numericParams{};
					numericParams.fieldId = makeFieldIdPrefix("parent-id");
					numericParams.initialText = devNumericValueToText(
						devNumericInputValueKind::UnsignedInt,
						static_cast<double>(normalizedValue.floatingElementConfig.parentId));
					numericParams.showHint = false;
					numericParams.valueKind = devNumericInputValueKind::UnsignedInt;
					numericParams.minValue = 0.0;
					numericParams.maxValue = static_cast<double>(std::numeric_limits<uint32_t>::max());
					numericParams.sizing = context.params.inputSizing;
					numericParams.fontId = context.params.fontId;
					numericParams.fontSize = context.params.fontSize;
					numericParams.valueTextColor = context.params.valueTextColor;
					numericParams.onValueChangedCallback = [emitChanged, normalizedValue](double changedValue) {
						DevCompositeStructValue updated = normalizedValue;
						updated.floatingElementConfig.parentId = static_cast<uint32_t>(std::llround(changedValue));
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevNumericInput, context.createChildElementId("parent-id/input"))
						.setParameters(numericParams)
						.draw();
				});

				drawLabeledField("z-index", "Z Index:", [&](){
					devNumericInputParams numericParams{};
					numericParams.fieldId = makeFieldIdPrefix("z-index");
					numericParams.initialText = devNumericValueToText(
						devNumericInputValueKind::SignedInt,
						static_cast<double>(normalizedValue.floatingElementConfig.zIndex));
					numericParams.showHint = false;
					numericParams.valueKind = devNumericInputValueKind::SignedInt;
					numericParams.minValue = static_cast<double>(std::numeric_limits<int16_t>::min());
					numericParams.maxValue = static_cast<double>(std::numeric_limits<int16_t>::max());
					numericParams.sizing = context.params.inputSizing;
					numericParams.fontId = context.params.fontId;
					numericParams.fontSize = context.params.fontSize;
					numericParams.valueTextColor = context.params.valueTextColor;
					numericParams.onValueChangedCallback = [emitChanged, normalizedValue](double changedValue) {
						DevCompositeStructValue updated = normalizedValue;
						updated.floatingElementConfig.zIndex = static_cast<int16_t>(std::llround(changedValue));
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevNumericInput, context.createChildElementId("z-index/input"))
						.setParameters(numericParams)
						.draw();
				});

				drawLabeledField("attach-points", "Attach Points:", [&](){
					const uint64_t enum2TypeHash = typeHash<Clay_FloatingAttachPoints>();
					const FlowUi::devMode::DevEnum2TypeInfo* enum2Info =
						FlowUi::devMode::findDevEnum2TypeInfo(enum2TypeHash);
					if (enum2Info == nullptr)
					{
						return;
					}

					std::string firstHint = "first";
					std::string secondHint = "second";
					devCompositeStructEnum2HintTexts(enum2TypeHash, firstHint, secondHint);

					const std::string firstValue = devCompositeStructEnumValueFromNumeric(
						enum2Info->firstEnumTypeHash,
						normalizedValue.floatingElementConfig.attachPoints.first.numeric);
					const std::string secondValue = devCompositeStructEnumValueFromNumeric(
						enum2Info->secondEnumTypeHash,
						normalizedValue.floatingElementConfig.attachPoints.second.numeric);

					devEnum2InputParams enum2Params{};
					enum2Params.firstHintText = firstHint;
					enum2Params.secondHintText = secondHint;
					enum2Params.firstSelectedValue = firstValue;
					enum2Params.firstSelectedLabel =
						devCompositeStructEnumLabelForValue(enum2Info->firstEnumTypeHash, firstValue);
					enum2Params.secondSelectedValue = secondValue;
					enum2Params.secondSelectedLabel =
						devCompositeStructEnumLabelForValue(enum2Info->secondEnumTypeHash, secondValue);
					enum2Params.firstOptions =
						devCompositeStructEnumOptionsForType(enum2Info->firstEnumTypeHash);
					enum2Params.secondOptions =
						devCompositeStructEnumOptionsForType(enum2Info->secondEnumTypeHash);
					enum2Params.sizing = context.params.inputSizing;
					enum2Params.fontId = context.params.fontId;
					enum2Params.fontSize = context.params.fontSize;
					enum2Params.hintTextColor = context.params.hintTextColor;
					enum2Params.valueTextColor = context.params.valueTextColor;
					enum2Params.onValuePairChangedCallback = [
						emitChanged,
						normalizedValue,
						enum2TypeHash
					](std::string_view firstName, std::string_view secondName) {
						DevEnum2Value parsed{};
						if (!FlowUi::devMode::tryDevEnum2NamesToValue(enum2TypeHash, firstName, secondName, parsed))
						{
							return;
						}
						DevCompositeStructValue updated = normalizedValue;
						updated.floatingElementConfig.attachPoints = parsed;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevEnum2Input, context.createChildElementId("attach-points/input"))
						.setParameters(enum2Params)
						.draw();
				});

				auto drawFloatingEnum = [&](std::string_view fieldId, std::string_view label, uint64_t enumTypeHash, uint8_t currentNumeric, auto applyNumeric) {
					drawLabeledField(fieldId, label, [&](){
						const std::string selectedValue =
							devCompositeStructEnumValueFromNumeric(enumTypeHash, currentNumeric);

						devEnum1InputParams enumParams{};
						enumParams.hintText = "";
						enumParams.selectedValue = selectedValue;
						enumParams.selectedLabel =
							devCompositeStructEnumLabelForValue(enumTypeHash, selectedValue);
						enumParams.options = devCompositeStructEnumOptionsForType(enumTypeHash);
						enumParams.sizing = context.params.inputSizing;
						enumParams.fontId = context.params.fontId;
						enumParams.fontSize = context.params.fontSize;
						enumParams.hintTextColor = context.params.hintTextColor;
						enumParams.valueTextColor = context.params.valueTextColor;
						enumParams.onValueChangedCallback = [
							emitChanged,
							normalizedValue,
							enumTypeHash,
							applyNumeric
						](std::string_view changedName) {
							uint8_t numeric = 0u;
							if (!FlowUi::devMode::tryDevEnum1NameToValue(enumTypeHash, changedName, numeric))
							{
								return;
							}
							DevCompositeStructValue updated = normalizedValue;
							applyNumeric(updated, numeric);
							emitChanged(updated);
						};

						context.uiManager
							.createElement(kDevEnum1Input, context.createChildElementId(std::string(fieldId) + "/input"))
							.setParameters(enumParams)
							.draw();
					});
				};

				drawFloatingEnum(
					"pointer-capture-mode",
					"Pointer Capture Mode:",
					typeHash<Clay_PointerCaptureMode>(),
					normalizedValue.floatingElementConfig.pointerCaptureMode.numeric,
					[](DevCompositeStructValue& updated, uint8_t changed) {
						updated.floatingElementConfig.pointerCaptureMode.numeric = changed;
					});
				drawFloatingEnum(
					"attach-to",
					"Attach To:",
					typeHash<Clay_FloatingAttachToElement>(),
					normalizedValue.floatingElementConfig.attachTo.numeric,
					[](DevCompositeStructValue& updated, uint8_t changed) {
						updated.floatingElementConfig.attachTo.numeric = changed;
					});
				drawFloatingEnum(
					"clip-to",
					"Clip To:",
					typeHash<Clay_FloatingClipToElement>(),
					normalizedValue.floatingElementConfig.clipTo.numeric,
					[](DevCompositeStructValue& updated, uint8_t changed) {
						updated.floatingElementConfig.clipTo.numeric = changed;
					});
			}
			else if (context.params.fieldTypeHash == typeHash<Clay_ClipElementConfig>())
			{
				drawLabeledField("horizontal", "Horizontal:", [&](){
					devBasicToggleParams toggleParams{};
					toggleParams.defaultEnabled = normalizedValue.clipElementConfig.horizontal;
					toggleParams.sizing = {
						.width = CLAY_SIZING_FIXED(22),
						.height = CLAY_SIZING_FIXED(22),
					};
					toggleParams.fontId = context.params.fontId;
					toggleParams.fontSize = context.params.fontSize;
					toggleParams.onValueChangedCallback = [emitChanged, normalizedValue](DevBasicToggleInteractionContext, bool enabled) {
						DevCompositeStructValue updated = normalizedValue;
						updated.clipElementConfig.horizontal = enabled;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevBasicToggle, context.createChildElementId("horizontal/input"))
						.setParameters(toggleParams)
						.draw();
				});

				drawLabeledField("vertical", "Vertical:", [&](){
					devBasicToggleParams toggleParams{};
					toggleParams.defaultEnabled = normalizedValue.clipElementConfig.vertical;
					toggleParams.sizing = {
						.width = CLAY_SIZING_FIXED(22),
						.height = CLAY_SIZING_FIXED(22),
					};
					toggleParams.fontId = context.params.fontId;
					toggleParams.fontSize = context.params.fontSize;
					toggleParams.onValueChangedCallback = [emitChanged, normalizedValue](DevBasicToggleInteractionContext, bool enabled) {
						DevCompositeStructValue updated = normalizedValue;
						updated.clipElementConfig.vertical = enabled;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevBasicToggle, context.createChildElementId("vertical/input"))
						.setParameters(toggleParams)
						.draw();
				});

				drawLabeledField("scroll-input-disabled", "Disable Scroll Input:", [&](){
					devBasicToggleParams toggleParams{};
					toggleParams.defaultEnabled = normalizedValue.clipElementConfig.scrollInputDisabled;
					toggleParams.sizing = {
						.width = CLAY_SIZING_FIXED(22),
						.height = CLAY_SIZING_FIXED(22),
					};
					toggleParams.fontId = context.params.fontId;
					toggleParams.fontSize = context.params.fontSize;
					toggleParams.onValueChangedCallback = [emitChanged, normalizedValue](DevBasicToggleInteractionContext, bool disabled) {
						DevCompositeStructValue updated = normalizedValue;
						updated.clipElementConfig.scrollInputDisabled = disabled;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevBasicToggle, context.createChildElementId("scroll-input-disabled/input"))
						.setParameters(toggleParams)
						.draw();
				});

				drawLabeledField("child-offset", "Child Offset:", [&](){
					devFloat2InputParams float2Params{};
					float2Params.fieldIdPrefix = makeFieldIdPrefix("child-offset");
					float2Params.firstHintText = "X";
					float2Params.secondHintText = "Y";
					float2Params.firstValue = normalizedValue.clipElementConfig.childOffset.first;
					float2Params.secondValue = normalizedValue.clipElementConfig.childOffset.second;
					float2Params.minValue = kFloatMin;
					float2Params.maxValue = kFloatMax;
					float2Params.sizing = context.params.inputSizing;
					float2Params.fontId = context.params.fontId;
					float2Params.fontSize = context.params.fontSize;
					float2Params.hintTextColor = context.params.hintTextColor;
					float2Params.valueTextColor = context.params.valueTextColor;
					float2Params.onValuePairChangedCallback = [emitChanged, normalizedValue](double x, double y) {
						DevCompositeStructValue updated = normalizedValue;
						updated.clipElementConfig.childOffset = DevFloat2Value{.first = x, .second = y};
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevFloat2Input, context.createChildElementId("child-offset/input"))
						.setParameters(float2Params)
						.draw();
				});
			}
			else if (context.params.fieldTypeHash == typeHash<Clay_BorderElementConfig>())
			{
				drawLabeledField("color", "Color:", [&](){
					devFloat4InputParams float4Params{};
					float4Params.fieldIdPrefix = makeFieldIdPrefix("color");
					float4Params.firstHintText = "Red";
					float4Params.secondHintText = "Green";
					float4Params.thirdHintText = "Blue";
					float4Params.fourthHintText = "Alpha";
					float4Params.firstValue = normalizedValue.borderElementConfig.color.first;
					float4Params.secondValue = normalizedValue.borderElementConfig.color.second;
					float4Params.thirdValue = normalizedValue.borderElementConfig.color.third;
					float4Params.fourthValue = normalizedValue.borderElementConfig.color.fourth;
					float4Params.useColorEditor = true;
					float4Params.sizing = context.params.inputSizing;
					float4Params.fontId = context.params.fontId;
					float4Params.fontSize = context.params.fontSize;
					float4Params.hintTextColor = context.params.hintTextColor;
					float4Params.valueTextColor = context.params.valueTextColor;
					float4Params.onValueQuadChangedCallback = [
						emitChanged,
						normalizedValue
					](double first, double second, double third, double fourth) {
						DevFloat4Value parsed{};
						if (!FlowUi::devMode::tryMakeDevFloat4Value(typeHash<Clay_Color>(), first, second, third, fourth, parsed))
						{
							return;
						}
						DevCompositeStructValue updated = normalizedValue;
						updated.borderElementConfig.color = parsed;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevFloat4Input, context.createChildElementId("color/input"))
						.setParameters(float4Params)
						.draw();
				});

				drawLabeledField("width", "Width:", [&](){
					devEdgeU16InputParams edgeParams{};
					edgeParams.fieldIdPrefix = makeFieldIdPrefix("width");
					edgeParams.firstHintText = "left";
					edgeParams.secondHintText = "right";
					edgeParams.thirdHintText = "top";
					edgeParams.fourthHintText = "bottom";
					edgeParams.fifthHintText = "between";
					edgeParams.showFifth = true;
					edgeParams.firstValue = static_cast<int64_t>(normalizedValue.borderElementConfig.width.first);
					edgeParams.secondValue = static_cast<int64_t>(normalizedValue.borderElementConfig.width.second);
					edgeParams.thirdValue = static_cast<int64_t>(normalizedValue.borderElementConfig.width.third);
					edgeParams.fourthValue = static_cast<int64_t>(normalizedValue.borderElementConfig.width.fourth);
					edgeParams.fifthValue = static_cast<int64_t>(normalizedValue.borderElementConfig.width.fifth);
					edgeParams.useNineSplitEdges = true;
					edgeParams.showFifthAfterNineSplit = true;
					edgeParams.nineSplitHintText = "";
					edgeParams.sizing = context.params.inputSizing;
					edgeParams.fontId = context.params.fontId;
					edgeParams.fontSize = context.params.fontSize;
					edgeParams.hintTextColor = context.params.hintTextColor;
					edgeParams.valueTextColor = context.params.valueTextColor;
					edgeParams.onValueChangedCallback = [emitChanged, normalizedValue](
						int64_t first,
						int64_t second,
						int64_t third,
						int64_t fourth,
						int64_t fifth) {
						DevCompositeStructValue updated = normalizedValue;
						updated.borderElementConfig.width.first = static_cast<uint16_t>(std::max<int64_t>(0, first));
						updated.borderElementConfig.width.second = static_cast<uint16_t>(std::max<int64_t>(0, second));
						updated.borderElementConfig.width.third = static_cast<uint16_t>(std::max<int64_t>(0, third));
						updated.borderElementConfig.width.fourth = static_cast<uint16_t>(std::max<int64_t>(0, fourth));
						updated.borderElementConfig.width.fifth = static_cast<uint16_t>(std::max<int64_t>(0, fifth));
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevEdgeU16Input, context.createChildElementId("width/input"))
						.setParameters(edgeParams)
						.draw();
				});
			}
			else if (context.params.fieldTypeHash == typeHash<Clay_ElementDeclaration>())
			{
				auto drawElementIdNumeric = [&](std::string_view fieldId, std::string_view label, uint32_t value, auto applyValue) {
					drawLabeledField(fieldId, label, [&](){
						devNumericInputParams numericParams{};
						numericParams.fieldId = makeFieldIdPrefix(std::string(fieldId));
						numericParams.initialText = devNumericValueToText(
							devNumericInputValueKind::UnsignedInt,
							static_cast<double>(value));
						numericParams.showHint = false;
						numericParams.valueKind = devNumericInputValueKind::UnsignedInt;
						numericParams.minValue = 0.0;
						numericParams.maxValue = static_cast<double>(std::numeric_limits<uint32_t>::max());
						numericParams.sizing = context.params.inputSizing;
						numericParams.fontId = context.params.fontId;
						numericParams.fontSize = context.params.fontSize;
						numericParams.valueTextColor = context.params.valueTextColor;
						numericParams.onValueChangedCallback = [emitChanged, normalizedValue, applyValue](double changedValue) {
							DevCompositeStructValue updated = normalizedValue;
							applyValue(updated, static_cast<uint32_t>(std::llround(changedValue)));
							emitChanged(updated);
						};

						context.uiManager
							.createElement(kDevNumericInput, context.createChildElementId(std::string(fieldId) + "/input"))
							.setParameters(numericParams)
							.draw();
					});
				};

				drawElementIdNumeric(
					"id-value",
					"Id:",
					normalizedValue.elementDeclaration.id.id,
					[](DevCompositeStructValue& updated, uint32_t changed) {
						updated.elementDeclaration.id.id = changed;
					});
				drawElementIdNumeric(
					"id-offset",
					"Id Offset:",
					normalizedValue.elementDeclaration.id.offset,
					[](DevCompositeStructValue& updated, uint32_t changed) {
						updated.elementDeclaration.id.offset = changed;
					});
				drawElementIdNumeric(
					"id-base",
					"Id Base:",
					normalizedValue.elementDeclaration.id.baseId,
					[](DevCompositeStructValue& updated, uint32_t changed) {
						updated.elementDeclaration.id.baseId = changed;
					});

				drawLabeledField("id-static", "Id Static:", [&](){
					devBasicToggleParams toggleParams{};
					toggleParams.defaultEnabled = normalizedValue.elementDeclaration.id.isStaticallyAllocated;
					toggleParams.sizing = {
						.width = CLAY_SIZING_FIXED(22),
						.height = CLAY_SIZING_FIXED(22),
					};
					toggleParams.fontId = context.params.fontId;
					toggleParams.fontSize = context.params.fontSize;
					toggleParams.onValueChangedCallback = [emitChanged, normalizedValue](DevBasicToggleInteractionContext, bool enabled) {
						DevCompositeStructValue updated = normalizedValue;
						updated.elementDeclaration.id.isStaticallyAllocated = enabled;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevBasicToggle, context.createChildElementId("id-static/input"))
						.setParameters(toggleParams)
						.draw();
				});

				drawLabeledField("id-string", "Id String:", [&](){
					devBasicInputFieldParams inputParams{};
					inputParams.fieldId = makeFieldIdPrefix("id-string");
					inputParams.initialText = normalizedValue.elementDeclaration.id.stringId;
					inputParams.sizing = context.params.inputSizing;
					inputParams.padding = CLAY_PADDING_ALL(6);
					inputParams.backgroundColor = FlowUi::Flow_Color("#252932ff");
					inputParams.borderColor = FlowUi::Flow_Color("#8f8d8dff");
					inputParams.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
					inputParams.cornerRadius = CLAY_CORNER_RADIUS(6);
					inputParams.fontId = context.params.fontId;
					inputParams.fontSize = context.params.fontSize;
					inputParams.textColor = context.params.valueTextColor;
					inputParams.onTextChangedCallback = [emitChanged, normalizedValue](std::string_view changedText) {
						DevCompositeStructValue updated = normalizedValue;
						updated.elementDeclaration.id.stringId = std::string(changedText);
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevBasicInputField, context.createChildElementId("id-string/input"))
						.setParameters(inputParams)
						.draw();
				});

				drawLabeledField("layout", "Layout:", [&](){
					DevCompositeStructValue nested{};
					nested.typeHash = typeHash<Clay_LayoutConfig>();
					nested.layoutConfig = normalizedValue.elementDeclaration.layout;

					devCompositeStructInputParams nestedParams{};
					nestedParams.fieldIdPrefix = makeFieldIdPrefix("layout");
					nestedParams.fieldTypeHash = typeHash<Clay_LayoutConfig>();
					nestedParams.value = nested;
					nestedParams.sizing = context.params.inputSizing;
					nestedParams.fontId = context.params.fontId;
					nestedParams.fontSize = context.params.fontSize;
					nestedParams.hintTextColor = context.params.hintTextColor;
					nestedParams.valueTextColor = context.params.valueTextColor;
					nestedParams.onValueChangedCallback = [emitChanged, normalizedValue](const DevCompositeStructValue& changed) {
						DevCompositeStructValue updated = normalizedValue;
						updated.elementDeclaration.layout = changed.layoutConfig;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevCompositeStructInput, context.createChildElementId("layout/input"))
						.setParameters(nestedParams)
						.draw();
				});

				auto drawElementFloat4 = [&](std::string_view fieldId, std::string_view label, const DevFloat4Value& value, bool useColorEditor, bool useNineSplitCorners, auto applyValue) {
					drawLabeledField(fieldId, label, [&](){
						devFloat4InputParams float4Params{};
						float4Params.fieldIdPrefix = makeFieldIdPrefix(std::string(fieldId));
						float4Params.firstHintText = useColorEditor ? "Red" : "Top left";
						float4Params.secondHintText = useColorEditor ? "Green" : "Top right";
						float4Params.thirdHintText = useColorEditor ? "Blue" : "Bottom left";
						float4Params.fourthHintText = useColorEditor ? "Alpha" : "Bottom right";
						float4Params.firstValue = value.first;
						float4Params.secondValue = value.second;
						float4Params.thirdValue = value.third;
						float4Params.fourthValue = value.fourth;
						float4Params.useColorEditor = useColorEditor;
						float4Params.useNineSplitCorners = useNineSplitCorners;
						float4Params.nineSplitHintText = "Corner radii:";
						float4Params.sizing = context.params.inputSizing;
						float4Params.fontId = context.params.fontId;
						float4Params.fontSize = context.params.fontSize;
						float4Params.hintTextColor = context.params.hintTextColor;
						float4Params.valueTextColor = context.params.valueTextColor;
						float4Params.onValueQuadChangedCallback = [
							emitChanged,
							normalizedValue,
							applyValue,
							useColorEditor,
							useNineSplitCorners
						](double first, double second, double third, double fourth) {
							const uint64_t float4TypeHash =
								useColorEditor ? typeHash<Clay_Color>() : typeHash<Clay_CornerRadius>();
							DevFloat4Value parsed{};
							if (!FlowUi::devMode::tryMakeDevFloat4Value(float4TypeHash, first, second, third, fourth, parsed))
							{
								return;
							}
							DevCompositeStructValue updated = normalizedValue;
							applyValue(updated, parsed);
							emitChanged(updated);
						};

						context.uiManager
							.createElement(kDevFloat4Input, context.createChildElementId(std::string(fieldId) + "/input"))
							.setParameters(float4Params)
							.draw();
					});
				};

				drawElementFloat4(
					"background-color",
					"Background Color:",
					normalizedValue.elementDeclaration.backgroundColor,
					true,
					false,
					[](DevCompositeStructValue& updated, const DevFloat4Value& changed) {
						updated.elementDeclaration.backgroundColor = changed;
					});

				drawElementFloat4(
					"corner-radius",
					"Corner Radius:",
					normalizedValue.elementDeclaration.cornerRadius,
					false,
					true,
					[](DevCompositeStructValue& updated, const DevFloat4Value& changed) {
						updated.elementDeclaration.cornerRadius = changed;
					});

				drawLabeledField("aspect-ratio", "Aspect Ratio:", [&](){
					devNumericInputParams numericParams{};
					numericParams.fieldId = makeFieldIdPrefix("aspect-ratio");
					numericParams.initialText = devNumericValueToText(
						devNumericInputValueKind::Floating,
						normalizedValue.elementDeclaration.aspectRatio);
					numericParams.showHint = false;
					numericParams.valueKind = devNumericInputValueKind::Floating;
					numericParams.minValue = kFloatMin;
					numericParams.maxValue = kFloatMax;
					numericParams.sizing = context.params.inputSizing;
					numericParams.fontId = context.params.fontId;
					numericParams.fontSize = context.params.fontSize;
					numericParams.valueTextColor = context.params.valueTextColor;
					numericParams.onValueChangedCallback = [emitChanged, normalizedValue](double changedValue) {
						DevCompositeStructValue updated = normalizedValue;
						updated.elementDeclaration.aspectRatio = changedValue;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevNumericInput, context.createChildElementId("aspect-ratio/input"))
						.setParameters(numericParams)
						.draw();
				});

				auto drawPointerBits = [&](std::string_view fieldId, std::string_view label, uint64_t bits, auto applyBits) {
					drawLabeledField(fieldId, label, [&](){
						devNumericInputParams numericParams{};
						numericParams.fieldId = makeFieldIdPrefix(std::string(fieldId));
						numericParams.initialText = devNumericValueToText(
							devNumericInputValueKind::UnsignedInt,
							static_cast<double>(bits));
						numericParams.showHint = false;
						numericParams.valueKind = devNumericInputValueKind::UnsignedInt;
						numericParams.minValue = 0.0;
						numericParams.maxValue = static_cast<double>(std::numeric_limits<int64_t>::max());
						numericParams.sizing = context.params.inputSizing;
						numericParams.fontId = context.params.fontId;
						numericParams.fontSize = context.params.fontSize;
						numericParams.valueTextColor = context.params.valueTextColor;
						numericParams.onValueChangedCallback = [emitChanged, normalizedValue, applyBits](double changedValue) {
							DevCompositeStructValue updated = normalizedValue;
							applyBits(updated, static_cast<uint64_t>(std::llround(changedValue)));
							emitChanged(updated);
						};

						context.uiManager
							.createElement(kDevNumericInput, context.createChildElementId(std::string(fieldId) + "/input"))
							.setParameters(numericParams)
							.draw();
					});
				};

				drawPointerBits(
					"image-data",
					"Image Data:",
					normalizedValue.elementDeclaration.imageData.bits,
					[](DevCompositeStructValue& updated, uint64_t changed) {
						updated.elementDeclaration.imageData.bits = changed;
					});

				drawLabeledField("floating", "Floating:", [&](){
					DevCompositeStructValue nested{};
					nested.typeHash = typeHash<Clay_FloatingElementConfig>();
					nested.floatingElementConfig = normalizedValue.elementDeclaration.floating;

					devCompositeStructInputParams nestedParams{};
					nestedParams.fieldIdPrefix = makeFieldIdPrefix("floating");
					nestedParams.fieldTypeHash = typeHash<Clay_FloatingElementConfig>();
					nestedParams.value = nested;
					nestedParams.sizing = context.params.inputSizing;
					nestedParams.fontId = context.params.fontId;
					nestedParams.fontSize = context.params.fontSize;
					nestedParams.hintTextColor = context.params.hintTextColor;
					nestedParams.valueTextColor = context.params.valueTextColor;
					nestedParams.onValueChangedCallback = [emitChanged, normalizedValue](const DevCompositeStructValue& changed) {
						DevCompositeStructValue updated = normalizedValue;
						updated.elementDeclaration.floating = changed.floatingElementConfig;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevCompositeStructInput, context.createChildElementId("floating/input"))
						.setParameters(nestedParams)
						.draw();
				});

				drawPointerBits(
					"custom-data",
					"Custom Data:",
					normalizedValue.elementDeclaration.customData.bits,
					[](DevCompositeStructValue& updated, uint64_t changed) {
						updated.elementDeclaration.customData.bits = changed;
					});

				drawLabeledField("clip", "Clip:", [&](){
					DevCompositeStructValue nested{};
					nested.typeHash = typeHash<Clay_ClipElementConfig>();
					nested.clipElementConfig = normalizedValue.elementDeclaration.clip;

					devCompositeStructInputParams nestedParams{};
					nestedParams.fieldIdPrefix = makeFieldIdPrefix("clip");
					nestedParams.fieldTypeHash = typeHash<Clay_ClipElementConfig>();
					nestedParams.value = nested;
					nestedParams.sizing = context.params.inputSizing;
					nestedParams.fontId = context.params.fontId;
					nestedParams.fontSize = context.params.fontSize;
					nestedParams.hintTextColor = context.params.hintTextColor;
					nestedParams.valueTextColor = context.params.valueTextColor;
					nestedParams.onValueChangedCallback = [emitChanged, normalizedValue](const DevCompositeStructValue& changed) {
						DevCompositeStructValue updated = normalizedValue;
						updated.elementDeclaration.clip = changed.clipElementConfig;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevCompositeStructInput, context.createChildElementId("clip/input"))
						.setParameters(nestedParams)
						.draw();
				});

				drawLabeledField("border", "Border:", [&](){
					DevCompositeStructValue nested{};
					nested.typeHash = typeHash<Clay_BorderElementConfig>();
					nested.borderElementConfig = normalizedValue.elementDeclaration.border;

					devCompositeStructInputParams nestedParams{};
					nestedParams.fieldIdPrefix = makeFieldIdPrefix("border");
					nestedParams.fieldTypeHash = typeHash<Clay_BorderElementConfig>();
					nestedParams.value = nested;
					nestedParams.sizing = context.params.inputSizing;
					nestedParams.fontId = context.params.fontId;
					nestedParams.fontSize = context.params.fontSize;
					nestedParams.hintTextColor = context.params.hintTextColor;
					nestedParams.valueTextColor = context.params.valueTextColor;
					nestedParams.onValueChangedCallback = [emitChanged, normalizedValue](const DevCompositeStructValue& changed) {
						DevCompositeStructValue updated = normalizedValue;
						updated.elementDeclaration.border = changed.borderElementConfig;
						emitChanged(updated);
					};

					context.uiManager
						.createElement(kDevCompositeStructInput, context.createChildElementId("border/input"))
						.setParameters(nestedParams)
						.draw();
				});

				drawPointerBits(
					"user-data",
					"User Data:",
					normalizedValue.elementDeclaration.userData.bits,
					[](DevCompositeStructValue& updated, uint64_t changed) {
						updated.elementDeclaration.userData.bits = changed;
					});
			}
			else
			{
				CLAY(context.uiManager.toClayEID(context.createChildElementId("unsupported")), {
					.layout = {
						.sizing = context.params.inputSizing,
					},
				}){
					CLAY_TEXT(
						context.uiManager.toClayString(std::string("<unsupported composite struct>")),
						CLAY_TEXT_CONFIG(hintTextConfig));
				};
			}
		};
	},
};
