#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

/** App-owned dependencies and identities made available to the interface root. */
struct DevInterfaceParameters {
	App* app = nullptr;
	WindowId interfaceWindowId = InvalidWindowId;
	WindowId mainWindowId = MainWindowId;
};

/** Singleton drawable root for the dedicated developer-interface window. */
struct DevInterface {
	using Parameters = DevInterfaceParameters;
	using State = DevInterfaceState;
	using BuildContext = ElementBuildContext<DevInterface>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.root");
	static constexpr std::string_view debugName = "Developer Interface";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy =
		ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);

private:
	static void buildPermanentHeader(BuildContext& context);
	static void buildContentHeader(BuildContext& context);
	static void buildContent(BuildContext& context);
	static void buildPermanentFooter(BuildContext& context);
};

inline constexpr DevInterface kDevInterface{};
static_assert(DrawableFlowElement<DevInterface>);

} // namespace FlowUi::devSystems::interface_elements

#endif
