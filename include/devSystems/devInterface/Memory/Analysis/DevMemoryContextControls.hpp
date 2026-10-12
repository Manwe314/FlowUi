#pragma once
#include "devSystems/devInterface/Memory/DevMemoryContentParameters.hpp"
#if FLOW_UI_DEV_MODE
#include "managers/FlowUiElementBuilder.hpp"
namespace FlowUi::devSystems::interface_elements {
/** Dataset selection and recording controls shared by the complete Memory surface. */
struct DevMemoryContextControls {
	using Parameters = DevMemoryContentParameters;
	using BuildContext = ElementBuildContext<DevMemoryContextControls>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev.memory.context-controls");
	static constexpr std::string_view debugName = "Memory Dataset Controls";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};
inline constexpr DevMemoryContextControls kDevMemoryContextControls{};
} // namespace FlowUi::devSystems::interface_elements
#endif
