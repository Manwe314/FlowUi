#include "Vulkan/Vk_Frames.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

static void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

} // namespace

void FrameVk::create(const FlowUi::AppConfig& config, VulkanContext& vk, size_t swapImageCount) {
	if (vk.device == VK_NULL_HANDLE) {
		throw std::runtime_error("Vulkan device must be created before frame resources.");
	}

	uint32_t frameCount = std::max<uint32_t>(1u, config.vk.framesInFlight);
	frames.resize(frameCount);
	currentFrame = 0;

	imageInFlight.assign(swapImageCount, VK_NULL_HANDLE);
	renderFinishedBySwapImage.assign(swapImageCount, VK_NULL_HANDLE);

	for (auto& frame : frames) {
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = vk.graphicsQFamily;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		vkCheck(vkCreateCommandPool(vk.device, &poolInfo, nullptr, &frame.pool),
			"Failed to create command pool.");

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = frame.pool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		vkCheck(vkAllocateCommandBuffers(vk.device, &allocInfo, &frame.cmd),
			"Failed to allocate command buffer.");

		VkSemaphoreCreateInfo semInfo{};
		semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		vkCheck(vkCreateSemaphore(vk.device, &semInfo, nullptr, &frame.imageAvailable),
			"Failed to create image available semaphore.");

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCheck(vkCreateFence(vk.device, &fenceInfo, nullptr, &frame.inFlight),
			"Failed to create in-flight fence.");
	}

	VkSemaphoreCreateInfo semInfo{};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (VkSemaphore& semaphore : renderFinishedBySwapImage) {
		vkCheck(vkCreateSemaphore(vk.device, &semInfo, nullptr, &semaphore),
			"Failed to create per-swapchain-image render finished semaphore.");
	}
}

void FrameVk::destroy(VulkanContext& vk) {
	if (vk.device == VK_NULL_HANDLE) {
		frames.clear();
		imageInFlight.clear();
		renderFinishedBySwapImage.clear();
		currentFrame = 0;
		return;
	}

	for (auto& frame : frames) {
		if (frame.inFlight != VK_NULL_HANDLE) {
			vkDestroyFence(vk.device, frame.inFlight, nullptr);
			frame.inFlight = VK_NULL_HANDLE;
		}
		if (frame.imageAvailable != VK_NULL_HANDLE) {
			vkDestroySemaphore(vk.device, frame.imageAvailable, nullptr);
			frame.imageAvailable = VK_NULL_HANDLE;
		}
		if (frame.pool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(vk.device, frame.pool, nullptr);
			frame.pool = VK_NULL_HANDLE;
			frame.cmd = VK_NULL_HANDLE;
		}
	}
	for (VkSemaphore& semaphore : renderFinishedBySwapImage) {
		if (semaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(vk.device, semaphore, nullptr);
			semaphore = VK_NULL_HANDLE;
		}
	}

	frames.clear();
	imageInFlight.clear();
	renderFinishedBySwapImage.clear();
	currentFrame = 0;
}

void FrameVk::onSwapchainRecreated(VulkanContext& vk, size_t newSwapImageCount) {
	imageInFlight.assign(newSwapImageCount, VK_NULL_HANDLE);

	if (vk.device == VK_NULL_HANDLE) {
		renderFinishedBySwapImage.assign(newSwapImageCount, VK_NULL_HANDLE);
		return;
	}

	for (VkSemaphore& semaphore : renderFinishedBySwapImage) {
		if (semaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(vk.device, semaphore, nullptr);
			semaphore = VK_NULL_HANDLE;
		}
	}

	renderFinishedBySwapImage.assign(newSwapImageCount, VK_NULL_HANDLE);
	VkSemaphoreCreateInfo semInfo{};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (VkSemaphore& semaphore : renderFinishedBySwapImage) {
		vkCheck(vkCreateSemaphore(vk.device, &semInfo, nullptr, &semaphore),
			"Failed to recreate per-swapchain-image render finished semaphore.");
	}
}
