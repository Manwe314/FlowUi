#pragma once

#include <array>
#include <cstddef>
#include <vector>



/** @addtogroup flowui_config
 * @{
 */

/** @brief Per-frame input snapshot consumed by FlowUi managers. */
struct FrameInput {
	/** @brief Number of tracked mouse buttons. */
	static constexpr std::size_t kMouseButtonCount = 5;
	/** @brief Number of tracked keyboard key slots. */
	static constexpr std::size_t kKeyboardKeyCount = 512;

	/** @brief Delta time for the frame in seconds. */
	double dt = 0.0;
	/** @brief Mouse X position. */
	float mouseX = 0.0f;
	/** @brief Mouse Y position. */
	float mouseY = 0.0f;
	/** @brief Mouse button down states. */
	std::array<bool, kMouseButtonCount> mouseDown{};
	/** @brief Horizontal scroll delta. */
	float scrollX = 0.0f;
	/** @brief Vertical scroll delta. */
	float scrollY = 0.0f;

	/** @brief Keyboard key down states. */
	std::array<bool, kKeyboardKeyCount> keyDown{};
	/** @brief Shift modifier state. */
	bool shift = false;
	/** @brief Ctrl modifier state. */
	bool ctrl = false;
	/** @brief Alt modifier state. */
	bool alt = false;
	/** @brief Super/Command modifier state. */
	bool super = false;

	/** @brief Text input codepoints received this frame. */
	std::vector<char32_t> text;
};

/** @brief Queues backend input events and drains them into FrameInput snapshots. */
struct InputQueue {
	/** @brief Queue a text input character. */
	void pushChar(char32_t c);
	/** @brief Queue a key state change. */
	void pushKey(int key, bool down);
	/** @brief Queue a mouse button state change. */
	void pushMouseButton(int mouseButton, bool down);
	/** @brief Queue a scroll delta. */
	void pushScroll(float dx, float dy);
	/** @brief Set latest mouse position. */
	void setMousePos(float x, float y);
	/** @brief Clear queued keyboard state. */
	void clearKeyboardState();
	/** @brief Clear queued mouse button state. */
	void clearMouseButtonsState();

	/** @brief Drain queued state into a frame input snapshot. */
	FrameInput drain(double dt);

private:
	std::array<bool, FrameInput::kMouseButtonCount> queuedMouseButtonsDown_{};
	std::array<bool, FrameInput::kKeyboardKeyCount> queuedKeysDown_{};
	std::vector<char32_t> queuedTextInput_;
	float latestMouseX_ = 0.0f;
	float latestMouseY_ = 0.0f;
	float queuedScrollX_ = 0.0f;
	float queuedScrollY_ = 0.0f;
};

/** @} */
