#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "FSEL/ComboBox.hpp"
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevInspectContentParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

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

#define FLOWUI_DECLARE_DEV_INSPECT_CONTENT_AREA(TypeName, Id, Label) \
	struct TypeName { \
		using Parameters = DevInspectContentParameters; \
		using BuildContext = ElementBuildContext<TypeName>; \
		static constexpr FlowDefinitionID definitionId = DefinitionID(Id); \
		static constexpr std::string_view debugName = Label; \
		static constexpr bool isDevInternal = true; \
		static void buildElement(BuildContext& context); \
	}; \
	inline constexpr TypeName k##TypeName{}; \
	static_assert(DrawableFlowElement<TypeName>)

FLOWUI_DECLARE_DEV_INSPECT_CONTENT_AREA(
	DevInspectWorkbench, "flowui.dev_interface.inspect.workbench", "Inspect Workbench");
FLOWUI_DECLARE_DEV_INSPECT_CONTENT_AREA(
	DevInspectInspector, "flowui.dev_interface.inspect.inspector", "Inspect Inspector");

#undef FLOWUI_DECLARE_DEV_INSPECT_CONTENT_AREA

} // namespace FlowUi::devSystems::interface_elements

#endif
