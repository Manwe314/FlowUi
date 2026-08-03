#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "window/IWindow.hpp"

struct VmaAllocator_T;

struct VulkanContext {
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	VkPhysicalDevice phys = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator_T* allocator = nullptr;

	uint32_t graphicsQFamily = UINT32_MAX;
	uint32_t presentQFamily  = UINT32_MAX;
	VkQueue graphicsQ = VK_NULL_HANDLE;
	VkQueue presentQ  = VK_NULL_HANDLE;

	void createInstance(const FlowUi::AppConfig& config, const std::vector<const char*>& requiredExts);

	[[nodiscard]] VkSurfaceKHR createSurface(FlowUi::detail::IWindowBackend& window) const;

	void pickPhysicalDevice(const FlowUi::AppConfig& config, VkSurfaceKHR mainSurface);
	void createDevice(const FlowUi::AppConfig& config);
	[[nodiscard]] bool supportsPresentation(VkSurfaceKHR surface) const noexcept;

	void destroy();
};
