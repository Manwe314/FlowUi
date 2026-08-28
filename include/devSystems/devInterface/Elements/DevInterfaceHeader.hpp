#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevInterfaceHeaderParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	WindowId interfaceWindowId = InvalidWindowId;
};

/** Closed, build-only owner of the permanent developer-interface header. */
struct DevInterfaceHeader {
	using Parameters = DevInterfaceHeaderParameters;
	using BuildContext = ElementBuildContext<DevInterfaceHeader>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.header");
	static constexpr std::string_view debugName = "Developer Interface Header";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevInterfaceHeader kDevInterfaceHeader{};
static_assert(DrawableFlowElement<DevInterfaceHeader>);

} // namespace FlowUi::devSystems::interface_elements

#endif
