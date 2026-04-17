#pragma once

#include "devMode/devFlowElements/common.hpp"

struct devPropertiesHeadParams {
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	uint16_t childGap = 6;

	std::string titleText = "Properties";
	uint16_t titleFontId = 0;
	uint16_t titleFontSize = 12;
	Clay_Color titleTextColor = FlowUi::Flow_Color("#ffffffff");

	std::string selectedElementIdText = "placeholder";
	uint16_t selectedElementIdFontId = 0;
	uint16_t selectedElementIdFontSize = 12;
	Clay_Color selectedElementIdTextColor = FlowUi::Flow_Color("#ffffffff");
};

using DevPropertiesHeadDef = FlowUi::ElementDefinition<
	devPropertiesHeadParams,
	void,
	void,
	FLOW_DEF_ID("DevPropertiesHead"),
	true>;

inline const DevPropertiesHeadDef kDevPropertiesHead = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevPropertiesHeadDef::BuildContext& context) {
		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIT(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.padding = context.params.padding;
		root.layout.childGap = context.params.childGap;
		root.backgroundColor = context.params.backgroundColor;

		Clay_TextElementConfig titleTextConfig{};
		titleTextConfig.textColor = context.params.titleTextColor;
		titleTextConfig.fontId = context.params.titleFontId;
		titleTextConfig.fontSize = context.params.titleFontSize;
		titleTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		titleTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		Clay_TextElementConfig selectedTextConfig{};
		selectedTextConfig.textColor = context.params.selectedElementIdTextColor;
		selectedTextConfig.fontId = context.params.selectedElementIdFontId;
		selectedTextConfig.fontSize = context.params.selectedElementIdFontSize;
		selectedTextConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		selectedTextConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		CLAY(root){
			CLAY({.id = context.uiManager.toClayEID(context.createChildElementId("title"))}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.titleText),
					CLAY_TEXT_CONFIG(titleTextConfig));
			};

			CLAY({.id = context.uiManager.toClayEID(context.createChildElementId("selected-id"))}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.selectedElementIdText),
					CLAY_TEXT_CONFIG(selectedTextConfig));
			};
		};
	},
};
