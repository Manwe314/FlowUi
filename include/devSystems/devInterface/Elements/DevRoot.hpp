#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

/** Temporary root of the dedicated developer interface. */
struct DevRoot {
	using BuildContext = ElementBuildContext<DevRoot>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.root");
	static constexpr std::string_view debugName = "Developer Interface Root";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context) {
		Clay_ElementDeclaration root{};
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_CENTER,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		root.backgroundColor = Flow_Color("#111318ff");

		Clay_TextElementConfig text{};
		text.textColor = Flow_Color("#f4f4f5ff");
		text.fontId = 0;
		text.fontSize = 28;
		text.textAlignment = CLAY_TEXT_ALIGN_CENTER;

		CLAY(context.clayID(), root) {
			CLAY_TEXT(
				context.uiManager.toClayString("Under Construction"),
				CLAY_TEXT_CONFIG(text));
		}
	}
};

inline constexpr DevRoot kDevRoot{};
static_assert(DrawableFlowElement<DevRoot>);

} // namespace FlowUi::devSystems::interface_elements

#endif
