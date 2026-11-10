#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
#include <string>
#include <string_view>
namespace FlowUi::devSystems::interface_elements {
struct DevSelectorSearchParameters {
	std::string* query = nullptr;
	std::string_view placeholder = "Filter nodes...";
	/* Center text and caret in this search field without changing other selector defaults. */
	bool center_text = false;
	/* Fit a caller-owned horizontal filter section without adding a nested section border. */
	bool inline_control = false;
};

struct DevSelectorSearch {
	using Parameters = DevSelectorSearchParameters;
	using BuildContext = ElementBuildContext<DevSelectorSearch>;

	// Preserve the original identity when sharing the Inspect search element.
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.selector.search");
	static constexpr std::string_view debugName = "Selector Search";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevSelectorSearch kDevSelectorSearch{};
} // namespace FlowUi::devSystems::interface_elements
#endif
