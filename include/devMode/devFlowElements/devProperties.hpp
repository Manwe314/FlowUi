#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devPropertiesHead.hpp"
#include "devMode/devFlowElements/devPropertiesContent.hpp"

struct devPropertiesParams {
	int width = 0;
	int height = 0;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color headBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color contentBackgroundColor = FlowUi::Flow_Color("#00000000");
	std::string selectedElementIdText = "placeholder";
};

using DevPropertiesDef = FlowUi::ElementDefinition<
	devPropertiesParams,
	void,
	void,
	FLOW_DEF_ID("DevProperties"),
	true>;

inline const DevPropertiesDef kDevProperties = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevPropertiesDef::BuildContext& context) {
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
			.width =
				width > 0
				? CLAY_SIZING_FIXED(static_cast<float>(width))
				: CLAY_SIZING_GROW(0),
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
				.createElement(kDevPropertiesHead, context.createChildElementId("head"))
				.setParameters(devPropertiesHeadParams{
					.backgroundColor = context.params.headBackgroundColor,
					.selectedElementIdText = context.params.selectedElementIdText,
				})
				.draw();

			context.uiManager
				.createElement(kDevPropertiesContent, context.createChildElementId("content"))
				.setParameters(devPropertiesContentParams{
					.backgroundColor = context.params.contentBackgroundColor,
				})
				.draw();
		};
	},
};
