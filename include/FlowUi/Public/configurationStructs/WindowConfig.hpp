#pragma once

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include <optional>
#include <string>

namespace FlowUi {

/**
 * Native window creation settings. AppConfig::window supplies the main-window defaults;
 * secondary windows supply a complete WindowConfig or inherit through WindowConfigOverrides.
 * Vulkan and UI defaults always come from the application configuration.
 */
struct WindowConfig {
	/** Initial width in screen coordinates; must be positive. */
	int width = 1280;

	/** Initial height in screen coordinates; must be positive. */
	int height = 720;

	/** @brief Initial native window title. */
	std::string title = "FlowUi App";

	/** @brief Allow the user or window manager to resize the window. */
	bool resizable = true;

	/** @brief Create the window with native title bar and borders. */
	bool decorated = true;

	/** @brief Create the window in a maximized state when supported. */
	bool maximized = false;

	/** Request fullscreen mode on the primary monitor. */
	bool fullscreen = false;

	/** Prefer high-DPI framebuffer behavior where the window platform supports it. */
	bool highDPI = true;

	/** @brief Initial low-level input behavior for the window. */
	WindowInputConfig input{};
};

/**
 * Partial overrides for a secondary window cloned from AppConfig::window.
 * Absent values inherit; present false values override true. An absent title adds a window suffix.
 */
struct WindowConfigOverrides {
	/** Replacement width in screen coordinates; absent preserves the main-window default. */
	std::optional<int> width;
	/** Replacement height in screen coordinates; absent preserves the main-window default. */
	std::optional<int> height;
	/** Replacement title; absent uses the main-window title with a generated window suffix. */
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
