#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/Memory/DevMemoryContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"
#include <string>
#include <vector>

namespace FlowUi::devSystems::interface_elements {

/** Owned option labels keep ComboBox borrowed views valid across popup interactions. */
struct DevMemorySelectorState {
	std::vector<DevWindowInfo> windows{};
	std::vector<std::string> window_labels{};
	std::vector<FSEL::ComboBoxOption> window_options{};
};

struct DevMemorySelector {
	using Parameters = DevMemoryContentParameters;
	using State = DevMemorySelectorState;
	using BuildContext = ElementBuildContext<DevMemorySelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.memory.selector");
	static constexpr std::string_view debugName = "Memory Selector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevMemorySelector kDevMemorySelector{};
static_assert(DrawableFlowElement<DevMemorySelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
