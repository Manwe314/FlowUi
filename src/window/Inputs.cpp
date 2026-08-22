#include "internal/InputQueue.hpp"

#include <algorithm>
#include <utility>

#include <GLFW/glfw3.h>

namespace {

bool isValidKeyboardKey(int key) {
	return key >= 0 && key < static_cast<int>(FlowUi::FrameInput::kKeyboardKeyCount);
}

bool isValidMouseButton(int button) {
	return button >= 0 && button < static_cast<int>(FlowUi::FrameInput::kMouseButtonCount);
}

} // namespace

namespace FlowUi::detail {

InputQueue::InputQueue(std::size_t textCapacity, InputQueueOverflowPolicy overflowPolicy)
	: textCapacity_(textCapacity), overflowPolicy_(overflowPolicy) {
	queuedTextInput_.reserve(textCapacity_);
}

void InputQueue::pushChar(char32_t c) noexcept {
	if (queuedTextInput_.size() < textCapacity_) {
		queuedTextInput_.push_back(c);
		return;
	}
	++droppedTextInputCount_;
	if (overflowPolicy_ != InputQueueOverflowPolicy::DropOldest || queuedTextInput_.empty()) {
		return;
	}
	std::move(queuedTextInput_.begin() + 1, queuedTextInput_.end(), queuedTextInput_.begin());
	queuedTextInput_.back() = c;
}

std::uint64_t InputQueue::takeDroppedTextInputCount() noexcept {
	const std::uint64_t result = droppedTextInputCount_;
	droppedTextInputCount_ = 0;
	return result;
}

void InputQueue::pushKey(int key, bool down) noexcept {
	if (!isValidKeyboardKey(key)) {
		return;
	}
	queuedKeysDown_[static_cast<std::size_t>(key)] = down;
}

void InputQueue::pushMouseButton(int mouseButton, bool down) noexcept {
	if (!isValidMouseButton(mouseButton)) {
		return;
	}
	queuedMouseButtonsDown_[static_cast<std::size_t>(mouseButton)] = down;
}

void InputQueue::pushScroll(float dx, float dy) noexcept {
	queuedScrollX_ += dx;
	queuedScrollY_ += dy;
}

void InputQueue::setMousePos(float x, float y) noexcept {
	latestMouseX_ = x;
	latestMouseY_ = y;
}

void InputQueue::clearKeyboardState() noexcept {
	std::fill(queuedKeysDown_.begin(), queuedKeysDown_.end(), false);
}

void InputQueue::clearMouseButtonsState() noexcept {
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
	frameInput.text.assign(queuedTextInput_.begin(), queuedTextInput_.end());
	frameInput.droppedTextInputCount = takeDroppedTextInputCount();

	queuedTextInput_.clear();
	queuedScrollX_ = 0.0f;
	queuedScrollY_ = 0.0f;
	return frameInput;
}

} // namespace FlowUi::detail
