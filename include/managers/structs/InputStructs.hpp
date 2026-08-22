#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace FlowUi {

/** @addtogroup flowui_config
 * @{
 */

/**
 * @brief Per-frame input snapshot consumed by FlowUi managers and elements.
 *
 * FrameInput is the normalized input state for one FlowUi frame. App builds it
 * from the active window backend, scales pointer and scroll values into FlowUi
 * layout space, and then passes it through UiManager to input fields,
 * shortcuts, and custom element interaction callbacks.
 *
 * Keyboard state is stored as backend key slots. In the default backend those
 * slots are GLFW-compatible key codes. Text input is stored separately as
 * Unicode codepoints received during the frame.
 */
struct FrameInput {
	/**
	 * @brief Number of tracked mouse button slots.
	 *
	 * The default backend maps the first slots to common mouse buttons, with
	 * index 0 used as the primary button.
	 */
	static constexpr std::size_t kMouseButtonCount = 5;

	/**
	 * @brief Number of tracked keyboard key slots.
	 *
	 * Key indices are backend key codes. Code that reads keyDown should bounds
	 * check against this value before indexing.
	 */
	static constexpr std::size_t kKeyboardKeyCount = 512;

	/**
	 * @brief Delta time for the frame in seconds.
	 *
	 * Managers use this for time-based behavior such as caret blinking and key
	 * repeat. Custom elements may use it for interaction animation or
	 * drag-related timing.
	 */
	double dt = 0.0;

	/**
	 * @brief Mouse X position in FlowUi layout coordinates.
	 *
	 * This value has already been adjusted from window input space into the UI
	 * coordinate space used by Clay layout and element hit testing.
	 */
	float mouseX = 0.0f;

	/**
	 * @brief Mouse Y position in FlowUi layout coordinates.
	 *
	 * This value has already been adjusted from window input space into the UI
	 * coordinate space used by Clay layout and element hit testing.
	 */
	float mouseY = 0.0f;

	/**
	 * @brief Mouse button down states for the current frame.
	 *
	 * Each entry is true while the corresponding backend mouse button is held.
	 * Index 0 is the primary mouse button in the default backend.
	 */
	std::array<bool, kMouseButtonCount> mouseDown{};

	/**
	 * @brief Horizontal scroll delta for this frame.
	 *
	 * The value is a per-frame delta, not an accumulated scroll position.
	 */
	float scrollX = 0.0f;

	/**
	 * @brief Vertical scroll delta for this frame.
	 *
	 * The value is a per-frame delta, not an accumulated scroll position.
	 */
	float scrollY = 0.0f;

	/**
	 * @brief Keyboard key down states for the current frame.
	 *
	 * Each entry is true while the corresponding backend key slot is held. For
	 * command shortcuts, prefer ShortcutManager unless the element needs direct
	 * low-level key state.
	 */
	std::array<bool, kKeyboardKeyCount> keyDown{};

	/** @brief Whether either Shift key is currently held. */
	bool shift = false;

	/** @brief Whether either Ctrl key is currently held. */
	bool ctrl = false;

	/** @brief Whether either Alt key is currently held. */
	bool alt = false;

	/** @brief Whether either Super/Command key is currently held. */
	bool super = false;

	/**
	 * @brief Text input codepoints received this frame.
	 *
	 * This is character input, not physical key input. Text editing controls use
	 * this for inserted text while using keyDown and modifier state for commands
	 * such as navigation, deletion, copy, and paste.
	 */
	std::vector<char32_t> text;

	/** Text codepoints discarded by the bounded platform queue since the prior frame. */
	std::uint64_t droppedTextInputCount = 0;
};

/** @} */

} // namespace FlowUi
