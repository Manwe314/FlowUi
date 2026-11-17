#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

struct VmaAllocation_T;

namespace FlowUi::Font {

/**
 * @brief Vulkan resources for the font atlas array.
 *
 * FlowUi::FontManager::getAtlasResource() returns this object through
 * `Result<std::reference_wrapper<const AtlasArrayResource>>`. Check the Result, then
 * bind const AtlasArrayResource& atlas = result->get() for convenient field access.
 * Include this header to read fields; FontManager.hpp only forward-declares the type.
 * All handles are borrowed from the font manager; do not destroy them. Atlas
 * growth may replace binding resources, so re-read them when bindingRevision
 * changes and do not retain handles beyond the owning App lifetime.
 */
struct AtlasArrayResource {
	/** @brief Vulkan image handle. */
	VkImage image = VK_NULL_HANDLE;
	/** @brief VMA allocation handle. */
	VmaAllocation_T* allocation = nullptr;
	/** @brief Vulkan image view handle. */
	VkImageView view = VK_NULL_HANDLE;
	/** @brief Vulkan sampler handle. */
	VkSampler sampler = VK_NULL_HANDLE;
	/** @brief Atlas width in pixels. */
	uint32_t width = 0;
	/** @brief Atlas height in pixels. */
	uint32_t height = 0;
	/** @brief Number of atlas layers currently used. */
	uint32_t layersUsed = 0;
	/** @brief Number of atlas layers currently allocated. */
	uint32_t layersCapacity = 0;
	/** @brief Revision incremented when atlas binding data changes. */
	uint32_t bindingRevision = 0;
};

} // namespace FlowUi::Font
