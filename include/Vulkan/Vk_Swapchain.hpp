#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"
#include "Vulkan/Vk_Context.hpp"


struct Swapchain {
	Swapchain() = default;
	Swapchain(const Swapchain&) = delete;
	Swapchain& operator=(const Swapchain&) = delete;
	Swapchain(Swapchain&& other) noexcept;
	Swapchain& operator=(Swapchain&& other) noexcept;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D extent{};

	std::vector<VkImage> images;
	std::vector<VkImageView> views;

	void create(
		const FlowUi::WindowConfig& windowConfig,
		const FlowUi::VulkanConfig& vulkanConfig,
		VulkanContext& vk,
		VkSurfaceKHR surface,
		VkExtent2D preferredExtent = {},
		VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
	void destroy(VulkanContext& vk);
};

struct SwapchainGeneration {
	SwapchainGeneration() = default;
	SwapchainGeneration(const SwapchainGeneration&) = delete;
	SwapchainGeneration& operator=(const SwapchainGeneration&) = delete;
	SwapchainGeneration(SwapchainGeneration&& other) noexcept;
	SwapchainGeneration& operator=(SwapchainGeneration&& other) noexcept;

	Swapchain swapchain{};
	std::vector<VkImageLayout> layouts{};
	std::vector<VkFence> imageInFlight{};
	std::vector<VkSemaphore> renderFinished{};
	std::vector<VkFence> presentComplete{};
	std::vector<uint8_t> presentPending{};
	uint64_t lastPresentId = 0;
	FlowUi::detail::storage::SubmissionSerial lastGraphicsUse = 0;

	void create(
		const FlowUi::WindowConfig& windowConfig,
		const FlowUi::VulkanConfig& vulkanConfig,
		VulkanContext& vk,
		VkSurfaceKHR surface,
		VkExtent2D preferredExtent = {},
		VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
	void waitForPresentCompletion(VulkanContext& vk) const;
	void destroy(VulkanContext& vk);
};
