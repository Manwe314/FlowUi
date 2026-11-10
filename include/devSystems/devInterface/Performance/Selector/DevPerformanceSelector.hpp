#pragma once

#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Performance/DevPerformanceContentParameters.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevPerformanceSelector {
	using Parameters = DevPerformanceContentParameters;
	using BuildContext = ElementBuildContext<DevPerformanceSelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.performance.selector");
	static constexpr std::string_view debugName = "Performance Selector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevPerformanceSelector kDevPerformanceSelector{};
static_assert(DrawableFlowElement<DevPerformanceSelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
