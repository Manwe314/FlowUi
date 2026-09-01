#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Catalogue/DevCatalogueContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevCatalogueWorkbench {
	using Parameters = DevCatalogueContentParameters;
	using BuildContext = ElementBuildContext<DevCatalogueWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.catalogue.workbench");
	static constexpr std::string_view debugName = "Catalogue Workbench";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevCatalogueWorkbench kDevCatalogueWorkbench{};
static_assert(DrawableFlowElement<DevCatalogueWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
