#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Diagnostics/DevDiagnosticsContentParameters.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsIcons.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsModel.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevDiagnosticsSelector {
	struct State {
		DevDiagnosticsState* diagnostics = nullptr;
		uint64_t focused_identity = 0, focused_domain = 0;
		bool domain_focus = true, keyboard_active = false;
	};
	using Parameters = DevDiagnosticsContentParameters;
	using Resources = diagnostics_icons::Resources;
	using BuildContext = ElementBuildContext<DevDiagnosticsSelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.diagnostics.selector");
	static constexpr std::string_view debugName = "Diagnostics Selector";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

inline constexpr DevDiagnosticsSelector kDevDiagnosticsSelector{};
static_assert(DrawableFlowElement<DevDiagnosticsSelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
