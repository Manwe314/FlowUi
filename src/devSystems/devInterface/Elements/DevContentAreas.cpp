#include "devSystems/devInterface/Elements/DevContentAreas.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/DevTheme.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

enum class AreaKind {
	Selector,
	Workbench,
	Inspector,
};

DevInterfaceTab activeTab(const DevInterfaceState* state) {
	if (!state || state->activeTab > static_cast<uint64_t>(DevInterfaceTab::Catalogue)) {
		return DevInterfaceTab::Inspect;
	}
	return static_cast<DevInterfaceTab>(state->activeTab);
}

std::string_view areaStub(DevInterfaceTab tab, AreaKind area) {
	// This switch is the replacement boundary for real tab-specific area
	// elements. Each case will eventually dispatch three concrete surfaces.
	switch (tab) {
	case DevInterfaceTab::Inspect:
		switch (area) {
		case AreaKind::Selector: return "Inspect selector stub";
		case AreaKind::Workbench: return "Inspect workbench stub";
		case AreaKind::Inspector: return "Inspect inspector stub";
		}
		break;
	case DevInterfaceTab::Performance:
		switch (area) {
		case AreaKind::Selector: return "Performance selector stub";
		case AreaKind::Workbench: return "Performance workbench stub";
		case AreaKind::Inspector: return "Performance inspector stub";
		}
		break;
	case DevInterfaceTab::Memory:
		switch (area) {
		case AreaKind::Selector: return "Memory selector stub";
		case AreaKind::Workbench: return "Memory workbench stub";
		case AreaKind::Inspector: return "Memory inspector stub";
		}
		break;
	case DevInterfaceTab::Diagnostics:
		switch (area) {
		case AreaKind::Selector: return "Diagnostics selector stub";
		case AreaKind::Workbench: return "Diagnostics workbench stub";
		case AreaKind::Inspector: return "Diagnostics inspector stub";
		}
		break;
	case DevInterfaceTab::Changes:
		switch (area) {
		case AreaKind::Selector: return "Changes selector stub";
		case AreaKind::Workbench: return "Changes workbench stub";
		case AreaKind::Inspector: return "Changes inspector stub";
		}
		break;
	case DevInterfaceTab::Catalogue:
		switch (area) {
		case AreaKind::Selector: return "Catalogue selector stub";
		case AreaKind::Workbench: return "Catalogue workbench stub";
		case AreaKind::Inspector: return "Catalogue inspector stub";
		}
		break;
	}
	return "Content area stub";
}

template <typename Context>
void drawArea(
	Context& context,
	AreaKind area,
	Clay_SizingAxis width,
	Clay_Color background) {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {
		.width = width,
		.height = CLAY_SIZING_GROW(0),
	};
	declaration.layout.padding = Clay_Padding{14, 14, 14, 14};
	declaration.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_TOP,
	};
	declaration.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	declaration.backgroundColor = background;

	Clay_TextElementConfig text{};
	text.textColor = interface_theme::kTextMuted;
	text.fontId = 0;
	text.fontSize = 13;
	text.wrapMode = CLAY_TEXT_WRAP_NONE;
	text.textAlignment = CLAY_TEXT_ALIGN_LEFT;

	CLAY(context.clayID(), declaration) {
		CLAY_TEXT(
			context.uiManager.toClayString(
				areaStub(activeTab(context.params.interfaceState), area)),
			CLAY_TEXT_CONFIG(text));
	}
}

} // namespace

void DevSelectorArea::buildElement(BuildContext& context) {
	const float width = context.params.interfaceState
		? context.params.interfaceState->selectorWidth : 280.0f;
	drawArea(
		context, AreaKind::Selector, CLAY_SIZING_FIXED(width),
		interface_theme::kDepth1Panel);
}

void DevWorkbenchArea::buildElement(BuildContext& context) {
	drawArea(
		context, AreaKind::Workbench, CLAY_SIZING_GROW(0),
		interface_theme::kDepth0Keel);
}

void DevInspectorArea::buildElement(BuildContext& context) {
	const float width = context.params.interfaceState
		? context.params.interfaceState->inspectorWidth : 320.0f;
	drawArea(
		context, AreaKind::Inspector, CLAY_SIZING_FIXED(width),
		interface_theme::kDepth1Panel);
}

} // namespace FlowUi::devSystems::interface_elements

#endif
