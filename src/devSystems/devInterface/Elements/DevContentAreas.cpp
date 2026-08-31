#include "devSystems/devInterface/Elements/DevContentAreas.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/DevTheme.hpp"
#include "devSystems/devInterface/Elements/DevCatalogueContent.hpp"
#include "devSystems/devInterface/Elements/DevChangesContent.hpp"
#include "devSystems/devInterface/Elements/DevDiagnosticsContent.hpp"
#include "devSystems/devInterface/Elements/DevInspectContent.hpp"
#include "devSystems/devInterface/Elements/DevMemoryContent.hpp"
#include "devSystems/devInterface/Elements/DevPerformanceContent.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

inline constexpr LocalElementName kSelectedContent{"selected-content"};

DevInterfaceTab activeTab(const DevInterfaceState* state) {
	if (!state || state->activeTab > static_cast<uint64_t>(DevInterfaceTab::Catalogue)) {
		return DevInterfaceTab::Inspect;
	}
	return static_cast<DevInterfaceTab>(state->activeTab);
}

Clay_ElementDeclaration areaDeclaration(
	Clay_SizingAxis width,
	Clay_Color background) {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {
		.width = width,
		.height = CLAY_SIZING_GROW(0),
	};
	declaration.backgroundColor = background;
	return declaration;
}

template <typename Context, typename Element, typename Parameters>
void drawSelected(Context& context, const Element& element, Parameters parameters) {
	context.uiManager.createElement(element, kSelectedContent)
		.setParameters(parameters)
		.draw();
}

template <typename Context>
void drawSelectedSelector(Context& context) {
	switch (activeTab(context.params.interfaceState)) {
	case DevInterfaceTab::Inspect:
		drawSelected(context, kDevInspectSelector, DevInspectContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Performance:
		drawSelected(context, kDevPerformanceSelector, DevPerformanceContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Memory:
		drawSelected(context, kDevMemorySelector, DevMemoryContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Diagnostics:
		drawSelected(context, kDevDiagnosticsSelector, DevDiagnosticsContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Changes:
		drawSelected(context, kDevChangesSelector, DevChangesContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Catalogue:
		drawSelected(context, kDevCatalogueSelector, DevCatalogueContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	}
}

template <typename Context>
void drawSelectedWorkbench(Context& context) {
	switch (activeTab(context.params.interfaceState)) {
	case DevInterfaceTab::Inspect:
		drawSelected(context, kDevInspectWorkbench, DevInspectContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Performance:
		drawSelected(context, kDevPerformanceWorkbench, DevPerformanceContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Memory:
		drawSelected(context, kDevMemoryWorkbench, DevMemoryContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Diagnostics:
		drawSelected(context, kDevDiagnosticsWorkbench, DevDiagnosticsContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Changes:
		drawSelected(context, kDevChangesWorkbench, DevChangesContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Catalogue:
		drawSelected(context, kDevCatalogueWorkbench, DevCatalogueContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	}
}

template <typename Context>
void drawSelectedInspector(Context& context) {
	switch (activeTab(context.params.interfaceState)) {
	case DevInterfaceTab::Inspect:
		drawSelected(context, kDevInspectInspector, DevInspectContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Performance:
		drawSelected(context, kDevPerformanceInspector, DevPerformanceContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Memory:
		drawSelected(context, kDevMemoryInspector, DevMemoryContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Diagnostics:
		drawSelected(context, kDevDiagnosticsInspector, DevDiagnosticsContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Changes:
		drawSelected(context, kDevChangesInspector, DevChangesContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	case DevInterfaceTab::Catalogue:
		drawSelected(context, kDevCatalogueInspector, DevCatalogueContentParameters{
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
		});
		break;
	}
}

} // namespace

void DevSelectorArea::buildElement(BuildContext& context) {
	const float width = context.params.interfaceState
		? context.params.interfaceState->selectorWidth : 280.0f;
	CLAY(context.clayID(), areaDeclaration(
		CLAY_SIZING_FIXED(width), interface_theme::kDepth1Panel)) {
		drawSelectedSelector(context);
	}
}

void DevWorkbenchArea::buildElement(BuildContext& context) {
	CLAY(context.clayID(), areaDeclaration(
		CLAY_SIZING_GROW(0), interface_theme::kDepth0Keel)) {
		drawSelectedWorkbench(context);
	}
}

void DevInspectorArea::buildElement(BuildContext& context) {
	const float width = context.params.interfaceState
		? context.params.interfaceState->inspectorWidth : 320.0f;
	CLAY(context.clayID(), areaDeclaration(
		CLAY_SIZING_FIXED(width), interface_theme::kDepth1Panel)) {
		drawSelectedInspector(context);
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
