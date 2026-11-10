#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#include <filesystem>
#include <string_view>
#if FLOW_UI_DEV_MODE
#include <cstdint>
#include <string>
#include <vector>
#include "FlowUi/Public/identificationTypes/TextureHandle.hpp"
#endif

#include "FlowUi/Public/TextureRef.hpp"
#include "FlowUi/Public/errorManagement/ErrorPolicy.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"

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
 * const auto registered = images.registerImage("hero/logo", "assets/images/logo.png");
 * if (!registered) {
 *     return; // Report registered.error() to the application's error handler.
 * }
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
#if FLOW_UI_DEV_MODE
	/** @brief Borrowed image registration metadata valid during visitation. */
	struct DevImageView {
		/** @brief Borrowed application-defined image registration key. */
		std::string_view key{};
		/** @brief Borrowed UTF-8 source path retained for development inspection. */
		std::string_view sourcePath{};
		/** @brief Borrowed image texture generation; removal/replacement invalidates it. */
		TextureHandle texture{};
	};
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevImageVisitor = bool (*)(void*, const DevImageView&);
	/** @brief Return the current development-observation revision; zero when support is disabled. */
	[[nodiscard]] std::uint64_t devRevision() const noexcept { return devRevision_; }
	/** @brief Return the number of currently published entries in this development catalogue. */
	[[nodiscard]] std::size_t devImageCount() const noexcept { return devImages_.size(); }
	/** @brief Visit borrowed catalogue entries synchronously; return false when a visitor stops early. */
	bool visitDevImages(void* userData, DevImageVisitor visitor) const;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Attach a borrowed development memory recorder; null disables recording. */
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
	 * @return A result whose value is true for a newly registered key or false
	 * when an existing image was replaced. Initialization, key, file, decode,
	 * and upload failures are returned as structured errors.
	 * @throws std::bad_alloc if allocating path or registration metadata fails.
	 *
	 * @code{.cpp}
	 * const auto registered = app.images().registerImage(
	 *     "profile/avatar",
	 *     "assets/images/avatar.png");
	 * if (!registered) {
	 *     return; // Report registered.error() to the application's error handler.
	 * }
	 * const bool inserted = *registered;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerImage(ResourceKey key, std::string_view filePath);
	/** Load an image through a native filesystem path. */
	[[nodiscard]] Result<bool> register_image_file(ResourceKey key, const std::filesystem::path& file_path);
	/** Load an image through a native filesystem path under a string key. */
	[[nodiscard]] Result<bool> register_image_file(std::string_view key, const std::filesystem::path& file_path) {
		return register_image_file(ResourceKey{.name = key}, file_path);
	}
	/** @brief Load or replace an image under an app-shared string key; filePath is UTF-8. */
	[[nodiscard]] Result<bool> registerImage(std::string_view key, std::string_view filePath) {
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
	 * @return A result whose value is true when an image was removed or false
	 * when the key was absent. Initialization and storage failures are returned
	 * as structured errors.
	 *
	 * @code{.cpp}
	 * const auto removed = app.images().removeImage("profile/avatar");
	 * if (!removed) {
	 *     return; // Report removed.error() to the application's error handler.
	 * }
	 * if (*removed) {
	 *     // The image can be registered again under the same key.
	 * }
	 * @endcode
	 */
	[[nodiscard]] Result<bool> removeImage(ResourceKey key);
	/** @brief Remove an image under an app-shared string key; return false when absent. */
	[[nodiscard]] Result<bool> removeImage(std::string_view key) {
		return removeImage(ResourceKey{.name = key});
	}

	/**
	 * @brief Return whether an image key is registered.
	 *
	 * This lookup does not perform file IO or GPU work. Resource-key
	 * normalization may allocate or report an invalid key.
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
	[[nodiscard]] bool contains(ResourceKey key) const;
	/** @brief Test whether an app-shared image string key is registered. */
	[[nodiscard]] bool contains(std::string_view key) const {
		return contains(ResourceKey{.name = key});
	}

	/**
	 * @brief Return the texture reference for a registered image key.
	 *
	 * The returned TextureRef contains a generation-checked logical handle and
	 * source image dimensions. If key is missing, this function returns an
	 * invalid handle and reports the missing key once. SkipVisual marks that result
	 * to be skipped; otherwise the renderer uses its fallback texture. The result and its source
	 * key borrow manager-owned metadata; removal invalidates the handle.
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
	[[nodiscard]] TextureRef getTexture(ResourceKey key) const;
	/** @brief Resolve an app-shared image string key using the configured missing-visual policy. */
	[[nodiscard]] TextureRef getTexture(std::string_view key) const {
		return getTexture(ResourceKey{.name = key});
	}

private:
	friend class App;

	void init(detail::storage::IStorageSystem& storageSystem, MissingVisualPolicy missingPolicy);
	void destroy() noexcept;

	detail::storage::IStorageSystem* storage_ = nullptr;
	MissingVisualPolicy missingPolicy_ = MissingVisualPolicy::UseFallbackTexture;
#if FLOW_UI_DEV_MODE
	struct DevImageRecord {
		std::string key{};
		std::string sourcePath{};
		TextureHandle texture{};
	};
	std::vector<DevImageRecord> devImages_{};
	std::uint64_t devRevision_ = 1;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevMemoryRecorder* devMemoryRecorder_ = nullptr;
#endif
};

/** @} */

} // namespace FlowUi
