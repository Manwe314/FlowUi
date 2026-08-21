#define VMA_IMPLEMENTATION
#include "internal/Vma.hpp"

#include "Vulkan/Vk_Context.hpp"

void VulkanContext::destroy() {
	if (device != VK_NULL_HANDLE) {
		if (allocator) {
			vmaDestroyAllocator(allocator);
			allocator = nullptr;
		}
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}
	if (instance != VK_NULL_HANDLE) {
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
	phys = VK_NULL_HANDLE;
	graphicsQFamily = UINT32_MAX;
	presentQFamily = UINT32_MAX;
	graphicsQ = VK_NULL_HANDLE;
	presentQ = VK_NULL_HANDLE;
}
