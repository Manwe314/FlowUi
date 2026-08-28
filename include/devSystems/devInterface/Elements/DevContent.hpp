#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

/** Constructible owner for the tab-specific developer-interface workspace. */
struct DevContent {
	using BuildContext = ElementBuildContext<DevContent>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.content");
	static constexpr std::string_view debugName = "Developer Interface Content";
	static constexpr bool isDevInternal = true;

	static Clay_ElementDeclaration constructElement(BuildContext& context);
};

inline constexpr DevContent kDevContent{};
static_assert(ConstructibleFlowElement<DevContent>);

} // namespace FlowUi::devSystems::interface_elements

#endif
