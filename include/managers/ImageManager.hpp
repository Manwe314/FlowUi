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

/**
 * @brief Loads image files and exposes them as FlowUi texture references.
 *
 * ImageManager uploads decoded image files as standalone Vulkan textures and
 * registers them under application-defined keys. The returned TextureRef can be
 * passed to FlowUi image drawing code and adjusted with TextureRef options such
 * as fitMode and tintEnabled.
 *
 * @code{.cpp}
 * FlowUi::ImageManager& images = app.images();
 * (void)images.registerImage("hero/logo", "assets/images/logo.png");
 *
 * FlowUi::TextureRef logo = images.getTexture("hero/logo");
 * logo.fitMode = FlowUi::TextureFitMode::Contain;
 * @endcode
 *
 * @see @ref md_docs_2tutorials_2images__icons__textures "Images, Icons, and Texture References"
 * @see @ref md_docs_2concepts_2managers "Managers"
 */
class ImageManager {
public:
	/**
	 * @brief Load and register an image from disk under a caller-provided key.
	 *
	 * The image is decoded with stb_image and uploaded as an RGBA8 sRGB Vulkan
	 * texture. Supported file formats are the stb_image formats compiled into
	 * FlowUi: JPEG, PNG, TGA, BMP, PSD, GIF, HDR, PIC, PNM/PPM, and PGM.
	 *
	 * Registering an existing key replaces the previous image resource and keeps
	 * the key active with the new texture data.
	 *
	 * @param key string image key.
	 * @param filePath Path to an image file on disk.
	 * @retval true The key was newly registered.
	 * @retval false key already existed and the previous image was replaced.
	 *
	 * @throws std::runtime_error if ImageManager is not initialized, the texture
	 * registry is not installed, key is empty, filePath does not refer to a
	 * regular file, the image cannot be decoded, or GPU upload fails.
	 *
	 * @code{.cpp}
	 * const bool inserted = app.images().registerImage(
	 *     "profile/avatar",
	 *     "assets/images/avatar.png");
	 * (void)inserted;
	 * @endcode
	 */
	bool registerImage(std::string_view key, std::string_view filePath);

	/**
	 * @brief Remove a registered image by key.
	 *
	 * Removing an image unregisters its texture slot and retires the GPU resource
	 * after the current frame bucket is safe to clean up. TextureRef values
	 * previously returned for the key should be treated as invalid after removal.
	 *
	 * @param key string image key to remove.
	 * @retval true A registered image existed and was removed.
	 * @retval false No image was registered for key.
	 *
	 * @throws std::runtime_error if ImageManager is not initialized or the
	 * texture registry is not installed.
	 *
	 * @code{.cpp}
	 * if (app.images().removeImage("profile/avatar")) {
	 *     // The image can be registered again under the same key.
	 * }
	 * @endcode
	 */
	bool removeImage(std::string_view key);

	/**
	 * @brief Return whether an image key is registered.
	 *
	 * This is a non-throwing lookup in the currently registered image table. It
	 * does not perform file IO or GPU work.
	 *
	 * @param key string image key to test.
	 * @retval true key is registered.
	 * @retval false key is not registered.
	 *
	 * @code{.cpp}
	 * if (!app.images().contains("profile/avatar")) {
	 *     (void)app.images().registerImage("profile/avatar", "assets/images/avatar.png");
	 * }
	 * @endcode
	 */
	bool contains(std::string_view key) const;

	/**
	 * @brief Return the texture reference for a registered image key.
	 *
	 * The returned TextureRef contains the manager-owned texture slot id and
	 * source image dimensions. If key is missing, this function returns the
	 * fallback texture id 0 and logs one warning for that key.
	 *
	 * Application code may adjust TextureRef fields such as fitMode and
	 * tintEnabled before passing it to image drawing code. Manager-owned id and
	 * UV fields should be left unchanged.
	 *
	 * @param key string image key to resolve.
	 * @return TextureRef for key, or a fallback TextureRef with id 0 when missing.
	 *
	 * @code{.cpp}
	 * FlowUi::TextureRef avatar = app.images().getTexture("profile/avatar");
	 * avatar.fitMode = FlowUi::TextureFitMode::Cover;
	 * avatar.tintEnabled = false;
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2images__icons__textures "Images, Icons, and Texture References"
	 */
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
