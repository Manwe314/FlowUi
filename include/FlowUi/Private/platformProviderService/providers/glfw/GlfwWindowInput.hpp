#pragma once

#include "FlowUi/Private/platformProviderService/providers/glfw/GlfwWindow.hpp"
#include "FlowUi/Public/input/InputSink.hpp"
#include "FlowUi/Public/input/Key.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <string_view>

namespace FlowUi::detail::platform {
[[nodiscard]] inline int toGlfwCursorMode(FlowUi::CursorMode cursorMode) noexcept {
	switch (cursorMode) {
	case FlowUi::CursorMode::Hidden:
		return GLFW_CURSOR_HIDDEN;
	case FlowUi::CursorMode::Disabled:
		return GLFW_CURSOR_DISABLED;
	case FlowUi::CursorMode::Normal:
	default:
		return GLFW_CURSOR_NORMAL;
	}
}

[[nodiscard]] inline int toGlfwCursorShape(FlowUi::CursorType cursorType) noexcept {
	switch (cursorType) {
	case FlowUi::CursorType::IBeam:
		return GLFW_IBEAM_CURSOR;
	case FlowUi::CursorType::Crosshair:
		return GLFW_CROSSHAIR_CURSOR;
	case FlowUi::CursorType::PointingHand:
		return GLFW_HAND_CURSOR;
	case FlowUi::CursorType::ResizeHorizontal:
#ifdef GLFW_RESIZE_EW_CURSOR
		return GLFW_RESIZE_EW_CURSOR;
#else
		return GLFW_HRESIZE_CURSOR;
#endif
	case FlowUi::CursorType::ResizeVertical:
#ifdef GLFW_RESIZE_NS_CURSOR
		return GLFW_RESIZE_NS_CURSOR;
#else
		return GLFW_VRESIZE_CURSOR;
#endif
	case FlowUi::CursorType::ResizeDiagonalTL:
#ifdef GLFW_RESIZE_NWSE_CURSOR
		return GLFW_RESIZE_NWSE_CURSOR;
#else
		return GLFW_HRESIZE_CURSOR;
#endif
	case FlowUi::CursorType::ResizeDiagonalTR:
#ifdef GLFW_RESIZE_NESW_CURSOR
		return GLFW_RESIZE_NESW_CURSOR;
#else
		return GLFW_HRESIZE_CURSOR;
#endif
	case FlowUi::CursorType::ResizeAll:
#ifdef GLFW_RESIZE_ALL_CURSOR
		return GLFW_RESIZE_ALL_CURSOR;
#else
		return GLFW_CROSSHAIR_CURSOR;
#endif
	case FlowUi::CursorType::NotAllowed:
#ifdef GLFW_NOT_ALLOWED_CURSOR
		return GLFW_NOT_ALLOWED_CURSOR;
#else
		return GLFW_ARROW_CURSOR;
#endif
	case FlowUi::CursorType::Wait:
		return GLFW_ARROW_CURSOR;
	case FlowUi::CursorType::Progress:
		return GLFW_ARROW_CURSOR;
	case FlowUi::CursorType::Grab:
		return GLFW_HAND_CURSOR;
	case FlowUi::CursorType::Grabbing:
		return GLFW_HAND_CURSOR;
	case FlowUi::CursorType::Custom:
		return GLFW_ARROW_CURSOR;
	case FlowUi::CursorType::Default:
	case FlowUi::CursorType::Arrow:
	default:
		return GLFW_ARROW_CURSOR;
	}
}

/** Per-window cursor, clipboard and normalized native input endpoints. */
class GlfwWindowInput {
public:
	GlfwWindowInput() noexcept = default;
	~GlfwWindowInput() noexcept { shutdown(); }
	GlfwWindowInput(const GlfwWindowInput&) = delete;
	GlfwWindowInput& operator=(const GlfwWindowInput&) = delete;
	/** Acquire input modes and the initial cursor for this exact window. */
	[[nodiscard]] Status initialize(GlfwWindow& window, const WindowInputConfig& config) noexcept {
		if (window_)
			return unexpectedError(
				makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::WindowPollInput));
		if (const auto status = window.validate(); !status)
			return status;
		window_ = &window;
		if (const auto status = setInputConfig(config); !status) {
			shutdown();
			return status;
		}
		if (const auto status = setCursor(CursorType::Arrow); !status) {
			shutdown();
			return status;
		}
		return {};
	}
	/** Release cached native cursors before releasing the native window. */
	void shutdown() noexcept {
		if (!window_)
			return;
		if (const auto status = window_->validate(); !status) {
			reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
			return;
		}
		glfwSetCursor(window_->native(), nullptr);
		for (auto*& cursor : standardCursors_) {
			if (cursor) {
				glfwDestroyCursor(cursor);
				cursor = nullptr;
			}
		}
		window_ = nullptr;
		sink_ = nullptr;
		keysDown_.fill(false);
		buttonsDown_.fill(false);
		currentCursor_ = CursorType::Default;
	}
	/** Observe acquired input state. */
	[[nodiscard]] bool ready() const noexcept { return window_ != nullptr; }
	/** Borrow this input endpoint's exact typed window. */
	[[nodiscard]] const GlfwWindow* window() const noexcept { return window_; }
	/** Apply effective GLFW input modes. */
	[[nodiscard]] Status setInputConfig(const WindowInputConfig& config) noexcept {
		if (const auto status = validate(); !status)
			return status;
		glfwGetError(nullptr);
		auto* native = window_->native();
		glfwSetInputMode(native, GLFW_CURSOR, toGlfwCursorMode(config.cursorMode));
		glfwSetInputMode(native, GLFW_STICKY_KEYS, config.stickyKeys ? GLFW_TRUE : GLFW_FALSE);
		glfwSetInputMode(native, GLFW_STICKY_MOUSE_BUTTONS,
						 config.stickyMouseButtons ? GLFW_TRUE : GLFW_FALSE);
		inputConfig_ = config;
#ifdef GLFW_LOCK_KEY_MODS
		glfwSetInputMode(native, GLFW_LOCK_KEY_MODS, config.lockKeyMods ? GLFW_TRUE : GLFW_FALSE);
#else
		inputConfig_.lockKeyMods = false;
#endif
#ifdef GLFW_RAW_MOUSE_MOTION
		inputConfig_.rawMouseMotion = config.rawMouseMotion && supportsRawMouseMotion();
		if (supportsRawMouseMotion())
			glfwSetInputMode(native, GLFW_RAW_MOUSE_MOTION,
							 inputConfig_.rawMouseMotion ? GLFW_TRUE : GLFW_FALSE);
#else
		inputConfig_.rawMouseMotion = false;
#endif
		return GlfwRuntime::nativeResult(ErrorSite::WindowPollInput);
	}
	/** Observe effective input settings. */
	[[nodiscard]] WindowInputConfig inputConfig() const noexcept { return inputConfig_; }
	/** Observe actual runtime raw-motion support. */
	[[nodiscard]] bool supportsRawMouseMotion() const noexcept {
#ifdef GLFW_RAW_MOUSE_MOTION
		return window_ && glfwRawMouseMotionSupported() == GLFW_TRUE;
#else
		return false;
#endif
	}
	/** Apply a cached cursor, suppressing an unchanged selection. */
	[[nodiscard]] Status setCursor(CursorType cursorType) noexcept {
		if (const auto status = validate(); !status)
			return status;
		const auto cursorIndex = static_cast<std::size_t>(cursorType);
		if (cursorIndex >= standardCursors_.size())
			return unexpectedError(
				makeError(ErrorCode::WindowOperationUnsupported, ErrorSite::WindowSetCursor));
		if (cursorType == currentCursor_)
			return {};
		glfwGetError(nullptr);
		GLFWcursor* cursor = nullptr;
		if (cursorType != CursorType::Default) {
			auto*& cachedCursor = standardCursors_[cursorIndex];
			if (!cachedCursor)
				cachedCursor = glfwCreateStandardCursor(toGlfwCursorShape(cursorType));
			if (!cachedCursor) {
				const auto status = GlfwRuntime::nativeResult(ErrorSite::WindowSetCursor);
				return status ? unexpectedError(makeError(ErrorCode::WindowOperationFailed,
														  ErrorSite::WindowSetCursor))
							  : status;
			}
			cursor = cachedCursor;
		}
		glfwSetCursor(window_->native(), cursor);
		if (const auto status = GlfwRuntime::nativeResult(ErrorSite::WindowSetCursor); !status)
			return status;
		currentCursor_ = cursorType;
		return {};
	}
	/** Write clipboard text synchronously. */
	[[nodiscard]] Status setClipboardText(std::string_view text) {
		if (const auto status = validate(); !status)
			return status;
		try {
			const std::string terminatedText(text);
			glfwGetError(nullptr);
			glfwSetClipboardString(window_->native(), terminatedText.c_str());
			return GlfwRuntime::nativeResult(ErrorSite::WindowWriteClipboard);
		} catch (const std::bad_alloc&) {
			return unexpectedError(
				makeError(ErrorCode::AllocationFailed, ErrorSite::WindowWriteClipboard));
		}
	}
	/** Read clipboard text; successful empty text is distinct from native failure. */
	[[nodiscard]] Result<std::string> clipboardText() const {
		if (const auto status = validate(); !status)
			return unexpectedError(status.error());
		glfwGetError(nullptr);
		const char* text = glfwGetClipboardString(window_->native());
		if (const auto status = GlfwRuntime::nativeResult(ErrorSite::WindowReadClipboard); !status)
			return unexpectedError(status.error());
		if (!text)
			return unexpectedError(
				makeError(ErrorCode::WindowOperationUnsupported, ErrorSite::WindowReadClipboard));
		try {
			return std::string(text);
		} catch (const std::bad_alloc&) {
			return unexpectedError(
				makeError(ErrorCode::AllocationFailed, ErrorSite::WindowReadClipboard));
		}
	}
	/** Observe synchronous clipboard capability. */
	[[nodiscard]] bool hasClipboardAccess() const noexcept { return ready(); }
	/** Refresh native state without pumping native events again. */
	[[nodiscard]] Status refresh() noexcept {
		if (const auto status = validate(); !status)
			return status;
		if (!sink_)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowPollInput));
		double pointerX = 0.0;
		double pointerY = 0.0;
		auto* native = window_->native();
		glfwGetCursorPos(native, &pointerX, &pointerY);
		emit(
			sink_->pointerMove({.device = 1u,
								.target = target_,
								.x = static_cast<float>(pointerX),
								.y = static_cast<float>(pointerY),
								.inside = glfwGetWindowAttrib(native, GLFW_HOVERED) == GLFW_TRUE}));
		const bool focused = window_->isFocused();
		if (focused != focused_)
			focus(focused);
		if (focused) {
			for (int button = 0; button < static_cast<int>(buttonsDown_.size()); ++button)
				pointerButton(button, glfwGetMouseButton(native, button) == GLFW_PRESS);
		}
		return GlfwRuntime::nativeResult(ErrorSite::WindowPollInput);
	}
	/** Cancel this native source's held contributions. */
	void reset() noexcept {
		keysDown_.fill(false);
		buttonsDown_.fill(false);
		if (sink_)
			emit(sink_->reset({.device = 1u, .target = target_}));
	}

private:
	friend class GlfwEventBinding;
	[[nodiscard]] Status validate() const noexcept {
		if (!window_)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowPollInput));
		return window_->validate();
	}
	static void emit(const Status& status) noexcept {
		if (!status)
			reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
	}
	void key(int nativeKey, bool pressed, std::uint8_t modifierBits) noexcept {
		const auto semanticKey = keyFromCode(nativeKey);
		if (!semanticKey || !sink_)
			return;
		const auto slot = static_cast<std::size_t>(keyCode(*semanticKey));
		if (keysDown_[slot] == pressed)
			return;
		keysDown_[slot] = pressed;
		emit(sink_->key({.device = 1u,
						 .target = target_,
						 .key = *semanticKey,
						 .pressed = pressed,
						 .modifiers = modifierBits}));
	}
	void pointerButton(int nativeButton, bool pressed) noexcept {
		if (!sink_ || nativeButton < 0 ||
			static_cast<std::size_t>(nativeButton) >= buttonsDown_.size())
			return;
		const auto slot = static_cast<std::size_t>(nativeButton);
		if (buttonsDown_[slot] == pressed)
			return;
		buttonsDown_[slot] = pressed;
		emit(sink_->pointerButton({.device = 1u,
								   .target = target_,
								   .button = static_cast<PointerButton>(nativeButton),
								   .pressed = pressed}));
	}
	void focus(bool focused) noexcept {
		focused_ = focused;
		if (sink_)
			emit(sink_->focus({.target = target_, .focused = focused}));
		if (!focused)
			reset();
	}
	std::array<bool, FrameInput::kKeyboardKeyCount> keysDown_{};
	std::array<GLFWcursor*, static_cast<std::size_t>(CursorType::Custom) + 1u> standardCursors_{};
	std::array<bool, FrameInput::kMouseButtonCount> buttonsDown_{};
	WindowInputConfig inputConfig_{};
	GlfwWindow* window_ = nullptr;
	InputSink* sink_ = nullptr;
	WindowId target_ = InvalidWindowId;
	CursorType currentCursor_ = CursorType::Default;
	bool focused_ = true;
};
} // namespace FlowUi::detail::platform
