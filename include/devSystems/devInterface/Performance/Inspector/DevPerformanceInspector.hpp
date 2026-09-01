#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Performance/DevPerformanceContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevPerformanceInspector {
	using Parameters = DevPerformanceContentParameters;
	using BuildContext = ElementBuildContext<DevPerformanceInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.performance.inspector");
	static constexpr std::string_view debugName = "Performance Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevPerformanceInspector kDevPerformanceInspector{};
static_assert(DrawableFlowElement<DevPerformanceInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
