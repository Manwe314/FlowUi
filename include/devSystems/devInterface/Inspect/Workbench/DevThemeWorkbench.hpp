#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
namespace FlowUi::devSystems::interface_elements {
/* Theme workbench surface. */
struct DevThemeWorkbench {
	using Parameters = DevInspectContentParameters;
	struct State {
		std::string sample_text = "The quick brown fox";
		std::string code{};
		std::string token_search{};
		std::string copy_feedback{};
		std::vector<FSEL::ComboBoxOption> preview_options{};
		struct FilteredChange {
			std::size_t variant_index = 0, leaf_index = 0;
		};
		std::vector<std::size_t> token_indices{};
		std::vector<FilteredChange> changed_fields{};
		std::string filter_query{}, filter_token_query{}, filter_variant{};
		uint64_t filter_generation = 0, filter_type = 0;
		tooling::DevFieldId filter_group = 0;
		bool filter_all_variants = false;
		std::size_t live_count = 0, startup_count = 0;
		uint64_t code_generation = 0, code_type = 0;
		std::string code_variant{};
		bool code_all_variants = false;
		std::size_t assignment_count = 0, omitted_count = 0;
		std::size_t emitted_token_rows = 0;
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
