#pragma once

#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevClayInspectorParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
	uint64_t selectionKey = 0;
};

/**
 * @brief Minimal Inspector panel view for a selected Clay layout element.
 *
 * Displays Clay element identification (ID, name/key, active flags) and the
 * debug identity of its owning Flow Element.
 */
struct DevClayInspector {
	using Parameters = DevClayInspectorParameters;
	using BuildContext = ElementBuildContext<DevClayInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.clay_inspector");
	static constexpr std::string_view debugName = "Clay Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevClayInspector kDevClayInspector{};
static_assert(DrawableFlowElement<DevClayInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
