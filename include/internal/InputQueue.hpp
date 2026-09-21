#pragma once

#include <array>
#include <vector>

#include "FlowUi/Error.hpp"
#include "managers/structs/InputStructs.hpp"

namespace FlowUi::detail {

struct InputQueue {
	explicit InputQueue(
		std::size_t textCapacity = 4096,
		InputQueueOverflowPolicy overflowPolicy = InputQueueOverflowPolicy::DropNewest);

	void pushChar(char32_t c) noexcept;
	void pushKey(int key, bool down) noexcept;
	void pushMouseButton(int mouseButton, bool down) noexcept;
	void pushScroll(float dx, float dy) noexcept;
	void setMousePos(float x, float y) noexcept;
	/** Update native pointer presence independently of stale cursor coordinates. */
	void setPointerInside(bool inside) noexcept { pointer_inside_ = inside; }
	void clearKeyboardState() noexcept;
	void clearMouseButtonsState() noexcept;
	[[nodiscard]] std::uint64_t takeDroppedTextInputCount() noexcept;

	FrameInput drain(double dt);
	std::vector<char32_t> queuedTextInput_;
	std::size_t textCapacity_ = 0;
	std::uint64_t droppedTextInputCount_ = 0;
	InputQueueOverflowPolicy overflowPolicy_ = InputQueueOverflowPolicy::DropNewest;
	float latestMouseX_ = 0.0f;
	float latestMouseY_ = 0.0f;
	float queuedScrollX_ = 0.0f;
	float queuedScrollY_ = 0.0f;
	std::array<bool, FrameInput::kKeyboardKeyCount> queuedKeysDown_{};

private:
	bool pointer_inside_ = false;
	std::array<bool, FrameInput::kMouseButtonCount> queuedMouseButtonsDown_{};
};

} // namespace FlowUi::detail
