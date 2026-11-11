#pragma once

namespace FlowUi {

/** Runtime native-window operations supported by a window backend. */
struct WindowOperationCapabilities {
	/** Set the window's close-request flag. */
	bool close_window = false;
	/** Request native minimization. */
	bool minimize = false;
	/** Toggle the observed native maximized state while windowed. */
	bool maximize_restore = false;
	/** Enter and leave fullscreen while restoring windowed geometry and state. */
	bool toggle_fullscreen = false;
};

} // namespace FlowUi
