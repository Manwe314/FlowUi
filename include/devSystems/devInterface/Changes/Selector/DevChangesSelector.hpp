#pragma once

#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/internal/SelectableSurfaceBehavior.hpp"
#include "devSystems/devInterface/Changes/DevChangesContentParameters.hpp"
#include "devSystems/devInterface/Changes/DevChangesModel.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsIcons.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

/* Compact Changes tree row with independent selection and disclosure hit targets. */
struct DevChangesSelectorNode {
	struct Parameters {
		std::string label{}, trailing{}, tooltip{};
		DevChangesState* changes = nullptr;
		Clay_Color operation_color{};
		uint64_t identity = 0;
		uint16_t depth = 0;
		bool leaf = false, expanded = false, selected = false, focused = false, error = false;
	};
	struct State : FSEL::detail::selectable_surface::State {
		bool disclosure_armed = false;
	};
	using Resources = diagnostics_icons::Resources;
	using BuildContext = ElementBuildContext<DevChangesSelectorNode>;
	using InteractionContext = ElementInteractionContext<DevChangesSelectorNode>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.changes.selector.node");
	static constexpr std::string_view debugName = "Changes Tree Node";
	static constexpr bool isDevInternal = true;
	static constexpr float row_height = 25;

	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	static void onHovered(InteractionContext& context);
	static void buildElement(BuildContext& context);
};

inline constexpr DevChangesSelectorNode kDevChangesSelectorNode{};
static_assert(DrawableFlowElement<DevChangesSelectorNode>);

struct DevChangesSelector {
	using Parameters = DevChangesContentParameters;
	using BuildContext = ElementBuildContext<DevChangesSelector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.changes.selector");
	static constexpr std::string_view debugName = "Changes Selector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevChangesSelector kDevChangesSelector{};
static_assert(DrawableFlowElement<DevChangesSelector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
