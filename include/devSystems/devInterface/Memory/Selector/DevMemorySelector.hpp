#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Memory/DevMemoryContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevMemorySelector {
	using Parameters = DevMemoryContentParameters;
	using BuildContext = ElementBuildContext<DevMemorySelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.memory.selector");
	static constexpr std::string_view debugName = "Memory Selector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevMemorySelector kDevMemorySelector{};
static_assert(DrawableFlowElement<DevMemorySelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
