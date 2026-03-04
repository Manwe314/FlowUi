#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "flowui/PublicStructs.hpp"
#include "Vulkan/Vk_Context.hpp"


struct Swapchain {
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D extent{};

	std::vector<VkImage> images;
	std::vector<VkImageView> views;

	void create(const FlowUi::AppConfig& config, VulkanContext& vk);
	void destroy(VulkanContext& vk);

	void recreate(const FlowUi::AppConfig& config, VulkanContext& vk) {
		destroy(vk);
		create(config, vk);
	}
};
