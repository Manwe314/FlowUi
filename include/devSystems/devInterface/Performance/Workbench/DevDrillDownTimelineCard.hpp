#pragma once
#include "devSystems/devInterface/Performance/Workbench/DevTimelineElements.hpp"
#if FLOW_UI_DEV_MODE
namespace FlowUi::devSystems::interface_elements {
/** Fixed-domain parent and child hierarchy, with independent depth. */
struct DevDrillDownTimelineCard {
	using Parameters = DevTimelineParameters;
	using BuildContext = ElementBuildContext<DevDrillDownTimelineCard>;
	using InteractionContext = ElementInteractionContext<DevDrillDownTimelineCard>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.performance.DevDrillDownTimelineCard");
	static constexpr std::string_view debugName = "DevDrillDownTimelineCard";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};
inline constexpr DevDrillDownTimelineCard kDevDrillDownTimelineCard{};
static_assert(DrawableFlowElement<DevDrillDownTimelineCard>);
} // namespace FlowUi::devSystems::interface_elements
#endif
