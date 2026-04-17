#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicInputField.hpp"
#include "devMode/devFlowElements/devBasicToggle.hpp"
#include "devMode/devFlowElements/devEnum1Input.hpp"
#include "devMode/devFlowElements/devEnum2Input.hpp"
#include "devMode/devFlowElements/devFloat2Input.hpp"
#include "devMode/devFlowElements/devFloat4Input.hpp"
#include "devMode/devFlowElements/devEdgeU16Input.hpp"
#include "devMode/devFlowElements/devTaggedUnionInput.hpp"
#include "devMode/devFlowElements/devCompositeStructInput.hpp"
#include "devMode/devFlowElements/devPropertiesEditableHelpers.hpp"
#include "devMode/devFlowElements/devPropertiesContentBase.hpp"

inline const DevPropertiesContentDef kDevPropertiesContent = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevPropertiesContentDef::BuildContext& context) {
		devPropertiesContentState& state = DevPropertiesContentDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		const devPropertiesSelectionNode& selection = state.selectedNode;

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.childGap = context.params.childGap;
		root.layout.padding = context.params.padding;
		root.backgroundColor = context.params.backgroundColor;
		root.clip = {
			.horizontal = false,
			.vertical = true,
			.childOffset = Clay_GetScrollOffset(),
		};

		Clay_TextElementConfig textConfig{};
		textConfig.textColor = context.params.textColor;
		textConfig.fontId = context.params.fontId;
		textConfig.fontSize = context.params.fontSize;
		textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		textConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

			CLAY(root){
				if (isDevPropertiesSelectionNull(selection))
				{
					Clay_TextElementConfig noSelectionTextConfig = textConfig;
				noSelectionTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.noSelectionText),
					CLAY_TEXT_CONFIG(noSelectionTextConfig));
				}
				else
				{
					FlowUi::devMode::DevRuntime& runtime = context.uiManager.devRuntime();
					const FlowUi::devMode::DevRegistry& registry = FlowUi::devMode::DevRegistry::instance();
					const FlowUi::devMode::ElementDescriptor* descriptor =
						registry.findElementByDefinitionId(selection.definitionId);

				uint64_t structTypeHash = 0u;
				if (descriptor != nullptr)
				{
					switch (selection.structScope)
					{
					case devPropertiesStructScope::Parameters:
						structTypeHash = descriptor->paramsStructTypeHash;
						break;
					case devPropertiesStructScope::State:
						structTypeHash = descriptor->stateStructTypeHash;
						break;
					case devPropertiesStructScope::Resources:
						structTypeHash = descriptor->resourcesStructTypeHash;
						break;
					}
				}

					const FlowUi::devMode::StructDescriptor* structure =
						(structTypeHash == 0u) ? nullptr : registry.findStructByTypeHash(structTypeHash);

					const bool unsupportedScope =
						selection.kind == devPropertiesSelectionKind::Definition &&
						selection.structScope == devPropertiesStructScope::State;

					if (unsupportedScope)
					{
						Clay_TextElementConfig unsupportedScopeTextConfig = textConfig;
						unsupportedScopeTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
						CLAY_TEXT(
							context.uiManager.toClayString(context.params.unsupportedScopeText),
							CLAY_TEXT_CONFIG(unsupportedScopeTextConfig));
					}
					else if (descriptor == nullptr || structure == nullptr)
					{
						Clay_TextElementConfig missingSelectionTextConfig = textConfig;
						missingSelectionTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.missingSelectionText),
						CLAY_TEXT_CONFIG(missingSelectionTextConfig));
				}
				else if (structure->fields.empty())
				{
					Clay_TextElementConfig noFieldsTextConfig = textConfig;
					noFieldsTextConfig.wrapMode = CLAY_TEXT_WRAP_WORDS;
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.noEditableFieldsText),
						CLAY_TEXT_CONFIG(noFieldsTextConfig));
					}
					else
					{
						Clay_ElementDeclaration fieldsColumn{};
						fieldsColumn.id = context.uiManager.toClayEID(context.createChildElementId("rows"));
						fieldsColumn.layout.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_FIT(0),
						};
						fieldsColumn.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
						fieldsColumn.layout.childGap = context.params.rowGap;
						fieldsColumn.backgroundColor = FlowUi::Flow_Color("#00000000");

						CLAY(fieldsColumn){
							for (std::size_t i = 0; i < structure->fields.size(); ++i)
							{
								const FlowUi::devMode::FieldDescriptor& field = structure->fields[i];
								const std::string fieldIdentity = makeDevPropertiesFieldIdentity(selection, field.fieldHash);
								const std::optional<FlowUi::devMode::DevValue> fieldValue =
									findCurrentEditableFieldValue(runtime, selection, field.fieldHash);
								const bool isBoolField = devFieldTypeIsBool(field.fieldTypeHash);
								const bool isEnum1Field = devFieldTypeIsEnum1(field.fieldTypeHash);
								const bool isEnum2Field = devFieldTypeIsEnum2(field.fieldTypeHash);
								const bool isFloat2Field = devFieldTypeIsFloat2(field.fieldTypeHash);
								const bool isFloat4Field = devFieldTypeIsFloat4(field.fieldTypeHash);
								const bool isEdgeU16Field = devFieldTypeIsEdgeU16(field.fieldTypeHash);
								const bool isTaggedUnionField = devFieldTypeIsTaggedUnion(field.fieldTypeHash);
								const bool isCompositeStructField = devFieldTypeIsCompositeStruct(field.fieldTypeHash);
								const bool isInputField =
									devFieldTypeIsString(field.fieldTypeHash) ||
									devFieldTypeIsIntegral(field.fieldTypeHash) ||
									devFieldTypeIsFloating(field.fieldTypeHash);

								bool toggleDefaultEnabled = false;
								if (fieldValue.has_value())
								{
									(void)tryCoerceDevValueToBool(*fieldValue, toggleDefaultEnabled);
								}
								const std::string inputInitialText =
									fieldValue.has_value()
									? devValueToEditableText(*fieldValue)
									: std::string{};
								const std::string enumDisplayText =
									fieldValue.has_value()
									? devValueToEditableTextForField(field.fieldTypeHash, *fieldValue)
									: std::string{};

								Clay_ElementDeclaration row{};
								row.id = context.uiManager.toClayEID(
									context.createChildElementId("row-" + std::to_string(i)));
								row.layout.sizing = {
									.width = CLAY_SIZING_GROW(0),
									.height = CLAY_SIZING_FIT(0),
								};
								row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
								row.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
								row.layout.childGap = context.params.rowChildGap;
								row.backgroundColor = FlowUi::Flow_Color("#00000000");

								CLAY(row){
									CLAY({
										.id = context.uiManager.toClayEID(
											context.createChildElementId("row-" + std::to_string(i) + "/label")),
										.layout = {
											.sizing = {
												.width = CLAY_SIZING_GROW(0),
												.height = CLAY_SIZING_FIT(0),
											},
										},
									}){
										CLAY_TEXT(
											context.uiManager.toClayString(field.name),
											CLAY_TEXT_CONFIG(textConfig));
									};

									if (isBoolField)
									{
										context.uiManager
											.createElement(
												kDevBasicToggle,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/toggle/" + fieldIdentity))
											.setParameters(devBasicToggleParams{
												.defaultEnabled = toggleDefaultEnabled,
												.onValueChangedCallback = [
													uiManager = &context.uiManager,
													selection,
													fieldHash = field.fieldHash
												](DevBasicToggleInteractionContext, bool isEnabled) {
													setEditableFieldOverride(
														uiManager->devRuntime(),
														selection,
														fieldHash,
														FlowUi::devMode::DevValue{isEnabled});
												},
												.sizing = context.params.valueEditorSizing,
											})
											.draw();
									}
									else if (isInputField)
									{
										const std::string inputFieldId =
											"flowui/dev/input/" + fieldIdentity;
										context.uiManager
											.createElement(
												kDevBasicInputField,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/input/" + fieldIdentity))
											.setParameters(devBasicInputFieldParams{
												.fieldId = inputFieldId,
												.initialText = inputInitialText,
												.onTextChangedCallback = [
													uiManager = &context.uiManager,
													selection,
													fieldHash = field.fieldHash,
													fieldTypeHash = field.fieldTypeHash,
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
									else if (isEnum1Field)
									{
										context.uiManager
											.createElement(
												kDevEnum1Input,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/enum/" + fieldIdentity))
											.setParameters(devEnum1InputParams{
												.text =
													enumDisplayText.empty()
													? std::string("<enum>")
													: enumDisplayText,
												.sizing = context.params.valueEditorSizing,
												.fontId = context.params.fontId,
												.fontSize = context.params.fontSize,
											})
											.draw();
									}
									else if (isEnum2Field)
									{
										context.uiManager
											.createElement(
												kDevEnum2Input,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/enum2/" + fieldIdentity))
											.setParameters(devEnum2InputParams{
												.text =
													enumDisplayText.empty()
													? std::string("<enum2>")
													: enumDisplayText,
												.sizing = context.params.valueEditorSizing,
												.fontId = context.params.fontId,
												.fontSize = context.params.fontSize,
											})
											.draw();
									}
									else if (isFloat2Field)
									{
										context.uiManager
											.createElement(
												kDevFloat2Input,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/float2/" + fieldIdentity))
											.setParameters(devFloat2InputParams{
												.text =
													enumDisplayText.empty()
													? std::string("<float2>")
													: enumDisplayText,
												.sizing = context.params.valueEditorSizing,
												.fontId = context.params.fontId,
												.fontSize = context.params.fontSize,
											})
											.draw();
									}
									else if (isFloat4Field)
									{
										context.uiManager
											.createElement(
												kDevFloat4Input,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/float4/" + fieldIdentity))
											.setParameters(devFloat4InputParams{
												.text =
													enumDisplayText.empty()
													? std::string("<float4>")
													: enumDisplayText,
												.sizing = context.params.valueEditorSizing,
												.fontId = context.params.fontId,
												.fontSize = context.params.fontSize,
											})
											.draw();
									}
									else if (isEdgeU16Field)
									{
										context.uiManager
											.createElement(
												kDevEdgeU16Input,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/edge_u16/" + fieldIdentity))
											.setParameters(devEdgeU16InputParams{
												.text =
													enumDisplayText.empty()
													? std::string("<edge_u16>")
													: enumDisplayText,
												.sizing = context.params.valueEditorSizing,
												.fontId = context.params.fontId,
												.fontSize = context.params.fontSize,
											})
											.draw();
									}
									else if (isTaggedUnionField)
									{
										context.uiManager
											.createElement(
												kDevTaggedUnionInput,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/tagged_union/" + fieldIdentity))
											.setParameters(devTaggedUnionInputParams{
												.text =
													enumDisplayText.empty()
													? std::string("<tagged union>")
													: enumDisplayText,
												.sizing = context.params.valueEditorSizing,
												.fontId = context.params.fontId,
												.fontSize = context.params.fontSize,
											})
											.draw();
									}
									else if (isCompositeStructField)
									{
										context.uiManager
											.createElement(
												kDevCompositeStructInput,
												context.createChildElementId(
													"row-" + std::to_string(i) + "/composite/" + fieldIdentity))
											.setParameters(devCompositeStructInputParams{
												.text =
													enumDisplayText.empty()
													? std::string("<composite struct>")
													: enumDisplayText,
												.sizing = context.params.valueEditorSizing,
												.fontId = context.params.fontId,
												.fontSize = context.params.fontSize,
											})
											.draw();
									}
									else
									{
										CLAY({
											.id = context.uiManager.toClayEID(
												context.createChildElementId("row-" + std::to_string(i) + "/unsupported")),
											.layout = {
												.sizing = {
													.width = context.params.valueEditorSizing.width,
													.height = CLAY_SIZING_FIT(0),
												},
											},
										}){
											CLAY_TEXT(
												context.uiManager.toClayString(context.params.unsupportedFieldTypeText),
												CLAY_TEXT_CONFIG(textConfig));
										};
									}
								};
							}
						};
					}
				}
			};
	},
};
