#pragma once

#include "devMode/devFlowElements/common.hpp"

struct devBasicButtonParams;
using DevBasicButtonDef = FlowUi::ElementDefinition<
	devBasicButtonParams,
	void,
	void,
	FLOW_DEF_ID("DevBasicButton"),
	true>;
using DevBasicButtonInteractionContext = DevBasicButtonDef::InteractionContext;

struct devBasicButtonParams {
	enum class ContentMode : uint8_t {
		None,
		TextOnly,
		IconOnly,
		IconThenText,
		TextThenIcon,
	};

	std::string text = "";
	FlowUi::TextureRef icon = FlowUi::TextureRef{};
	std::function<void(DevBasicButtonInteractionContext)> onHoveredCallback = nullptr;
	std::function<void(DevBasicButtonInteractionContext)> onPressedCallback = nullptr;
	ContentMode contentMode = ContentMode::None;

	Clay_Padding padding = CLAY_PADDING_ALL(10);
	Clay_Sizing sizing = Clay_Sizing{.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)};
	Clay_Color backgroundColor = FlowUi::Flow_Color("#cfcfcfff");
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_Color borderColor = FlowUi::Flow_Color("#8f8d8dff");
	Clay_BorderWidth borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	Clay_LayoutDirection childLayoutDirection = CLAY_LEFT_TO_RIGHT;
	Clay_ChildAlignment childAlignment = Clay_ChildAlignment{.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	uint16_t childGap = 8;

	Clay_TextElementConfigWrapMode textWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment textAlignment = CLAY_TEXT_ALIGN_LEFT;
	uint16_t fontId = 0;
	uint16_t fontSize = 16;
	Clay_Color textColor = FlowUi::Flow_Color("#000000ff");

	Clay_Sizing iconContainerSizing = Clay_Sizing{.width = CLAY_SIZING_FIXED(18), .height = CLAY_SIZING_FIXED(18)};
	Clay_Padding iconContainerPadding = CLAY_PADDING_ALL(0);
	Clay_LayoutDirection iconContainerChildLayoutDirection = CLAY_LEFT_TO_RIGHT;
	Clay_ChildAlignment iconContainerChildAlignment = Clay_ChildAlignment{.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
	uint16_t iconContainerChildGap = 0;
	Clay_Color iconContainerBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color iconContainerBorderColor = FlowUi::Flow_Color("#00000000");
	Clay_BorderWidth iconContainerBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0};

	Clay_Sizing iconSizing = Clay_Sizing{.width = CLAY_SIZING_PERCENT(1.0f), .height = CLAY_SIZING_PERCENT(1.0f)};
	Clay_Color iconTintColor = FlowUi::Flow_Color("#00000000");
};

inline const DevBasicButtonDef kDevBasicButton = {
	+[](DevBasicButtonDef::InteractionContext& context) {
		context.uiManager.requestCursor(FlowUi::CursorType::PointingHand);
		if (context.params.onHoveredCallback != nullptr)
		{
			context.params.onHoveredCallback(context);
		}
	},
	+[](DevBasicButtonDef::InteractionContext& context) {
		if (context.params.onPressedCallback != nullptr)
		{
			context.params.onPressedCallback(context);
		}
	},
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevBasicButtonDef::BuildContext& context) {
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);

		const devBasicButtonParams::ContentMode contentMode = context.params.contentMode;
		const bool needsText =
			contentMode == devBasicButtonParams::ContentMode::TextOnly ||
			contentMode == devBasicButtonParams::ContentMode::IconThenText ||
			contentMode == devBasicButtonParams::ContentMode::TextThenIcon;
		const bool needsIcon =
			contentMode == devBasicButtonParams::ContentMode::IconOnly ||
			contentMode == devBasicButtonParams::ContentMode::IconThenText ||
			contentMode == devBasicButtonParams::ContentMode::TextThenIcon;

		Clay_LayoutConfig rootLayout{};
		rootLayout.layoutDirection = context.params.childLayoutDirection;
		rootLayout.sizing = context.params.sizing;
		rootLayout.padding = context.params.padding;
		rootLayout.childAlignment = context.params.childAlignment;
		rootLayout.childGap = context.params.childGap;

		Clay_ElementDeclaration root{};
		root.layout = rootLayout;
		root.backgroundColor = context.params.backgroundColor;
		root.cornerRadius = context.params.cornerRadius;
		root.border = {.color = context.params.borderColor, .width = context.params.borderWidth};

		Clay_TextElementConfig textConfig{};
		Clay_ElementId textId{};
		if (needsText)
		{
			const std::string textPath = context.createChildElementId("text");
			textId = context.uiManager.toClayEID(textPath);
			textConfig.textColor = context.params.textColor;
			textConfig.fontSize = context.params.fontSize;
			textConfig.wrapMode = context.params.textWrapMode;
			textConfig.textAlignment = context.params.textAlignment;
			textConfig.fontId = context.params.fontId;
		}

		Clay_ElementDeclaration iconContainer{};
		Clay_ElementDeclaration iconElement{};
		Clay_ElementId iconContainerId{};
		Clay_ElementId iconElementId{};
		if (needsIcon)
		{
			const std::string iconContainerPath = context.createChildElementId("icon-container");
			const std::string iconPath = context.createChildElementId("icon");
			iconContainerId = context.uiManager.toClayEID(iconContainerPath);
			iconElementId = context.uiManager.toClayEID(iconPath);

			Clay_LayoutConfig iconContainerLayout{};
			iconContainerLayout.layoutDirection = context.params.iconContainerChildLayoutDirection;
			iconContainerLayout.sizing = context.params.iconContainerSizing;
			iconContainerLayout.padding = context.params.iconContainerPadding;
			iconContainerLayout.childAlignment = context.params.iconContainerChildAlignment;
			iconContainerLayout.childGap = context.params.iconContainerChildGap;
			iconContainer.layout = iconContainerLayout;
			iconContainer.backgroundColor = context.params.iconContainerBackgroundColor;
			iconContainer.border = {.color = context.params.iconContainerBorderColor, .width = context.params.iconContainerBorderWidth};

			Clay_LayoutConfig iconLayout{};
			iconLayout.sizing = context.params.iconSizing;

			iconElement.layout = iconLayout;
			iconElement.backgroundColor = context.params.iconTintColor;
			iconElement.image = {
				.imageData = context.uiManager.storeTexture(context.params.icon),
			};
		}

		auto drawTextChild = [&]() {
			CLAY(textId, {}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.text),
					CLAY_TEXT_CONFIG(textConfig)
				);
			};
		};

		auto drawIconChild = [&]() {
			CLAY(iconContainerId, iconContainer){
				CLAY(iconElementId, iconElement){};
			};
		};

		CLAY(rootId, root){
			switch (contentMode)
			{
			case devBasicButtonParams::ContentMode::None:
				break;
			case devBasicButtonParams::ContentMode::TextOnly:
				drawTextChild();
				break;
			case devBasicButtonParams::ContentMode::IconOnly:
				drawIconChild();
				break;
			case devBasicButtonParams::ContentMode::IconThenText:
				drawIconChild();
				drawTextChild();
				break;
			case devBasicButtonParams::ContentMode::TextThenIcon:
				drawTextChild();
				drawIconChild();
				break;
			}
		};
	},
};
