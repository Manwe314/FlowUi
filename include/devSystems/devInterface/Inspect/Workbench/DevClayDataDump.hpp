#pragma once

#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevClayDataDumpParameters {
	DevInspectContentParameters inspect{};
	uint64_t selectionKey = 0;
};

/**
 * @brief Full scrollable struct dump for a selected Clay layout element.
 *
 * Displays all fields in Clay_ElementDeclaration, computed layout metrics,
 * and text configurations with explicit indication of defaults vs set values.
 */
struct DevClayDataDump {
	using Parameters = DevClayDataDumpParameters;
	using BuildContext = ElementBuildContext<DevClayDataDump>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.clay_data_dump");
	static constexpr std::string_view debugName = "Clay Data Dump";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevClayDataDump kDevClayDataDump{};
static_assert(DrawableFlowElement<DevClayDataDump>);

} // namespace FlowUi::devSystems::interface_elements

#endif
