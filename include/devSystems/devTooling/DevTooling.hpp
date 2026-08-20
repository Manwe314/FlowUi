#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

namespace FlowUi::devSystems {

/** Development-only shell for live and offline developer-tool surfaces. */
class DevTooling {
public:
	DevTooling() noexcept;
	~DevTooling();

	DevTooling(const DevTooling&) = delete;
	DevTooling& operator=(const DevTooling&) = delete;
	DevTooling(DevTooling&&) = delete;
	DevTooling& operator=(DevTooling&&) = delete;
};

} // namespace FlowUi::devSystems

#endif
