#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Changes/DevChangesContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevChangesSelector {
	using Parameters = DevChangesContentParameters;
	using BuildContext = ElementBuildContext<DevChangesSelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.changes.selector");
	static constexpr std::string_view debugName = "Changes Selector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevChangesSelector kDevChangesSelector{};
static_assert(DrawableFlowElement<DevChangesSelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
