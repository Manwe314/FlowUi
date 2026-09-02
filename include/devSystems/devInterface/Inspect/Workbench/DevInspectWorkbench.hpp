#pragma once

#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevInspectWorkbenchHeaderParameters {
	std::string_view instanceName{};
	bool constructed = false;
	bool drawn = false;
	uint32_t directClayNodeCount = 0u;
	uint32_t overrideCount = 0u;
};

struct DevInspectWorkbenchHeader {
	using Parameters = DevInspectWorkbenchHeaderParameters;
	using BuildContext = ElementBuildContext<DevInspectWorkbenchHeader>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.header");
	static constexpr std::string_view debugName = "Inspect Workbench Header";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevInspectWorkbenchSubtitleParameters {
	std::string_view definitionName{};
	std::string_view instancePath{};
};

struct DevInspectWorkbenchSubtitle {
	using Parameters = DevInspectWorkbenchSubtitleParameters;
	using BuildContext = ElementBuildContext<DevInspectWorkbenchSubtitle>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.subtitle");
	static constexpr std::string_view debugName = "Inspect Workbench Subtitle";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevInspectWorkbenchContent {
	using BuildContext = ElementBuildContext<DevInspectWorkbenchContent>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench.content");
	static constexpr std::string_view debugName = "Inspect Workbench Content";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

struct DevInspectWorkbench {
	using Parameters = DevInspectContentParameters;
	using BuildContext = ElementBuildContext<DevInspectWorkbench>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.inspect.workbench");
	static constexpr std::string_view debugName = "Inspect Workbench";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevInspectWorkbenchHeader kDevInspectWorkbenchHeader{};
inline constexpr DevInspectWorkbenchSubtitle kDevInspectWorkbenchSubtitle{};
inline constexpr DevInspectWorkbenchContent kDevInspectWorkbenchContent{};
inline constexpr DevInspectWorkbench kDevInspectWorkbench{};
static_assert(DrawableFlowElement<DevInspectWorkbenchHeader>);
static_assert(DrawableFlowElement<DevInspectWorkbenchSubtitle>);
static_assert(DrawableFlowElement<DevInspectWorkbenchContent>);
static_assert(DrawableFlowElement<DevInspectWorkbench>);

} // namespace FlowUi::devSystems::interface_elements

#endif
