#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Catalogue/DevCatalogueContentParameters.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevCatalogueSelector {
	using Parameters = DevCatalogueContentParameters;
	struct BrowserRow {
		std::string name{};
		uint64_t identity = 0;
		uint64_t asset_identity = 0, raster_identity = 0, raster_index = 0;
		bool group = false;
		bool raster = false;
	};
	struct State {
		std::vector<BrowserRow> rows{};
		std::vector<size_t> browser_assets{};
		DevCatalogueState* catalogue = nullptr;
		UiManager* ui_manager = nullptr;
		Clay_ElementId domains_id{}, browser_id{};
		uint64_t browser_selection = 0;
		float domain_scroll = 0;
		bool browser_expanded = false;
		bool restore_scroll = true;
	};
	using BuildContext = ElementBuildContext<DevCatalogueSelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.catalogue.selector");
	static constexpr std::string_view debugName = "Catalogue Selector";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

inline constexpr DevCatalogueSelector kDevCatalogueSelector{};
static_assert(DrawableFlowElement<DevCatalogueSelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
