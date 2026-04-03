#pragma once

#include <cstdint>

#include "FlowUi/BuildConfig.hpp"

namespace FlowUi {
class UiManager;
}

namespace FlowUi::devMode {

struct DebugViewParams {
	float leftPanelWidthPx = 300.0f;
	float footerHeightPx = 34.0f;
};

struct DebugViewState {
	uint64_t selectedDefinitionId = 0u;
};

struct DebugViewResources {};

#if FLOW_UI_DEV_MODE
void drawDebugView(UiManager& uiManager);
#else
inline void drawDebugView(UiManager&) {}
#endif

} // namespace FlowUi::devMode
