#pragma once

#include "FlowUi/Public/managerStructs/InputTypes.hpp"
#include <clay.h>

namespace FlowUi {

/**
 * Window input modes applied at creation or by App::setWindowInputConfig().
 * Backend support can limit raw motion and lock-key reporting.
 */
struct WindowInputConfig {
	/** @brief Cursor visibility and capture mode applied to the window. */
	CursorMode cursorMode = CursorMode::Normal;

	/**
	 * Retain short key presses until polled, including presses released between polls.
	 */
	bool stickyKeys = false;

	/**
	 * Retain short mouse-button presses until polled, including clicks between polls.
	 */
	bool stickyMouseButtons = false;

	/**
	 * Include active Caps Lock and Num Lock modifier bits when supported by the backend.
	 */
	bool lockKeyMods = false;

	/**
	 * Request unaccelerated mouse motion where supported; cursor/backend mode may limit its effect.
	 */
	bool rawMouseMotion = false;
};

/**
 * Text-field caret and selection appearance; dimensions are pixels and colors are RGBA, 0–255.
 * Window event behavior is configured separately by WindowInputConfig.
 */
struct InputManagerConfig {
	/** @brief Caret width in pixels. */
	float caretWidthPx = 2.0f;
	/** Extra caret height above the text bounds, in pixels. */
	float caretHeightOverflowTopPx = 1.0f;
	/** Extra caret height below the text bounds, in pixels. */
	float caretHeightOverflowBottomPx = 1.0f;
	/** @brief Caret color. */
	Clay_Color caretColor = Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
	/** @brief Selection highlight rectangle color. */
	Clay_Color highlightBoxColor = Clay_Color{66.0f, 133.0f, 244.0f, 150.0f};
	/** @brief Text color used for selected text. */
	Clay_Color highlightedTextColor = Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
};

} // namespace FlowUi
