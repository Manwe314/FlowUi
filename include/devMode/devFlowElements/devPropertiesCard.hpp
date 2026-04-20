#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicInputField.hpp"
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
					break;
				}
				case devPropertiesCardInputType::Enum1:
					context.uiManager
						.createElement(
							kDevEnum1Input,
							context.createChildElementId("value/enum1/" + context.params.fieldIdentity))
						.setParameters(devEnum1InputParams{
							.text = fieldDisplayText.empty() ? std::string("<enum>") : fieldDisplayText,
							.sizing = context.params.valueEditorSizing,
							.textWrapMode = CLAY_TEXT_WRAP_WORDS,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.textColor = context.params.valueTextColor,
						})
						.draw();
					break;
				case devPropertiesCardInputType::Enum2:
					context.uiManager
						.createElement(
							kDevEnum2Input,
							context.createChildElementId("value/enum2/" + context.params.fieldIdentity))
						.setParameters(devEnum2InputParams{
							.text = fieldDisplayText.empty() ? std::string("<enum2>") : fieldDisplayText,
							.sizing = context.params.valueEditorSizing,
							.textWrapMode = CLAY_TEXT_WRAP_WORDS,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.textColor = context.params.valueTextColor,
						})
						.draw();
					break;
				case devPropertiesCardInputType::Float2:
					context.uiManager
						.createElement(
							kDevFloat2Input,
							context.createChildElementId("value/float2/" + context.params.fieldIdentity))
						.setParameters(devFloat2InputParams{
							.text = fieldDisplayText.empty() ? std::string("<float2>") : fieldDisplayText,
							.sizing = context.params.valueEditorSizing,
							.textWrapMode = CLAY_TEXT_WRAP_WORDS,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.textColor = context.params.valueTextColor,
						})
						.draw();
					break;
				case devPropertiesCardInputType::Float4:
					context.uiManager
						.createElement(
							kDevFloat4Input,
							context.createChildElementId("value/float4/" + context.params.fieldIdentity))
						.setParameters(devFloat4InputParams{
							.text = fieldDisplayText.empty() ? std::string("<float4>") : fieldDisplayText,
							.sizing = context.params.valueEditorSizing,
							.textWrapMode = CLAY_TEXT_WRAP_WORDS,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.textColor = context.params.valueTextColor,
						})
						.draw();
					break;
				case devPropertiesCardInputType::EdgeU16:
					context.uiManager
						.createElement(
							kDevEdgeU16Input,
							context.createChildElementId("value/edge_u16/" + context.params.fieldIdentity))
						.setParameters(devEdgeU16InputParams{
							.text = fieldDisplayText.empty() ? std::string("<edge_u16>") : fieldDisplayText,
							.sizing = context.params.valueEditorSizing,
							.textWrapMode = CLAY_TEXT_WRAP_WORDS,
							.fontId = context.params.fontId,
							.fontSize = context.params.fontSize,
							.textColor = context.params.valueTextColor,
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
