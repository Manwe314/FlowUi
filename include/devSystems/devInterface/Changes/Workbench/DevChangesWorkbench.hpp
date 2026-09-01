#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Changes/DevChangesContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevChangesWorkbench {
	using Parameters = DevChangesContentParameters;
	using BuildContext = ElementBuildContext<DevChangesWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.changes.workbench");
	static constexpr std::string_view debugName = "Changes Workbench";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevChangesWorkbench kDevChangesWorkbench{};
static_assert(DrawableFlowElement<DevChangesWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
