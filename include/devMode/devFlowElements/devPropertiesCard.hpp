#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicInputField.hpp"
#include "devMode/devFlowElements/devNumericInput.hpp"
#include "devMode/devFlowElements/devBasicToggle.hpp"
#include "devMode/devFlowElements/devCompositeStructInput.hpp"
#include "devMode/devFlowElements/devEdgeU16Input.hpp"
#include "devMode/devFlowElements/devEnum1Input.hpp"
#include "devMode/devFlowElements/devEnum2Input.hpp"
#include "devMode/devFlowElements/devFloat2Input.hpp"
#include "devMode/devFlowElements/devFloat4Input.hpp"
#include "devMode/devFlowElements/devPropertiesEditableHelpers.hpp"
#include "devMode/devFlowElements/devTaggedUnionInput.hpp"

enum class devPropertiesCardInputType : uint8_t {
	Toggle = 0,
	InputField = 1,
	Enum1 = 2,
	Enum2 = 3,
	Float2 = 4,
	Float4 = 5,
	EdgeU16 = 6,
	TaggedUnion = 7,
	CompositeStruct = 8,
	Unsupported = 9,
};

inline devPropertiesCardInputType devPropertiesCardInputTypeFromFieldTypeHash(uint64_t fieldTypeHash) {
	if (devFieldTypeIsBool(fieldTypeHash))
	{
		return devPropertiesCardInputType::Toggle;
	}
	if (
		devFieldTypeIsString(fieldTypeHash) ||
		devFieldTypeIsIntegral(fieldTypeHash) ||
		devFieldTypeIsFloating(fieldTypeHash))
	{
		return devPropertiesCardInputType::InputField;
	}
	if (devFieldTypeIsEnum1(fieldTypeHash))
	{
		return devPropertiesCardInputType::Enum1;
	}
	if (devFieldTypeIsEnum2(fieldTypeHash))
	{
		return devPropertiesCardInputType::Enum2;
	}
	if (devFieldTypeIsFloat2(fieldTypeHash))
	{
		return devPropertiesCardInputType::Float2;
	}
	if (devFieldTypeIsFloat4(fieldTypeHash))
	{
		return devPropertiesCardInputType::Float4;
	}
	if (devFieldTypeIsEdgeU16(fieldTypeHash))
	{
		return devPropertiesCardInputType::EdgeU16;
	}
	if (devFieldTypeIsTaggedUnion(fieldTypeHash))
	{
		return devPropertiesCardInputType::TaggedUnion;
	}
	if (devFieldTypeIsCompositeStruct(fieldTypeHash))
	{
		return devPropertiesCardInputType::CompositeStruct;
	}
	return devPropertiesCardInputType::Unsupported;
}

inline std::string devFormatEnum1ClayHintText(std::string_view typeName) {
	std::string normalizedTypeName(typeName);
	constexpr std::string_view kClayPrefix = "Clay_";
	if (normalizedTypeName.rfind(kClayPrefix, 0u) == 0u)
	{
		normalizedTypeName.erase(0u, kClayPrefix.size());
	}
	while (!normalizedTypeName.empty() && normalizedTypeName.front() == '_')
	{
		normalizedTypeName.erase(normalizedTypeName.begin());
	}

	std::string formatted{};
	formatted.reserve(normalizedTypeName.size() + 6u);
	bool previousWasSpace = false;
	for (std::size_t i = 0; i < normalizedTypeName.size(); ++i)
	{
		const char c = normalizedTypeName[i];
		if (c == '_')
		{
			if (!formatted.empty() && !previousWasSpace)
			{
				formatted.push_back(' ');
				previousWasSpace = true;
			}
			continue;
		}

		const bool isUpper = std::isupper(static_cast<unsigned char>(c)) != 0;
		if (isUpper && !formatted.empty() && !previousWasSpace)
		{
			const char previousSource = normalizedTypeName[i - 1u];
			const bool previousWasLower =
				std::islower(static_cast<unsigned char>(previousSource)) != 0;
			const bool previousWasDigit =
				std::isdigit(static_cast<unsigned char>(previousSource)) != 0;
			if (previousWasLower || previousWasDigit || previousSource == '_')
			{
				formatted.push_back(' ');
			}
		}

		formatted.push_back(c);
		previousWasSpace = false;
	}

	while (!formatted.empty() && formatted.back() == ' ')
	{
		formatted.pop_back();
	}

	return formatted.empty() ? std::string("type") : formatted;
}

inline std::string devEnum1HintTextForFieldType(
	uint64_t fieldTypeHash,
	std::string_view fieldTypeName) {
	const FlowUi::devMode::DevEnum1TypeInfo* builtinInfo =
		FlowUi::devMode::findDevEnum1BuiltinTypeInfo(fieldTypeHash);
	if (builtinInfo == nullptr)
	{
		return "type";
	}

	const std::string_view typeNameForHint =
		!fieldTypeName.empty()
		? fieldTypeName
		: builtinInfo->typeName;
	return devFormatEnum1ClayHintText(typeNameForHint);
}

inline std::string devEnum1UserFacingTailName(std::string_view value) {
	const std::size_t scopeSeparator = value.rfind("::");
	if (scopeSeparator == std::string_view::npos || (scopeSeparator + 2u) >= value.size())
	{
		return std::string(value);
	}
	return std::string(value.substr(scopeSeparator + 2u));
}

inline std::string devEnum1HumanizeClayValue(std::string_view value) {
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

inline std::string devEnum1OptionLabelForValue(
	uint64_t fieldTypeHash,
	std::string_view value) {
	if (value.empty())
	{
		return {};
	}
	if (FlowUi::devMode::findDevEnum1BuiltinTypeInfo(fieldTypeHash) != nullptr)
	{
		return devEnum1HumanizeClayValue(value);
	}
	return devEnum1UserFacingTailName(value);
}

inline std::vector<devEnum1InputOption> devEnum1OptionsForFieldType(uint64_t fieldTypeHash) {
	std::vector<devEnum1InputOption> options{};
	if (const FlowUi::devMode::DevEnum1TypeInfo* builtinInfo =
		FlowUi::devMode::findDevEnum1BuiltinTypeInfo(fieldTypeHash))
	{
		options.reserve(builtinInfo->valueCount);
		for (std::size_t i = 0; i < builtinInfo->valueCount; ++i)
		{
			const std::string rawName(builtinInfo->values[i].name);
			options.push_back(devEnum1InputOption{
				.value = rawName,
				.label = devEnum1OptionLabelForValue(fieldTypeHash, rawName),
			});
		}
		return options;
	}

	const FlowUi::devMode::DevRegistry& registry = FlowUi::devMode::DevRegistry::instance();
	const FlowUi::devMode::EnumDescriptor* descriptor = registry.findEnumByTypeHash(fieldTypeHash);
	if (descriptor == nullptr)
	{
		return options;
	}

	options.reserve(descriptor->values.size());
	for (const FlowUi::devMode::EnumValueDescriptor& valueDescriptor : descriptor->values)
	{
		options.push_back(devEnum1InputOption{
			.value = valueDescriptor.name,
			.label = devEnum1OptionLabelForValue(fieldTypeHash, valueDescriptor.name),
		});
	}
	return options;
}

inline void devEnum2HintTextsForFieldType(
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

inline bool devFieldTypeIsUnsignedIntegral(uint64_t fieldTypeHash) {
	return
		fieldTypeHash == FlowUi::devMode::typeHash<uint8_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint16_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint32_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint64_t>();
}

inline devNumericInputValueKind devNumericKindForScalarFieldType(uint64_t fieldTypeHash) {
	if (devFieldTypeIsIntegral(fieldTypeHash))
	{
		return
			devFieldTypeIsUnsignedIntegral(fieldTypeHash)
			? devNumericInputValueKind::UnsignedInt
			: devNumericInputValueKind::SignedInt;
	}
	return devNumericInputValueKind::Floating;
}

inline std::string devNumericHintTextForScalarFieldType(uint64_t fieldTypeHash) {
	const devNumericInputValueKind kind = devNumericKindForScalarFieldType(fieldTypeHash);
	switch (kind)
	{
	case devNumericInputValueKind::SignedInt:
		return "int";
	case devNumericInputValueKind::UnsignedInt:
		return "uint";
	case devNumericInputValueKind::Floating:
		return "float";
	}
	return "float";
}

inline bool devScalarNumericRangeForFieldType(
	uint64_t fieldTypeHash,
	double& outMinValue,
	double& outMaxValue) {
	using FlowUi::devMode::typeHash;

	if (fieldTypeHash == typeHash<int8_t>())
	{
		outMinValue = static_cast<double>(std::numeric_limits<int8_t>::min());
		outMaxValue = static_cast<double>(std::numeric_limits<int8_t>::max());
		return true;
	}
	if (fieldTypeHash == typeHash<int16_t>())
	{
		outMinValue = static_cast<double>(std::numeric_limits<int16_t>::min());
		outMaxValue = static_cast<double>(std::numeric_limits<int16_t>::max());
		return true;
	}
	if (fieldTypeHash == typeHash<int32_t>())
	{
		outMinValue = static_cast<double>(std::numeric_limits<int32_t>::min());
		outMaxValue = static_cast<double>(std::numeric_limits<int32_t>::max());
		return true;
	}
	if (fieldTypeHash == typeHash<int64_t>())
	{
		outMinValue = static_cast<double>(std::numeric_limits<int64_t>::min());
		outMaxValue = static_cast<double>(std::numeric_limits<int64_t>::max());
		return true;
	}
	if (fieldTypeHash == typeHash<uint8_t>())
	{
		outMinValue = 0.0;
		outMaxValue = static_cast<double>(std::numeric_limits<uint8_t>::max());
		return true;
	}
	if (fieldTypeHash == typeHash<uint16_t>())
	{
		outMinValue = 0.0;
		outMaxValue = static_cast<double>(std::numeric_limits<uint16_t>::max());
		return true;
	}
	if (fieldTypeHash == typeHash<uint32_t>())
	{
		outMinValue = 0.0;
		outMaxValue = static_cast<double>(std::numeric_limits<uint32_t>::max());
		return true;
	}
	if (fieldTypeHash == typeHash<uint64_t>())
	{
		outMinValue = 0.0;
		outMaxValue = static_cast<double>(std::numeric_limits<int64_t>::max());
		return true;
	}
	if (fieldTypeHash == typeHash<float>())
	{
		outMinValue = static_cast<double>(-std::numeric_limits<float>::max());
		outMaxValue = static_cast<double>(std::numeric_limits<float>::max());
		return true;
	}
	if (fieldTypeHash == typeHash<double>())
	{
		outMinValue = -std::numeric_limits<double>::max();
		outMaxValue = std::numeric_limits<double>::max();
		return true;
	}
	if (FlowUi::devMode::isDevFloat1TypeHash(fieldTypeHash))
	{
		outMinValue = static_cast<double>(-std::numeric_limits<float>::max());
		outMaxValue = static_cast<double>(std::numeric_limits<float>::max());
		return true;
	}
	return false;
}

inline std::optional<FlowUi::devMode::DevValue> devScalarNumericDevValueFromInput(
	uint64_t fieldTypeHash,
	double numericValue) {
	double minValue = 0.0;
	double maxValue = 0.0;
	if (!devScalarNumericRangeForFieldType(fieldTypeHash, minValue, maxValue))
	{
		return std::nullopt;
	}

	const devNumericInputValueKind kind = devNumericKindForScalarFieldType(fieldTypeHash);
	double normalized = 0.0;
	if (!devNumericNormalizeValue(kind, numericValue, minValue, maxValue, normalized))
	{
		return std::nullopt;
	}

	if (devFieldTypeIsIntegral(fieldTypeHash))
	{
		const int64_t integralValue = static_cast<int64_t>(std::llround(normalized));
		if (
			devFieldTypeIsUnsignedIntegral(fieldTypeHash) &&
			integralValue < 0)
		{
			return std::nullopt;
		}
		return FlowUi::devMode::DevValue{integralValue};
	}

	if (FlowUi::devMode::isDevFloat1TypeHash(fieldTypeHash))
	{
		double float1Value = 0.0;
		if (!FlowUi::devMode::tryMakeDevFloat1Value(fieldTypeHash, normalized, float1Value))
		{
			return std::nullopt;
		}
		return FlowUi::devMode::DevValue{float1Value};
	}
	if (devFieldTypeIsFloating(fieldTypeHash))
	{
		return FlowUi::devMode::DevValue{normalized};
	}
	return std::nullopt;
}

struct devPropertiesCardParams {
	devPropertiesSelectionNode selection{};
	uint64_t fieldHash = 0u;
	uint64_t fieldTypeHash = 0u;
	std::string fieldName = "";
	std::string fieldTypeName = "";
	std::string fieldIdentity = "";

	Clay_Padding padding = CLAY_PADDING_ALL(8);
	uint16_t rowGap = 6;
	uint16_t headerChildGap = 8;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#191f28ff");
	Clay_Color borderColor = FlowUi::Flow_Color("#8f8d8d66");
	Clay_BorderWidth borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(8);

	Clay_Sizing valueEditorSizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};

	std::string unsupportedFieldTypeText = "<unsupported type>";

	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color nameTextColor = FlowUi::Flow_Color("#ffffffff");
	Clay_Color typeTextColor = FlowUi::Flow_Color("#a8b4ccff");
	Clay_Color valueTextColor = FlowUi::Flow_Color("#ffffffff");
};

using DevPropertiesCardDef = FlowUi::ElementDefinition<
	devPropertiesCardParams,
	void,
	void,
	FLOW_DEF_ID("DevPropertiesCard"),
	true>;

inline const DevPropertiesCardDef kDevPropertiesCard = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevPropertiesCardDef::BuildContext& context) {
		FlowUi::devMode::DevRuntime& runtime = context.uiManager.devRuntime();
		const std::optional<FlowUi::devMode::DevValue> fieldValue = findCurrentEditableFieldValue(
			runtime,
			context.params.selection,
			context.params.fieldHash);
		const devPropertiesCardInputType inputType =
			devPropertiesCardInputTypeFromFieldTypeHash(context.params.fieldTypeHash);

		bool toggleDefaultEnabled = false;
		if (fieldValue.has_value())
		{
			(void)tryCoerceDevValueToBool(*fieldValue, toggleDefaultEnabled);
		}

		const std::string inputInitialText =
			fieldValue.has_value()
			? devValueToEditableText(*fieldValue)
			: std::string{};
		const std::string fieldDisplayText =
			fieldValue.has_value()
			? devValueToEditableTextForField(context.params.fieldTypeHash, *fieldValue)
			: std::string{};
		const std::string enum1SelectedValue =
			fieldDisplayText.empty()
			? inputInitialText
			: fieldDisplayText;
		const std::string enum1SelectedLabel =
			devEnum1OptionLabelForValue(
				context.params.fieldTypeHash,
				enum1SelectedValue);

		std::vector<devEnum1InputOption> enum1Options{};
		std::string enum1HintText = "type";
		if (inputType == devPropertiesCardInputType::Enum1)
		{
			enum1Options = devEnum1OptionsForFieldType(context.params.fieldTypeHash);
			enum1HintText = devEnum1HintTextForFieldType(
				context.params.fieldTypeHash,
				context.params.fieldTypeName);
		}

			std::string enum2FirstHintText = "first";
			std::string enum2SecondHintText = "second";
			std::vector<devEnum1InputOption> enum2FirstOptions{};
			std::vector<devEnum1InputOption> enum2SecondOptions{};
		std::string enum2FirstSelectedValue{};
		std::string enum2SecondSelectedValue{};
		std::string enum2FirstSelectedLabel{};
		std::string enum2SecondSelectedLabel{};
		if (inputType == devPropertiesCardInputType::Enum2)
		{
			devEnum2HintTextsForFieldType(
				context.params.fieldTypeHash,
				enum2FirstHintText,
				enum2SecondHintText);

			const FlowUi::devMode::DevEnum2TypeInfo* info =
				FlowUi::devMode::findDevEnum2TypeInfo(context.params.fieldTypeHash);
			if (info != nullptr)
			{
				enum2FirstOptions = devEnum1OptionsForFieldType(info->firstEnumTypeHash);
				enum2SecondOptions = devEnum1OptionsForFieldType(info->secondEnumTypeHash);

				FlowUi::devMode::DevEnum2Value enum2Value{};
				if (
					fieldValue.has_value() &&
					tryCoerceDevValueToEnum2Value(
						context.params.fieldTypeHash,
						*fieldValue,
						enum2Value))
				{
					std::string_view firstName{};
					std::string_view secondName{};
					if (FlowUi::devMode::tryDevEnum2ValueToNames(
						context.params.fieldTypeHash,
						enum2Value,
						firstName,
						secondName))
					{
						enum2FirstSelectedValue = std::string(firstName);
						enum2SecondSelectedValue = std::string(secondName);
					}
				}

				enum2FirstSelectedLabel = devEnum1OptionLabelForValue(
					info->firstEnumTypeHash,
					enum2FirstSelectedValue);
					enum2SecondSelectedLabel = devEnum1OptionLabelForValue(
						info->secondEnumTypeHash,
						enum2SecondSelectedValue);
				}
			}

			bool scalarUsesNumericInput =
				inputType == devPropertiesCardInputType::InputField &&
				!devFieldTypeIsString(context.params.fieldTypeHash);
			devNumericInputValueKind scalarNumericKind = devNumericInputValueKind::Floating;
			std::string scalarNumericHintText = "float";
			double scalarNumericMinValue = -1000000.0;
			double scalarNumericMaxValue = 1000000.0;
			double scalarNumericInitialValue = 0.0;
			std::string scalarNumericInitialText = "0";
			if (scalarUsesNumericInput)
			{
				scalarNumericKind = devNumericKindForScalarFieldType(context.params.fieldTypeHash);
				scalarNumericHintText = devNumericHintTextForScalarFieldType(context.params.fieldTypeHash);
				(void)devScalarNumericRangeForFieldType(
					context.params.fieldTypeHash,
					scalarNumericMinValue,
					scalarNumericMaxValue);

				double rawInitial = 0.0;
				if (fieldValue.has_value())
				{
					(void)tryCoerceDevValueToDouble(*fieldValue, rawInitial);
				}
				else
				{
					(void)devNumericTryParseText(scalarNumericKind, inputInitialText, rawInitial);
				}

				double normalizedInitial = 0.0;
				if (!devNumericNormalizeValue(
					scalarNumericKind,
					rawInitial,
					scalarNumericMinValue,
					scalarNumericMaxValue,
					normalizedInitial))
				{
					normalizedInitial = 0.0;
				}
				scalarNumericInitialValue = normalizedInitial;
				scalarNumericInitialText = devNumericValueToText(scalarNumericKind, normalizedInitial);
			}

			std::string float2FirstHintText = "first";
			std::string float2SecondHintText = "second";
			double float2FirstValue = 0.0;
			double float2SecondValue = 0.0;
			if (inputType == devPropertiesCardInputType::Float2)
			{
				if (const FlowUi::devMode::DevFloat2TypeInfo* info =
					FlowUi::devMode::findDevFloat2TypeInfo(context.params.fieldTypeHash))
				{
					float2FirstHintText = std::string(info->firstFieldName);
					float2SecondHintText = std::string(info->secondFieldName);
				}
				FlowUi::devMode::DevFloat2Value float2Value{};
				if (
					fieldValue.has_value() &&
					tryCoerceDevValueToFloat2Value(context.params.fieldTypeHash, *fieldValue, float2Value))
				{
					float2FirstValue = float2Value.first;
					float2SecondValue = float2Value.second;
				}
			}

			std::string float4FirstHintText = "first";
			std::string float4SecondHintText = "second";
			std::string float4ThirdHintText = "third";
			std::string float4FourthHintText = "fourth";
			double float4FirstValue = 0.0;
			double float4SecondValue = 0.0;
			double float4ThirdValue = 0.0;
			double float4FourthValue = 0.0;
			if (inputType == devPropertiesCardInputType::Float4)
			{
				if (const FlowUi::devMode::DevFloat4TypeInfo* info =
					FlowUi::devMode::findDevFloat4TypeInfo(context.params.fieldTypeHash))
				{
					float4FirstHintText = std::string(info->firstFieldName);
					float4SecondHintText = std::string(info->secondFieldName);
					float4ThirdHintText = std::string(info->thirdFieldName);
					float4FourthHintText = std::string(info->fourthFieldName);
				}
				FlowUi::devMode::DevFloat4Value float4Value{};
				if (
					fieldValue.has_value() &&
					tryCoerceDevValueToFloat4Value(context.params.fieldTypeHash, *fieldValue, float4Value))
				{
					float4FirstValue = float4Value.first;
					float4SecondValue = float4Value.second;
					float4ThirdValue = float4Value.third;
					float4FourthValue = float4Value.fourth;
				}
			}

			std::string edgeFirstHintText = "first";
			std::string edgeSecondHintText = "second";
			std::string edgeThirdHintText = "third";
			std::string edgeFourthHintText = "fourth";
			std::string edgeFifthHintText = "fifth";
			bool edgeShowFifth = false;
			int64_t edgeFirstValue = 0;
			int64_t edgeSecondValue = 0;
			int64_t edgeThirdValue = 0;
			int64_t edgeFourthValue = 0;
			int64_t edgeFifthValue = 0;
			if (inputType == devPropertiesCardInputType::EdgeU16)
			{
				if (const FlowUi::devMode::DevEdgeU16TypeInfo* info =
					FlowUi::devMode::findDevEdgeU16TypeInfo(context.params.fieldTypeHash))
				{
					edgeFirstHintText = std::string(info->firstFieldName);
					edgeSecondHintText = std::string(info->secondFieldName);
					edgeThirdHintText = std::string(info->thirdFieldName);
					edgeFourthHintText = std::string(info->fourthFieldName);
					edgeFifthHintText = std::string(info->fifthFieldName);
					edgeShowFifth = info->fifthFieldUsed;
				}
				FlowUi::devMode::DevEdgeU16Value edgeValue{};
				if (
					fieldValue.has_value() &&
					tryCoerceDevValueToEdgeU16Value(context.params.fieldTypeHash, *fieldValue, edgeValue))
				{
					edgeFirstValue = static_cast<int64_t>(edgeValue.first);
					edgeSecondValue = static_cast<int64_t>(edgeValue.second);
					edgeThirdValue = static_cast<int64_t>(edgeValue.third);
					edgeFourthValue = static_cast<int64_t>(edgeValue.fourth);
					edgeFifthValue = static_cast<int64_t>(edgeValue.fifth);
				}
			}

			Clay_ElementDeclaration root{};
			root.id = context.uiManager.toClayEID(context.elementID);
			root.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.rowGap;
		root.backgroundColor = context.params.backgroundColor;
		root.border = {
			.color = context.params.borderColor,
			.width = context.params.borderWidth,
		};
		root.cornerRadius = context.params.cornerRadius;
		root.clip = {
			.horizontal = true,
			.vertical = true,
		};

		Clay_TextElementConfig nameTextConfig{};
		nameTextConfig.textColor = context.params.nameTextColor;
		nameTextConfig.fontId = context.params.fontId;
		nameTextConfig.fontSize = context.params.fontSize;
		nameTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
		nameTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		Clay_TextElementConfig typeTextConfig{};
		typeTextConfig.textColor = context.params.typeTextColor;
		typeTextConfig.fontId = context.params.fontId;
		typeTextConfig.fontSize = context.params.fontSize;
		typeTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
		typeTextConfig.textAlignment = CLAY_TEXT_ALIGN_RIGHT;

		Clay_TextElementConfig valueTextConfig{};
		valueTextConfig.textColor = context.params.valueTextColor;
		valueTextConfig.fontId = context.params.fontId;
		valueTextConfig.fontSize = context.params.fontSize;
		valueTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
		valueTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		CLAY(root){
			Clay_ElementDeclaration infoRow{};
			infoRow.id = context.uiManager.toClayEID(context.createChildElementId("row/info"));
			infoRow.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
			infoRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			infoRow.layout.childGap = context.params.headerChildGap;
			infoRow.layout.childAlignment = {
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_TOP,
			};
			infoRow.backgroundColor = FlowUi::Flow_Color("#00000000");

			CLAY(infoRow){
				CLAY({
					.id = context.uiManager.toClayEID(context.createChildElementId("row/info/name")),
					.layout = {
						.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_FIT(0),
						},
					},
				}){
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.fieldName),
						CLAY_TEXT_CONFIG(nameTextConfig));
				};

				CLAY({
					.id = context.uiManager.toClayEID(context.createChildElementId("row/info/spacer")),
					.layout = {
						.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_FIT(0),
						},
					},
				}){};

				CLAY({
					.id = context.uiManager.toClayEID(context.createChildElementId("row/info/type")),
					.layout = {
						.sizing = {
							.width = CLAY_SIZING_FIT(0),
							.height = CLAY_SIZING_FIT(0),
						},
					},
				}){
					const std::string typeLabel =
						context.params.fieldTypeName.empty()
						? std::string("<unknown>")
						: context.params.fieldTypeName;
					CLAY_TEXT(
						context.uiManager.toClayString(typeLabel),
						CLAY_TEXT_CONFIG(typeTextConfig));
				};
			};

			Clay_ElementDeclaration valueRow{};
			valueRow.id = context.uiManager.toClayEID(context.createChildElementId("row/value"));
			valueRow.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
			valueRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			valueRow.layout.childAlignment = {
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_TOP,
			};
			valueRow.backgroundColor = FlowUi::Flow_Color("#00000000");

				CLAY(valueRow){
					switch (inputType)
					{
				case devPropertiesCardInputType::Toggle:
					context.uiManager
						.createElement(
							kDevBasicToggle,
							context.createChildElementId("value/toggle/" + context.params.fieldIdentity))
						.setParameters(devBasicToggleParams{
							.defaultEnabled = toggleDefaultEnabled,
							.onValueChangedCallback = [
								uiManager = &context.uiManager,
								selection = context.params.selection,
								fieldHash = context.params.fieldHash
							](DevBasicToggleInteractionContext, bool isEnabled) {
								setEditableFieldOverride(
									uiManager->devRuntime(),
									selection,
									fieldHash,
									FlowUi::devMode::DevValue{isEnabled});
							},
						})
						.draw();
					break;
					case devPropertiesCardInputType::InputField:
					{
						const std::string inputFieldId =
							"flowui/dev/input/" + context.params.fieldIdentity;
						if (devFieldTypeIsString(context.params.fieldTypeHash))
						{
							context.uiManager
								.createElement(
									kDevBasicInputField,
									context.createChildElementId("value/input/" + context.params.fieldIdentity))
								.setParameters(devBasicInputFieldParams{
									.fieldId = inputFieldId,
									.initialText = inputInitialText,
									.onTextChangedCallback = [
										uiManager = &context.uiManager,
										selection = context.params.selection,
										fieldHash = context.params.fieldHash,
										fieldTypeHash = context.params.fieldTypeHash,
										initialText = inputInitialText
									](std::string_view text) {
										const std::optional<FlowUi::devMode::DevValue> parsed =
											parseEditableTextToDevValue(fieldTypeHash, text);
										if (!parsed.has_value())
										{
											return;
										}
										FlowUi::devMode::DevRuntime& runtime = uiManager->devRuntime();
										const std::optional<FlowUi::devMode::DevValue> currentValue =
											findCurrentEditableFieldValue(runtime, selection, fieldHash);
										if (currentValue.has_value())
										{
											if (devValuesEquivalentForEditableField(
												fieldTypeHash,
												*currentValue,
												*parsed))
											{
												return;
											}
										}
										else if (text == std::string_view(initialText))
										{
											return;
										}
										setEditableFieldOverride(
											runtime,
											selection,
											fieldHash,
											*parsed);
									},
									.sizing = context.params.valueEditorSizing,
									.fontId = context.params.fontId,
									.fontSize = context.params.fontSize,
								})
								.draw();
						}
						else
						{
							context.uiManager
								.createElement(
									kDevNumericInput,
									context.createChildElementId("value/numeric/" + context.params.fieldIdentity))
								.setParameters(devNumericInputParams{
									.fieldId = inputFieldId,
									.initialText = scalarNumericInitialText,
									.hintText = scalarNumericHintText,
									.valueKind = scalarNumericKind,
									.minValue = scalarNumericMinValue,
									.maxValue = scalarNumericMaxValue,
									.onValueChangedCallback = [
										uiManager = &context.uiManager,
										selection = context.params.selection,
										fieldHash = context.params.fieldHash,
										fieldTypeHash = context.params.fieldTypeHash,
										initialNumericValue = scalarNumericInitialValue
									](double numericValue) {
										const std::optional<FlowUi::devMode::DevValue> parsed =
											devScalarNumericDevValueFromInput(fieldTypeHash, numericValue);
										if (!parsed.has_value())
										{
											return;
										}

										FlowUi::devMode::DevRuntime& runtime = uiManager->devRuntime();
										const std::optional<FlowUi::devMode::DevValue> currentValue =
											findCurrentEditableFieldValue(runtime, selection, fieldHash);
										if (currentValue.has_value())
										{
											if (devValuesEquivalentForEditableField(
												fieldTypeHash,
												*currentValue,
												*parsed))
											{
												return;
											}
										}
										else
										{
											const std::optional<FlowUi::devMode::DevValue> initialValue =
												devScalarNumericDevValueFromInput(fieldTypeHash, initialNumericValue);
											if (
												initialValue.has_value() &&
												devValuesEquivalentForEditableField(
													fieldTypeHash,
													*initialValue,
													*parsed))
											{
												return;
											}
										}

										setEditableFieldOverride(
											runtime,
											selection,
											fieldHash,
											*parsed);
									},
									.fontId = context.params.fontId,
									.fontSize = context.params.fontSize,
									.hintTextColor = context.params.typeTextColor,
									.valueTextColor = context.params.valueTextColor,
								})
								.draw();
						}
						break;
					}
				case devPropertiesCardInputType::Enum1:
					context.uiManager
						.createElement(
							kDevEnum1Input,
							context.createChildElementId("value/enum1/" + context.params.fieldIdentity))
						.setParameters(devEnum1InputParams{
							.hintText = enum1HintText,
							.selectedValue = enum1SelectedValue,
							.selectedLabel = enum1SelectedLabel,
							.options = enum1Options,
							.onValueChangedCallback = [
								uiManager = &context.uiManager,
								selection = context.params.selection,
								fieldHash = context.params.fieldHash,
								fieldTypeHash = context.params.fieldTypeHash,
								initialText = enum1SelectedValue
							](std::string_view text) {
								const std::optional<FlowUi::devMode::DevValue> parsed =
									parseEditableTextToDevValue(fieldTypeHash, text);
								if (!parsed.has_value())
								{
									return;
								}

								FlowUi::devMode::DevRuntime& runtime = uiManager->devRuntime();
								const std::optional<FlowUi::devMode::DevValue> currentValue =
									findCurrentEditableFieldValue(runtime, selection, fieldHash);
								if (currentValue.has_value())
								{
									if (devValuesEquivalentForEditableField(
										fieldTypeHash,
										*currentValue,
										*parsed))
									{
										return;
									}
								}
								else if (text == std::string_view(initialText))
								{
									return;
								}

								setEditableFieldOverride(
									runtime,
									selection,
									fieldHash,
									*parsed);
							},
							.sizing = context.params.valueEditorSizing,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.hintTextColor = context.params.typeTextColor,
							.valueTextColor = context.params.valueTextColor,
							.optionTextColor = context.params.valueTextColor,
						})
						.draw();
					break;
				case devPropertiesCardInputType::Enum2:
					context.uiManager
						.createElement(
							kDevEnum2Input,
							context.createChildElementId("value/enum2/" + context.params.fieldIdentity))
						.setParameters(devEnum2InputParams{
							.firstHintText = enum2FirstHintText,
							.secondHintText = enum2SecondHintText,
							.firstSelectedValue = enum2FirstSelectedValue,
							.firstSelectedLabel = enum2FirstSelectedLabel,
							.secondSelectedValue = enum2SecondSelectedValue,
							.secondSelectedLabel = enum2SecondSelectedLabel,
							.firstOptions = enum2FirstOptions,
							.secondOptions = enum2SecondOptions,
							.onValuePairChangedCallback = [
								uiManager = &context.uiManager,
								selection = context.params.selection,
								fieldHash = context.params.fieldHash,
								fieldTypeHash = context.params.fieldTypeHash,
								initialFirstValue = enum2FirstSelectedValue,
								initialSecondValue = enum2SecondSelectedValue
							](std::string_view firstValue, std::string_view secondValue) {
								FlowUi::devMode::DevEnum2Value parsedEnum2{};
								if (!FlowUi::devMode::tryDevEnum2NamesToValue(
									fieldTypeHash,
									firstValue,
									secondValue,
									parsedEnum2))
								{
									return;
								}

								const FlowUi::devMode::DevValue parsedValue = parsedEnum2;
								FlowUi::devMode::DevRuntime& runtime = uiManager->devRuntime();
								const std::optional<FlowUi::devMode::DevValue> currentValue =
									findCurrentEditableFieldValue(runtime, selection, fieldHash);
								if (currentValue.has_value())
								{
									if (devValuesEquivalentForEditableField(
										fieldTypeHash,
										*currentValue,
										parsedValue))
									{
										return;
									}
								}
								else if (
									firstValue == std::string_view(initialFirstValue) &&
									secondValue == std::string_view(initialSecondValue))
								{
									return;
								}

								setEditableFieldOverride(
									runtime,
									selection,
									fieldHash,
									parsedValue);
							},
							.sizing = context.params.valueEditorSizing,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.hintTextColor = context.params.typeTextColor,
							.valueTextColor = context.params.valueTextColor,
						})
						.draw();
					break;
					case devPropertiesCardInputType::Float2:
						context.uiManager
							.createElement(
								kDevFloat2Input,
								context.createChildElementId("value/float2/" + context.params.fieldIdentity))
							.setParameters(devFloat2InputParams{
								.fieldIdPrefix = "flowui/dev/input/" + context.params.fieldIdentity + "/float2",
								.firstHintText = float2FirstHintText,
								.secondHintText = float2SecondHintText,
								.firstValue = float2FirstValue,
								.secondValue = float2SecondValue,
								.onValuePairChangedCallback = [
									uiManager = &context.uiManager,
									selection = context.params.selection,
									fieldHash = context.params.fieldHash,
									fieldTypeHash = context.params.fieldTypeHash,
									initialFirstValue = float2FirstValue,
									initialSecondValue = float2SecondValue
								](double firstValue, double secondValue) {
									FlowUi::devMode::DevFloat2Value parsedFloat2{};
									if (!FlowUi::devMode::tryMakeDevFloat2Value(
										fieldTypeHash,
										firstValue,
										secondValue,
										parsedFloat2))
									{
										return;
									}
									const FlowUi::devMode::DevValue parsedValue = parsedFloat2;

									FlowUi::devMode::DevRuntime& runtime = uiManager->devRuntime();
									const std::optional<FlowUi::devMode::DevValue> currentValue =
										findCurrentEditableFieldValue(runtime, selection, fieldHash);
									if (currentValue.has_value())
									{
										if (devValuesEquivalentForEditableField(
											fieldTypeHash,
											*currentValue,
											parsedValue))
										{
											return;
										}
									}
									else if (
										devNumericValuesEqual(
											devNumericInputValueKind::Floating,
											firstValue,
											initialFirstValue) &&
										devNumericValuesEqual(
											devNumericInputValueKind::Floating,
											secondValue,
											initialSecondValue))
									{
										return;
									}

									setEditableFieldOverride(
										runtime,
										selection,
										fieldHash,
										parsedValue);
								},
								.sizing = context.params.valueEditorSizing,
								.fontId = context.params.fontId,
								.fontSize = context.params.fontSize,
								.hintTextColor = context.params.typeTextColor,
								.valueTextColor = context.params.valueTextColor,
							})
							.draw();
						break;
					case devPropertiesCardInputType::Float4:
						context.uiManager
							.createElement(
								kDevFloat4Input,
								context.createChildElementId("value/float4/" + context.params.fieldIdentity))
							.setParameters(devFloat4InputParams{
								.fieldIdPrefix = "flowui/dev/input/" + context.params.fieldIdentity + "/float4",
								.firstHintText = float4FirstHintText,
								.secondHintText = float4SecondHintText,
								.thirdHintText = float4ThirdHintText,
								.fourthHintText = float4FourthHintText,
								.firstValue = float4FirstValue,
								.secondValue = float4SecondValue,
								.thirdValue = float4ThirdValue,
								.fourthValue = float4FourthValue,
								.onValueQuadChangedCallback = [
									uiManager = &context.uiManager,
									selection = context.params.selection,
									fieldHash = context.params.fieldHash,
									fieldTypeHash = context.params.fieldTypeHash,
									initialFirstValue = float4FirstValue,
									initialSecondValue = float4SecondValue,
									initialThirdValue = float4ThirdValue,
									initialFourthValue = float4FourthValue
								](double firstValue, double secondValue, double thirdValue, double fourthValue) {
									FlowUi::devMode::DevFloat4Value parsedFloat4{};
									if (!FlowUi::devMode::tryMakeDevFloat4Value(
										fieldTypeHash,
										firstValue,
										secondValue,
										thirdValue,
										fourthValue,
										parsedFloat4))
									{
										return;
									}
									const FlowUi::devMode::DevValue parsedValue = parsedFloat4;

									FlowUi::devMode::DevRuntime& runtime = uiManager->devRuntime();
									const std::optional<FlowUi::devMode::DevValue> currentValue =
										findCurrentEditableFieldValue(runtime, selection, fieldHash);
									if (currentValue.has_value())
									{
										if (devValuesEquivalentForEditableField(
											fieldTypeHash,
											*currentValue,
											parsedValue))
										{
											return;
										}
									}
									else if (
										devNumericValuesEqual(
											devNumericInputValueKind::Floating,
											firstValue,
											initialFirstValue) &&
										devNumericValuesEqual(
											devNumericInputValueKind::Floating,
											secondValue,
											initialSecondValue) &&
										devNumericValuesEqual(
											devNumericInputValueKind::Floating,
											thirdValue,
											initialThirdValue) &&
										devNumericValuesEqual(
											devNumericInputValueKind::Floating,
											fourthValue,
											initialFourthValue))
									{
										return;
									}

									setEditableFieldOverride(
										runtime,
										selection,
										fieldHash,
										parsedValue);
								},
								.sizing = context.params.valueEditorSizing,
								.fontId = context.params.fontId,
								.fontSize = context.params.fontSize,
								.hintTextColor = context.params.typeTextColor,
								.valueTextColor = context.params.valueTextColor,
							})
							.draw();
						break;
					case devPropertiesCardInputType::EdgeU16:
						context.uiManager
							.createElement(
								kDevEdgeU16Input,
								context.createChildElementId("value/edge_u16/" + context.params.fieldIdentity))
							.setParameters(devEdgeU16InputParams{
								.fieldIdPrefix = "flowui/dev/input/" + context.params.fieldIdentity + "/edge",
								.firstHintText = edgeFirstHintText,
								.secondHintText = edgeSecondHintText,
								.thirdHintText = edgeThirdHintText,
								.fourthHintText = edgeFourthHintText,
								.fifthHintText = edgeFifthHintText,
								.showFifth = edgeShowFifth,
								.firstValue = edgeFirstValue,
								.secondValue = edgeSecondValue,
								.thirdValue = edgeThirdValue,
								.fourthValue = edgeFourthValue,
								.fifthValue = edgeFifthValue,
								.onValueChangedCallback = [
									uiManager = &context.uiManager,
									selection = context.params.selection,
									fieldHash = context.params.fieldHash,
									fieldTypeHash = context.params.fieldTypeHash,
									initialFirstValue = edgeFirstValue,
									initialSecondValue = edgeSecondValue,
									initialThirdValue = edgeThirdValue,
									initialFourthValue = edgeFourthValue,
									initialFifthValue = edgeFifthValue
								](int64_t firstValue, int64_t secondValue, int64_t thirdValue, int64_t fourthValue, int64_t fifthValue) {
									FlowUi::devMode::DevEdgeU16Value parsedEdge{};
									if (!FlowUi::devMode::tryMakeDevEdgeU16Value(
										fieldTypeHash,
										firstValue,
										secondValue,
										thirdValue,
										fourthValue,
										fifthValue,
										parsedEdge))
									{
										return;
									}
									const FlowUi::devMode::DevValue parsedValue = parsedEdge;

									FlowUi::devMode::DevRuntime& runtime = uiManager->devRuntime();
									const std::optional<FlowUi::devMode::DevValue> currentValue =
										findCurrentEditableFieldValue(runtime, selection, fieldHash);
									if (currentValue.has_value())
									{
										if (devValuesEquivalentForEditableField(
											fieldTypeHash,
											*currentValue,
											parsedValue))
										{
											return;
										}
									}
									else if (
										firstValue == initialFirstValue &&
										secondValue == initialSecondValue &&
										thirdValue == initialThirdValue &&
										fourthValue == initialFourthValue &&
										fifthValue == initialFifthValue)
									{
										return;
									}

									setEditableFieldOverride(
										runtime,
										selection,
										fieldHash,
										parsedValue);
								},
								.sizing = context.params.valueEditorSizing,
								.fontId = context.params.fontId,
								.fontSize = context.params.fontSize,
								.hintTextColor = context.params.typeTextColor,
								.valueTextColor = context.params.valueTextColor,
							})
							.draw();
						break;
				case devPropertiesCardInputType::TaggedUnion:
					context.uiManager
						.createElement(
							kDevTaggedUnionInput,
							context.createChildElementId("value/tagged_union/" + context.params.fieldIdentity))
						.setParameters(devTaggedUnionInputParams{
							.text = fieldDisplayText.empty() ? std::string("<tagged union>") : fieldDisplayText,
							.sizing = context.params.valueEditorSizing,
							.textWrapMode = CLAY_TEXT_WRAP_WORDS,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.textColor = context.params.valueTextColor,
						})
						.draw();
					break;
				case devPropertiesCardInputType::CompositeStruct:
					context.uiManager
						.createElement(
							kDevCompositeStructInput,
							context.createChildElementId("value/composite/" + context.params.fieldIdentity))
						.setParameters(devCompositeStructInputParams{
							.text = fieldDisplayText.empty() ? std::string("<composite struct>") : fieldDisplayText,
							.sizing = context.params.valueEditorSizing,
							.textWrapMode = CLAY_TEXT_WRAP_WORDS,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.textColor = context.params.valueTextColor,
						})
						.draw();
					break;
				case devPropertiesCardInputType::Unsupported:
					CLAY({
						.id = context.uiManager.toClayEID(
							context.createChildElementId("value/unsupported/" + context.params.fieldIdentity)),
						.layout = {
							.sizing = {
								.width = context.params.valueEditorSizing.width,
								.height = CLAY_SIZING_FIT(0),
							},
						},
					}){
						CLAY_TEXT(
							context.uiManager.toClayString(context.params.unsupportedFieldTypeText),
							CLAY_TEXT_CONFIG(valueTextConfig));
					};
					break;
				}
			};
		};
	},
};
