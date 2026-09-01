#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Catalogue/DevCatalogueContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevCatalogueSelector {
	using Parameters = DevCatalogueContentParameters;
	using BuildContext = ElementBuildContext<DevCatalogueSelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.catalogue.selector");
	static constexpr std::string_view debugName = "Catalogue Selector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevCatalogueSelector kDevCatalogueSelector{};
static_assert(DrawableFlowElement<DevCatalogueSelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
