#pragma once

#include <vulkan/vulkan.h>

#include "FlowUi/Private/storageSystem/StorageTypes.hpp"

namespace FlowUi::detail::storage {

[[nodiscard]] constexpr VkFormat toVkFormat(PixelFormat format) noexcept {
	switch (format) {
	case PixelFormat::R8Unorm:
		return VK_FORMAT_R8_UNORM;
	case PixelFormat::Rgba8Unorm:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case PixelFormat::Rgba8Srgb:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case PixelFormat::Bgra8Unorm:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case PixelFormat::Bgra8Srgb:
		return VK_FORMAT_B8G8R8A8_SRGB;
	case PixelFormat::Rgba16Float:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case PixelFormat::D32Float:
		return VK_FORMAT_D32_SFLOAT;
	default:
		return VK_FORMAT_UNDEFINED;
	}
}

[[nodiscard]] constexpr VkImageUsageFlags toVkImageUsage(ImageUsage usage) noexcept {
	VkImageUsageFlags flags = 0;
	if (hasFlag(usage, ImageUsage::Sampled))
		flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (hasFlag(usage, ImageUsage::TransferSource))
		flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (hasFlag(usage, ImageUsage::TransferDestination))
		flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (hasFlag(usage, ImageUsage::ColorAttachment))
		flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (hasFlag(usage, ImageUsage::DepthAttachment))
		flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if (hasFlag(usage, ImageUsage::Storage))
		flags |= VK_IMAGE_USAGE_STORAGE_BIT;
	return flags;
}

[[nodiscard]] constexpr VkBufferUsageFlags toVkBufferUsage(BufferUsage usage) noexcept {
	VkBufferUsageFlags flags = 0;
	if (hasFlag(usage, BufferUsage::TransferSource))
		flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (hasFlag(usage, BufferUsage::TransferDestination))
		flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (hasFlag(usage, BufferUsage::Vertex))
		flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (hasFlag(usage, BufferUsage::Index))
		flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (hasFlag(usage, BufferUsage::Uniform))
		flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (hasFlag(usage, BufferUsage::Storage))
		flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (hasFlag(usage, BufferUsage::Indirect))
		flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	return flags;
}

[[nodiscard]] constexpr VkFilter toVkFilter(FilterMode mode) noexcept {
	return mode == FilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

[[nodiscard]] constexpr VkSamplerAddressMode toVkAddressMode(AddressMode mode) noexcept {
	switch (mode) {
	case AddressMode::Repeat:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case AddressMode::MirroredRepeat:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case AddressMode::ClampToBorder:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	default:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	}
}

} // namespace FlowUi::detail::storage
