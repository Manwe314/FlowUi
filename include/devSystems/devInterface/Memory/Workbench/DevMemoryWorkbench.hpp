#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Memory/DevMemoryContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevMemoryWorkbench {
	using Parameters = DevMemoryContentParameters;
	using BuildContext = ElementBuildContext<DevMemoryWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.memory.workbench");
	static constexpr std::string_view debugName = "Memory Workbench";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevMemoryWorkbench kDevMemoryWorkbench{};
static_assert(DrawableFlowElement<DevMemoryWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
