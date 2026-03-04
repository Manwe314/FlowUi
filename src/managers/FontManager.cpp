#include "managers/FontManager.hpp"

void FontManager::init(VulkanContext& vk, uint32_t atlasSize) {
	(void)vk;
	(void)atlasSize;
}

int FontManager::loadFont(VulkanContext& vk, std::string_view path, float px) {
	(void)vk;
	(void)path;
	(void)px;
	return -1;
}

void FontManager::destroy(VulkanContext& vk) {
	(void)vk;
}
