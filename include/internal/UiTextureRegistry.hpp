#pragma once

#include <cstdint>
#include <string_view>

#include <vulkan/vulkan.h>

struct VulkanContext;

namespace FlowUi::detail {

struct IUiTextureRegistry {
	virtual ~IUiTextureRegistry() = default;

	virtual uint32_t registerOrReplaceSlot(
		VulkanContext& vk,
		std::string_view namespacedKey,
		VkImageView imageView,
		VkSampler sampler,
		bool& inserted) = 0;

	virtual bool updateSlotBinding(
		std::string_view namespacedKey,
		VkImageView imageView,
		VkSampler sampler) = 0;

	virtual bool removeSlot(std::string_view namespacedKey) = 0;
	virtual bool containsSlot(std::string_view namespacedKey) const = 0;
};

} // namespace FlowUi::detail
