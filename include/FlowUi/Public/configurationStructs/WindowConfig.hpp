#pragma once

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include <optional>
#include <string>

namespace FlowUi {

/**
 * @brief Describes a native application window.
 *
 * Width, height, title, and input settings also provide the initial runtime
 * defaults used by FlowUi managers. AppConfig::window configures the semantic
 * main window. Explicit secondary windows inherit the application's Vulkan and
 * UI configuration and supply only this native-window configuration.
 */
struct WindowConfig {
	/** @brief Initial window width in screen coordinates.
	 *
	 * Must be a positive integer greater than 0.
	 */
	int width = 1280;

	/** @brief Initial window height in screen coordinates.
	 *
	 * Must be a positive integer greater than 0.
	 */
	int height = 720;

	/** @brief Initial native window title. */
	std::string title = "FlowUi App";

	/** @brief Allow the user or window manager to resize the window. */
	bool resizable = true;

	/** @brief Create the window with native title bar and borders. */
	bool decorated = true;

	/** @brief Create the window in a maximized state when supported. */
	bool maximized = false;

	/** @brief Create the window on the primary monitor in fullscreen mode.
	 *
	 * @warning The GLFW backend creates a true fullscreen window without
	 * normal title-bar controls, so users may not have a visible close or
	 * minimize button. Provide an app-level exit path, such as a key chord
	 * or menu action, before enabling fullscreen by default.
	 */
	bool fullscreen = false;

	/** @brief Prefer high-DPI framebuffer behavior for capable backends. */
	bool highDPI = true;

	/** @brief Initial low-level input behavior for the window. */
	WindowInputConfig input{};
};

/** Partial overrides for a secondary window cloned from AppConfig::window. */
struct WindowConfigOverrides {
	/** Replacement width in screen coordinates; absent preserves the main-window default. */
	std::optional<int> width;
	/** Replacement height in screen coordinates; absent preserves the main-window default. */
	std::optional<int> height;
	/** Replacement title; absent preserves the main-window default. */
	std::optional<std::string> title;
	/** Replacement user-resize policy. */
	std::optional<bool> resizable;
	/** Replacement native decoration policy. */
	std::optional<bool> decorated;
	/** Replacement initial maximized state. */
	std::optional<bool> maximized;
	/** Replacement fullscreen policy. */
	std::optional<bool> fullscreen;
	/** Replacement high-DPI preference. */
	std::optional<bool> highDPI;
	/** Replacement input configuration; replaces the entire block when present. */
	std::optional<WindowInputConfig> input;
};

} // namespace FlowUi
