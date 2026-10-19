#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {
/** Registered theme hierarchy with variant and token-group selection. */
struct DevThemeSelector {
	using Parameters = DevInspectContentParameters;
	struct State {
		uint64_t resolved_key = 0;
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
/** Reflected theme fields and authored/baked/live changes. */
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
/** Live specimens, variant token comparison, and C++ patch export. */
struct DevThemeWorkbench {
	using Parameters = DevInspectContentParameters;
	struct State {
		std::string sample_text = "The quick brown fox";
		std::string code{};
		double sample_value = 0.42;
		bool sample_checked = true;
		bool sample_switch = false;
	};
	using BuildContext = ElementBuildContext<DevThemeWorkbench>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.themes.workbench");
	static constexpr std::string_view debugName = "Theme Workbench";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();
	static void buildElement(BuildContext& context);
};
inline constexpr DevThemeSelector kDevThemeSelector{};
inline constexpr DevThemeInspector kDevThemeInspector{};
inline constexpr DevThemeWorkbench kDevThemeWorkbench{};
} // namespace FlowUi::devSystems::interface_elements
#endif
