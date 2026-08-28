#pragma once

#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevInterfaceFooterParameters {
	uint32_t errorCount = 0u;
	uint32_t unbakedChangeCount = 0u;
	std::string_view lastActionMessage = "Developer interface initialized";
};

struct DevInterfaceFooterResources {
	TextureRef errorIcon{};
	TextureRef unbakedChangesIcon{};

	DevInterfaceFooterResources() = default;
	explicit DevInterfaceFooterResources(App& app);
};

/** Closed, build-only owner of the permanent developer-interface footer. */
struct DevInterfaceFooter {
	using Parameters = DevInterfaceFooterParameters;
	using Resources = DevInterfaceFooterResources;
	using BuildContext = ElementBuildContext<DevInterfaceFooter>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.footer");
	static constexpr std::string_view debugName = "Developer Interface Footer";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevInterfaceFooter kDevInterfaceFooter{};
static_assert(DrawableFlowElement<DevInterfaceFooter>);

} // namespace FlowUi::devSystems::interface_elements

#endif
