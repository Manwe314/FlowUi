#pragma once

#include <array>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "IWindow.hpp"
#include "internal/InputQueue.hpp"

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

namespace FlowUi::detail {

inline void glfwErrorCallback(int code, const char* description) {
	std::fprintf(stderr, "[GLFW] (%d) %s\n", code, description ? description : "");
}

inline int toGlfwCursorMode(FlowUi::CursorMode cursorMode) {
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

inline int toGlfwCursorShape(FlowUi::CursorType cursorType) {
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
			return GLFW_ARROW_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Progress:
			return GLFW_ARROW_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Grab:
			return GLFW_HAND_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Grabbing:
			return GLFW_HAND_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Custom:
			return GLFW_ARROW_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Default:
		case FlowUi::CursorType::Arrow:
		default:
			return GLFW_ARROW_CURSOR;
	}
}

struct GlfwLibrary {
	GlfwLibrary() {
		if (refCount++ == 0) {
			glfwSetErrorCallback(glfwErrorCallback);
			if (!glfwInit()) {
				refCount = 0;
				throw FlowUiException(makeError(ErrorCode::PlatformInitializationFailed));
			}
		}
	}

	~GlfwLibrary() {
		if (--refCount == 0) {
			glfwTerminate();
		}
	}

	inline static int refCount = 0;
};

class GlfwWindowBackend final : public IWindowBackend {
public:
	explicit GlfwWindowBackend(const FlowUi::WindowConfig& config, InputQueue* inputQueue)
		: input(inputQueue) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
		glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
		if (config.maximized) {
			glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
		}
#ifdef GLFW_SCALE_TO_MONITOR
		glfwWindowHint(GLFW_SCALE_TO_MONITOR, config.highDPI ? GLFW_TRUE : GLFW_FALSE);
#endif
#ifdef GLFW_COCOA_RETINA_FRAMEBUFFER
		glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, config.highDPI ? GLFW_TRUE : GLFW_FALSE);
#endif

		GLFWmonitor* monitor = nullptr;
		int width = std::max(1, config.width);
		int height = std::max(1, config.height);
		if (config.fullscreen) {
			monitor = glfwGetPrimaryMonitor();
			if (!monitor) {
				throw FlowUiException(makeError(ErrorCode::PlatformMonitorUnavailable));
			}
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			if (mode) {
				width = mode->width;
				height = mode->height;
			}
		}

		window = glfwCreateWindow(width, height, config.title.c_str(), monitor, nullptr);
		if (!window) {
			throw FlowUiException(makeError(ErrorCode::WindowCreationFailed));
		}

		glfwSetWindowUserPointer(window, this);
		installCallbacks();
		setInputConfig(config.input);
		setCursorType(FlowUi::CursorType::Arrow);
	}

	~GlfwWindowBackend() override {
		detachCallbacks();
		destroyStandardCursors();
		if (window) {
			glfwDestroyWindow(window);
			window = nullptr;
		}
	}

	void detachCallbacks() noexcept override {
		input = nullptr;
		if (!window) return;
		glfwSetWindowUserPointer(window, nullptr);
		glfwSetCursorPosCallback(window, nullptr);
		glfwSetMouseButtonCallback(window, nullptr);
		glfwSetScrollCallback(window, nullptr);
		glfwSetKeyCallback(window, nullptr);
		glfwSetCharCallback(window, nullptr);
		glfwSetWindowFocusCallback(window, nullptr);
	}

	void refreshInputSnapshot() override {
		if (!window || !input) {
			return;
		}

		double mouseX = 0.0;
		double mouseY = 0.0;
		glfwGetCursorPos(window, &mouseX, &mouseY);
		input->setMousePos(static_cast<float>(mouseX), static_cast<float>(mouseY));

		for (int mouseButton = 0; mouseButton < static_cast<int>(FrameInput::kMouseButtonCount); ++mouseButton) {
			const int buttonState = glfwGetMouseButton(window, mouseButton);
			input->pushMouseButton(mouseButton, buttonState == GLFW_PRESS);
		}
	}

	bool shouldClose() const override {
		return window ? glfwWindowShouldClose(window) == GLFW_TRUE : true;
	}

	void setShouldClose(int value) override {
		if (window) {
			glfwSetWindowShouldClose(window, value);
		}
	}

	std::vector<const char*> requiredInstanceExtensions() const override {
		uint32_t count = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&count);
		if (!extensions || count == 0) {
			throw FlowUiException(makeError(ErrorCode::PlatformRequiredExtensionMissing));
		}
		return std::vector<const char*>(extensions, extensions + count);
	}

	VkSurfaceKHR createSurface(VkInstance instance) override {
		if (!window) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized));
		}
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		const VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
		if (result != VK_SUCCESS) {
			throw FlowUiException(makeError(
				ErrorCode::PlatformSurfaceCreationFailed,
				ErrorSubjectKind::None,
				0u,
				0u,
				static_cast<std::uint32_t>(result)));
		}
		return surface;
	}

	VkExtent2D windowExtent() const override {
		int width = 0;
		int height = 0;
		glfwGetWindowSize(window, &width, &height);
		VkExtent2D extent{};
		extent.width = static_cast<uint32_t>(std::max(0, width));
		extent.height = static_cast<uint32_t>(std::max(0, height));
		return extent;
	}

	VkExtent2D framebufferExtent() const override {
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D extent{};
		extent.width = static_cast<uint32_t>(std::max(0, width));
		extent.height = static_cast<uint32_t>(std::max(0, height));
		return extent;
	}

	void setTitle(std::string_view title) override {
		if (window) {
			std::string tmp(title);
			glfwSetWindowTitle(window, tmp.c_str());
		}
	}

	void setInputConfig(const FlowUi::WindowInputConfig& config) override {
		inputConfig_ = config;
		if (!window) {
			return;
		}

		glfwSetInputMode(window, GLFW_CURSOR, toGlfwCursorMode(config.cursorMode));
		glfwSetInputMode(window, GLFW_STICKY_KEYS, config.stickyKeys ? GLFW_TRUE : GLFW_FALSE);
		glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, config.stickyMouseButtons ? GLFW_TRUE : GLFW_FALSE);
#ifdef GLFW_LOCK_KEY_MODS
		glfwSetInputMode(window, GLFW_LOCK_KEY_MODS, config.lockKeyMods ? GLFW_TRUE : GLFW_FALSE);
#else
		inputConfig_.lockKeyMods = false;
#endif
#ifdef GLFW_RAW_MOUSE_MOTION
		if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, config.rawMouseMotion ? GLFW_TRUE : GLFW_FALSE);
			inputConfig_.rawMouseMotion = config.rawMouseMotion;
		} else {
			inputConfig_.rawMouseMotion = false;
		}
#else
		inputConfig_.rawMouseMotion = false;
#endif
	}

	FlowUi::WindowInputConfig getInputConfig() const override {
		return inputConfig_;
	}

	void setCursorType(FlowUi::CursorType cursorType) override {
		if (!window || cursorType == currentCursorType_) {
			return;
		}

		if (cursorType == FlowUi::CursorType::Default) {
			glfwSetCursor(window, nullptr);
			currentCursorType_ = cursorType;
			return;
		}

		GLFWcursor* cursorHandle = acquireStandardCursor(cursorType);
		glfwSetCursor(window, cursorHandle);
		currentCursorType_ = cursorType;
	}

	bool supportsRawMouseMotion() const override {
#ifdef GLFW_RAW_MOUSE_MOTION
		return glfwRawMouseMotionSupported() == GLFW_TRUE;
#else
		return false;
#endif
	}

	void setClipboardText(std::string_view text) override {
		if (!window) {
			return;
		}
		std::string copy(text);
		glfwSetClipboardString(window, copy.c_str());
	}

	std::string getClipboardText() const override {
		if (!window) {
			return {};
		}
		const char* clipboard = glfwGetClipboardString(window);
		if (!clipboard) {
			return {};
		}
		return std::string(clipboard);
	}

	void* nativeHandle() const override { return window; }

private:
	GLFWcursor* acquireStandardCursor(FlowUi::CursorType cursorType) {
		const std::size_t cursorIndex = static_cast<std::size_t>(cursorType);
		if (cursorIndex >= standardCursors_.size()) {
			return nullptr;
		}

		GLFWcursor*& cachedCursor = standardCursors_[cursorIndex];
		if (cachedCursor) {
			return cachedCursor;
		}

		cachedCursor = glfwCreateStandardCursor(toGlfwCursorShape(cursorType));
		return cachedCursor;
	}

	void destroyStandardCursors() {
		for (GLFWcursor*& cursor : standardCursors_) {
			if (!cursor) {
				continue;
			}
			glfwDestroyCursor(cursor);
			cursor = nullptr;
		}
	}

	void installCallbacks() {
		glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) noexcept {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->setMousePos(static_cast<float>(x), static_cast<float>(y));
			}
		});

		glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) noexcept {
			(void)mods;
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushMouseButton(button, action != GLFW_RELEASE);
			}
		});

		glfwSetScrollCallback(window, [](GLFWwindow* win, double dx, double dy) noexcept {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushScroll(static_cast<float>(dx), static_cast<float>(dy));
			}
		});

		glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) noexcept {
			(void)scancode;
			(void)mods;
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushKey(key, action != GLFW_RELEASE);
			}
		});

		glfwSetCharCallback(window, [](GLFWwindow* win, unsigned int codepoint) noexcept {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushChar(static_cast<char32_t>(codepoint));
			}
		});

		glfwSetWindowFocusCallback(window, [](GLFWwindow* win, int focused) noexcept {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (!self || !self->input || focused == GLFW_TRUE) {
				return;
			}
			self->input->clearKeyboardState();
			self->input->clearMouseButtonsState();
		});
	}

	GlfwLibrary library;
	GLFWwindow* window = nullptr;
	std::array<GLFWcursor*, static_cast<std::size_t>(FlowUi::CursorType::Custom) + 1u> standardCursors_{};
	FlowUi::CursorType currentCursorType_ = FlowUi::CursorType::Default;
	InputQueue* input = nullptr;
	FlowUi::WindowInputConfig inputConfig_{};
};

inline std::unique_ptr<IWindowBackend> makeDefaultWindowBackend(
	const FlowUi::WindowConfig& config,
	FlowUi::detail::InputQueue* inputQueue)
{
	return std::make_unique<FlowUi::detail::GlfwWindowBackend>(config, inputQueue);
}

inline void pollWindowSystemEvents() {
	glfwPollEvents();
}

} // namespace FlowUi::detail
