#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevInspectSelectorState {
	uint64_t definitionSchemaGeneration = UINT64_MAX;
	std::vector<std::string> definitionLabels{};
	std::vector<FSEL::ComboBoxOption> definitionOptions{};
};

struct DevInspectSelector {
	using Parameters = DevInspectContentParameters;
	using State = DevInspectSelectorState;
	using BuildContext = ElementBuildContext<DevInspectSelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.selector");
	static constexpr std::string_view debugName = "Inspect Selector";
	static constexpr bool isDevInternal = true;
	static constexpr ElementStatePolicy statePolicy =
		ElementStatePolicy::windowLifetime();

	static void buildElement(BuildContext& context);
};

inline constexpr DevInspectSelector kDevInspectSelector{};
static_assert(DrawableFlowElement<DevInspectSelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
