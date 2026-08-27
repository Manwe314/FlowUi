#include "Vulkan/Vk_Frames.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

static void vkCheck(VkResult result) {
	if (result != VK_SUCCESS) {
		FlowUi::ErrorCode code = result == VK_ERROR_DEVICE_LOST
			? FlowUi::ErrorCode::VulkanDeviceLost : FlowUi::ErrorCode::VulkanNativeCallFailed;
		if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
			code = FlowUi::ErrorCode::AllocationFailed;
		}
		throw FlowUi::FlowUiException(FlowUi::makeError(
			code, FlowUi::ErrorSite::VulkanFramesInitialize, 0u, 0u,
			static_cast<std::uint32_t>(result)));
	}
}

} // namespace

void FrameVk::create(uint32_t framesInFlight, VulkanContext& vk) {
	if (vk.device == VK_NULL_HANDLE) {
		throw FlowUi::FlowUiException(FlowUi::makeError(
			FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::VulkanFramesInitialize));
	}

	uint32_t frameCount = std::max<uint32_t>(1u, framesInFlight);
	frames.resize(frameCount);
	currentFrame = 0;

	for (auto& frame : frames) {
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = vk.graphicsQFamily;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		vkCheck(vkCreateCommandPool(vk.device, &poolInfo, nullptr, &frame.pool));

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = frame.pool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		vkCheck(vkAllocateCommandBuffers(vk.device, &allocInfo, &frame.cmd));

		VkSemaphoreCreateInfo semInfo{};
		semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		vkCheck(vkCreateSemaphore(vk.device, &semInfo, nullptr, &frame.imageAvailable));

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCheck(vkCreateFence(vk.device, &fenceInfo, nullptr, &frame.inFlight));
	}

}

void FrameVk::destroy(VulkanContext& vk) {
	if (vk.device == VK_NULL_HANDLE) {
		frames.clear();
		currentFrame = 0;
		return;
	}

	for (auto& frame : frames) {
#if FLOW_UI_DEV_MODE
		if (frame.gpuTiming.queryPool != VK_NULL_HANDLE) {
			vkDestroyQueryPool(vk.device, frame.gpuTiming.queryPool, nullptr);
			frame.gpuTiming = {};
		}
#endif
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
	frames.clear();
	currentFrame = 0;
}
