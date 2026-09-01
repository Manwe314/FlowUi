#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Changes/DevChangesContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevChangesInspector {
	using Parameters = DevChangesContentParameters;
	using BuildContext = ElementBuildContext<DevChangesInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.changes.inspector");
	static constexpr std::string_view debugName = "Changes Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevChangesInspector kDevChangesInspector{};
static_assert(DrawableFlowElement<DevChangesInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
