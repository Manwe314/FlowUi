#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevChangesContentParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

#define FLOWUI_DECLARE_DEV_CHANGES_CONTENT_AREA(TypeName, Id, Label) \
	struct TypeName { \
		using Parameters = DevChangesContentParameters; \
		using BuildContext = ElementBuildContext<TypeName>; \
		static constexpr FlowDefinitionID definitionId = DefinitionID(Id); \
		static constexpr std::string_view debugName = Label; \
		static constexpr bool isDevInternal = true; \
		static void buildElement(BuildContext& context); \
	}; \
	inline constexpr TypeName k##TypeName{}; \
	static_assert(DrawableFlowElement<TypeName>)

FLOWUI_DECLARE_DEV_CHANGES_CONTENT_AREA(
	DevChangesSelector, "flowui.dev_interface.changes.selector", "Changes Selector");
FLOWUI_DECLARE_DEV_CHANGES_CONTENT_AREA(
	DevChangesWorkbench, "flowui.dev_interface.changes.workbench", "Changes Workbench");
FLOWUI_DECLARE_DEV_CHANGES_CONTENT_AREA(
	DevChangesInspector, "flowui.dev_interface.changes.inspector", "Changes Inspector");

#undef FLOWUI_DECLARE_DEV_CHANGES_CONTENT_AREA

} // namespace FlowUi::devSystems::interface_elements

#endif
