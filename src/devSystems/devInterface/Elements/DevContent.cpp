#include "devSystems/devInterface/Elements/DevContent.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/DevTheme.hpp"

namespace FlowUi::devSystems::interface_elements {

Clay_ElementDeclaration DevContent::constructElement(BuildContext&) {
	Clay_ElementDeclaration content{};
	content.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	content.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	content.backgroundColor = interface_theme::kDepth0Keel;
	return content;
}

} // namespace FlowUi::devSystems::interface_elements

#endif
