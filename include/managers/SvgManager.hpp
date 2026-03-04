#pragma once

#include <string_view>

#include "Vulkan/Vk_Context.hpp"


struct IconManager {
	
	void init(VulkanContext& vk, uint32_t atlasSize);
	int loadSvg(VulkanContext& vk, std::string_view path, int px);
	void destroy(VulkanContext& vk);
};
