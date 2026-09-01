#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Diagnostics/DevDiagnosticsContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevDiagnosticsSelector {
	using Parameters = DevDiagnosticsContentParameters;
	using BuildContext = ElementBuildContext<DevDiagnosticsSelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.diagnostics.selector");
	static constexpr std::string_view debugName = "Diagnostics Selector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevDiagnosticsSelector kDevDiagnosticsSelector{};
static_assert(DrawableFlowElement<DevDiagnosticsSelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
