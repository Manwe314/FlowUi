#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevCatalogueContentParameters {
	App* app = nullptr;
	DevInterfaceState* interfaceState = nullptr;
};

} // namespace FlowUi::devSystems::interface_elements

#endif
