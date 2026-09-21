#pragma once

#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevClayCodeGeneratorState {
	bool copiedRecently = false;
};

struct DevClayCodeGeneratorParameters {
	DevInspectContentParameters inspect{};
	uint64_t selectionKey = 0;
};

/**
 * @brief C++ code generator element for the selected Clay layout element.
 *
 * Emits a copyable C++ declaration with a header containing 'Element Code' and
 * an interactive 'Copy' button.
 */
struct DevClayCodeGenerator {
	using Parameters = DevClayCodeGeneratorParameters;
	using State = DevClayCodeGeneratorState;
	using BuildContext = ElementBuildContext<DevClayCodeGenerator>;
	using InteractionContext = ElementInteractionContext<DevClayCodeGenerator>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.clay_code_gen");
	static constexpr std::string_view debugName = "Clay Code Generator";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy = ElementStatePolicy::windowLifetime();

	static void onPressed(InteractionContext& context);
	static void buildElement(BuildContext& context);
};

inline constexpr DevClayCodeGenerator kDevClayCodeGenerator{};
static_assert(DrawableFlowElement<DevClayCodeGenerator>);

} // namespace FlowUi::devSystems::interface_elements

#endif
