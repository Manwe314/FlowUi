#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"
namespace FlowUi::devSystems::interface_elements {
/** Theme workbench surface. */
struct DevThemeWorkbench {
	using Parameters = DevInspectContentParameters;
	struct State {
		std::string sample_text = "The quick brown fox";
		std::string code{};
		std::string token_search{};
		std::string copy_feedback{};
		std::vector<FSEL::ComboBoxOption> preview_options{};
		uint64_t preview_layout = 0;
		uint64_t preview_theme_type = 0;
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
inline constexpr DevThemeWorkbench kDevThemeWorkbench{};
} // namespace FlowUi::devSystems::interface_elements
#endif
