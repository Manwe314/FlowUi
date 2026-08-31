#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevCatalogueContentParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

#define FLOWUI_DECLARE_DEV_CATALOGUE_CONTENT_AREA(TypeName, Id, Label) \
	struct TypeName { \
		using Parameters = DevCatalogueContentParameters; \
		using BuildContext = ElementBuildContext<TypeName>; \
		static constexpr FlowDefinitionID definitionId = DefinitionID(Id); \
		static constexpr std::string_view debugName = Label; \
		static constexpr bool isDevInternal = true; \
		static void buildElement(BuildContext& context); \
	}; \
	inline constexpr TypeName k##TypeName{}; \
	static_assert(DrawableFlowElement<TypeName>)

FLOWUI_DECLARE_DEV_CATALOGUE_CONTENT_AREA(
	DevCatalogueSelector, "flowui.dev_interface.catalogue.selector", "Catalogue Selector");
FLOWUI_DECLARE_DEV_CATALOGUE_CONTENT_AREA(
	DevCatalogueWorkbench, "flowui.dev_interface.catalogue.workbench", "Catalogue Workbench");
FLOWUI_DECLARE_DEV_CATALOGUE_CONTENT_AREA(
	DevCatalogueInspector, "flowui.dev_interface.catalogue.inspector", "Catalogue Inspector");

#undef FLOWUI_DECLARE_DEV_CATALOGUE_CONTENT_AREA

} // namespace FlowUi::devSystems::interface_elements

#endif
