#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Memory/DevMemoryContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevMemoryInspector {
	using Parameters = DevMemoryContentParameters;
	using BuildContext = ElementBuildContext<DevMemoryInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.memory.inspector");
	static constexpr std::string_view debugName = "Memory Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevMemoryInspector kDevMemoryInspector{};
static_assert(DrawableFlowElement<DevMemoryInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
