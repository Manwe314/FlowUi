#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevContentHeaderParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

/** Closed owner of navigation tabs and their contextual control strip. */
struct DevContentHeader {
	using Parameters = DevContentHeaderParameters;
	using BuildContext = ElementBuildContext<DevContentHeader>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.content-header");
	static constexpr std::string_view debugName = "Developer Interface Content Header";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevContentHeader kDevContentHeader{};
static_assert(DrawableFlowElement<DevContentHeader>);

} // namespace FlowUi::devSystems::interface_elements

#endif
