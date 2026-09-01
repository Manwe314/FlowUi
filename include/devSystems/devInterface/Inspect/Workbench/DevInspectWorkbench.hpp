#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevInspectWorkbench {
	using Parameters = DevInspectContentParameters;
	using BuildContext = ElementBuildContext<DevInspectWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench");
	static constexpr std::string_view debugName = "Inspect Workbench";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevInspectWorkbench kDevInspectWorkbench{};
static_assert(DrawableFlowElement<DevInspectWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
