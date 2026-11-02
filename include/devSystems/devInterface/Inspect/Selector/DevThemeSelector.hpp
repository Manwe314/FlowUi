#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"
namespace FlowUi::devSystems::interface_elements {
/** Theme selector surface. */
struct DevThemeSelector {
	using Parameters = DevInspectContentParameters;
	struct State {
		uint64_t resolved_key = 0;
		uint64_t options_generation = ~uint64_t{0};
		std::vector<std::string> labels{};
		std::vector<FSEL::ComboBoxOption> options{};
	};
	using BuildContext = ElementBuildContext<DevThemeSelector>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.themes.selector");
	static constexpr std::string_view debugName = "Theme Selector";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();
	static void buildElement(BuildContext& context);
};
inline constexpr DevThemeSelector kDevThemeSelector{};
} // namespace FlowUi::devSystems::interface_elements
#endif
