#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Performance/DevPerformanceContentParameters.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevPerformanceWorkbench {
	using Parameters = DevPerformanceContentParameters;
	using State = DevTimelineState;
	using BuildContext = ElementBuildContext<DevPerformanceWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.performance.workbench");
	static constexpr std::string_view debugName = "Performance Workbench";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevPerformanceWorkbench kDevPerformanceWorkbench{};
static_assert(DrawableFlowElement<DevPerformanceWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
