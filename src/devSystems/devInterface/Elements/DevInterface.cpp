#include "devSystems/devInterface/Elements/DevInterface.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <utility>

#include "FSEL/SplitterHandle.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "devSystems/devInterface/Elements/DevContent.hpp"
#include "devSystems/devInterface/Elements/DevContentAreas.hpp"
#include "devSystems/devInterface/Elements/DevContentHeader.hpp"
#include "devSystems/devInterface/Elements/DevInterfaceFooter.hpp"
#include "devSystems/devInterface/Elements/DevInterfaceHeader.hpp"
#include "devSystems/devInterface/DevTheme.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

inline constexpr LocalElementName kSelectorArea{"selector-area"};
inline constexpr LocalElementName kSelectorSplitter{"selector-splitter"};
inline constexpr LocalElementName kWorkbenchArea{"workbench-area"};
inline constexpr LocalElementName kInspectorSplitter{"inspector-splitter"};
inline constexpr LocalElementName kInspectorArea{"inspector-area"};

constexpr float kSelectorMinimumWidth = 180.0f;
constexpr float kSelectorMaximumWidth = 520.0f;
constexpr float kInspectorMinimumWidth = 240.0f;
constexpr float kInspectorMaximumWidth = 560.0f;

} // namespace

void DevInterface::buildElement(BuildContext& context) {
	FLOWUI_DEV_TIMING_ZONE_IF(
		context.devTimingRecorder(), TimingCategory::DevTool,
		TimingZoneRole::DevToolWork, "flowui.dev_interface.build");

	State& state = context.state();
	if (state.selectedWindowId == InvalidWindowId) {
		state.selectedWindowId = context.params.mainWindowId;
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth0Keel;

	CLAY(context.clayID(), root) {
		buildPermanentHeader(context);
		buildContentHeader(context);
		buildContent(context);
		buildPermanentFooter(context);
	}
}

void DevInterface::buildPermanentHeader(BuildContext& context) {
	context.uiManager.createElement(
		kDevInterfaceHeader, LocalElementName{"permanent-header"})
		.setParameters(DevInterfaceHeaderParameters{
			.app = context.params.app,
			.interfaceState = &context.state(),
			.interfaceWindowId = context.params.interfaceWindowId,
		})
		.draw();
}

void DevInterface::buildContentHeader(BuildContext& context) {
	context.uiManager.createElement(
		kDevContentHeader, LocalElementName{"content-header"})
		.setParameters(DevContentHeaderParameters{
			.app = context.params.app,
			.interfaceState = &context.state(),
		})
		.draw();
}

void DevInterface::buildContent(BuildContext& context) {
	State& state = context.state();
	state.selectorWidth = std::clamp(
		state.selectorWidth, kSelectorMinimumWidth, kSelectorMaximumWidth);
	state.inspectorWidth = std::clamp(
		state.inspectorWidth, kInspectorMinimumWidth, kInspectorMaximumWidth);

	context.uiManager.createElement(kDevContent, LocalElementName{"content"})
		.construct();
	{
		context.uiManager.createElement(kDevSelectorArea, kSelectorArea)
			.setParameters(DevContentAreaParameters{.interfaceState = &state})
			.draw();

		FSEL::SplitterHandleParameters selectorSplitter{};
		selectorSplitter.axis = FSEL::SplitterAxis::Horizontal;
		selectorSplitter.position = FSEL::SplitterPosition::Trailing;
		selectorSplitter.targetExtent = &state.selectorWidth;
		selectorSplitter.minExtent = kSelectorMinimumWidth;
		selectorSplitter.maxExtent = kSelectorMaximumWidth;
		selectorSplitter.thickness = 1.0f;
		selectorSplitter.hitThickness = 10.0f;
		selectorSplitter.backgroundColor = interface_theme::kBorderPrimary;
		selectorSplitter.hoverColor = interface_theme::kAccentCurrent;
		selectorSplitter.draggingColor = interface_theme::kSelectedRow;
		context.uiManager.createElement(FSEL::kSplitterHandle, kSelectorSplitter)
			.setParameters(std::move(selectorSplitter))
			.setDevInternalCapture(true)
			.draw();

		context.uiManager.createElement(kDevWorkbenchArea, kWorkbenchArea)
			.setParameters(DevContentAreaParameters{.interfaceState = &state})
			.draw();

		FSEL::SplitterHandleParameters inspectorSplitter{};
		inspectorSplitter.axis = FSEL::SplitterAxis::Horizontal;
		inspectorSplitter.position = FSEL::SplitterPosition::Leading;
		inspectorSplitter.targetExtent = &state.inspectorWidth;
		inspectorSplitter.minExtent = kInspectorMinimumWidth;
		inspectorSplitter.maxExtent = kInspectorMaximumWidth;
		inspectorSplitter.thickness = 1.0f;
		inspectorSplitter.hitThickness = 10.0f;
		inspectorSplitter.backgroundColor = interface_theme::kBorderPrimary;
		inspectorSplitter.hoverColor = interface_theme::kAccentCurrent;
		inspectorSplitter.draggingColor = interface_theme::kSelectedRow;
		context.uiManager.createElement(FSEL::kSplitterHandle, kInspectorSplitter)
			.setParameters(std::move(inspectorSplitter))
			.setDevInternalCapture(true)
			.draw();

		context.uiManager.createElement(kDevInspectorArea, kInspectorArea)
			.setParameters(DevContentAreaParameters{.interfaceState = &state})
			.draw();
	}
	context.uiManager.drawConstructed();
}

void DevInterface::buildPermanentFooter(BuildContext& context) {
	const State& state = context.state();
	context.uiManager.createElement(
		kDevInterfaceFooter, LocalElementName{"permanent-footer"})
		.setParameters(DevInterfaceFooterParameters{
			.errorCount = state.activeErrorCount,
			.unbakedChangeCount = state.unbakedChangeCount,
			.lastActionMessage = state.lastActionMessage,
		})
		.draw();
}

} // namespace FlowUi::devSystems::interface_elements

#endif
