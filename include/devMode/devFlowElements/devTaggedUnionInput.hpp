#pragma once

#include "devMode/devFlowElements/common.hpp"

struct devTaggedUnionInputParams {
	std::string text = "";
	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(220),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(10);
	Clay_Color backgroundColor = FlowUi::Flow_Color("#252932ff");
	Clay_Color borderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_TextElementConfigWrapMode textWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment textAlignment = CLAY_TEXT_ALIGN_LEFT;
	uint16_t fontId = 0;
	uint16_t fontSize = 14;
	Clay_Color textColor = FlowUi::Flow_Color("#ffffffff");
};

using DevTaggedUnionInputDef = FlowUi::ElementDefinition<
	devTaggedUnionInputParams,
	void,
	void,
	FLOW_DEF_ID("DevTaggedUnionInput"),
	true>;

inline const DevTaggedUnionInputDef kDevTaggedUnionInput = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevTaggedUnionInputDef::BuildContext& context) {
		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.padding = context.params.padding;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		root.backgroundColor = context.params.backgroundColor;
		root.cornerRadius = context.params.cornerRadius;
		root.border = {
			.color = context.params.borderColor,
			.width = context.params.borderWidth,
		};

		Clay_TextElementConfig textConfig{};
		textConfig.textColor = context.params.textColor;
		textConfig.fontId = context.params.fontId;
		textConfig.fontSize = context.params.fontSize;
		textConfig.wrapMode = context.params.textWrapMode;
		textConfig.textAlignment = context.params.textAlignment;

		CLAY(root){
			CLAY({.id = context.uiManager.toClayEID(context.createChildElementId("text"))}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.text),
					CLAY_TEXT_CONFIG(textConfig));
			};
		};
	},
};
