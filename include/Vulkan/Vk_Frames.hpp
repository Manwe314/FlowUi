#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "Vulkan/Vk_Context.hpp"


struct FrameVk {
	struct Frame {
		VkCommandPool pool = VK_NULL_HANDLE;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VkSemaphore imageAvailable = VK_NULL_HANDLE;
		VkFence inFlight = VK_NULL_HANDLE;
	};

	std::vector<Frame> frames;
	uint32_t currentFrame = 0;

	// track swapchain images (size = swap.images.size())
	std::vector<VkFence> imageInFlight;
	std::vector<VkSemaphore> renderFinishedBySwapImage;

	void create(const FlowUi::AppConfig& config, VulkanContext& vk, size_t swapImageCount);
	void destroy(VulkanContext& vk);

	Frame& getCurrentFrame() { return frames[currentFrame]; }
	void advance() { currentFrame = (currentFrame + 1) % (uint32_t)frames.size(); }

	void onSwapchainRecreated(VulkanContext& vk, size_t newSwapImageCount);
};
