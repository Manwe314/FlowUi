#pragma once

#include "FlowUi/Private/ApiBoundary.hpp"

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
 * @brief App-shared SVG registration and lazy frame-size rasterization.
 *
 * Recoverable operations return checked Result/Status values. A successful bool
 * payload may be false; inspect the payload only after checking the Result.
 * Obtain App-owned managers through the checked App getter and unwrap its
 * reference_wrapper with result->get().
 * Returned text, metadata, resource views, and pointers remain borrowed under
 * each operation's documented invalidation rules.
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
	/**
	 * @brief Observe development catalogue entries without invoking application operations.
	 *
	 * Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts materialized raster rows (unrasterized SVG rows are excluded); completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure. With icon support disabled, this hook succeeds with true and makes no callbacks.
	 *
	 * @return Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * std::size_t count = 0;
	 * auto result = manager.visitDevIcons(&count,
	 *     [](void* target, const FlowUi::IconManager::DevIconView& icon) {
	 *         auto& observedCount = *static_cast<std::size_t*>(target);
	 *         if (static_cast<bool>(icon.texture)) {
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
	Result<bool> visitDevIcons(void* userData, DevIconVisitor visitor) const noexcept;
	/**
	 * @brief Observe development catalogue entries without invoking application operations.
	 *
	 * Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts allocated nonempty atlas pages; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure. With icon support disabled, this hook succeeds with true and makes no callbacks.
	 *
	 * @return Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * std::size_t count = 0;
	 * auto result = manager.visitDevAtlases(&count,
	 *     [](void* target, const FlowUi::IconManager::DevAtlasView& page) {
	 *         auto& observedCount = *static_cast<std::size_t*>(target);
	 *         if (page.width != 0 && page.height != 0) {
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
	Result<bool> visitDevAtlases(void* userData, DevAtlasVisitor visitor) const noexcept;
	/**
	 * @brief Materialize or reuse one exact development SVG raster.
	 *
	 * The key must name a registered SVG; width and height must be in [1, 512]. The example expects status/check to be registered. The returned DevIconView and texture identity are borrowed and invalidated by raster/document removal or manager destruction. newlyRasterized distinguishes a new cached raster from a reused one. No exception escapes. With icon support disabled, return UnsupportedBuildFeature.
	 *
	 * @return A checked raster view, or initialization, IconRasterInvalid, IconNotFound, publication, allocation, or storage failure.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto result = icons.devRasterize("status/check", 32, 32);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool newlyRasterized = result->newlyRasterized;
	 * const auto retained = result->texture;
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<DevIconView> devRasterize(std::string_view key, uint32_t width,
												uint32_t height) noexcept;
	/**
	 * @brief Retire only the exact development raster texture generation.
	 *
	 * The SVG registration remains. A removed raster can be regenerated by later UI usage or devRasterize(). Its previous borrowed texture identity must be discarded; backing allocations retire after their recorded uses. No exception escapes. With icon support disabled, return UnsupportedBuildFeature.
	 *
	 * @return Success contains true when a cached raster was removed or false when the texture did not match any cached raster; initialization/storage/allocation failures are errors.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto result = icons.devRemoveRaster(raster);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<bool> devRemoveRaster(TextureHandle texture) noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only memory accounting to sink without transferring storage ownership. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
	/** @brief Attach a borrowed development memory recorder; null disables recording. */
	void setDevMemoryRecorder(devSystems::DevMemoryRecorder* recorder) noexcept { devMemoryRecorder_ = recorder; }
#endif
	/**
	 * @brief Parse and register SVG source for lazy rasterization.
	 *
	 * Source bytes are consumed/copied during registration; rasterization occurs later at rendered size and cached variants are reused.
	 *
	 * @return Success contains true for insertion or false when the key already exists; a duplicate preserves the original document. Invalid SVG is an error.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * constexpr std::string_view svgSource = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M2 8l4 4 8-8" fill="none" stroke="#fff"/></svg>)svg";
	 * auto result = manager.registerSvg("status/check", svgSource);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerSvg(std::string_view key, std::string_view svgSource) noexcept;
	/**
	 * @brief Parse and register SVG source for lazy rasterization.
	 *
	 * Source bytes are consumed/copied during registration; rasterization occurs later at rendered size and cached variants are reused. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains true for insertion or false when the key already exists; a duplicate preserves the original document. Invalid SVG is an error.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "status/check"};
	 * constexpr std::string_view svgSource = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M2 8l4 4 8-8" fill="none" stroke="#fff"/></svg>)svg";
	 * auto result = manager.registerSvg(key, svgSource);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerSvg(ResourceKey key, std::string_view svgSource) noexcept;

	/**
	 * @brief Read, parse, and register an SVG file from a UTF-8 path.
	 *
	 * The file path and bytes are consumed synchronously. Registration does not rasterize the SVG.
	 *
	 * @return Success contains true for insertion or false for an existing unchanged key. File/path/read/parse failures are errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.registerFromFile("status/check", "assets/icons/check.svg");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerFromFile(std::string_view key, std::string_view filePath) noexcept;
	/**
	 * @brief Read, parse, and register an SVG file from a UTF-8 path.
	 *
	 * The file path and bytes are consumed synchronously. Registration does not rasterize the SVG. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains true for insertion or false for an existing unchanged key. File/path/read/parse failures are errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "status/check"};
	 * auto result = manager.registerFromFile(key, "assets/icons/check.svg");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerFromFile(ResourceKey key, std::string_view filePath) noexcept;
	/**
	 * @brief Read and register an SVG from a native filesystem path.
	 *
	 * Use native filesystem paths where platform path encoding matters; rasterization remains lazy. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains true for insertion or false for an existing unchanged key. File/path/read/parse failures are errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "status/check"};
	 * const std::filesystem::path assetPath{"assets/icons/check.svg"};
	 * auto result = manager.register_file(key, assetPath);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> register_file(ResourceKey key, const std::filesystem::path& file_path) noexcept;
	/**
	 * @brief Read and register an SVG from a native filesystem path.
	 *
	 * Use native filesystem paths where platform path encoding matters; rasterization remains lazy.
	 *
	 * @return Success contains true for insertion or false for an existing unchanged key. File/path/read/parse failures are errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const std::filesystem::path assetPath{"assets/icons/check.svg"};
	 * auto result = manager.register_file("status/check", assetPath);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> register_file(std::string_view key, const std::filesystem::path& file_path) noexcept {
		return register_file(ResourceKey{.name = key}, file_path);
	}

	/**
	 * @brief Remove an SVG document and retire its cached raster generations.
	 *
	 * Previously returned request/cached texture generations are invalid after removal. Reacquire after registration; actual GPU allocations retire when recorded uses finish.
	 *
	 * @return Success contains true if removed and false if absent.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.remove("status/check");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> remove(std::string_view key) noexcept;
	/**
	 * @brief Remove an SVG document and retire its cached raster generations.
	 *
	 * Previously returned request/cached texture generations are invalid after removal. Reacquire after registration; actual GPU allocations retire when recorded uses finish. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains true if removed and false if absent.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "status/check"};
	 * auto result = manager.remove(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> remove(ResourceKey key) noexcept;

	/**
	 * @brief Test whether an SVG key is registered.
	 *
	 * This does not trigger rasterization.
	 *
	 * @return Success contains true for a registered document or false for absence.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.contains("status/check");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto registered = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> contains(std::string_view key) const noexcept;
	/**
	 * @brief Test whether an SVG key is registered.
	 *
	 * This does not trigger rasterization. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Success contains true for a registered document or false for absence.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "status/check"};
	 * auto result = manager.contains(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto registered = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> contains(ResourceKey key) const noexcept;

	/**
	 * @brief Resolve an SVG key to a request TextureRef for UI rendering.
	 *
	 * This non-const query may publish a request texture; final size-dependent rasterization occurs during frame preparation. sourceKey is borrowed App-owned metadata. Do not overwrite manager-owned handle/UV fields. Reacquire after removal and keep borrowed metadata within the App lifetime.
	 *
	 * @return Missing keys return IconNotFound under IconGenerationFailurePolicy::FailRequest. Other policies successfully return an unavailable reference requesting a fallback or omission, with a once-per-key diagnostic.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.textureRef("status/check");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto texture = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<TextureRef> textureRef(std::string_view key) noexcept;
	/**
	 * @brief Resolve an SVG key to a request TextureRef for UI rendering.
	 *
	 * This non-const query may publish a request texture; final size-dependent rasterization occurs during frame preparation. sourceKey is borrowed App-owned metadata. Do not overwrite manager-owned handle/UV fields. Reacquire after removal and keep borrowed metadata within the App lifetime. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.
	 *
	 * @return Missing keys return IconNotFound under IconGenerationFailurePolicy::FailRequest. Other policies successfully return an unavailable reference requesting a fallback or omission, with a once-per-key diagnostic.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "status/check"};
	 * auto result = manager.textureRef(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto texture = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<TextureRef> textureRef(ResourceKey key) noexcept;

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
 * @brief App-shared SVG registration and lazy frame-size rasterization.
 *
 * Recoverable operations return checked Result/Status values. A successful bool
 * payload may be false; inspect the payload only after checking the Result.
 * Obtain App-owned managers through the checked App getter and unwrap its
 * reference_wrapper with result->get().
 * Returned text, metadata, resource views, and pointers remain borrowed under
 * each operation's documented invalidation rules.
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
	/**
	 * @brief Observe development catalogue entries without invoking application operations.
	 *
	 * Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts materialized raster rows (unrasterized SVG rows are excluded); completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure. With icon support disabled, this hook succeeds with true and makes no callbacks.
	 *
	 * @return Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * std::size_t count = 0;
	 * auto result = manager.visitDevIcons(&count,
	 *     [](void* target, const FlowUi::IconManager::DevIconView& icon) {
	 *         auto& observedCount = *static_cast<std::size_t*>(target);
	 *         if (static_cast<bool>(icon.texture)) {
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
	Result<bool> visitDevIcons(void*, DevIconVisitor) const noexcept { return true; }
	/**
	 * @brief Observe development catalogue entries without invoking application operations.
	 *
	 * Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts allocated nonempty atlas pages; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure. With icon support disabled, this hook succeeds with true and makes no callbacks.
	 *
	 * @return Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * std::size_t count = 0;
	 * auto result = manager.visitDevAtlases(&count,
	 *     [](void* target, const FlowUi::IconManager::DevAtlasView& page) {
	 *         auto& observedCount = *static_cast<std::size_t*>(target);
	 *         if (page.width != 0 && page.height != 0) {
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
	Result<bool> visitDevAtlases(void*, DevAtlasVisitor) const noexcept { return true; }
	/**
	 * @brief Materialize or reuse one exact development SVG raster.
	 *
	 * The key must name a registered SVG; width and height must be in [1, 512]. The example expects status/check to be registered. The returned DevIconView and texture identity are borrowed and invalidated by raster/document removal or manager destruction. newlyRasterized distinguishes a new cached raster from a reused one. No exception escapes. With icon support disabled, return UnsupportedBuildFeature.
	 *
	 * @return A checked raster view, or initialization, IconRasterInvalid, IconNotFound, publication, allocation, or storage failure.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto result = icons.devRasterize("status/check", 32, 32);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool newlyRasterized = result->newlyRasterized;
	 * const auto retained = result->texture;
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<DevIconView> devRasterize(std::string_view, uint32_t, uint32_t) noexcept {
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRasterize));
	}
	/**
	 * @brief Retire only the exact development raster texture generation.
	 *
	 * The SVG registration remains. A removed raster can be regenerated by later UI usage or devRasterize(). Its previous borrowed texture identity must be discarded; backing allocations retire after their recorded uses. No exception escapes. With icon support disabled, return UnsupportedBuildFeature.
	 *
	 * @return Success contains true when a cached raster was removed or false when the texture did not match any cached raster; initialization/storage/allocation failures are errors.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto result = icons.devRemoveRaster(raster);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * #endif
	 * @endcode
	 */
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
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * constexpr std::string_view svgSource = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M2 8l4 4 8-8" fill="none" stroke="#fff"/></svg>)svg";
	 * auto result = manager.registerSvg("status/check", svgSource);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerSvg(std::string_view, std::string_view) noexcept {
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRegisterSource));
	}
	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "resource"};
	 * constexpr std::string_view svgSource = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M2 8l4 4 8-8" fill="none" stroke="#fff"/></svg>)svg";
	 * auto result = manager.registerSvg(key, svgSource);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerSvg(ResourceKey key, std::string_view svgSource) noexcept {
		return registerSvg(key.name, svgSource);
	}

	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.registerFromFile("status/check", "assets/icons/check.svg");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerFromFile(std::string_view, std::string_view) noexcept {
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRegisterSource));
	}
	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "resource"};
	 * auto result = manager.registerFromFile(key, "assets/icons/check.svg");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> registerFromFile(ResourceKey key, std::string_view filePath) noexcept {
		return registerFromFile(key.name, filePath);
	}
	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "resource"};
	 * const std::filesystem::path assetPath{"assets/logo.png"};
	 * auto result = manager.register_file(key, assetPath);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> register_file(ResourceKey, const std::filesystem::path&) noexcept {
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRegisterSource));
	}
	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const std::filesystem::path assetPath{"assets/logo.png"};
	 * auto result = manager.register_file("status/check", assetPath);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto inserted = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> register_file(std::string_view key, const std::filesystem::path& file_path) noexcept {
		return register_file(ResourceKey{.name = key}, file_path);
	}

	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.remove("status/check");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> remove(std::string_view) noexcept {
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconRemove));
	}
	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "resource"};
	 * auto result = manager.remove(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> remove(ResourceKey key) noexcept { return remove(key.name); }

	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.contains("status/check");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto registered = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> contains(std::string_view) const noexcept {
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconLookup));
	}
	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "resource"};
	 * auto result = manager.contains(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto registered = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> contains(ResourceKey key) const noexcept { return contains(key.name); }

	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.textureRef("status/check");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto texture = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<TextureRef> textureRef(std::string_view) noexcept {
		return unexpectedError(makeError(ErrorCode::UnsupportedBuildFeature, ErrorSite::IconLookup));
	}
	/**
	 * @brief Report unavailable icon support for this overload.
	 *
	 * Enable icon-manager support to use SVG registration, removal, or lookup. ResourceKey adapters in this build forward to the unsupported operation.
	 *
	 * @return Always returns UnsupportedBuildFeature when FLOWUI_INCLUDE_ICON_MANAGER is disabled.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "resource"};
	 * auto result = manager.textureRef(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto texture = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<TextureRef> textureRef(ResourceKey key) noexcept { return textureRef(key.name); }

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
