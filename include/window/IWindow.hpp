#pragma once

#include <vector>
#include <string_view>
#include <vulkan/vulkan.h>


struct IWindowBackend {
	virtual ~IWindowBackend() = default;

	virtual void pollEvents() = 0;
	virtual bool shouldClose() const = 0;

	virtual std::vector<const char*> requiredInstanceExtensions() const = 0;
	virtual VkSurfaceKHR createSurface(VkInstance instance) = 0;

	virtual VkExtent2D framebufferExtent() const = 0;
	virtual void setTitle(std::string_view title) = 0;

	virtual void* nativeHandle() const = 0;
};
