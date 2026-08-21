#pragma once

#include "FlowUi/BuildConfig.hpp"

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

#include <clay.h>

#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/ResourceKey.hpp"

struct plutosvg_document;
using plutosvg_document_t = plutosvg_document;
struct plutovg_surface;
using plutovg_surface_t = plutovg_surface;
namespace FlowUi {

#if FLOWUI_INCLUDE_ICON_MANAGER
class App;
struct IconManagerConfig;

namespace detail::storage { class IStorageSystem; }
namespace devSystems { class MemorySampleSink; class DevMemoryRecorder; }
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
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
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
	 * @retval true The SVG was parsed and registered under key.
	 * @retval false key was already registered and the existing icon was left
	 * unchanged.
	 *
	 * @throws std::runtime_error if IconManager is not initialized, key is empty,
	 * svgSource is empty, svgSource is too large, or the SVG cannot be parsed.
	 *
	 * @code{.cpp}
	 * constexpr std::string_view kCheckSvg = R"(
	 * <svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
	 *     <path d="M6.2 11.3 2.9 8l1.2-1.2 2.1 2.1 5.7-5.7L13.1 4z"/>
	 * </svg>
	 * )";
	 *
	 * const bool inserted = app.icons().registerSvg("status/check", kCheckSvg);
	 * (void)inserted;
	 * @endcode
	 */
	bool registerSvg(std::string_view key, std::string_view svgSource);
	bool registerSvg(ResourceKey key, std::string_view svgSource);

	/**
	 * @brief Register an SVG document from a file path.
	 *
	 * The key is the stable string name used later with contains(),
	 * remove(), and textureRef(). The file is parsed immediately; raster variants
	 * are created lazily when the icon is rendered.
	 *
	 * @param key string icon key.
	 * @param filePath Path to an SVG file on disk.
	 * @retval true The SVG file was parsed and registered under key.
	 * @retval false key was already registered and the existing icon was left
	 * unchanged.
	 *
	 * @throws std::runtime_error if IconManager is not initialized, key is empty,
	 * filePath does not refer to a regular file, or the SVG cannot be parsed.
	 *
	 * @code{.cpp}
	 * const bool inserted = app.icons().registerFromFile(
	 *     "toolbar/open",
	 *     "assets/icons/open.svg");
	 * (void)inserted;
	 * @endcode
	 */
	bool registerFromFile(std::string_view key, std::string_view filePath);
	bool registerFromFile(ResourceKey key, std::string_view filePath);

	/**
	 * @brief Remove a registered SVG document by key.
	 *
	 * Removing an icon also removes its request texture id and cached atlas
	 * variants. TextureRef values previously returned for the key should be
	 * treated as invalid after removal.
	 *
	 * @param key Application-defined icon key to remove.
	 * @retval true A registered SVG existed and was removed.
	 * @retval false No SVG was registered for key.
	 *
	 * @throws std::runtime_error if IconManager is not initialized.
	 *
	 * @code{.cpp}
	 * if (app.icons().remove("toolbar/open")) {
	 *     // The icon can be registered again under the same key.
	 * }
	 * @endcode
	 */
	bool remove(std::string_view key);
	bool remove(ResourceKey key);

	/**
	 * @brief Return whether an SVG key is registered.
	 *
	 * This is a non-throwing lookup in the currently registered SVG document
	 * table. It does not force rasterization or atlas allocation.
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
	bool contains(std::string_view key) const;
	bool contains(ResourceKey key) const;

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
	 * @throws std::runtime_error if IconManager is not initialized, the texture
	 * registry is not installed, atlas pages are unavailable, key is empty, or key
	 * is not registered.
	 *
	 * @code{.cpp}
	 * FlowUi::TextureRef deleteIcon = app.icons().textureRef("toolbar/delete");
	 * deleteIcon.fitMode = FlowUi::TextureFitMode::Contain;
	 * deleteIcon.tintEnabled = true;
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2images__icons__textures "Images, Icons, and Texture References"
	 */
	TextureRef textureRef(std::string_view key);
	TextureRef textureRef(ResourceKey key);

private:
	friend class App;

	void init(detail::storage::IStorageSystem& storage, const IconManagerConfig& config);
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
		uint32_t requestedHeight);
	const std::string* findRequestedKeyByTextureHandle(TextureHandle texture) const;
	detail::storage::IStorageSystem* storage_ = nullptr;
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
 * disabled. Public calls throw std::runtime_error so code paths that require
 * icon support fail explicitly.
 */
struct IconManager {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	void appendDevMemorySamples(devSystems::MemorySampleSink&) const noexcept {}
	void setDevMemoryRecorder(devSystems::DevMemoryRecorder*) noexcept {}
#endif
	/**
	 * @brief Throws because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @param svgSource Unused SVG source.
	 * @throws std::runtime_error always.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * (void)app.icons().registerSvg("status/check", svgSource);
	 * @endcode
	 */
	bool registerSvg(std::string_view key, std::string_view svgSource) {
		(void)key;
		(void)svgSource;
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}
	bool registerSvg(ResourceKey key, std::string_view svgSource) {
		return registerSvg(key.name, svgSource);
	}

	/**
	 * @brief Throws because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @param filePath Unused SVG file path.
	 * @throws std::runtime_error always.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * (void)app.icons().registerFromFile("toolbar/open", "assets/icons/open.svg");
	 * @endcode
	 */
	bool registerFromFile(std::string_view key, std::string_view filePath) {
		(void)key;
		(void)filePath;
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}
	bool registerFromFile(ResourceKey key, std::string_view filePath) {
		return registerFromFile(key.name, filePath);
	}

	/**
	 * @brief Throws because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @throws std::runtime_error always.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * (void)app.icons().remove("toolbar/open");
	 * @endcode
	 */
	bool remove(std::string_view key) {
		(void)key;
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}
	bool remove(ResourceKey key) { return remove(key.name); }

	/**
	 * @brief Throws because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @throws std::runtime_error always.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * const bool hasIcon = app.icons().contains("toolbar/open");
	 * (void)hasIcon;
	 * @endcode
	 */
	bool contains(std::string_view key) const {
		(void)key;
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}
	bool contains(ResourceKey key) const { return contains(key.name); }

	/**
	 * @brief Throws because icon support is disabled.
	 *
	 * @param key Unused icon key.
	 * @throws std::runtime_error always.
	 *
	 * @code{.cpp}
	 * // Requires FLOWUI_INCLUDE_ICON_MANAGER enabled.
	 * FlowUi::TextureRef icon = app.icons().textureRef("toolbar/open");
	 * @endcode
	 */
	TextureRef textureRef(std::string_view key) {
		(void)key;
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}
	TextureRef textureRef(ResourceKey key) { return textureRef(key.name); }

private:
	friend class App;

	void init(detail::storage::IStorageSystem&, const IconManagerConfig&) {
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}

	void beginAppTick() {}

	void prepareFrameTextures(Clay_RenderCommandArray&, float, float) {}

	void destroy() noexcept {}
};

/** @} */
#endif

} // namespace FlowUi
