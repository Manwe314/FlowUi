#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Changes/DevChangesContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"
namespace FlowUi::devSystems::interface_elements {
/** Open whole-plan Output review and commit the reviewed manifest with the sole Bake action. */
struct DevChangesControls {
	using Parameters = DevChangesContentParameters;
	using BuildContext = ElementBuildContext<DevChangesControls>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.changes.controls");
	static constexpr std::string_view debugName = "Changes Review Controls";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};
inline constexpr DevChangesControls kDevChangesControls{};
} // namespace FlowUi::devSystems::interface_elements
#endif
