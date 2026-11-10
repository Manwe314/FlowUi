#pragma once

#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Catalogue/DevCatalogueContentParameters.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevCatalogueInspector {
	using Parameters = DevCatalogueContentParameters;
	struct State {
		bool overview_expanded = true, registration_expanded = true;
		bool rendering_expanded = true, preview_expanded = true, scope_expanded = true;
	};
	using BuildContext = ElementBuildContext<DevCatalogueInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.catalogue.inspector");
	static constexpr std::string_view debugName = "Catalogue Inspector";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

inline constexpr DevCatalogueInspector kDevCatalogueInspector{};
static_assert(DrawableFlowElement<DevCatalogueInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
