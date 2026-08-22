#pragma once

#include "FlowUi/BuildConfig.hpp"

#include <string_view>

#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/ResourceKey.hpp"

namespace FlowUi {

namespace detail {
namespace storage { class IStorageSystem; }
} // namespace detail
namespace devSystems { class DevMemoryRecorder; }

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
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	void setDevMemoryRecorder(devSystems::DevMemoryRecorder* recorder) noexcept { devMemoryRecorder_ = recorder; }
#endif
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
	Result<bool> registerImage(ResourceKey key, std::string_view filePath);
	Result<bool> registerImage(std::string_view key, std::string_view filePath) {
		return registerImage(ResourceKey{.name = key}, filePath);
	}

	/**
	 * @brief Remove a registered image by key.
	 *
	 * Removing an image retires its logical texture generation and GPU resource
	 * after the exact last storage submission completes. TextureRef values
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
	Result<bool> removeImage(ResourceKey key);
	Result<bool> removeImage(std::string_view key) { return removeImage(ResourceKey{.name = key}); }

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
	bool contains(ResourceKey key) const;
	bool contains(std::string_view key) const { return contains(ResourceKey{.name = key}); }

	/**
	 * @brief Return the texture reference for a registered image key.
	 *
	 * The returned TextureRef contains a generation-checked logical handle and
	 * source image dimensions. If key is missing, this function returns the
	 * an invalid handle and logs one warning for that key.
	 *
	 * Application code may adjust TextureRef fields such as fitMode and
	 * tintEnabled before passing it to image drawing code. Manager-owned handle and
	 * UV fields should be left unchanged.
	 *
	 * @param key string image key to resolve.
	 * @return TextureRef for key, or an invalid fallback reference when missing.
	 *
	 * @code{.cpp}
	 * FlowUi::TextureRef avatar = app.images().getTexture("profile/avatar");
	 * avatar.fitMode = FlowUi::TextureFitMode::Cover;
	 * avatar.tintEnabled = false;
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2images__icons__textures "Images, Icons, and Texture References"
	 */
	TextureRef getTexture(ResourceKey key) const;
	TextureRef getTexture(std::string_view key) const { return getTexture(ResourceKey{.name = key}); }

private:
	friend class App;

	void init(detail::storage::IStorageSystem& storageSystem, MissingVisualPolicy missingPolicy);
	void destroy() noexcept;

	detail::storage::IStorageSystem* storage_ = nullptr;
	MissingVisualPolicy missingPolicy_ = MissingVisualPolicy::UseFallbackTexture;
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevMemoryRecorder* devMemoryRecorder_ = nullptr;
#endif
};

/** @} */

} // namespace FlowUi
