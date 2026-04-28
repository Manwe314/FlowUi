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

/** @addtogroup flowui_image_manager
 * @{
 */

/** @brief Loads image files and exposes them as FlowUi texture references. */
class ImageManager {
public:
	/** @brief Load and register an image from disk under a caller-provided key. */
	bool registerImage(std::string_view key, std::string_view filePath);
	/** @brief Remove a registered image by key. */
	bool removeImage(std::string_view key);
	/** @brief Return true if an image key is registered. */
	bool contains(std::string_view key) const;
	/** @brief Return the texture reference for a registered image key. */
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

/** @} */

} // namespace FlowUi
