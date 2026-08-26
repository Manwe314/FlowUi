#pragma once

#include "FlowUi/BuildConfig.hpp"

namespace FlowUi {
class UiManager;
}

namespace FlowUi::devMode {

#if FLOW_UI_DEV_MODE && !defined(FLOWUI_SKIP_LEGACY_DEV_ELEMENTS)
bool exportOverridesAsJson(UiManager& uiManager);
#else
inline bool exportOverridesAsJson(UiManager&) {
	return false;
}
#endif

} // namespace FlowUi::devMode
