#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevMemoryContentParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

#define FLOWUI_DECLARE_DEV_MEMORY_CONTENT_AREA(TypeName, Id, Label) \
	struct TypeName { \
		using Parameters = DevMemoryContentParameters; \
		using BuildContext = ElementBuildContext<TypeName>; \
		static constexpr FlowDefinitionID definitionId = DefinitionID(Id); \
		static constexpr std::string_view debugName = Label; \
		static constexpr bool isDevInternal = true; \
		static void buildElement(BuildContext& context); \
	}; \
	inline constexpr TypeName k##TypeName{}; \
	static_assert(DrawableFlowElement<TypeName>)

FLOWUI_DECLARE_DEV_MEMORY_CONTENT_AREA(
	DevMemorySelector, "flowui.dev_interface.memory.selector", "Memory Selector");
FLOWUI_DECLARE_DEV_MEMORY_CONTENT_AREA(
	DevMemoryWorkbench, "flowui.dev_interface.memory.workbench", "Memory Workbench");
FLOWUI_DECLARE_DEV_MEMORY_CONTENT_AREA(
	DevMemoryInspector, "flowui.dev_interface.memory.inspector", "Memory Inspector");

#undef FLOWUI_DECLARE_DEV_MEMORY_CONTENT_AREA

} // namespace FlowUi::devSystems::interface_elements

#endif
