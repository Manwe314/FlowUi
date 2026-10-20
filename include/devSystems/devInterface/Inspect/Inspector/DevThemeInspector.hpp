#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"
namespace FlowUi::devSystems::interface_elements {
/** Theme inspector surface. */
struct DevThemeInspector {
	using Parameters = DevInspectContentParameters;
	struct State {
		std::string clone_name = "custom";
		App* app = nullptr;
		DevInterfaceState* interface_state = nullptr;
	};
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();
	using BuildContext = ElementBuildContext<DevThemeInspector>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.themes.inspector");
	static constexpr std::string_view debugName = "Theme Inspector";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};
inline constexpr DevThemeInspector kDevThemeInspector{};
} // namespace FlowUi::devSystems::interface_elements
#endif
