#pragma once

#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Catalogue/DevCatalogueContentParameters.hpp"
#include "devSystems/devInterface/Inspect/Inspector/TypeEditorElements/DevTypeEditorElements.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevFselThemeSpecimens.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevCatalogueWorkbench {
	using Parameters = DevCatalogueContentParameters;
	struct State {
		DevTypeEditorState tint_editor{};
		FselThemeSpecimenState specimens{};
		Clay_Color tint_color{169, 218, 209, 255};
		uint64_t last_domain = UINT64_MAX, last_selection = UINT64_MAX;
		float drag_mouse_x = 0, drag_mouse_y = 0;
		bool dragging = false;
	};
	using BuildContext = ElementBuildContext<DevCatalogueWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.catalogue.workbench");
	static constexpr std::string_view debugName = "Catalogue Workbench";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

inline constexpr DevCatalogueWorkbench kDevCatalogueWorkbench{};
static_assert(DrawableFlowElement<DevCatalogueWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
