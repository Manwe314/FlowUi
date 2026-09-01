#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevInspectInspector {
	using Parameters = DevInspectContentParameters;
	using BuildContext = ElementBuildContext<DevInspectInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.inspector");
	static constexpr std::string_view debugName = "Inspect Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevInspectInspector kDevInspectInspector{};
static_assert(DrawableFlowElement<DevInspectInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
