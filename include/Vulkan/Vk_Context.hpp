#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "window/IWindow.hpp"

#ifndef VK_EXT_swapchain_maintenance1
#define VK_EXT_swapchain_maintenance1 1
#define VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME "VK_EXT_swapchain_maintenance1"
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT \
	static_cast<VkStructureType>(1000275000)
#define VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT static_cast<VkStructureType>(1000275001)
typedef struct VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT {
	VkStructureType sType;
	void* pNext;
	VkBool32 swapchainMaintenance1;
} VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT;
typedef struct VkSwapchainPresentFenceInfoEXT {
	VkStructureType sType;
	const void* pNext;
	const VkFence* pFences;
	uint32_t swapchainCount;
} VkSwapchainPresentFenceInfoEXT;
#endif

struct VmaAllocator_T;

enum class WsiRetirementMode : uint8_t {
	LegacyDeviceIdle = 0,
	PresentFence,
	PresentWait,
};

struct VulkanContext {
#if FLOW_UI_DEV_MODE
#endif

	void createInstance(const FlowUi::AppConfig& config, const std::vector<const char*>& requiredExts);

	[[nodiscard]] VkSurfaceKHR createSurface(FlowUi::detail::IWindowBackend& window) const;

	void pickPhysicalDevice(const FlowUi::AppConfig& config, VkSurfaceKHR mainSurface);
	void createDevice(const FlowUi::AppConfig& config);
	[[nodiscard]] bool supportsPresentation(VkSurfaceKHR surface) const noexcept;
	[[nodiscard]] bool supportsExactPresentCompletion() const noexcept {
		return wsiRetirementMode != WsiRetirementMode::LegacyDeviceIdle;
	}
	VkResult waitForPresent(VkSwapchainKHR swapchain, uint64_t presentId, uint64_t timeout) const noexcept;

	void destroy();
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	VkPhysicalDevice phys = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator_T* allocator = nullptr;
	VkQueue graphicsQ = VK_NULL_HANDLE;
	VkQueue presentQ  = VK_NULL_HANDLE;
	PFN_vkWaitForPresentKHR waitForPresentKHR = nullptr;
	PFN_vkGetCalibratedTimestampsEXT getCalibratedTimestampsEXT = nullptr;

	uint32_t graphicsQFamily = UINT32_MAX;
	uint32_t presentQFamily  = UINT32_MAX;
	WsiRetirementMode wsiRetirementMode = WsiRetirementMode::LegacyDeviceIdle;
	bool surfaceMaintenanceEnabled = false;
	bool devGpuTimingRequested = true;
	bool devGpuMemoryRequested = true;
	bool memoryBudgetEnabled = false;
	bool synchronization2Enabled = false;
	bool calibratedTimestampsEnabled = false;
};
