#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/internal/SelectableSurfaceBehavior.hpp"
#include "devSystems/devInterface/Performance/DevPerformanceSelection.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

/** A selectable tree row or independently toggleable category surface. */
struct DevPerformanceRowParameters {
	std::string_view label{};
	uint64_t* selection = nullptr;
	DevPerformanceScope* scope_selection = nullptr;
	bool* expanded = nullptr;
	uint32_t* category_mask = nullptr;
	uint64_t value = 0;
	uint32_t category_bit = 0;
	DevPerformanceScopeKind scope_kind = DevPerformanceScopeKind::Window;
	bool child = false;
};

struct DevPerformanceRowResources {
	TextureRef expanded_icon{};
	TextureRef collapsed_icon{};
	explicit DevPerformanceRowResources(App& app);
};

struct DevPerformanceRow {
	using Parameters = DevPerformanceRowParameters;
	using State = FSEL::detail::selectable_surface::State;
	using Resources = DevPerformanceRowResources;
	using BuildContext = ElementBuildContext<DevPerformanceRow>;
	using InteractionContext = ElementInteractionContext<DevPerformanceRow>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.performance.row");
	static constexpr std::string_view debugName = "Performance Selection Row";
	static constexpr bool isDevInternal = true;
	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	static void onHovered(InteractionContext& context);
	static void buildElement(BuildContext& context);
};
inline constexpr DevPerformanceRow kDevPerformanceRow{};
static_assert(DrawableFlowElement<DevPerformanceRow>);
} // namespace FlowUi::devSystems::interface_elements
#endif
