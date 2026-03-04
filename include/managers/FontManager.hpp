#pragma once

#include <string_view>

#include "Vulkan/Vk_Context.hpp"


struct FontManager {

	void init(VulkanContext& vk, uint32_t atlasSize);
	int loadFont(VulkanContext& vk, std::string_view path, float px);
	void destroy(VulkanContext& vk);
};
