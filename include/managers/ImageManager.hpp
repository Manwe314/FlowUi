#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "FlowUi/PublicStructs.hpp"

struct VmaAllocation_T;
struct VulkanContext;
struct VulkanUiRenderer;
struct VkImage_T;
struct VkImageView_T;
struct VkSampler_T;
struct VkCommandPool_T;

using VkImage = VkImage_T*;
using VkImageView = VkImageView_T*;
using VkSampler = VkSampler_T*;
using VkCommandPool = VkCommandPool_T*;

namespace FlowUi {

namespace detail {
struct IUiTextureRegistry;

} // namespace detail

class App;

class ImageManager {
public:
	bool registerImage(std::string_view key, std::string_view filePath);
	bool removeImage(std::string_view key);
	bool contains(std::string_view key) const;
	TextureRef getTexture(std::string_view key) const;

private:
	friend class App;

	void setRegistry(detail::IUiTextureRegistry* registry);
	void init(VulkanContext& vk, VulkanUiRenderer& renderer, uint32_t framesInFlight);
	void onFrameStart(VulkanContext& vk, uint32_t frameIndex);
	void destroy(VulkanContext& vk);

	struct ImageResource {
		VkImage image = nullptr;
		VmaAllocation_T* allocation = nullptr;
		VkImageView view = nullptr;
		VkSampler sampler = nullptr;
	};

	struct ImageRecord {
		ImageResource resource{};
		uint32_t slotId = 0;
		int32_t sourceWidth = 0;
		int32_t sourceHeight = 0;
		std::filesystem::path filePath{};
	};

	ImageResource createImageResource(VulkanContext& vk, const uint8_t* rgbaPixels, uint32_t width, uint32_t height);
	void destroyImageResource(VulkanContext& vk, ImageResource& resource);
	void enqueueRetiredResource(ImageResource&& resource);
	std::string makeNamespacedKey(std::string_view key) const;

	detail::IUiTextureRegistry* registry_ = nullptr;
	VulkanContext* vk_ = nullptr;
	VulkanUiRenderer* renderer_ = nullptr;
	VkCommandPool uploadCommandPool_ = nullptr;
	uint32_t framesInFlight_ = 1u;
	uint32_t currentFrameIndex_ = 0u;

	std::unordered_map<std::string, ImageRecord> imagesByKey_;
	mutable std::unordered_set<std::string> missingTextureWarnings_;
	std::vector<std::vector<ImageResource>> retiredResourcesByFrame_;
};

} // namespace FlowUi
