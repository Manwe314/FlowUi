#include "devSystems/devInterface/Elements/DevChangesContent.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/DevTheme.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

template <typename Context>
void drawStub(Context& context, std::string_view label) {
	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	root.layout.padding = Clay_Padding{14, 14, 14, 14};

	Clay_TextElementConfig text{};
	text.textColor = interface_theme::kTextMuted;
	text.fontSize = 13;
	text.wrapMode = CLAY_TEXT_WRAP_NONE;
	text.textAlignment = CLAY_TEXT_ALIGN_LEFT;

	CLAY(context.clayID(), root) {
		CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(text));
	}
}

} // namespace

void DevChangesSelector::buildElement(BuildContext& context) {
	drawStub(context, "Changes selector stub");
}

void DevChangesWorkbench::buildElement(BuildContext& context) {
	drawStub(context, "Changes workbench stub");
}

void DevChangesInspector::buildElement(BuildContext& context) {
	drawStub(context, "Changes inspector stub");
}

} // namespace FlowUi::devSystems::interface_elements

#endif
