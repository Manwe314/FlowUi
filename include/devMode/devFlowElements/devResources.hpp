#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devEnum1Input.hpp"
#include "devMode/devFlowElements/devHeader.hpp"
#include "devMode/devFlowElements/devHierarchyContent.hpp"

#if FLOW_UI_DEV_MODE
namespace FlowUi::devMode {

inline void initializeDevFlowElementResourcesFromApp(App& app) {
	if (!DevHeaderDef::resources.has_value()) {
		DevHeaderDef::resources.emplace();
	}
	if (!DevHierarchyContentDef::resources.has_value()) {
		DevHierarchyContentDef::resources.emplace();
	}
	if (!DevEnum1InputDef::resources.has_value()) {
		DevEnum1InputDef::resources.emplace();
	}

	devHeaderResources& headerResources = *DevHeaderDef::resources;
	devHierarchyContentResources& hierarchyResources = *DevHierarchyContentDef::resources;
	devEnum1InputResources& enum1Resources = *DevEnum1InputDef::resources;

#if FLOWUI_INCLUDE_SVG_MANAGER
	constexpr std::string_view kExportIconKey = "flowui/dev/header/export";
	constexpr std::string_view kDownArrowIconKey = "flowui/dev/hierarchy/arrow-down";
	constexpr std::string_view kRightArrowIconKey = "flowui/dev/hierarchy/arrow-right";
	constexpr std::string_view kEnum1DownArrowIconKey = "flowui/dev/enum1/arrow-down";
	constexpr std::string_view kEnum1UpArrowIconKey = "flowui/dev/enum1/arrow-up";

	(void)app.icons().registerSvg(kExportIconKey, ::kExport);
	(void)app.icons().registerSvg(kDownArrowIconKey, ::kDownArrow);
	(void)app.icons().registerSvg(kRightArrowIconKey, ::kRightArrow);
	(void)app.icons().registerSvg(kEnum1DownArrowIconKey, ::kDownArrow);
	(void)app.icons().registerSvg(kEnum1UpArrowIconKey, ::kUpArrow);

	headerResources.exportIcon = app.icons().textureRef(kExportIconKey);
	hierarchyResources.downArrowIcon = app.icons().textureRef(kDownArrowIconKey);
	hierarchyResources.rightArrowIcon = app.icons().textureRef(kRightArrowIconKey);
	enum1Resources.downArrowIcon = app.icons().textureRef(kEnum1DownArrowIconKey);
	enum1Resources.upArrowIcon = app.icons().textureRef(kEnum1UpArrowIconKey);
#endif

	headerResources.exportIconPrepared = true;
	hierarchyResources.disclosureIconsPrepared = true;
	enum1Resources.disclosureIconsPrepared = true;
}

} // namespace FlowUi::devMode
#endif
