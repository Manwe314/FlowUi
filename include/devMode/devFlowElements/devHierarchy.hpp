#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devHierarchyHead.hpp"
#include "devMode/devFlowElements/devHierarchyContent.hpp"

struct devHierarchyParams {
	int width = 280;
	int height = 0;
	int minRowContentWidthPx = 180;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color headBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color contentBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color modeButtonActiveBackgroundColor = FlowUi::Flow_Color("#434957ff");
	Clay_Color modeButtonInactiveBackgroundColor = FlowUi::Flow_Color("#2f323aff");
};

using DevHierarchyDef = FlowUi::ElementDefinition<
	devHierarchyParams,
	void,
	void,
	FLOW_DEF_ID("DevHierarchy"),
	true>;

inline const DevHierarchyDef kDevHierarchy = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevHierarchyDef::BuildContext& context) {
		int width = context.params.width;
		if (width < 0)
		{
			width = 0;
		}
		int height = context.params.height;
		if (height < 0)
		{
			height = 0;
		}

		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_FIXED(static_cast<float>(width)),
			.height =
				height > 0
				? CLAY_SIZING_FIXED(static_cast<float>(height))
				: CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.layout.childGap = 0;
		root.backgroundColor = context.params.backgroundColor;

		CLAY(rootId, root){
			context.uiManager
				.createElement(kDevHierarchyHead, context.createChildElementId("head"))
				.setParameters(devHierarchyHeadParams{
					.backgroundColor = context.params.headBackgroundColor,
					.modeButtonActiveBackgroundColor = context.params.modeButtonActiveBackgroundColor,
					.modeButtonInactiveBackgroundColor = context.params.modeButtonInactiveBackgroundColor,
				})
				.draw();

			context.uiManager
				.createElement(kDevHierarchyContent, context.createChildElementId("content"))
				.setParameters(devHierarchyContentParams{
					.backgroundColor = context.params.contentBackgroundColor,
					.panelWidthPx = width,
					.minRowContentWidthPx = context.params.minRowContentWidthPx,
				})
				.draw();
		};
	},
};
