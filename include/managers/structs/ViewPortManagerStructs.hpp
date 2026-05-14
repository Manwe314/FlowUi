#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOWUI_PUBLIC_VULKAN_INTEROP || defined(FLOWUI_INTERNAL_VIEWPORT_MANAGER)

#include <array>
#include <cstdint>
#include <string_view>

#include <vulkan/vulkan.h>

struct VmaAllocator_T;

namespace FlowUi {

/** @addtogroup flowui_viewport_manager
 * @{
 */

/**
 * @brief Vulkan handles exposed to viewport render callbacks.
 *
 * ViewPortVulkanInterop provides shared application Vulkan objects for custom
 * viewport render code. 
 * @warning The handles are owned by FlowUi's App runtime and must
 * not be destroyed by viewport callbacks.
 */
struct ViewPortVulkanInterop {
	/** @brief Vulkan instance owned by the FlowUi app. */
	VkInstance instance = VK_NULL_HANDLE;

	/** @brief Selected physical device used by FlowUi. */
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	/** @brief Logical device used for viewport rendering. */
	VkDevice device = VK_NULL_HANDLE;

	/** @brief VMA allocator shared with FlowUi GPU resources. */
	VmaAllocator_T* allocator = nullptr;

	/** @brief Graphics queue used by FlowUi rendering. */
	VkQueue graphicsQueue = VK_NULL_HANDLE;

	/** @brief Graphics queue family index for command pool or resource setup. */
	uint32_t graphicsQueueFamily = UINT32_MAX;

	/** @brief Number of viewport frame resources FlowUi keeps in flight. */
	uint32_t framesInFlight = 1u;
};

/**
 * @brief Per-frame context passed to a viewport render callback.
 *
 * The callback records draw commands into commandBuffer for the current
 * viewport image. FlowUi owns the render pass setup, image transitions, and
 * final sampling layout; user code records the viewport contents.
 */
struct ViewPortRenderContext {
	/**
	 * @brief Secondary command buffer for viewport rendering.
	 *
	 * The command buffer is already begun before the callback is invoked and is
	 * ended by FlowUi after the callback returns.
	 */
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

	/** @brief Current viewport render target extent in pixels. */
	VkExtent2D extent{};

	/** @brief Color format of the viewport render target. */
	VkFormat colorFormat = VK_FORMAT_UNDEFINED;

	/** @brief Frame-resource index for this callback invocation. */
	uint32_t frameIndex = 0u;

	/** @brief Application-defined viewport key. */
	std::string_view key{};

	/** @brief Shared Vulkan interop handles, or nullptr if unavailable. */
	const ViewPortVulkanInterop* vulkan = nullptr;
};

/**
 * @brief Creation settings for a viewport render target.
 *
 * ViewPortCreateInfo controls the image format and clear behavior for a
 * viewport. The actual target size is selected by FlowUi from the UI image area
 * where the viewport texture is used.
 */
struct ViewPortCreateInfo {
	/** @brief Color format for the viewport render target image. */
	VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;

	/** @brief Clear color used by the viewport pass. */
	std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

	/**
	 * @brief Whether the viewport should be cleared every frame.
	 *
	 * When false, FlowUi loads the previous contents after the image has been
	 * initialized, allowing persistent viewport rendering patterns.
	 */
	bool clearEveryFrame = true;
};

/** @} */

} // namespace FlowUi

#endif
