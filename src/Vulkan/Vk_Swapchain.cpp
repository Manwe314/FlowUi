#include "Vulkan/Vk_Swapchain.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

static void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats, bool preferSrgb) {
	if (formats.empty()) {
		throw std::runtime_error("No surface formats available.");
	}

	const VkColorSpaceKHR desiredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
		VkFormat desiredFormat = preferSrgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
		return { desiredFormat, desiredColorSpace };
	}

	if (preferSrgb) {
		for (const auto& format : formats) {
			if ((format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_SRGB) &&
				format.colorSpace == desiredColorSpace) {
				return format;
			}
		}
	} else {
		for (const auto& format : formats) {
			if ((format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
				format.colorSpace == desiredColorSpace) {
				return format;
			}
		}
	}

	return formats[0];
}

static VkPresentModeKHR choosePresentMode(
	const std::vector<VkPresentModeKHR>& modes,
	FlowUi::PresentMode requested) {
	VkPresentModeKHR desired = VK_PRESENT_MODE_FIFO_KHR;

	switch (requested) {
		case FlowUi::PresentMode::Mailbox:
			desired = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		case FlowUi::PresentMode::Immediate:
			desired = VK_PRESENT_MODE_IMMEDIATE_KHR;
			break;
		case FlowUi::PresentMode::Fifo:
		default:
			desired = VK_PRESENT_MODE_FIFO_KHR;
			break;
	}

	for (auto mode : modes) {
		if (mode == desired) {
			return desired;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, const FlowUi::AppConfig& config) {
	if (caps.currentExtent.width != UINT32_MAX) {
		return caps.currentExtent;
	}

	uint32_t width = static_cast<uint32_t>(std::max(1, config.window.width));
	uint32_t height = static_cast<uint32_t>(std::max(1, config.window.height));

	VkExtent2D extent{};
	extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
	extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
	return extent;
}

static VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) {
	const VkCompositeAlphaFlagBitsKHR options[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for (auto option : options) {
		if (supported & option) {
			return option;
		}
	}

	return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

} // namespace

void Swapchain::create(const FlowUi::AppConfig& config, VulkanContext& vk) {
	if (vk.device == VK_NULL_HANDLE || vk.phys == VK_NULL_HANDLE || vk.surface == VK_NULL_HANDLE) {
		throw std::runtime_error("Swapchain creation requires valid Vulkan device, physical device, and surface.");
	}

	VkSurfaceCapabilitiesKHR caps{};
	vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.phys, vk.surface, &caps),
		"Failed to query surface capabilities.");

	uint32_t formatCount = 0;
	vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.phys, vk.surface, &formatCount, nullptr),
		"Failed to query surface formats.");
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	if (formatCount > 0) {
		vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.phys, vk.surface, &formatCount, formats.data()),
			"Failed to query surface formats.");
	}

	uint32_t presentCount = 0;
	vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.phys, vk.surface, &presentCount, nullptr),
		"Failed to query present modes.");
	std::vector<VkPresentModeKHR> presentModes(presentCount);
	if (presentCount > 0) {
		vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.phys, vk.surface, &presentCount, presentModes.data()),
			"Failed to query present modes.");
	}

	if (formats.empty() || presentModes.empty()) {
		throw std::runtime_error("Surface does not support required formats or present modes.");
	}

	VkSurfaceFormatKHR chosenFormat = chooseSurfaceFormat(formats, config.vk.srgbBackbuffer);
	VkPresentModeKHR presentMode = choosePresentMode(presentModes, config.vk.presentMode);
	VkExtent2D chosenExtent = chooseExtent(caps, config);

	uint32_t imageCount = caps.minImageCount + 1;
	uint32_t minDesired = std::max<uint32_t>(1u, config.vk.framesInFlight);
	if (imageCount < minDesired) {
		imageCount = minDesired;
	}
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
		imageCount = caps.maxImageCount;
	}

	uint32_t queueFamilyIndices[] = { vk.graphicsQFamily, vk.presentQFamily };
	const bool useConcurrent = vk.graphicsQFamily != vk.presentQFamily;

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = vk.surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = chosenFormat.format;
	createInfo.imageColorSpace = chosenFormat.colorSpace;
	createInfo.imageExtent = chosenExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	createInfo.imageSharingMode = useConcurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = useConcurrent ? 2u : 0u;
	createInfo.pQueueFamilyIndices = useConcurrent ? queueFamilyIndices : nullptr;
	createInfo.preTransform = caps.currentTransform;
	createInfo.compositeAlpha = chooseCompositeAlpha(caps.supportedCompositeAlpha);
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	vkCheck(vkCreateSwapchainKHR(vk.device, &createInfo, nullptr, &swapchain),
		"Failed to create swapchain.");

	uint32_t swapImageCount = 0;
	vkCheck(vkGetSwapchainImagesKHR(vk.device, swapchain, &swapImageCount, nullptr),
		"Failed to query swapchain images.");
	images.resize(swapImageCount);
	vkCheck(vkGetSwapchainImagesKHR(vk.device, swapchain, &swapImageCount, images.data()),
		"Failed to query swapchain images.");

	views.resize(images.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < images.size(); ++i) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = images[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = chosenFormat.format;
		viewInfo.components = {
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY
		};
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		vkCheck(vkCreateImageView(vk.device, &viewInfo, nullptr, &views[i]),
			"Failed to create swapchain image view.");
	}

	format = chosenFormat.format;
	colorSpace = chosenFormat.colorSpace;
	extent = chosenExtent;
}

void Swapchain::destroy(VulkanContext& vk) {
	if (vk.device == VK_NULL_HANDLE) {
		views.clear();
		images.clear();
		swapchain = VK_NULL_HANDLE;
		format = VK_FORMAT_UNDEFINED;
		colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		extent = {};
		return;
	}

	for (auto& view : views) {
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(vk.device, view, nullptr);
			view = VK_NULL_HANDLE;
		}
	}
	views.clear();
	images.clear();

	if (swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(vk.device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}

	format = VK_FORMAT_UNDEFINED;
	colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	extent = {};
}
