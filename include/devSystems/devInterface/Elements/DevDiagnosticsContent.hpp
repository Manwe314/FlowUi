#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevDiagnosticsContentParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

#define FLOWUI_DECLARE_DEV_DIAGNOSTICS_CONTENT_AREA(TypeName, Id, Label) \
	struct TypeName { \
		using Parameters = DevDiagnosticsContentParameters; \
		using BuildContext = ElementBuildContext<TypeName>; \
		static constexpr FlowDefinitionID definitionId = DefinitionID(Id); \
		static constexpr std::string_view debugName = Label; \
		static constexpr bool isDevInternal = true; \
		static void buildElement(BuildContext& context); \
	}; \
	inline constexpr TypeName k##TypeName{}; \
	static_assert(DrawableFlowElement<TypeName>)

FLOWUI_DECLARE_DEV_DIAGNOSTICS_CONTENT_AREA(
	DevDiagnosticsSelector, "flowui.dev_interface.diagnostics.selector", "Diagnostics Selector");
FLOWUI_DECLARE_DEV_DIAGNOSTICS_CONTENT_AREA(
	DevDiagnosticsWorkbench, "flowui.dev_interface.diagnostics.workbench", "Diagnostics Workbench");
FLOWUI_DECLARE_DEV_DIAGNOSTICS_CONTENT_AREA(
	DevDiagnosticsInspector, "flowui.dev_interface.diagnostics.inspector", "Diagnostics Inspector");

#undef FLOWUI_DECLARE_DEV_DIAGNOSTICS_CONTENT_AREA

} // namespace FlowUi::devSystems::interface_elements

#endif
