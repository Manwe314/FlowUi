#include "devSystems/devInterface/Permanents/Elements/DevInterface.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <utility>

#include "FSEL/SplitterHandle.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevContent.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevContentAreas.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevContentHeader.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevInterfaceFooter.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevInterfaceHeader.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "managers/UiManager.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInspectSelection.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include "devSystems/devInterface/Catalogue/DevCatalogueFocus.hpp"
#include "devSystems/devInterface/Catalogue/Selector/DevCatalogueSelector.hpp"
#include "devSystems/devInterface/Catalogue/Inspector/DevCatalogueInspector.hpp"

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

void toggle_catalogue_drawer(void* target, uint64_t action, uint64_t) {
	auto& state = *static_cast<DevCatalogueState*>(target);
	if (action == 0)
		state.selector_drawer = !state.selector_drawer;
	else
		state.inspector_drawer = !state.inspector_drawer;
}

template <class Context>
void build_catalogue_content(Context& context, DevInterfaceState& state) {
	namespace ui = diagnostics_ui;
	catalogue_focus::begin_frame(context, state.catalogue);
	const float available_width = Clay_GetLayoutDimensions().width;
	const bool hide_inspector = available_width < state.selectorWidth + state.inspectorWidth + 482;
	const bool hide_selector = available_width < state.selectorWidth + 482;
	if (hide_inspector || hide_selector) {
		auto toolbar = ui::row(30);
		toolbar.layout.padding = {10, 10, 0, 0};
		CLAY(context.clayID("catalogue-drawers"), toolbar) {
			if (hide_selector)
				ui::button(context, "catalogue-browse",
						   state.catalogue.selector_drawer ? "Close browser" : "Browse assets",
						   &state.catalogue, toggle_catalogue_drawer, 0);
			if (hide_selector)
				catalogue_focus::register_element(context, state.catalogue,
												  ui::Control::definitionId,
												  LocalElementName{"catalogue-browse"});
			if (hide_inspector)
				ui::button(context, "catalogue-facts",
						   state.catalogue.inspector_drawer ? "Close facts" : "Asset facts",
						   &state.catalogue, toggle_catalogue_drawer, 1);
			if (hide_inspector)
				catalogue_focus::register_element(context, state.catalogue,
												  ui::Control::definitionId,
												  LocalElementName{"catalogue-facts"});
		}
	}
	context.uiManager.createElement(kDevContent, LocalElementName{"content"}).construct();
	const auto draw_area = [&](const auto& element, LocalElementName identity) {
		context.uiManager.createElement(element, identity)
			.setParameters(
				DevContentAreaParameters{.app = context.params.app, .interfaceState = &state})
			.setDevInternalCapture(true)
			.draw();
	};
	const auto splitter = [&](bool inspector) {
		FSEL::SplitterHandleParameters parameters{};
		parameters.position =
			inspector ? FSEL::SplitterPosition::Leading : FSEL::SplitterPosition::Trailing;
		parameters.targetExtent = inspector ? &state.inspectorWidth : &state.selectorWidth;
		parameters.minExtent = inspector ? kInspectorMinimumWidth : kSelectorMinimumWidth;
		parameters.maxExtent = inspector ? kInspectorMaximumWidth : kSelectorMaximumWidth;
		parameters.thickness = 1;
		parameters.hitThickness = 10;
		parameters.backgroundColor = interface_theme::kBorderPrimary;
		context.uiManager
			.createElement(FSEL::kSplitterHandle,
						   inspector ? kInspectorSplitter : kSelectorSplitter)
			.setParameters(parameters)
			.setDevInternalCapture(true)
			.draw();
	};
	if (!hide_selector) {
		draw_area(kDevSelectorArea, kSelectorArea);
		splitter(false);
	}
	draw_area(kDevWorkbenchArea, kWorkbenchArea);
	if (!hide_inspector) {
		splitter(true);
		draw_area(kDevInspectorArea, kInspectorArea);
	}
	const auto drawer = [&](bool inspector) {
		auto surface = ui::column(0);
		const float width =
			std::min(available_width - 16, inspector ? state.inspectorWidth : state.selectorWidth);
		surface.layout.sizing = {
			CLAY_SIZING_FIXED(std::max(1.0f, width)),
			CLAY_SIZING_FIXED(std::max(100.0f, Clay_GetLayoutDimensions().height - 180))};
		surface.floating.attachTo = CLAY_ATTACH_TO_PARENT;
		surface.floating.attachPoints = {CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP};
		surface.floating.offset.x = inspector ? std::max(0.0f, available_width - width - 8) : 8;
		surface.floating.zIndex = 70;
		surface.backgroundColor = interface_theme::kDepth1Panel;
		surface.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
		surface.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID(Keyed("catalogue-drawer", inspector ? 1 : 0)), surface) {
			if (inspector)
				context.uiManager.createElement(kDevCatalogueInspector, kInspectorArea)
					.setParameters(DevCatalogueContentParameters{.app = context.params.app,
																 .interfaceState = &state})
					.setDevInternalCapture(true)
					.draw();
			else
				context.uiManager.createElement(kDevCatalogueSelector, kSelectorArea)
					.setParameters(DevCatalogueContentParameters{.app = context.params.app,
																 .interfaceState = &state})
					.setDevInternalCapture(true)
					.draw();
		}
	};
	if (hide_selector && state.catalogue.selector_drawer)
		drawer(false);
	if (hide_inspector && state.catalogue.inspector_drawer)
		drawer(true);
	context.uiManager.drawConstructed();
}

} // namespace

void DevInterface::buildElement(BuildContext& context) {
	FLOWUI_DEV_TIMING_ZONE_IF(
		context.devTimingRecorder(), TimingCategory::DevTool,
		TimingZoneRole::DevToolWork, "flowui.dev_interface.build");

	if (!context.params.interface_state)
		return;
	DevInterfaceState& state = *context.params.interface_state;
	if (context.params.app) synchronize_inspect_selection(*context.params.app, state);
	if (state.selectedWindowId == InvalidWindowId) {
		state.selectedWindowId = context.params.mainWindowId;
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	if (state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Catalogue))
		root.layout.sizing.width = CLAY_SIZING_PERCENT(1);
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
	context.uiManager.createElement(kDevInterfaceHeader, LocalElementName{"permanent-header"})
		.setParameters(DevInterfaceHeaderParameters{
			.app = context.params.app,
			.interfaceState = context.params.interface_state,
			.interfaceWindowId = context.params.interfaceWindowId,
		})
		.draw();
}

void DevInterface::buildContentHeader(BuildContext& context) {
	context.uiManager.createElement(kDevContentHeader, LocalElementName{"content-header"})
		.setParameters(DevContentHeaderParameters{
			.app = context.params.app,
			.interfaceState = context.params.interface_state,
		})
		.draw();
}

void DevInterface::buildContent(BuildContext& context) {
	DevInterfaceState& state = *context.params.interface_state;
	state.selectorWidth = std::clamp(
		state.selectorWidth, kSelectorMinimumWidth, kSelectorMaximumWidth);
	state.inspectorWidth = std::clamp(
		state.inspectorWidth, kInspectorMinimumWidth, kInspectorMaximumWidth);
	if (state.activeTab == static_cast<uint64_t>(DevInterfaceTab::Catalogue)) {
		build_catalogue_content(context, state);
		return;
	}

	context.uiManager.createElement(kDevContent, LocalElementName{"content"})
		.construct();
	{
		context.uiManager.createElement(kDevSelectorArea, kSelectorArea)
			.setParameters(DevContentAreaParameters{
				.app = context.params.app,
				.interfaceState = &state,
			})
			.setDevInternalCapture(true)
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
			.setParameters(DevContentAreaParameters{
				.app = context.params.app,
				.interfaceState = &state,
			})
			.setDevInternalCapture(true)
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
			.setParameters(DevContentAreaParameters{
				.app = context.params.app,
				.interfaceState = &state,
			})
			.setDevInternalCapture(true)
			.draw();
	}
	context.uiManager.drawConstructed();
}

void DevInterface::buildPermanentFooter(BuildContext& context) {
	const DevInterfaceState& state = *context.params.interface_state;
	context.uiManager.createElement(kDevInterfaceFooter, LocalElementName{"permanent-footer"})
		.setParameters(DevInterfaceFooterParameters{
			.errorCount = state.activeErrorCount,
			.unbakedChangeCount = state.unbakedChangeCount,
			.prepared_capacity_count =
				static_cast<uint32_t>(state.memory_workbench.analysis.staged.size()),
			.lastActionMessage = state.lastActionMessage,
		})
		.draw();
}

} // namespace FlowUi::devSystems::interface_elements

#endif
