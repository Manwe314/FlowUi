#pragma once

#include "devMode/devFlowElements/common.hpp"

struct devFooterParams {
	int height = 36;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
};

using DevFooterDef = FlowUi::ElementDefinition<
	devFooterParams,
	void,
	void,
	FLOW_DEF_ID("DevFooter"),
	true>;

inline const DevFooterDef kDevFooter = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevFooterDef::BuildContext& context) {
		int height = context.params.height;
		if (height < 0)
		{
			height = 0;
		}

		Clay_ElementDeclaration root{};
		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(static_cast<float>(height)),
		};
		root.backgroundColor = context.params.backgroundColor;

		CLAY(rootId, root){};
	},
};
