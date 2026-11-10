#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Changes/DevChangesContentParameters.hpp"
#include "devSystems/devInterface/Changes/DevChangesViewportState.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsIcons.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevChangesInspector {
	/* Disclosure state is local to the selected context and never changes a manifest decision. */
	struct State {
		std::string selected_key{};
		std::vector<uint64_t> expanded_details{};
		changes_ui::ViewportState scroll_restore{};
		DevChangesState* changes = nullptr;
		bool evidence_details_open = false, live_menu_open = false, reset_menu_open = false;
		bool restore_apply_focus = false, restore_reset_focus = false;
	};
	using Parameters = DevChangesContentParameters;
	using Resources = diagnostics_icons::Resources;
	using BuildContext = ElementBuildContext<DevChangesInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.changes.inspector");
	static constexpr std::string_view debugName = "Changes Inspector";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

inline constexpr DevChangesInspector kDevChangesInspector{};
static_assert(DrawableFlowElement<DevChangesInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
