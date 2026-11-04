#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Changes/DevChangesContentParameters.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsIcons.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevChangesWorkbench {
	/** Retained widget drafts never substitute for the serviced review model. */
	struct State {
		std::array<std::string, 5> decision_labels{};
		std::array<FSEL::ComboBoxOption, 5> decision_options{};
		std::array<uint32_t, 40> submitted_queue_ids{};
		std::vector<std::string> metadata_expanded{};
		std::string previous_selection{};
		UiManager* ui_manager = nullptr;
		DevChangesState* changes = nullptr;
		uint64_t decision_choice = 0, growth_intent = 0, runtime_intent = 0;
		uint64_t decision_highlight = UINT64_MAX;
		uint64_t raw_page = 0;
		uint64_t previous_view = UINT64_MAX;
		uint64_t previous_selection_revision = 0;
		std::size_t submitted_queue_count = 0;
		std::array<float, 3> view_scroll{};
		Clay_ElementId raw_scroll_id{};
		Clay_ElementId group_trigger_id{};
		Clay_ElementId queue_focus_target{};
		float raw_scroll_x = 0, raw_scroll_y = 0;
		bool reveal_pending = false, previous_pin = false;
		bool decision_open = false, decision_was_open = false;
		bool queue_navigation_handled = false;
		bool submitted_pinned = false;
	};
	using Parameters = DevChangesContentParameters;
	using Resources = diagnostics_icons::Resources;
	using BuildContext = ElementBuildContext<DevChangesWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.changes.workbench");
	static constexpr std::string_view debugName = "Changes Workbench";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

inline constexpr DevChangesWorkbench kDevChangesWorkbench{};
static_assert(DrawableFlowElement<DevChangesWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
