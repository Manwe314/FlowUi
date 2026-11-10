#pragma once

#include "FlowUi/Public/managerStructs/InputTypes.hpp"
#include <clay.h>

namespace FlowUi {

/**
 * @brief Configures low-level window input behavior.
 *
 * These options are forwarded to the window backend during window creation and
 * whenever the input configuration is updated at runtime.
 */
struct WindowInputConfig {
	/** @brief Cursor visibility and capture mode applied to the window. */
	CursorMode cursorMode = CursorMode::Normal;

	/**
	 * @brief Keep key press events available until they are polled.
	 *
	 * Useful when the app wants to avoid missing short key presses between
	 * input polling points.
	 */
	bool stickyKeys = false;

	/**
	 * @brief Keep mouse button press events available until they are polled.
	 *
	 * Useful when the app wants to avoid missing short mouse clicks between
	 * input polling points.
	 */
	bool stickyMouseButtons = false;

	/**
	 * @brief Preserve lock-key modifier state in reported input events.
	 *
	 * When supported by the backend, events include Caps Lock and Num Lock
	 * modifier bits while those locks are active.
	 */
	bool lockKeyMods = false;

	/**
	 * @brief Request unaccelerated mouse motion when the backend supports it.
	 *
	 * Unsupported backends or platforms disable this option automatically.
	 */
	bool rawMouseMotion = false;
};

/**
 * @brief Configures input field editing visuals.
 *
 * These values control caret and selection rendering for FlowUi text input
 * fields; they do not affect low-level window input behavior.
 */
struct InputManagerConfig {
	/** @brief Caret width in pixels. */
	float caretWidthPx = 2.0f;
	/** @brief Extra caret height above the text bounds. */
	float caretHeightOverflowTopPx = 1.0f;
	/** @brief Extra caret height below the text bounds. */
	float caretHeightOverflowBottomPx = 1.0f;
	/** @brief Caret color. */
	Clay_Color caretColor = Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
	/** @brief Selection highlight rectangle color. */
	Clay_Color highlightBoxColor = Clay_Color{66.0f, 133.0f, 244.0f, 150.0f};
	/** @brief Text color used for selected text. */
	Clay_Color highlightedTextColor = Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
};

} // namespace FlowUi
