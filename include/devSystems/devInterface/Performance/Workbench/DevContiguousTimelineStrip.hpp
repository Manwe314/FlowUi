#pragma once
#include "devSystems/devInterface/Performance/Workbench/DevTimelineControls.hpp"
#if FLOW_UI_DEV_MODE
namespace FlowUi::devSystems::interface_elements {
/** Shared CPU/GPU time transform with pan, zoom and depth controls. */
struct DevContiguousTimelineStrip {
	using Parameters = DevTimelineParameters;
	using BuildContext = ElementBuildContext<DevContiguousTimelineStrip>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.performance.DevContiguousTimelineStrip");
	static constexpr std::string_view debugName = "DevContiguousTimelineStrip";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};
inline constexpr DevContiguousTimelineStrip kDevContiguousTimelineStrip{};
static_assert(DrawableFlowElement<DevContiguousTimelineStrip>);
} // namespace FlowUi::devSystems::interface_elements
#endif
