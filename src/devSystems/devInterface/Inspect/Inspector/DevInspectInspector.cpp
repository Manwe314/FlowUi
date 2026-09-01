#include "devSystems/devInterface/Inspect/Inspector/DevInspectInspector.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cstdio>

#include "devSystems/devInterface/Inspect/Inspector/TypeEditorElements/DevTypeEditorElements.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {

void DevInspectInspector::buildElement(BuildContext& context) {
	DevInterfaceState* state = context.params.interfaceState;
	const tooling::DevTreeSnapshot* snapshot = nullptr;
	const tooling::DevFlowNode* selected = nullptr;
	if (state && state->selectedElementId && context.params.app &&
		context.params.app->hasWindow(state->selectedWindowId)) {
		snapshot = &context.params.app->ui(state->selectedWindowId).devTreeSnapshot();
		const auto found = std::ranges::find_if(
			snapshot->flow.nodes,
			[state](const tooling::DevFlowNode& node) {
				return node.instance.value == state->selectedElementId.value;
			});
		if (found != snapshot->flow.nodes.end()) selected = &*found;
	}
	char instanceFallback[24]{};
	char definitionFallback[24]{};
	std::string_view instanceName{};
	std::string_view definitionName{};
	if (selected && snapshot) {
		instanceName = snapshot->string(selected->debugName);
		definitionName = snapshot->string(selected->definitionName);
		if (instanceName.empty()) {
			std::snprintf(
				instanceFallback, sizeof(instanceFallback), "0x%016llX",
				static_cast<unsigned long long>(selected->instance.value));
			instanceName = instanceFallback;
		}
		if (definitionName.empty()) {
			std::snprintf(
				definitionFallback, sizeof(definitionFallback), "0x%016llX",
				static_cast<unsigned long long>(selected->definition.value));
			definitionName = definitionFallback;
		}
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;

	CLAY(context.clayID(), root) {
		context.uiManager.createElement(kDevInterfaceInspectorTitle, "title")
			.setDevInternalCapture(true)
			.draw();

		if (!selected || !snapshot) {
			Clay_ElementDeclaration empty{};
			empty.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			empty.layout.padding = Clay_Padding{18, 18, 18, 18};
			empty.layout.childAlignment = {
				.x = CLAY_ALIGN_X_CENTER,
				.y = CLAY_ALIGN_Y_CENTER,
			};
			Clay_TextElementConfig text{};
			text.textColor = interface_theme::kTextMuted;
			text.fontSize = 12;
			text.wrapMode = CLAY_TEXT_WRAP_WORDS;
			text.textAlignment = CLAY_TEXT_ALIGN_CENTER;
			CLAY(context.clayID("empty-selection"), empty) {
				CLAY_TEXT(
					context.uiManager.toClayString("Select an Element From the Left Selector"),
					CLAY_TEXT_CONFIG(text));
			}
		} else {
			context.uiManager.createElement(kDevInterfaceInspectorIdentity, "identity")
				.setParameters(DevInterfaceInspectorIdentityParameters{
					.instanceName = instanceName,
					.definitionName = definitionName,
				})
				.setDevInternalCapture(true)
				.draw();
			context.uiManager.createElement(kDevInterfaceInspectorTabs, "tabs")
				.setParameters(DevInterfaceInspectorTabsParameters{
					.selectedTab = state ? &state->inspectInspectorTab : nullptr,
				})
				.setDevInternalCapture(true)
				.draw();
			context.uiManager.createElement(kDevInterfaceInspectorFields, "fields")
				.setParameters(DevInterfaceInspectorFieldsParameters{
					.app = context.params.app,
					.interfaceState = state,
					.definition = selected->definition,
					.instance = selected->instance,
					.elementName = instanceName,
				})
				.setDevInternalCapture(true)
				.draw();
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
