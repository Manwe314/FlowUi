#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devBasicButton.hpp"
#include "devMode/devFlowElements/devPanelContentShared.hpp"

struct devHierarchyHeadParams {
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	uint16_t childGap = 8;

	std::string titleText = "Hierarchy";
	uint16_t titleFontId = 0;
	uint16_t titleFontSize = 12;
	Clay_Color titleTextColor = FlowUi::Flow_Color("#ffffffff");

	std::string instancesButtonText = "Instances";
	std::string definitionsButtonText = "Definitions";
	Clay_Color modeButtonActiveBackgroundColor = FlowUi::Flow_Color("#434957ff");
	Clay_Color modeButtonInactiveBackgroundColor = FlowUi::Flow_Color("#2f323aff");
	Clay_Color modeButtonTextColor = FlowUi::Flow_Color("#ffffffff");
	Clay_Color modeButtonBorderColor = FlowUi::Flow_Color("#00000000");
	Clay_BorderWidth modeButtonBorderWidth = Clay_BorderWidth{0, 0, 0, 0, 0};
	Clay_CornerRadius modeButtonCornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_Padding modeButtonPadding = CLAY_PADDING_ALL(8);
	uint16_t modeButtonsRowGap = 6;
	uint16_t modeButtonFontId = 0;
	uint16_t modeButtonFontSize = 12;
};

using DevHierarchyHeadDef = FlowUi::ElementDefinition<
	devHierarchyHeadParams,
	void,
	void,
	FLOW_DEF_ID("DevHierarchyHead"),
	true>;

inline const DevHierarchyHeadDef kDevHierarchyHead = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevHierarchyHeadDef::BuildContext& context) {
		bool isViewingInstances = true;
		const devPanelContentState* state = findSingleDevPanelContentStateConst();
		if (state != nullptr)
		{
			isViewingInstances = state->isViewingInstances;
		}

		const Clay_Color instancesButtonColor =
			isViewingInstances
			? context.params.modeButtonActiveBackgroundColor
			: context.params.modeButtonInactiveBackgroundColor;
		const Clay_Color definitionsButtonColor =
			isViewingInstances
			? context.params.modeButtonInactiveBackgroundColor
			: context.params.modeButtonActiveBackgroundColor;

		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
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

		Clay_ElementDeclaration modeButtonsRow{};
		const Clay_ElementId modeButtonsRowId = context.uiManager.toClayEID(context.createChildElementId("mode-buttons-row"));
		modeButtonsRow.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIT(0),
		};
		modeButtonsRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		modeButtonsRow.layout.childGap = context.params.modeButtonsRowGap;
		modeButtonsRow.backgroundColor = FlowUi::Flow_Color("#00000000");

		CLAY(rootId, root){
			CLAY(context.uiManager.toClayEID(context.createChildElementId("title")), {}){
				CLAY_TEXT(
					context.uiManager.toClayString(context.params.titleText),
					CLAY_TEXT_CONFIG(titleTextConfig));
			};

			CLAY(modeButtonsRowId, modeButtonsRow){
				context.uiManager
					.createElement(kDevBasicButton, context.createChildElementId("instances-button"))
					.setParameters(devBasicButtonParams{
						.text = context.params.instancesButtonText,
						.onPressedCallback = [](DevBasicButtonInteractionContext) {
							devPanelContentState* panelState = findSingleDevPanelContentState();
							if (panelState != nullptr)
							{
								panelState->isViewingInstances = true;
							}
						},
						.contentMode = devBasicButtonParams::ContentMode::TextOnly,
						.padding = context.params.modeButtonPadding,
						.sizing = Clay_Sizing{.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
						.backgroundColor = instancesButtonColor,
						.cornerRadius = context.params.modeButtonCornerRadius,
						.borderColor = context.params.modeButtonBorderColor,
						.borderWidth = context.params.modeButtonBorderWidth,
						.textWrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_LEFT,
						.fontId = context.params.modeButtonFontId,
						.fontSize = context.params.modeButtonFontSize,
						.textColor = context.params.modeButtonTextColor,
					})
					.draw();

				context.uiManager
					.createElement(kDevBasicButton, context.createChildElementId("definitions-button"))
					.setParameters(devBasicButtonParams{
						.text = context.params.definitionsButtonText,
						.onPressedCallback = [](DevBasicButtonInteractionContext) {
							devPanelContentState* panelState = findSingleDevPanelContentState();
							if (panelState != nullptr)
							{
								panelState->isViewingInstances = false;
							}
						},
						.contentMode = devBasicButtonParams::ContentMode::TextOnly,
						.padding = context.params.modeButtonPadding,
						.sizing = Clay_Sizing{.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
						.backgroundColor = definitionsButtonColor,
						.cornerRadius = context.params.modeButtonCornerRadius,
						.borderColor = context.params.modeButtonBorderColor,
						.borderWidth = context.params.modeButtonBorderWidth,
						.textWrapMode = CLAY_TEXT_WRAP_NONE,
						.textAlignment = CLAY_TEXT_ALIGN_LEFT,
						.fontId = context.params.modeButtonFontId,
						.fontSize = context.params.modeButtonFontSize,
						.textColor = context.params.modeButtonTextColor,
					})
					.draw();
			};
		};
	},
};
