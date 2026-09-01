#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Diagnostics/DevDiagnosticsContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevDiagnosticsWorkbench {
	using Parameters = DevDiagnosticsContentParameters;
	using BuildContext = ElementBuildContext<DevDiagnosticsWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.diagnostics.workbench");
	static constexpr std::string_view debugName = "Diagnostics Workbench";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevDiagnosticsWorkbench kDevDiagnosticsWorkbench{};
static_assert(DrawableFlowElement<DevDiagnosticsWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
