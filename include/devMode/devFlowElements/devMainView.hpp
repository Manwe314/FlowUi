#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devFooter.hpp"
#include "devMode/devFlowElements/devHeader.hpp"
#include "devMode/devFlowElements/devPanelContent.hpp"

struct mainDevViewParams {
	int width = 420;
	int footerHeight = 36;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#1f2127ff");
};

using MainDevViewDef = FlowUi::ElementDefinition<
	mainDevViewParams,
	void,
	void,
	FLOW_DEF_ID("MainDevView"),
	true>;

inline const MainDevViewDef kMainDevView = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](MainDevViewDef::BuildContext& context) {
		int width = context.params.width;
		if (width < 0)
		{
			width = 0;
		}

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_FIXED(static_cast<float>(width)),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.childGap = 0;
		root.backgroundColor = context.params.backgroundColor;

		CLAY(root){
			context.uiManager
				.createElement(kDevHeader, context.createChildElementId("header"))
				.setParameters(devHeaderParams{})
				.draw();

			context.uiManager
				.createElement(kDevPanelContent, context.createChildElementId("content"))
				.setParameters(devPanelContentParams{})
				.draw();

			context.uiManager
				.createElement(kDevFooter, context.createChildElementId("footer"))
				.setParameters(devFooterParams{
					.height = context.params.footerHeight,
				})
				.draw();
		};
	},
};
