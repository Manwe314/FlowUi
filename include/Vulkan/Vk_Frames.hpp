#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "internal/StorageSystem/StorageTypes.hpp"
#include "Vulkan/Vk_Context.hpp"


struct FrameVk {
	struct Frame {
		VkCommandPool pool = VK_NULL_HANDLE;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VkSemaphore imageAvailable = VK_NULL_HANDLE;
		VkFence inFlight = VK_NULL_HANDLE;
		FlowUi::detail::storage::SubmissionToken storageSubmission{};
	};

	std::vector<Frame> frames;
	uint32_t currentFrame = 0;

	void create(uint32_t framesInFlight, VulkanContext& vk);
	void destroy(VulkanContext& vk);

	Frame& getCurrentFrame() { return frames[currentFrame]; }
	void advance() { currentFrame = (currentFrame + 1) % (uint32_t)frames.size(); }
};
