#include "managers/SvgManager.hpp"

void IconManager::init(VulkanContext& vk, uint32_t atlasSize) {
	(void)vk;
	(void)atlasSize;
}

int IconManager::loadSvg(VulkanContext& vk, std::string_view path, int px) {
	(void)vk;
	(void)path;
	(void)px;
	return -1;
}

void IconManager::destroy(VulkanContext& vk) {
	(void)vk;
}
