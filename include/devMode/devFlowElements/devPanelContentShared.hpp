#pragma once

#include "devMode/devFlowElements/common.hpp"

struct devPanelContentParams {
	float defaultHierarchySplitRatio = 0.60f;
	int defaultHierarchyWidthPx = 280;
	int minHierarchyWidthPx = 180;
	int hardMinHierarchyWidthPx = 90;
	int maxHierarchyWidthPx = 640;
	int minPropertiesWidthPx = 220;
	int hardMinPropertiesWidthPx = 120;
	int separatorThicknessPx = 6;
	int panelWidthHintPx = 0;
	int panelHeightHintPx = 0;
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color hierarchyBackgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Color propertiesBackgroundColor = FlowUi::Flow_Color("#00000000");
};

struct devPanelContentState {
	bool isViewingInstances = true;
	float hierarchySplitRatio = 0.60f;
	bool hierarchySplitInitialized = false;
	int lastPanelWidthPx = 0;
	int lastPanelHeightPx = 0;
	std::string selectedElementId = "";
};

using DevPanelContentDef = FlowUi::ElementDefinition<
	devPanelContentParams,
	devPanelContentState,
	void,
	FLOW_DEF_ID("DevPanelContent"),
	true>;

inline devPanelContentState* findSingleDevPanelContentState(FlowUi::UiManager& uiManager) {
	constexpr std::string_view kSingleDevPanelContentElementId = "flowui/dev/debug-view/main-view/content";
	return uiManager.elements().modifyState(
		DevPanelContentDef{},
		uiManager.windowId(),
		FlowUi::toFlowId(kSingleDevPanelContentElementId));
}

inline const devPanelContentState* findSingleDevPanelContentStateConst(
	const FlowUi::UiManager& uiManager) {
	constexpr std::string_view kSingleDevPanelContentElementId = "flowui/dev/debug-view/main-view/content";
	return uiManager.elements().readState(
		DevPanelContentDef{},
		uiManager.windowId(),
		FlowUi::toFlowId(kSingleDevPanelContentElementId));
}
