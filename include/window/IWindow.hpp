#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <vulkan/vulkan.h>

#include "FlowUi/PublicStructs.hpp"


struct IWindowBackend {
	virtual ~IWindowBackend() = default;

	virtual void pollEvents() = 0;
	virtual bool shouldClose() const = 0;

	virtual std::vector<const char*> requiredInstanceExtensions() const = 0;
	virtual VkSurfaceKHR createSurface(VkInstance instance) = 0;

	virtual VkExtent2D windowExtent() const = 0;
	virtual VkExtent2D framebufferExtent() const = 0;
	virtual void setTitle(std::string_view title) = 0;
	virtual void setInputConfig(const FlowUi::WindowInputConfig& config) = 0;
	virtual FlowUi::WindowInputConfig getInputConfig() const = 0;
	virtual void setCursorType(FlowUi::CursorType cursorType) = 0;
	virtual bool supportsRawMouseMotion() const = 0;
	virtual void setClipboardText(std::string_view text) = 0;
	virtual std::string getClipboardText() const = 0;

	virtual void* nativeHandle() const = 0;
};
