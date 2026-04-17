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
	int defaultMainViewWidthPx = 420;
	int minMainViewWidthPx = 220;
	int maxMainViewWidthPx = 1200;
	int separatorThicknessPx = 6;
};

struct DebugViewState {
	int mainViewWidthPx = 0;
	bool widthInitialized = false;
};

struct DebugViewResources {};

#if FLOW_UI_DEV_MODE
void drawDebugView(UiManager& uiManager);
#else
inline void drawDebugView(UiManager&) {}
#endif

} // namespace FlowUi::devMode
