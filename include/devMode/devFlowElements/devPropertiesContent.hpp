#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devPropertiesCard.hpp"
#include "devMode/devFlowElements/devPropertiesContentBase.hpp"
#include "devMode/devFlowElements/devPropertiesEditableHelpers.hpp"

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

		Clay_TextElementConfig textConfig{};
		textConfig.textColor = context.params.textColor;
		textConfig.fontId = context.params.fontId;
		textConfig.fontSize = context.params.fontSize;
		textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		textConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

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
			.horizontal = true,
			.vertical = true,
			.childOffset = Clay_GetScrollOffset(),
		};

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
							const std::string cardId =
								"card-" + std::to_string(i) + "/" + fieldIdentity;
							const std::string fieldTypeName =
								devFieldTypeDisplayName(field.fieldTypeHash, field.fieldTypeToken);

							context.uiManager
								.createElement(kDevPropertiesCard, context.createChildElementId(cardId))
								.setParameters(devPropertiesCardParams{
									.selection = selection,
									.fieldHash = field.fieldHash,
									.fieldTypeHash = field.fieldTypeHash,
									.fieldName = field.name,
									.fieldTypeName = fieldTypeName,
									.fieldIdentity = fieldIdentity,
									.rowGap = context.params.rowChildGap,
									.headerChildGap = context.params.rowChildGap,
									.valueEditorSizing = context.params.valueEditorSizing,
									.unsupportedFieldTypeText = context.params.unsupportedFieldTypeText,
									.fontId = context.params.fontId,
									.fontSize = context.params.fontSize,
									.nameTextColor = context.params.textColor,
									.typeTextColor = context.params.textColor,
									.valueTextColor = context.params.textColor,
								})
								.draw();
						}
					};
				}
			}
		};
	},
};
