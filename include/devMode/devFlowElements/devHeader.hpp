#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicButton.hpp"

struct devHeaderParams {
	Clay_Sizing sizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	Clay_LayoutDirection childLayoutDirection = CLAY_LEFT_TO_RIGHT;
	Clay_ChildAlignment childAlignment = Clay_ChildAlignment{
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	uint16_t childGap = 8;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");

	std::string titleText = "FlowUi Dev tool";
	uint16_t titleFontId = 0;
	uint16_t titleFontSize = 12;
	Clay_Color titleTextColor = FlowUi::Flow_Color("#ffffffff");
	Clay_TextElementConfigWrapMode titleWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment titleTextAlignment = CLAY_TEXT_ALIGN_LEFT;

	std::string exportButtonText = "Export";
	uint16_t exportButtonFontId = 0;
	uint16_t exportButtonFontSize = 12;
	Clay_Color exportButtonTextColor = FlowUi::Flow_Color("#ffffffff");
	Clay_Color exportButtonBackgroundColor = FlowUi::Flow_Color("#2f323aff");
	Clay_Color exportButtonHoverBackgroundColor = FlowUi::Flow_Color("#434957ff");
	Clay_Color exportButtonBorderColor = FlowUi::Flow_Color("#00000000");
	Clay_BorderWidth exportButtonBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0};
	Clay_CornerRadius exportButtonCornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_Padding exportButtonPadding = CLAY_PADDING_ALL(8);
	uint16_t exportButtonChildGap = 6;
	Clay_Sizing exportButtonIconContainerSizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(14),
		.height = CLAY_SIZING_FIXED(14),
	};
	Clay_Sizing exportButtonIconSizing = Clay_Sizing{
		.width = CLAY_SIZING_PERCENT(1.0f),
		.height = CLAY_SIZING_PERCENT(1.0f),
	};
	Clay_Color exportButtonIconTintColor = FlowUi::Flow_Color("#00000000");
};

struct devHeaderResources {
	bool exportIconPrepared = false;
	FlowUi::TextureRef exportIcon = FlowUi::TextureRef{};
};

using DevHeaderDef = FlowUi::ElementDefinition<
	devHeaderParams,
	void,
	devHeaderResources,
	FLOW_DEF_ID("DevHeader"),
	true>;

inline const DevHeaderDef kDevHeader = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevHeaderDef::BuildContext& context) {
		FlowUi::TextureRef exportIcon{};
		if (DevHeaderDef::resources.has_value()) {
			const devHeaderResources& resources = *DevHeaderDef::resources;
			if (resources.exportIconPrepared) {
				exportIcon = resources.exportIcon;
			}
		}

		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = context.params.sizing;
		root.layout.padding = context.params.padding;
		root.layout.layoutDirection = context.params.childLayoutDirection;
		root.layout.childAlignment = context.params.childAlignment;
		root.layout.childGap = context.params.childGap;
		root.backgroundColor = context.params.backgroundColor;

		Clay_TextElementConfig titleTextConfig{};
		titleTextConfig.textColor = context.params.titleTextColor;
		titleTextConfig.fontId = context.params.titleFontId;
		titleTextConfig.fontSize = context.params.titleFontSize;
		titleTextConfig.wrapMode = context.params.titleWrapMode;
		titleTextConfig.textAlignment = context.params.titleTextAlignment;

		const Clay_ElementId titleId = context.uiManager.toClayEID(context.createChildElementId("title"));
		const Clay_ElementId spacerId = context.uiManager.toClayEID(context.createChildElementId("spacer"));
		const std::string exportButtonId = context.createChildElementId("export-button");

		CLAY(rootId, root){
			CLAY(titleId, {}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.titleText),
					CLAY_TEXT_CONFIG(titleTextConfig));
			};

			CLAY(spacerId, {
				.layout = {
					.sizing = {
						.width = CLAY_SIZING_GROW(0),
						.height = CLAY_SIZING_FIT(0),
					},
				},
			}){};

			const Clay_Color hoverBackgroundColor = context.params.exportButtonHoverBackgroundColor;
			context.uiManager
				.createElement(kDevBasicButton, exportButtonId)
				.setParameters(devBasicButtonParams{
					.text = context.params.exportButtonText,
					.icon = exportIcon,
					.onHoveredCallback = [hoverBackgroundColor](DevBasicButtonInteractionContext buttonContext) {
						buttonContext.params.backgroundColor = hoverBackgroundColor;
					},
					.onPressedCallback = [](DevBasicButtonInteractionContext buttonContext) {
						(void)FlowUi::devMode::exportOverridesAsJson(buttonContext.uiManager);
					},
					.contentMode = devBasicButtonParams::ContentMode::IconThenText,
					.padding = context.params.exportButtonPadding,
					.sizing = Clay_Sizing{.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
					.backgroundColor = context.params.exportButtonBackgroundColor,
					.cornerRadius = context.params.exportButtonCornerRadius,
					.borderColor = context.params.exportButtonBorderColor,
					.borderWidth = context.params.exportButtonBorderWidth,
					.childLayoutDirection = CLAY_LEFT_TO_RIGHT,
					.childAlignment = Clay_ChildAlignment{.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
					.childGap = context.params.exportButtonChildGap,
					.textWrapMode = CLAY_TEXT_WRAP_NONE,
					.textAlignment = CLAY_TEXT_ALIGN_LEFT,
					.fontId = context.params.exportButtonFontId,
					.fontSize = context.params.exportButtonFontSize,
					.textColor = context.params.exportButtonTextColor,
					.iconContainerSizing = context.params.exportButtonIconContainerSizing,
					.iconContainerPadding = CLAY_PADDING_ALL(0),
					.iconContainerChildLayoutDirection = CLAY_LEFT_TO_RIGHT,
					.iconContainerChildAlignment = Clay_ChildAlignment{.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
					.iconContainerChildGap = 0,
					.iconContainerBackgroundColor = FlowUi::Flow_Color("#00000000"),
					.iconContainerBorderColor = FlowUi::Flow_Color("#00000000"),
					.iconContainerBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0},
					.iconSizing = context.params.exportButtonIconSizing,
					.iconTintColor = context.params.exportButtonIconTintColor,
				})
				.draw();
		};
	},
};
