#pragma once

#include "devMode/devFlowElements/common.hpp"

struct devBasicToggleParams;
struct devBasicToggleState {
	bool initialized = false;
	bool isEnabled = false;
};

using DevBasicToggleDef = FlowUi::ElementDefinition<
	devBasicToggleParams,
	devBasicToggleState,
	void,
	FLOW_DEF_ID("DevBasicToggle"),
	true>;
using DevBasicToggleInteractionContext = DevBasicToggleDef::InteractionContext;

struct devBasicToggleParams {
	bool defaultEnabled = false;
	std::string text = "";
	std::function<void(DevBasicToggleInteractionContext, bool)> onValueChangedCallback = nullptr;

	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(26),
		.height = CLAY_SIZING_FIXED(26),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(2);
	Clay_Color enabledBackgroundColor = FlowUi::Flow_Color("#4b8c5aff");
	Clay_Color disabledBackgroundColor = FlowUi::Flow_Color("#2f323aff");
	Clay_Color borderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);
	Clay_ChildAlignment childAlignment = Clay_ChildAlignment{
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	Clay_TextElementConfigWrapMode textWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment textAlignment = CLAY_TEXT_ALIGN_CENTER;
	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color textColor = FlowUi::Flow_Color("#ffffffff");
};

inline const DevBasicToggleDef kDevBasicToggle = {
	nullptr,
	+[](DevBasicToggleDef::InteractionContext& context) {
		devBasicToggleState& state = DevBasicToggleDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		if (!state.initialized)
		{
			state.isEnabled = context.params.defaultEnabled;
			state.initialized = true;
		}
		state.isEnabled = !state.isEnabled;
		if (context.params.onValueChangedCallback != nullptr)
		{
			context.params.onValueChangedCallback(context, state.isEnabled);
		}
	},
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevBasicToggleDef::BuildContext& context) {
		devBasicToggleState& state = DevBasicToggleDef::getOrCreateState(FlowUi::toFlowId(context.elementID));
		if (!state.initialized)
		{
			state.isEnabled = context.params.defaultEnabled;
			state.initialized = true;
		}

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.padding = context.params.padding;
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.childAlignment = context.params.childAlignment;
		root.backgroundColor =
			state.isEnabled
			? context.params.enabledBackgroundColor
			: context.params.disabledBackgroundColor;
		root.cornerRadius = context.params.cornerRadius;
		root.border = {.color = context.params.borderColor, .width = context.params.borderWidth};

		Clay_TextElementConfig textConfig{};
		textConfig.textColor = context.params.textColor;
		textConfig.fontId = context.params.fontId;
		textConfig.fontSize = context.params.fontSize;
		textConfig.wrapMode = context.params.textWrapMode;
		textConfig.textAlignment = context.params.textAlignment;

		CLAY(root){
			if (!context.params.text.empty())
			{
				CLAY({
					.id = context.uiManager.toClayEID(context.createChildElementId("text")),
					.layout = {
						.sizing = {
							.width = CLAY_SIZING_GROW(0),
							.height = CLAY_SIZING_GROW(0),
						},
					},
				}){
					CLAY_TEXT(
						context.uiManager.toClayString(context.params.text),
						CLAY_TEXT_CONFIG(textConfig));
				};
			}
		};
	},
};
