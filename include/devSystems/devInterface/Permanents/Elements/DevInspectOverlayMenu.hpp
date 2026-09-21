#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {
struct DevInspectOverlayMenuParameters {
	App* app = nullptr;
};
struct DevInspectOverlayMenuState {
	uint8_t focused_row = 0u;
	bool open = false;
	bool keyboard_focus = false;
};

/** Shared vector artwork for the dropdown indicator and checked surfaces. */
struct DevInspectOverlayMenuResources {
	TextureRef checked_icon{};
	TextureRef dropdown_icon{};
	DevInspectOverlayMenuResources() = default;
	explicit DevInspectOverlayMenuResources(App& app);
};

/** Inspect-only contextual controls and their custom overlay configuration popup. */
struct DevInspectOverlayMenu {
	using Parameters = DevInspectOverlayMenuParameters;
	using State = DevInspectOverlayMenuState;
	using Resources = DevInspectOverlayMenuResources;
	using BuildContext = ElementBuildContext<DevInspectOverlayMenu>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.overlay-menu");
	static constexpr std::string_view debugName = "Inspect Overlay Menu";
	static constexpr bool isDevInternal = true;
	static void buildElement(BuildContext& context);
};
inline constexpr DevInspectOverlayMenu kDevInspectOverlayMenu{};
static_assert(DrawableFlowElement<DevInspectOverlayMenu>);
} // namespace FlowUi::devSystems::interface_elements
#endif
