#include "devMode/debugView.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <string>

#include "FlowUi/Flow.hpp"
#include "devMode/devFlowElements.hpp"

namespace FlowUi::devMode {
namespace {

using DebugViewDefinition = ElementDefinition<
	DebugViewParams,
	DebugViewState,
	DebugViewResources,
	FLOW_DEF_ID("flowui/dev/debug-view"),
	true>;

int minMainViewWidth(const DebugViewParams& params) {
	return std::max(0, params.minMainViewWidthPx);
}

int maxMainViewWidth(const DebugViewParams& params) {
	return std::max(minMainViewWidth(params), params.maxMainViewWidthPx);
}

int clampMainViewWidth(const DebugViewParams& params, int value) {
	return std::clamp(value, minMainViewWidth(params), maxMainViewWidth(params));
}

void initializeMainViewWidthIfNeeded(DebugViewState& state, const DebugViewParams& params) {
	if (state.widthInitialized) {
		return;
	}
	state.mainViewWidthPx = clampMainViewWidth(params, params.defaultMainViewWidthPx);
	state.widthInitialized = true;
}

void runDebugViewLogic(DebugViewDefinition::InteractionContext& context) {
	DebugViewState& state = DebugViewDefinition::getOrCreateState(toFlowId(context.elementID));
	initializeMainViewWidthIfNeeded(state, context.params);
	state.mainViewWidthPx = clampMainViewWidth(context.params, state.mainViewWidthPx);
}

void buildDebugView(DebugViewDefinition::BuildContext& context) {
	DebugViewState& state = DebugViewDefinition::getOrCreateState(toFlowId(context.elementID));
	initializeMainViewWidthIfNeeded(state, context.params);
	state.mainViewWidthPx = clampMainViewWidth(context.params, state.mainViewWidthPx);

	CLAY({
		.id = context.uiManager.toClaySID(context.elementID),
		.layout = {
			.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
			.childGap = 0,
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	}) {
		const int minWidth = minMainViewWidth(context.params);
		const int maxWidth = maxMainViewWidth(context.params);

		::devDynamicSeparatorParams separatorParams{};
		separatorParams.orientation = ::devDynamicSeparatorParams::Orientation::Vertical;
		separatorParams.reverseDrag = true;
		separatorParams.width = std::max(1, context.params.separatorThicknessPx);
		separatorParams.height = 1;
		separatorParams.minValue = minWidth;
		separatorParams.maxValue = maxWidth;
		separatorParams.getValue = [&state]() {
			return state.mainViewWidthPx;
		};
		separatorParams.setValue = [&state, minWidth, maxWidth](int nextWidth) {
			state.mainViewWidthPx = std::clamp(nextWidth, minWidth, maxWidth);
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
				.width = state.mainViewWidthPx,
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
	uiManager
		.createElement(kDebugViewElement, "flowui/dev/debug-view")
		.setParameters(DebugViewParams{})
		.draw();
}

} // namespace FlowUi::devMode

#endif
