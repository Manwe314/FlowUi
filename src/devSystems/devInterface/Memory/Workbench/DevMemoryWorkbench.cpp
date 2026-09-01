#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbench.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {

void DevMemoryWorkbench::buildElement(BuildContext& context) {
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
		CLAY_TEXT(
			context.uiManager.toClayString("Memory workbench stub"),
			CLAY_TEXT_CONFIG(text));
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
