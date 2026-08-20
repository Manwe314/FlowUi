#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

namespace FlowUi::devSystems {

/** Development-only shell for inspection, intervention, and persistence. */
class DevFunctionality {
public:
	DevFunctionality() noexcept;
	~DevFunctionality();

	DevFunctionality(const DevFunctionality&) = delete;
	DevFunctionality& operator=(const DevFunctionality&) = delete;
	DevFunctionality(DevFunctionality&&) = delete;
	DevFunctionality& operator=(DevFunctionality&&) = delete;
};

} // namespace FlowUi::devSystems

#endif
