#pragma once

#include "devSystems/devInterface/Diagnostics/DevDiagnosticsContentParameters.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsIcons.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

/* Local trigger identities and owned option labels for the contextual popovers. */
struct DevDiagnosticsControlsState {
	std::vector<std::string> code_labels{}, site_labels{};
	std::vector<FSEL::ComboBoxOption> code_options{}, site_options{};
	uint64_t trigger_code = 0, trigger_site = 0;
	bool compact_open = false;
};

/* Contextual capture, future-trigger, owned-freeze and diagnostics-export controls. */
struct DevDiagnosticsControls {
	using State = DevDiagnosticsControlsState;
	using Resources = diagnostics_icons::Resources;
	using Parameters = DevDiagnosticsContentParameters;
	using BuildContext = ElementBuildContext<DevDiagnosticsControls>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.diagnostics.controls");
	static constexpr std::string_view debugName = "Diagnostics Controls";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};

inline constexpr DevDiagnosticsControls kDevDiagnosticsControls{};
static_assert(DrawableFlowElement<DevDiagnosticsControls>);

} // namespace FlowUi::devSystems::interface_elements

#endif
