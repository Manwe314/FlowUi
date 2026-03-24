#include "window/Inputs.hpp"

#include <algorithm>
#include <utility>

#include <GLFW/glfw3.h>

namespace {

bool isValidKeyboardKey(int key) {
	return key >= 0 && key < static_cast<int>(FrameInput::kKeyboardKeyCount);
}

bool isValidMouseButton(int button) {
	return button >= 0 && button < static_cast<int>(FrameInput::kMouseButtonCount);
}

} // namespace

void InputQueue::pushChar(char32_t c) {
	queuedTextInput_.push_back(c);
}

void InputQueue::pushKey(int key, bool down) {
	if (!isValidKeyboardKey(key)) {
		return;
	}
	queuedKeysDown_[static_cast<std::size_t>(key)] = down;
}

void InputQueue::pushMouseButton(int mouseButton, bool down) {
	if (!isValidMouseButton(mouseButton)) {
		return;
	}
	queuedMouseButtonsDown_[static_cast<std::size_t>(mouseButton)] = down;
}

void InputQueue::pushScroll(float dx, float dy) {
	queuedScrollX_ += dx;
	queuedScrollY_ += dy;
}

void InputQueue::setMousePos(float x, float y) {
	latestMouseX_ = x;
	latestMouseY_ = y;
}

void InputQueue::clearKeyboardState() {
	std::fill(queuedKeysDown_.begin(), queuedKeysDown_.end(), false);
}

void InputQueue::clearMouseButtonsState() {
	std::fill(queuedMouseButtonsDown_.begin(), queuedMouseButtonsDown_.end(), false);
}

FrameInput InputQueue::drain(double dt) {
	FrameInput frameInput{};
	frameInput.dt = dt;
	frameInput.mouseX = latestMouseX_;
	frameInput.mouseY = latestMouseY_;
	frameInput.mouseDown = queuedMouseButtonsDown_;
	frameInput.scrollX = queuedScrollX_;
	frameInput.scrollY = queuedScrollY_;
	frameInput.keyDown = queuedKeysDown_;
	frameInput.shift = queuedKeysDown_[static_cast<std::size_t>(GLFW_KEY_LEFT_SHIFT)] ||
		queuedKeysDown_[static_cast<std::size_t>(GLFW_KEY_RIGHT_SHIFT)];
	frameInput.ctrl = queuedKeysDown_[static_cast<std::size_t>(GLFW_KEY_LEFT_CONTROL)] ||
		queuedKeysDown_[static_cast<std::size_t>(GLFW_KEY_RIGHT_CONTROL)];
	frameInput.alt = queuedKeysDown_[static_cast<std::size_t>(GLFW_KEY_LEFT_ALT)] ||
		queuedKeysDown_[static_cast<std::size_t>(GLFW_KEY_RIGHT_ALT)];
	frameInput.super = queuedKeysDown_[static_cast<std::size_t>(GLFW_KEY_LEFT_SUPER)] ||
		queuedKeysDown_[static_cast<std::size_t>(GLFW_KEY_RIGHT_SUPER)];
	frameInput.text = std::move(queuedTextInput_);

	queuedTextInput_.clear();
	queuedScrollX_ = 0.0f;
	queuedScrollY_ = 0.0f;
	return frameInput;
}
