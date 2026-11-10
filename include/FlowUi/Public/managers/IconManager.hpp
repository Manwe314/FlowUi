#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

#include <clay.h>

#include "FlowUi/Public/Resources.hpp"
#include "FlowUi/Public/TextureRef.hpp"
#include "FlowUi/Public/configurationStructs/IconManagerConfig.hpp"
#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorPolicy.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/identificationTypes/TextureHandle.hpp"
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"

namespace FlowUi {

namespace devSystems { class MemorySampleSink; class DevMemoryRecorder; }

#if FLOWUI_INCLUDE_ICON_MANAGER
class App;
struct IconManagerConfig;

namespace detail::storage { class IStorageSystem; }
namespace detail::manager_storage {
struct IconDocumentRecord; struct IconSurfaceOwner; struct IconTransientRasterResult;
struct IconAtlasRect; struct IconAtlasAllocation; struct IconVariantKey; struct IconVariantKeyHash;
struct IconVariantEntry; struct IconAtlasPage; struct IconRetiredRegion; class IconCacheController;
}

/** @addtogroup flowui_icon_manager
 * @{
 */

/**
 * @brief Registers SVG icons and resolves them to atlas-backed texture references.
 *
 * IconManager is available only when FlowUi is built with
 * FLOWUI_INCLUDE_ICON_MANAGER enabled. When enabled, it stores parsed SVG
 * documents by application-defined keys and returns TextureRef request handles
 * that FlowUi resolves into cached atlas variants at the size required by the
 * current frame.
 *
 * @code{.cpp}
 * FlowUi::IconManager& icons = app.icons();
 * (void)icons.registerFromFile("toolbar/save", "assets/icons/save.svg");
 *
 * FlowUi::TextureRef saveIcon = icons.textureRef("toolbar/save");
 * saveIcon.fitMode = FlowUi::TextureFitMode::Contain;
 * @endcode
 *
 * @see @ref md_docs_2tutorials_2images__icons__textures "Images, Icons, and Texture References"
 * @see @ref md_docs_2concepts_2managers "Managers"
 */
struct IconManager {
#if FLOW_UI_DEV_MODE
	/** @brief Borrowed SVG raster metadata; reacquire after icon cache mutation. */
	struct DevIconView {
		/** @brief Borrowed application-defined SVG registration key. */
		std::string_view key{};
		/** @brief Cached raster width in pixels. */
		std::uint32_t targetWidth = 0;
		/** @brief Cached raster height in pixels. */
		std::uint32_t targetHeight = 0;
		/** @brief Borrowed generation-checked raster texture binding. */
		TextureHandle texture{};
		/** @brief Normalized atlas UV bounds: uv0x/uv0y minimum, uv1x/uv1y maximum. */
		/** @var float uv0y
		 * @brief Normalized top UV coordinate of the cached raster.
		 */
		/** @var float uv1x
		 * @brief Normalized right UV coordinate of the cached raster.
		 */
		/** @var float uv1y
		 * @brief Normalized bottom UV coordinate of the cached raster.
		 */
		/** @var float uv0x
		 * @brief Normalized left UV coordinate of the cached raster.
		 */
		float uv0x = 0.0f, uv0y = 0.0f, uv1x = 1.0f, uv1y = 1.0f;
		/** @brief Zero-based page containing the raster. */
		std::uint32_t atlasPage = 0;
		/** @brief Whether this explicit raster request generated a new cached variant. */
		bool newlyRasterized = false;
	};
	/** @brief Borrowed SVG atlas-page allocation metrics. */
	struct DevAtlasView {
		/** @brief Borrowed generation-checked page texture binding. */
		TextureHandle texture{};
		/** @brief Zero-based atlas page index. */
		std::uint32_t page = 0;
		/** @brief Page width in pixels. */
		std::uint32_t width = 0;
		/** @brief Page height in pixels. */
		std::uint32_t height = 0;
		/** @brief Total allocated region area in square pixels. */
		std::uint64_t usedArea = 0;
		/** @brief Number of live region allocations on this page. */
		std::uint32_t allocatedRegions = 0;
	};
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevIconVisitor = bool (*)(void*, const DevIconView&);
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevAtlasVisitor = bool (*)(void*, const DevAtlasView&);
	/** @brief Return the current development-observation revision; zero when support is disabled. */
	[[nodiscard]] std::uint64_t devRevision() const noexcept;
	/** @brief Return the number of currently published entries in this development catalogue. */
	[[nodiscard]] std::size_t devIconCount() const noexcept;
	/** @brief Return the number of currently published entries in this development catalogue. */
	[[nodiscard]] std::size_t devAtlasCount() const noexcept;
	/** @brief Visit borrowed catalogue entries synchronously; return false when a visitor stops early. */
	bool visitDevIcons(void* userData, DevIconVisitor visitor) const;
	/** @brief Visit borrowed catalogue entries synchronously; return false when a visitor stops early. */
	bool visitDevAtlases(void* userData, DevAtlasVisitor visitor) const;
	/** Materialize one exact raster at an explicit development-tool request. */
	[[nodiscard]] Result<DevIconView> devRasterize(std::string_view key, uint32_t width,
												uint32_t height) noexcept;
	/** Retire only the exact cached texture generation; the SVG registration remains. */
	[[nodiscard]] Result<bool> devRemoveRaster(TextureHandle texture) noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only memory accounting to sink without transferring storage ownership. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
	/** @brief Attach a borrowed development memory recorder; null disables recording. */
	void setDevMemoryRecorder(devSystems::DevMemoryRecorder* recorder) noexcept { devMemoryRecorder_ = recorder; }
#endif
	/**
	 * @brief Register an SVG document from source text.
	 *
	 * The key is the stable string name used later with contains(),
	 * remove(), and textureRef(). The SVG source is parsed immediately; raster
	 * variants are created lazily when the icon is rendered.
	 *
	 * @param key string icon key.
	 * @param svgSource Complete SVG document source text.
	 * @return A result containing true when registered or false when the key
	 * already exists. Invalid source, key, initialization, and storage failures
	 * are returned as structured errors.
	 * @throws std::bad_alloc if allocating SVG or registration data fails.
	 *
	 * @code{.cpp}
	 * constexpr std::string_view kCheckSvg = R"(
	 * <svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
	 *     <path d="M6.2 11.3 2.9 8l1.2-1.2 2.1 2.1 5.7-5.7L13.1 4z"/>
	 * </svg>
	 * )";
	 *
	 * const auto registered = app.icons().registerSvg("status/check", kCheckSvg);
	 * if (!registered) {
	 *     return; // Report registered.error() to the application.
	 * }
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerSvg(std::string_view key, std::string_view svgSource);
	/** @brief Register parsed SVG source under an app-shared resource key.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<bool> registerSvg(ResourceKey key, std::string_view svgSource);

	/**
	 * @brief Register an SVG document from a file path.
	 *
	 * The key is the stable string name used later with contains(),
	 * remove(), and textureRef(). The file is parsed immediately; raster variants
	 * are created lazily when the icon is rendered.
	 *
	 * @param key string icon key.
	 * @param filePath Path to an SVG file on disk.
	 * @return A result containing true when registered or false when the key
	 * already exists. Initialization, key, file, parse, and storage failures
	 * are returned as structured errors.
	 * @throws std::bad_alloc if allocating SVG or registration data fails.
	 *
	 * @code{.cpp}
	 * const auto registered = app.icons().registerFromFile(
	 *     "toolbar/open",
	 *     "assets/icons/open.svg");
	 * if (!registered) {
	 *     return; // Report registered.error() to the application.
	 * }
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerFromFile(std::string_view key, std::string_view filePath);
	/** @brief Register an SVG file under an app-shared resource key.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<bool> registerFromFile(ResourceKey key, std::string_view filePath);
	/** Load SVG source through a native filesystem path. */
	[[nodiscard]] Result<bool> register_file(ResourceKey key, const std::filesystem::path& file_path);
	/** Load SVG source through a native filesystem path under a string key. */
	[[nodiscard]] Result<bool> register_file(std::string_view key, const std::filesystem::path& file_path) {
		return register_file(ResourceKey{.name = key}, file_path);
	}

	/**
	 * @brief Remove a registered SVG document by key.
	 *
	 * Removing an icon also removes its request texture id and cached atlas
	 * variants. TextureRef values previously returned for the key should be
	 * treated as invalid after removal.
	 *
	 * @param key Application-defined icon key to remove.
	 * @return A result containing true when removed or false when the key is
	 * absent. Initialization and storage failures are structured errors.
	 *
	 * @code{.cpp}
	 * const auto removed = app.icons().remove("toolbar/open");
	 * if (removed && *removed) {
	 *     // The icon can be registered again under the same key.
	 * }
	 * @endcode
	 */
	[[nodiscard]] Result<bool> remove(std::string_view key);
	/** @brief Remove an app-shared SVG registration and retire its cached raster generations.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<bool> remove(ResourceKey key);

	/**
	 * @brief Return whether an SVG key is registered.
	 *
	 * This lookup does not force rasterization or atlas allocation. Resource-key
	 * normalization and lookup metadata may allocate.
	 *
	 * @param key Application-defined icon key to test.
	 * @retval true key is registered.
	 * @retval false key is not registered.
	 *
	 * @code{.cpp}
	 * if (!app.icons().contains("toolbar/save")) {
	 *     (void)app.icons().registerFromFile("toolbar/save", "assets/icons/save.svg");
	 * }
	 * @endcode
	 */
	[[nodiscard]] bool contains(std::string_view key) const;
	/** @brief Test whether an app-shared SVG resource key is registered.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] bool contains(ResourceKey key) const;

	/**
	 * @brief Return a texture request reference for a registered icon key.
	 *
	 * The returned TextureRef is a request handle, not necessarily the final atlas
	 * variant. During frame preparation, FlowUi inspects image commands using this
	 * id, rasterizes the SVG at the requested draw size, caches it in an atlas
	 * page, and rewrites the command to the cached atlas region.
	 *
	 * Application code may adjust TextureRef fields such as fitMode and
	 * tintEnabled before passing it to image drawing code. Manager-owned id and
	 * UV fields should be left unchanged.
	 *
	 * @param key Registered application-defined icon key.
	 * @return TextureRef request handle for the icon.
	 *
	 * @throws FlowUiException if the manager or key is invalid, or if a missing
	 * icon is requested with the FailRequest policy. Other missing-icon policies
	 * return a fallback or skipped reference. Native atlas invariants are fatal.
	 *
	 * @code{.cpp}
	 * FlowUi::TextureRef deleteIcon = app.icons().textureRef("toolbar/delete");
	 * deleteIcon.fitMode = FlowUi::TextureFitMode::Contain;
	 * deleteIcon.tintEnabled = true;
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2images__icons__textures "Images, Icons, and Texture References"
	 */
	[[nodiscard]] TextureRef textureRef(std::string_view key);
	/** @brief Resolve an app-shared SVG key to a borrowed request texture reference.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] TextureRef textureRef(ResourceKey key);

private:
	friend class App;

	void init(
		detail::storage::IStorageSystem& storage,
		const IconManagerConfig& config,
		IconGenerationFailurePolicy generationPolicy,
		CacheCapacityPolicy capacityPolicy);
	void beginAppTick();
	void prepareFrameTextures(
		Clay_RenderCommandArray& renderCommands,
		float uiToFramebufferScaleX,
		float uiToFramebufferScaleY);
	void destroy() noexcept;

	using DocumentRecord = detail::manager_storage::IconDocumentRecord;
	using SurfaceOwner = detail::manager_storage::IconSurfaceOwner;
	using TransientRasterResult = detail::manager_storage::IconTransientRasterResult;
	using AtlasRect = detail::manager_storage::IconAtlasRect;
	using AtlasAllocation = detail::manager_storage::IconAtlasAllocation;
	using VariantKey = detail::manager_storage::IconVariantKey;
	using VariantEntry = detail::manager_storage::IconVariantEntry;
	using AtlasPage = detail::manager_storage::IconAtlasPage;

	TransientRasterResult rasterizeForAtlas(std::string_view key, uint32_t requestedWidth, uint32_t requestedHeight) const;
	VariantKey makeVariantKey(std::string_view key, uint32_t requestedWidth, uint32_t requestedHeight) const;
	void advanceFrameCounter();
	void touchVariant(VariantEntry& variant, uint32_t frameIndex);
	void resetVariantFrameMarks();
	static uint32_t frameAge(uint32_t currentFrame, uint32_t lastUsedFrame);

	AtlasPage createAtlasPage(uint32_t pageIndex) const;
	void destroyAtlasPage(AtlasPage& page);
	void destroyAtlasPages();
	bool tryAllocateInPage(
		AtlasPage& page,
		uint32_t contentWidth,
		uint32_t contentHeight,
		uint32_t padding,
		AtlasAllocation& outAllocation);
	void releasePageRegion(AtlasPage& page, const AtlasRect& paddedRect);
	void mergeFreeRects(AtlasPage& page);
	void recalcAtlasUvs(VariantEntry& variant, const AtlasPage& page) const;
	void uploadRasterToAtlasPage(
		const AtlasPage& page,
		const TransientRasterResult& raster,
		const AtlasRect& contentRect);
	bool tryAllocateAtlasRegion(
		uint32_t contentWidth,
		uint32_t contentHeight,
		AtlasAllocation& outAllocation);
	bool evictLeastRecentlyUsedVariant();
	VariantEntry* findBestCachedVariant(
		std::string_view nameKey,
		uint32_t requestedWidth,
		uint32_t requestedHeight);
	VariantEntry& ensureVariantForRequest(
		std::string_view nameKey,
		uint32_t requestedWidth,
		uint32_t requestedHeight,
		bool exactSize = false);
	const std::string* findRequestedKeyByTextureHandle(TextureHandle texture) const;
	detail::storage::IStorageSystem* storage_ = nullptr;
	IconGenerationFailurePolicy generationPolicy_ = IconGenerationFailurePolicy::UseFallbackTexture;
	CacheCapacityPolicy capacityPolicy_ = CacheCapacityPolicy::RejectOperation;
	uint64_t controllerHandle_ = 0;
	detail::manager_storage::IconCacheController* controller_ = nullptr;
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	devSystems::DevMemoryRecorder* devMemoryRecorder_ = nullptr;
#endif
};

/** @} */
#else
class App;
struct IconManagerConfig;

namespace detail::storage { class IStorageSystem; }

/** @addtogroup flowui_icon_manager
 * @{
 */

/**
 * @brief Stub IconManager used when FlowUi is built with icon support disabled.
 *
 * This struct preserves the public type when FLOWUI_INCLUDE_ICON_MANAGER is
 * disabled. Result-returning registration/removal calls report UnsupportedBuildFeature;
 * contains() and textureRef() throw FlowUiException with that error. Development
 * visitors are empty and counts are zero.
 */
struct IconManager {
#if FLOW_UI_DEV_MODE
	/** @brief Borrowed SVG raster metadata; reacquire after icon cache mutation. */
	struct DevIconView {
		/** @brief Borrowed application-defined SVG registration key. */
		std::string_view key{};
		/** @brief Cached raster width in pixels. */
		std::uint32_t targetWidth = 0;
		/** @brief Cached raster height in pixels. */
		std::uint32_t targetHeight = 0;
		/** @brief Borrowed generation-checked raster texture binding. */
		TextureHandle texture{};
		/** @brief Normalized atlas UV bounds: uv0x/uv0y minimum, uv1x/uv1y maximum. */
		/** @var float uv0y
		 * @brief Normalized top UV coordinate of the cached raster.
		 */
		/** @var float uv1x
		 * @brief Normalized right UV coordinate of the cached raster.
		 */
		/** @var float uv1y
		 * @brief Normalized bottom UV coordinate of the cached raster.
		 */
		/** @var float uv0x
		 * @brief Normalized left UV coordinate of the cached raster.
		 */
		float uv0x = 0.0f, uv0y = 0.0f, uv1x = 1.0f, uv1y = 1.0f;
		/** @brief Zero-based page containing the raster. */
		std::uint32_t atlasPage = 0;
		/** @brief Whether this explicit raster request generated a new cached variant. */
		bool newlyRasterized = false;
	};
	/** @brief Borrowed SVG atlas-page allocation metrics. */
	struct DevAtlasView {
		/** @brief Borrowed generation-checked page texture binding. */
		TextureHandle texture{};
		/** @brief Zero-based atlas page index. */
		std::uint32_t page = 0;
		/** @brief Page width in pixels. */
		std::uint32_t width = 0;
		/** @brief Page height in pixels. */
		std::uint32_t height = 0;
		/** @brief Total allocated region area in square pixels. */
		std::uint64_t usedArea = 0;
		/** @brief Number of live region allocations on this page. */
		std::uint32_t allocatedRegions = 0;
	};
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevIconVisitor = bool (*)(void*, const DevIconView&);
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevAtlasVisitor = bool (*)(void*, const DevAtlasView&);
	/** @brief Return the current development-observation revision; zero when support is disabled. */
	[[nodiscard]] std::uint64_t devRevision() const noexcept { return 0; }
	/** @brief Return the number of currently published entries in this development catalogue. */
	[[nodiscard]] std::size_t devIconCount() const noexcept { return 0; }
	/** @brief Return the number of currently published entries in this development catalogue. */
	[[nodiscard]] std::size_t devAtlasCount() const noexcept { return 0; }
	/** @brief Visit borrowed catalogue entries synchronously; return false when a visitor stops early. */
	bool visitDevIcons(void*, DevIconVisitor) const { return true; }
	/** @brief Visit borrowed catalogue entries synchronously; return false when a visitor stops early. */
	bool visitDevAtlases(void*, DevAtlasVisitor) const { return true; }
	/** Report unavailable icon support for an explicit development raster request. */
	[[nodiscard]] Result<DevIconView> devRasterize(std::string_view, uint32_t, uint32_t) noexcept {
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRasterize));
	}
	/** Report unavailable icon support for an explicit development raster removal. */
	[[nodiscard]] Result<bool> devRemoveRaster(TextureHandle) noexcept {
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRemove));
	}
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only memory accounting to sink without transferring storage ownership. */
	void appendDevMemorySamples(devSystems::MemorySampleSink&) const noexcept {}
	/** @brief Attach a borrowed development memory recorder; null disables recording. */
	void setDevMemoryRecorder(devSystems::DevMemoryRecorder*) noexcept {}
#endif
	/**
	 * @brief Return UnsupportedBuildFeature because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @param svgSource Unused SVG source.
	 * @return Structured unsupported-feature error.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * (void)app.icons().registerSvg("status/check", svgSource);
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerSvg(std::string_view key, std::string_view svgSource) {
		(void)key;
		(void)svgSource;
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRegisterSource));
	}
	/** @brief Register parsed SVG source under an app-shared resource key.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<bool> registerSvg(ResourceKey key, std::string_view svgSource) {
		return registerSvg(key.name, svgSource);
	}

	/**
	 * @brief Return UnsupportedBuildFeature because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @param filePath Unused SVG file path.
	 * @return Structured unsupported-feature error.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * (void)app.icons().registerFromFile("toolbar/open", "assets/icons/open.svg");
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerFromFile(std::string_view key, std::string_view filePath) {
		(void)key;
		(void)filePath;
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRegisterSource));
	}
	/** @brief Register an SVG file under an app-shared resource key.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<bool> registerFromFile(ResourceKey key, std::string_view filePath) {
		return registerFromFile(key.name, filePath);
	}
	/** Report unavailable SVG support for a native file path. */
	[[nodiscard]] Result<bool> register_file(ResourceKey key, const std::filesystem::path& file_path) {
		return registerFromFile(key, path_to_utf8(file_path));
	}
	/** Report unavailable SVG support for a native file path under a string key. */
	[[nodiscard]] Result<bool> register_file(std::string_view key, const std::filesystem::path& file_path) {
		return register_file(ResourceKey{.name = key}, file_path);
	}

	/**
	 * @brief Return UnsupportedBuildFeature because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @return Structured unsupported-feature error.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * (void)app.icons().remove("toolbar/open");
	 * @endcode
	 */
	[[nodiscard]] Result<bool> remove(std::string_view key) {
		(void)key;
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRemove));
	}
	/** @brief Remove an app-shared SVG registration and retire its cached raster generations.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<bool> remove(ResourceKey key) { return remove(key.name); }

	/**
	 * @brief Throws because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @throws FlowUiException always.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * const bool hasIcon = app.icons().contains("toolbar/open");
	 * (void)hasIcon;
	 * @endcode
	 */
	[[nodiscard]] bool contains(std::string_view key) const {
		(void)key;
		throw FlowUiException(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconLookup));
	}
	/** @brief Test whether an app-shared SVG resource key is registered.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] bool contains(ResourceKey key) const { return contains(key.name); }

	/**
	 * @brief Throws because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @throws FlowUiException always.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * FlowUi::TextureRef icon = app.icons().textureRef("toolbar/open");
	 * @endcode
	 */
	[[nodiscard]] TextureRef textureRef(std::string_view key) {
		(void)key;
		throw FlowUiException(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconLookup));
	}
	/** @brief Resolve an app-shared SVG key to a borrowed request texture reference.
	 * ResourceDomain::Icon is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] TextureRef textureRef(ResourceKey key) { return textureRef(key.name); }

private:
	friend class App;

	void init(detail::storage::IStorageSystem&, const IconManagerConfig&) {
		throw FlowUiException(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconManagerInitialize));
	}

	void beginAppTick() {}

	void prepareFrameTextures(Clay_RenderCommandArray&, float, float) {}

	void destroy() noexcept {}
};

/** @} */
#endif

} // namespace FlowUi
