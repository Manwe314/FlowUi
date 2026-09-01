#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Diagnostics/DevDiagnosticsContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevDiagnosticsInspector {
	using Parameters = DevDiagnosticsContentParameters;
	using BuildContext = ElementBuildContext<DevDiagnosticsInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.diagnostics.inspector");
	static constexpr std::string_view debugName = "Diagnostics Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevDiagnosticsInspector kDevDiagnosticsInspector{};
static_assert(DrawableFlowElement<DevDiagnosticsInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
