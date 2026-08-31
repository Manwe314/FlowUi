#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevPerformanceContentParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

#define FLOWUI_DECLARE_DEV_PERFORMANCE_CONTENT_AREA(TypeName, Id, Label) \
	struct TypeName { \
		using Parameters = DevPerformanceContentParameters; \
		using BuildContext = ElementBuildContext<TypeName>; \
		static constexpr FlowDefinitionID definitionId = DefinitionID(Id); \
		static constexpr std::string_view debugName = Label; \
		static constexpr bool isDevInternal = true; \
		static void buildElement(BuildContext& context); \
	}; \
	inline constexpr TypeName k##TypeName{}; \
	static_assert(DrawableFlowElement<TypeName>)

FLOWUI_DECLARE_DEV_PERFORMANCE_CONTENT_AREA(
	DevPerformanceSelector, "flowui.dev_interface.performance.selector", "Performance Selector");
FLOWUI_DECLARE_DEV_PERFORMANCE_CONTENT_AREA(
	DevPerformanceWorkbench, "flowui.dev_interface.performance.workbench", "Performance Workbench");
FLOWUI_DECLARE_DEV_PERFORMANCE_CONTENT_AREA(
	DevPerformanceInspector, "flowui.dev_interface.performance.inspector", "Performance Inspector");

#undef FLOWUI_DECLARE_DEV_PERFORMANCE_CONTENT_AREA

} // namespace FlowUi::devSystems::interface_elements

#endif
