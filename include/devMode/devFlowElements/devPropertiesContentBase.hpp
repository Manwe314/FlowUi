#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devPropertiesSelection.hpp"

struct devPropertiesContentParams {
	Clay_Color backgroundColor = FlowUi::Flow_Color("#00000000");
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	uint16_t childGap = 8;
	uint16_t rowGap = 6;
	uint16_t rowChildGap = 8;
	Clay_Sizing valueEditorSizing = Clay_Sizing{
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	std::string noSelectionText = "Select an Element or a Definition from the Hirearchy";
	std::string missingSelectionText = "Selected target is unavailable.";
	std::string unsupportedScopeText = "This scope cannot be overridden with current runtime keys.";
	std::string noEditableFieldsText = "No editable fields are registered for the selected target.";
	std::string unsupportedFieldTypeText = "<unsupported type>";
	uint16_t fontId = 0;
	uint16_t fontSize = 12;
	Clay_Color textColor = FlowUi::Flow_Color("#ffffffff");
};

struct devPropertiesContentState {
	devPropertiesSelectionNode selectedNode{};
};

using DevPropertiesContentDef = FlowUi::ElementDefinition<
	devPropertiesContentParams,
	devPropertiesContentState,
	void,
	FLOW_DEF_ID("DevPropertiesContent"),
	true>;

inline devPropertiesContentState* findSingleDevPropertiesContentState(
	FlowUi::UiManager& uiManager) {
	constexpr std::string_view kSingleDevPropertiesContentElementId =
		"flowui/dev/debug-view/main-view/content/properties/content";
	return uiManager.elements().modifyState(
		DevPropertiesContentDef{},
		uiManager.windowId(),
		FlowUi::toFlowId(kSingleDevPropertiesContentElementId));
}

inline bool setSelectedDevPropertiesNode(
	FlowUi::UiManager& uiManager,
	const devPropertiesSelectionNode& selection) {
	devPropertiesContentState* state = findSingleDevPropertiesContentState(uiManager);
	if (state == nullptr)
	{
		return false;
	}
	state->selectedNode = selection;
	return true;
}
