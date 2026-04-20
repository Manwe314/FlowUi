#pragma once

#include <cstdint>

#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/devFlowElements.hpp"
#endif

namespace FlowUi {
class UiManager;
}

namespace FlowUi::devMode {

struct DebugViewParams {
	float defaultMainViewSplitRatio = 0.30f;
	int defaultMainViewWidthPx = 420;
	int minMainViewWidthPx = 220;
	int maxMainViewWidthPx = 1200;
	int separatorThicknessPx = 6;
};

struct DebugViewState {
	float mainViewSplitRatio = 0.30f;
	bool splitInitialized = false;
	int lastRootWidthPx = 0;
};

struct DebugViewResources {};

#if FLOW_UI_DEV_MODE
void drawDebugView(UiManager& uiManager);
#else
inline void drawDebugView(UiManager&) {}
#endif

} // namespace FlowUi::devMode
