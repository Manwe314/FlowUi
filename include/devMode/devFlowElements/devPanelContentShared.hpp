#pragma once

#include "devMode/devFlowElements/common.hpp"

struct devPanelContentParams {
	int defaultHierarchyWidthPx = 280;
	int minHierarchyWidthPx = 180;
	int maxHierarchyWidthPx = 640;
	int separatorThicknessPx = 6;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color hierarchyBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color propertiesBackgroundColor = FlowUi::Flow_Color("#00000000");
};

struct devPanelContentState {
	bool isViewingInstances = true;
	int hierarchyWidthPx = 0;
	bool hierarchyWidthInitialized = false;
	std::string selectedElementId = "";
};

using DevPanelContentDef = FlowUi::ElementDefinition<
	devPanelContentParams,
	devPanelContentState,
	void,
	FLOW_DEF_ID("DevPanelContent"),
	true>;

inline devPanelContentState* findSingleDevPanelContentState() {
	constexpr std::string_view kSingleDevPanelContentElementId = "flowui/dev/debug-view/main-view/content";

	devPanelContentState* state =
		DevPanelContentDef::tryGetState(FlowUi::toFlowId(kSingleDevPanelContentElementId));
	if (state != nullptr)
	{
		return state;
	}
	if (!DevPanelContentDef::statePool.empty())
	{
		return &DevPanelContentDef::statePool.front().second;
	}
	return nullptr;
}

inline const devPanelContentState* findSingleDevPanelContentStateConst() {
	constexpr std::string_view kSingleDevPanelContentElementId = "flowui/dev/debug-view/main-view/content";

	const devPanelContentState* state =
		DevPanelContentDef::tryGetStateConst(FlowUi::toFlowId(kSingleDevPanelContentElementId));
	if (state != nullptr)
	{
		return state;
	}
	if (!DevPanelContentDef::statePool.empty())
	{
		return &DevPanelContentDef::statePool.front().second;
	}
	return nullptr;
}
