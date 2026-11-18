#pragma once

#include "FlowUi/Private/ApiBoundary.hpp"

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
namespace storage {
class StorageBinding;
}
} // namespace detail
namespace devSystems { class DevMemoryRecorder; }

class App;

/** @addtogroup flowui_image_manager
 * @{
 */

/**
 * @brief App-shared image loading and generation-checked texture lookup.
 *
 * Recoverable operations return checked Result/Status values. A successful bool
 * payload may be false; inspect the payload only after checking the Result.
 * Obtain App-owned managers through the checked App getter and unwrap its
 * reference_wrapper with result->get().
 * Returned text, metadata, resource views, and pointers remain borrowed under
 * each operation's documented invalidation rules.
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
	/**
	 * @brief Observe development catalogue entries without invoking application operations.
	 *
	 * Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts named image registrations; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure.
	 *
	 * @return Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * std::size_t count = 0;
	 * auto result = manager.visitDevImages(&count,
	 *     [](void* target, const FlowUi::ImageManager::DevImageView& image) {
	 *         auto& observedCount = *static_cast<std::size_t*>(target);
	 *         if (!image.key.empty()) {
	 *             ++observedCount;
	 *         }
	 *         return true;
	 *     });
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool completed = *result;
	 * #endif
	 * @endcode
	 */
	Result<bool> visitDevImages(void* userData, DevImageVisitor visitor) const noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Attach a borrowed development memory recorder; null disables recording. */
	void setDevMemoryRecorder(devSystems::DevMemoryRecorder* recorder) noexcept { devMemoryRecorder_ = recorder; }
#endif
	/**
	 * @brief Load an encoded image and publish its sampled texture.
	 *
	 * The UTF-8 file path is consumed synchronously. Supported stb_image formats are decoded as RGBA8 sRGB. The replacement is prepared before publication; replaced generations retire after their recorded GPU uses. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains true for a new key or false when an existing image was replaced; false is a completed replacement, not an error.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "brand/logo"};
	 * auto result = manager.registerImage(key, "assets/logo.png");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerImage(ResourceKey key, std::string_view filePath) noexcept;
	/**
	 * @brief Load an encoded image from a native filesystem path.
	 *
	 * Use this overload for native platform paths. Decoding/publication and lifetime rules match registerImage(). ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains true for insertion or false for a completed replacement; inspect the Result separately from that bool.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "brand/logo"};
	 * const std::filesystem::path assetPath{"assets/logo.png"};
	 * auto result = manager.register_image_file(key, assetPath);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> register_image_file(ResourceKey key, const std::filesystem::path& file_path) noexcept;
	/**
	 * @brief Load an encoded image from a native filesystem path.
	 *
	 * Use this overload for native platform paths. Decoding/publication and lifetime rules match registerImage().
	 *
	 * @return Success contains true for insertion or false for a completed replacement; inspect the Result separately from that bool.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const std::filesystem::path assetPath{"assets/logo.png"};
	 * auto result = manager.register_image_file("brand/logo", assetPath);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> register_image_file(std::string_view key, const std::filesystem::path& file_path) noexcept try {
		return register_image_file(ResourceKey{.name = key}, file_path);
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::ImageRegister));
	}
	/**
	 * @brief Load an encoded image and publish its sampled texture.
	 *
	 * The UTF-8 file path is consumed synchronously. Supported stb_image formats are decoded as RGBA8 sRGB. The replacement is prepared before publication; replaced generations retire after their recorded GPU uses.
	 *
	 * @return Success contains true for a new key or false when an existing image was replaced; false is a completed replacement, not an error.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.registerImage("brand/logo", "assets/logo.png");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerImage(std::string_view key, std::string_view filePath) noexcept try {
		return registerImage(ResourceKey{.name = key}, filePath);
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::ImageRegister));
	}

	/**
	 * @brief Unregister an image and retire its texture generation.
	 *
	 * Previously acquired texture handles no longer identify a published generation; reacquire a texture after registering the key again. GPU resources retire after recorded uses finish. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains true if removed and false if no image was registered.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "brand/logo"};
	 * auto result = manager.removeImage(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> removeImage(ResourceKey key) noexcept;
	/**
	 * @brief Unregister an image and retire its texture generation.
	 *
	 * Previously acquired texture handles no longer identify a published generation; reacquire a texture after registering the key again. GPU resources retire after recorded uses finish.
	 *
	 * @return Success contains true if removed and false if no image was registered.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.removeImage("brand/logo");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> removeImage(std::string_view key) noexcept try {
		return removeImage(ResourceKey{.name = key});
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::ImageRemove));
	}

	/**
	 * @brief Test whether an image key is registered.
	 *
	 * No file loading or GPU work is performed, but key normalization can allocate or fail. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains true when registered or false when absent; an invalid key or detached manager is an error.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "brand/logo"};
	 * auto result = manager.contains(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto registered = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> contains(ResourceKey key) const noexcept;
	/**
	 * @brief Test whether an image key is registered.
	 *
	 * No file loading or GPU work is performed, but key normalization can allocate or fail.
	 *
	 * @return Success contains true when registered or false when absent; an invalid key or detached manager is an error.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.contains("brand/logo");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto registered = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> contains(std::string_view key) const noexcept try {
		return contains(ResourceKey{.name = key});
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::ImageLookup));
	}

	/**
	 * @brief Resolve an image key to a TextureRef value.
	 *
	 * TextureRef is a value with borrowed source-key metadata and generation-checked resource identity, not ownership of a texture. Reacquire after replacement/removal and keep borrowed metadata within the App lifetime. fit and tint are caller presentation settings. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return A missing key is a successful unavailable reference with an invalid handle. MissingVisualPolicy::SkipVisual requests omission; the fallback policy requests the renderer fallback, and the missing-key diagnostic is reported once.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "brand/logo"};
	 * auto result = manager.getTexture(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto texture = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<TextureRef> getTexture(ResourceKey key) const noexcept;
	/**
	 * @brief Resolve an image key to a TextureRef value.
	 *
	 * TextureRef is a value with borrowed source-key metadata and generation-checked resource identity, not ownership of a texture. Reacquire after replacement/removal and keep borrowed metadata within the App lifetime. fit and tint are caller presentation settings.
	 *
	 * @return A missing key is a successful unavailable reference with an invalid handle. MissingVisualPolicy::SkipVisual requests omission; the fallback policy requests the renderer fallback, and the missing-key diagnostic is reported once.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.getTexture("brand/logo");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto texture = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<TextureRef> getTexture(std::string_view key) const noexcept try {
		return getTexture(ResourceKey{.name = key});
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::ImageLookup));
	}

private:
	friend class App;

	void init(detail::storage::StorageBinding& storageSystem, MissingVisualPolicy missingPolicy);
	void destroy() noexcept;

	detail::storage::StorageBinding* storage_ = nullptr;
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
