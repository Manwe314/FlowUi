#pragma once

#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Diagnostics/DevDiagnosticsContentParameters.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsIcons.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsModel.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevDiagnosticsWorkbench {
	struct State {
		DevDiagnosticsState* diagnostics = nullptr;
		std::vector<DevErrorAdviceEvidenceRef> evidence_actions{};
		uint64_t browser_choice = 0, previous_browser_choice = 0;
		uint64_t timeline_page = 0, selected_occurrence = 0;
		uint64_t scroll_to_section = 0;
		bool history_open = false;
	};
	using Parameters = DevDiagnosticsContentParameters;
	using Resources = diagnostics_icons::Resources;
	using BuildContext = ElementBuildContext<DevDiagnosticsWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.diagnostics.workbench");
	static constexpr std::string_view debugName = "Diagnostics Workbench";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

inline constexpr DevDiagnosticsWorkbench kDevDiagnosticsWorkbench{};
static_assert(DrawableFlowElement<DevDiagnosticsWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
