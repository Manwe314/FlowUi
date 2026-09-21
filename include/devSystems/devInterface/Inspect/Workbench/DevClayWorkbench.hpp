#pragma once

#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevClayWorkbenchParameters {
	DevInspectContentParameters inspect{};
	uint64_t selectionKey = 0;
};

/**
 * @brief Root Workbench layout coordinator for a selected Clay node.
 *
 * Renders a two-row responsive layout:
 * - Row 1: Viewport preview (left) and full scrollable struct dump (right).
 * - Row 2: Full-width scrollable C++ code generator with copy button.
 */
struct DevClayWorkbench {
	using Parameters = DevClayWorkbenchParameters;
	using BuildContext = ElementBuildContext<DevClayWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.clay");
	static constexpr std::string_view debugName = "Clay Workbench";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevClayWorkbench kDevClayWorkbench{};
static_assert(DrawableFlowElement<DevClayWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
