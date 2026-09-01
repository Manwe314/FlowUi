#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Catalogue/DevCatalogueContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevCatalogueInspector {
	using Parameters = DevCatalogueContentParameters;
	using BuildContext = ElementBuildContext<DevCatalogueInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.catalogue.inspector");
	static constexpr std::string_view debugName = "Catalogue Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevCatalogueInspector kDevCatalogueInspector{};
static_assert(DrawableFlowElement<DevCatalogueInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
