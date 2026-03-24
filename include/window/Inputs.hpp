#pragma once

#include <array>
#include <cstddef>
#include <vector>



struct FrameInput {
	static constexpr std::size_t kMouseButtonCount = 5;
	static constexpr std::size_t kKeyboardKeyCount = 512;

	double dt = 0.0;
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	std::array<bool, kMouseButtonCount> mouseDown{};
	float scrollX = 0.0f;
	float scrollY = 0.0f;

	std::array<bool, kKeyboardKeyCount> keyDown{};
	bool shift = false;
	bool ctrl = false;
	bool alt = false;
	bool super = false;

	std::vector<char32_t> text;
};

struct InputQueue {
  // filled by callbacks during pollEvents()
  // drained at beginFrame()
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
