#pragma once

#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "flowui/PublicStructs.hpp"
#include "IWindow.hpp"
#include "Inputs.hpp"

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

namespace FlowUi::detail {

inline void glfwErrorCallback(int code, const char* description) {
	std::fprintf(stderr, "[GLFW] (%d) %s\n", code, description ? description : "");
}

struct GlfwLibrary {
	GlfwLibrary() {
		if (refCount++ == 0) {
			glfwSetErrorCallback(glfwErrorCallback);
			if (!glfwInit()) {
				refCount = 0;
				throw std::runtime_error("Failed to initialize GLFW.");
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
				throw std::runtime_error("Failed to acquire primary monitor for fullscreen mode.");
			}
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			if (mode) {
				width = mode->width;
				height = mode->height;
			}
		}

		window = glfwCreateWindow(width, height, config.title.c_str(), monitor, nullptr);
		if (!window) {
			throw std::runtime_error("Failed to create GLFW window.");
		}

		glfwSetWindowUserPointer(window, this);
		installCallbacks();
	}

	~GlfwWindowBackend() override {
		if (window) {
			glfwDestroyWindow(window);
			window = nullptr;
		}
	}

	void pollEvents() override {
		glfwPollEvents();
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

		const int maxTrackedKeyCode = std::min(
			GLFW_KEY_LAST,
			static_cast<int>(FrameInput::kKeyboardKeyCount) - 1);
		for (int keyCode = 0; keyCode <= maxTrackedKeyCode; ++keyCode) {
			const int keyState = glfwGetKey(window, keyCode);
			const bool isKeyDown = (keyState == GLFW_PRESS || keyState == GLFW_REPEAT);
			input->pushKey(keyCode, isKeyDown);
		}
	}

	bool shouldClose() const override {
		return window ? glfwWindowShouldClose(window) == GLFW_TRUE : true;
	}

	std::vector<const char*> requiredInstanceExtensions() const override {
		uint32_t count = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&count);
		if (!extensions || count == 0) {
			throw std::runtime_error("GLFW did not provide required Vulkan extensions.");
		}
		return std::vector<const char*>(extensions, extensions + count);
	}

	VkSurfaceKHR createSurface(VkInstance instance) override {
		if (!window) {
			throw std::runtime_error("GLFW window not initialized.");
		}
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Vulkan surface via GLFW.");
		}
		return surface;
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

	void* nativeHandle() const override { return window; }

private:
	void installCallbacks() {
		glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->setMousePos(static_cast<float>(x), static_cast<float>(y));
			}
		});

		glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
			(void)mods;
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushMouseButton(button, action != GLFW_RELEASE);
			}
		});

		glfwSetScrollCallback(window, [](GLFWwindow* win, double dx, double dy) {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushScroll(static_cast<float>(dx), static_cast<float>(dy));
			}
		});

		glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
			(void)scancode;
			(void)mods;
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushKey(key, action != GLFW_RELEASE);
			}
		});

		glfwSetCharCallback(window, [](GLFWwindow* win, unsigned int codepoint) {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushChar(static_cast<char32_t>(codepoint));
			}
		});
	}

	GlfwLibrary library;
	GLFWwindow* window = nullptr;
	InputQueue* input = nullptr;
};

} // namespace FlowUi::detail

inline std::unique_ptr<IWindowBackend> makeDefaultWindowBackend( const FlowUi::WindowConfig& config, InputQueue* inputQueue)
{
	return std::make_unique<FlowUi::detail::GlfwWindowBackend>(config, inputQueue);
}
