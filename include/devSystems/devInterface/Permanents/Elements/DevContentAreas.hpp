#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevContentAreaParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

/** Closed tab-dependent content owner for the left selector region. */
struct DevSelectorArea {
	using Parameters = DevContentAreaParameters;
	using BuildContext = ElementBuildContext<DevSelectorArea>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.selector-area");
	static constexpr std::string_view debugName = "Developer Interface Selector Area";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

/** Closed tab-dependent content owner for the central workbench region. */
struct DevWorkbenchArea {
	using Parameters = DevContentAreaParameters;
	using BuildContext = ElementBuildContext<DevWorkbenchArea>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.workbench-area");
	static constexpr std::string_view debugName = "Developer Interface Workbench Area";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

/** Closed tab-dependent content owner for the right inspector region. */
struct DevInspectorArea {
	using Parameters = DevContentAreaParameters;
	using BuildContext = ElementBuildContext<DevInspectorArea>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspector-area");
	static constexpr std::string_view debugName = "Developer Interface Inspector Area";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevSelectorArea kDevSelectorArea{};
inline constexpr DevWorkbenchArea kDevWorkbenchArea{};
inline constexpr DevInspectorArea kDevInspectorArea{};

static_assert(DrawableFlowElement<DevSelectorArea>);
static_assert(DrawableFlowElement<DevWorkbenchArea>);
static_assert(DrawableFlowElement<DevInspectorArea>);

} // namespace FlowUi::devSystems::interface_elements

#endif
