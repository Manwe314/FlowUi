#include "devMode/debugView.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cmath>
#include <string>

#include "FlowUi/Flow.hpp"

namespace FlowUi::devMode {
namespace {

using DebugViewDefinition = ElementDefinition<
	DebugViewParams,
	DebugViewState,
	DebugViewResources,
	FLOW_DEF_ID("flowui/dev/debug-view"),
	true>;

struct MainViewSizing {
	int widthPx = 0;
	int minWidthPx = 0;
	int maxWidthPx = 0;
	int availableWidthPx = 1;
};

int minMainViewWidth(const DebugViewParams& params) {
	return std::max(0, params.minMainViewWidthPx);
}

int maxMainViewWidth(const DebugViewParams& params) {
	return std::max(minMainViewWidth(params), params.maxMainViewWidthPx);
}

int resolveRootWidthPx(
	FlowUi::UiManager& uiManager,
	DebugViewState& state,
	const DebugViewParams& params,
	int separatorWidthPx) {
	const Clay_ElementData rootData = Clay_GetElementData(uiManager.toClaySID("_Flow_Dev_root_"));
	int rootWidthPx = state.lastRootWidthPx;
	if (rootData.found)
	{
		rootWidthPx = static_cast<int>(std::lround(rootData.boundingBox.width));
	}
	if (rootWidthPx < 1)
	{
		rootWidthPx =
			std::max(1, params.defaultMainViewWidthPx) +
			std::max(1, separatorWidthPx) +
			std::max(1, minMainViewWidth(params));
	}
	state.lastRootWidthPx = rootWidthPx;
	return rootWidthPx;
}

MainViewSizing computeMainViewSizing(
	DebugViewState& state,
	const DebugViewParams& params,
	int availableWidthPx) {
	MainViewSizing sizing{};
	sizing.availableWidthPx = std::max(1, availableWidthPx);
	sizing.minWidthPx = minMainViewWidth(params);
	sizing.maxWidthPx = maxMainViewWidth(params);
	if (sizing.minWidthPx > sizing.availableWidthPx)
	{
		sizing.minWidthPx = sizing.availableWidthPx;
	}
	if (sizing.maxWidthPx > sizing.availableWidthPx)
	{
		sizing.maxWidthPx = sizing.availableWidthPx;
	}
	if (sizing.maxWidthPx < sizing.minWidthPx)
	{
		sizing.maxWidthPx = sizing.minWidthPx;
	}

	if (!state.splitInitialized)
	{
		float initialRatio = params.defaultMainViewSplitRatio;
		if (!std::isfinite(initialRatio) || initialRatio <= 0.0f || initialRatio >= 1.0f)
		{
			initialRatio =
				static_cast<float>(params.defaultMainViewWidthPx) /
				static_cast<float>(sizing.availableWidthPx);
		}
		state.mainViewSplitRatio = initialRatio;
		state.splitInitialized = true;
	}

	if (!std::isfinite(state.mainViewSplitRatio))
	{
		state.mainViewSplitRatio = params.defaultMainViewSplitRatio;
	}

	const float minRatio =
		static_cast<float>(sizing.minWidthPx) /
		static_cast<float>(sizing.availableWidthPx);
	const float maxRatio =
		static_cast<float>(sizing.maxWidthPx) /
		static_cast<float>(sizing.availableWidthPx);
	state.mainViewSplitRatio = std::clamp(state.mainViewSplitRatio, minRatio, maxRatio);

	sizing.widthPx = static_cast<int>(std::lround(
		state.mainViewSplitRatio *
		static_cast<float>(sizing.availableWidthPx)));
	sizing.widthPx = std::clamp(sizing.widthPx, sizing.minWidthPx, sizing.maxWidthPx);
	state.mainViewSplitRatio =
		static_cast<float>(sizing.widthPx) /
		static_cast<float>(sizing.availableWidthPx);
	return sizing;
}

void runDebugViewLogic(DebugViewDefinition::InteractionContext& context) {
	DebugViewState& state = DebugViewDefinition::getOrCreateState(toFlowId(context.elementID));
	if (!state.splitInitialized)
	{
		state.mainViewSplitRatio = context.params.defaultMainViewSplitRatio;
		state.splitInitialized = true;
	}
	if (!std::isfinite(state.mainViewSplitRatio))
	{
		state.mainViewSplitRatio = context.params.defaultMainViewSplitRatio;
	}
	state.mainViewSplitRatio = std::clamp(state.mainViewSplitRatio, 0.0f, 1.0f);
}

void buildDebugView(DebugViewDefinition::BuildContext& context) {
	DebugViewState& state = DebugViewDefinition::getOrCreateState(toFlowId(context.elementID));

	const int separatorWidthPx = std::max(1, context.params.separatorThicknessPx);
	const int rootWidthPx = resolveRootWidthPx(context.uiManager, state, context.params, separatorWidthPx);
	int availableWidthPx = rootWidthPx - separatorWidthPx;
	if (availableWidthPx < 1)
	{
		availableWidthPx = 1;
	}
	const MainViewSizing sizing = computeMainViewSizing(state, context.params, availableWidthPx);

	CLAY({
		.id = context.uiManager.toClaySID(context.elementID),
		.layout = {
			.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
			.childGap = 0,
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	}) {
		::devDynamicSeparatorParams separatorParams{};
		separatorParams.orientation = ::devDynamicSeparatorParams::Orientation::Vertical;
		separatorParams.reverseDrag = true;
		separatorParams.width = separatorWidthPx;
		separatorParams.height = 1;
		separatorParams.minValue = sizing.minWidthPx;
		separatorParams.maxValue = sizing.maxWidthPx;
		separatorParams.getValue = [widthPx = sizing.widthPx]() {
			return widthPx;
		};
		separatorParams.setValue = [
			&state,
			availableWidthPx = sizing.availableWidthPx,
			minWidthPx = sizing.minWidthPx,
			maxWidthPx = sizing.maxWidthPx
		](int nextWidth) {
			nextWidth = std::clamp(nextWidth, minWidthPx, maxWidthPx);
			if (availableWidthPx <= 0)
			{
				return;
			}
			state.mainViewSplitRatio =
				static_cast<float>(nextWidth) /
				static_cast<float>(availableWidthPx);
		};

		const std::string separatorId = context.createChildElementId("separator");
		context.uiManager
			.createElement(::kDevDynamicSeparator, separatorId)
			.setParameters(separatorParams)
			.draw();

		const std::string mainViewId = context.createChildElementId("main-view");
		context.uiManager
			.createElement(::kMainDevView, mainViewId)
			.setParameters(::mainDevViewParams{
				.width = sizing.widthPx,
			})
			.draw();
	}
}

inline const DebugViewDefinition kDebugViewElement = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DebugViewDefinition::InteractionContext& context) {
		runDebugViewLogic(context);
	},
	nullptr,
	+[](DebugViewDefinition::BuildContext& context) {
		buildDebugView(context);
	},
};

} // namespace

void drawDebugView(UiManager& uiManager) {
	DebugViewParams params{};
	params.defaultMainViewWidthPx = std::max(1, static_cast<int>(std::lround(uiManager.devToolsConfig().panelWidthPx)));
	uiManager
		.createElement(kDebugViewElement, "flowui/dev/debug-view")
		.setParameters(params)
		.draw();
}

} // namespace FlowUi::devMode

#endif
