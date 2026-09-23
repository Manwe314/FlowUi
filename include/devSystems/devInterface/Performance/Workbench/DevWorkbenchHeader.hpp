#pragma once
#include "devSystems/devInterface/Performance/Workbench/DevTimelineElements.hpp"
#if FLOW_UI_DEV_MODE
namespace FlowUi::devSystems::interface_elements {
/** Pinned transport, history scrubber and breadcrumb path. */
struct DevWorkbenchHeader {
	using Parameters = DevTimelineParameters;
	using BuildContext = ElementBuildContext<DevWorkbenchHeader>;
	using InteractionContext = ElementInteractionContext<DevWorkbenchHeader>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.performance.DevWorkbenchHeader");
	static constexpr std::string_view debugName = "DevWorkbenchHeader";
	static constexpr bool isDevInternal = true;
	static void runLogic(InteractionContext& context);
	static void buildElement(BuildContext& context);
};
inline constexpr DevWorkbenchHeader kDevWorkbenchHeader{};
static_assert(DrawableFlowElement<DevWorkbenchHeader>);
} // namespace FlowUi::devSystems::interface_elements
#endif
