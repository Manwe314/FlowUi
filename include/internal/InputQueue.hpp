#pragma once

#include <array>
#include <vector>

#include "managers/structs/InputStructs.hpp"

namespace FlowUi::detail {

struct InputQueue {
	void pushChar(char32_t c);
	void pushKey(int key, bool down);
	void pushMouseButton(int mouseButton, bool down);
	void pushScroll(float dx, float dy);
	void setMousePos(float x, float y);
	void clearKeyboardState();
	void clearMouseButtonsState();

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

} // namespace FlowUi::detail
