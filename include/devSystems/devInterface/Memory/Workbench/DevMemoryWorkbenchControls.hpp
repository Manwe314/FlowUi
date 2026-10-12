#pragma once
#include "FSEL/internal/SelectableSurfaceBehavior.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchData.hpp"
#if FLOW_UI_DEV_MODE
namespace FlowUi::devSystems::interface_elements {
/** Retained release-inside control for Memory transport and track focus. */
struct DevMemoryControl {
	struct Parameters {
		Clay_ElementDeclaration surface{};
		DevMemoryWorkbenchState* state = nullptr;
		std::string label{};
		uint32_t action = 0;
		uint64_t identity = 0;
		DevMemorySelection* selection = nullptr;
		bool whole_track = false, enabled = true;
	};
	using State = FSEL::detail::selectable_surface::State;
	using BuildContext = ElementBuildContext<DevMemoryControl>;
	using InteractionContext = ElementInteractionContext<DevMemoryControl>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.memory.workbench.control");
	static constexpr std::string_view debugName = "Memory Workbench Control";
	static constexpr bool isDevInternal = true;
	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void onHovered(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	/** Open a selectable track surface around caller-provided content. */
	[[nodiscard]] static Clay_ElementDeclaration constructElement(BuildContext& context);
	static void buildElement(BuildContext& context);
};
inline constexpr DevMemoryControl kDevMemoryControl{};
} // namespace FlowUi::devSystems::interface_elements
#endif
