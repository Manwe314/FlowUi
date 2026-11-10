#pragma once

#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Memory/DevMemoryContentParameters.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

/* Persistent presentation state for the Memory inspector sections. */
struct DevMemoryInspectorState {
	bool capacity_tuning_expanded = true, capacity_scope_settings_expanded = true;
};

struct DevMemoryInspector {
	using State = DevMemoryInspectorState;
	using Parameters = DevMemoryContentParameters;
	using BuildContext = ElementBuildContext<DevMemoryInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.memory.inspector");
	static constexpr std::string_view debugName = "Memory Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevMemoryInspector kDevMemoryInspector{};
static_assert(DrawableFlowElement<DevMemoryInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
