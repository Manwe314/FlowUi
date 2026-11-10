#pragma once

#include <functional>

#include "FlowUi/Public/identificationTypes/WindowId.hpp"

namespace FlowUi {
class UiManager;

/** Callback used to construct one managed window's UI each frame. */
using UiBuildCallback = std::function<void(UiManager&, WindowId)>;

/** Lifecycle policy for a window registered with a managed UI callback. */
struct ManagedWindowFlags {
	/**
	 * Destroy this managed secondary window after native close is requested; false leaves close
	 * handling to the application.
	 */
	bool autoDestroyOnClose = true;
};

} // namespace FlowUi
