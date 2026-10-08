#pragma once

#include "FSEL/ComboBox.hpp"
#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/internal/SelectableSurfaceBehavior.hpp"
#include "devSystems/devInterface/Performance/DevPerformanceContentParameters.hpp"
#include "devSystems/devInterface/Performance/Inspector/DevPerformanceAnalysis.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

/** Inspector navigation/clipboard control with owned payload and revision validation. */
struct DevPerformanceInspectorActionParameters {
	TimelineCommand command{};
	std::string label{}, clipboard_text{};
	DevTimelineState* timeline = nullptr;
	App* app = nullptr;
	size_t* expanded_row = nullptr;
	size_t row_identity = timeline_no_parent;
	bool enabled = true;
};
struct DevPerformanceInspectorAction {
	using Parameters = DevPerformanceInspectorActionParameters;
	using State = FSEL::detail::selectable_surface::State;
	using BuildContext = ElementBuildContext<DevPerformanceInspectorAction>;
	using InteractionContext = ElementInteractionContext<DevPerformanceInspectorAction>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.performance.inspector.action");
	static constexpr std::string_view debugName = "Performance Inspector Action";
	static constexpr bool isDevInternal = true;
	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void onHovered(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	static void buildElement(BuildContext& context);
};
inline constexpr DevPerformanceInspectorAction kDevPerformanceInspectorAction{};

struct DevPerformanceInspectorState {
	std::vector<std::string> window_labels{};
	std::vector<FSEL::ComboBoxOption> window_options{};
	PerformanceAnalysis analysis{};
	PerformanceRefreshGate refresh_gate{};
	std::vector<size_t> ranking_order{};
	uint64_t analysis_range = 600, analysis_source = 0, ranking = 0, domain = 0, role_filter = 0;
	uint64_t cached_range = UINT64_MAX, cached_source = UINT64_MAX;
	uint64_t cached_ranking = UINT64_MAX, cached_domain = UINT64_MAX, cached_role = UINT64_MAX;
	size_t expanded_zone = timeline_no_parent;
	uint64_t selected_revision = 0;
	size_t previous_selection = timeline_no_parent;
	float analysis_target_ms = 16.6f;
	bool capture_expanded = true, rolling_expanded = true, selected_expanded = true;
	bool capture_information = false, technical_details = false;
	bool all_zones = false, all_children = false, distribution_expanded = false;
};
struct DevPerformanceInspector {
	using State = DevPerformanceInspectorState;
	using Parameters = DevPerformanceContentParameters;
	using BuildContext = ElementBuildContext<DevPerformanceInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.performance.inspector");
	static constexpr std::string_view debugName = "Performance Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevPerformanceInspector kDevPerformanceInspector{};
static_assert(DrawableFlowElement<DevPerformanceInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
