#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <span>
#include <vector>

#include "devSystems/devTooling/catalogue/DevCatalogLease.hpp"
#include "devSystems/devTooling/catalogue/DevCatalogTypes.hpp"

namespace FlowUi {
class ActionManager;
struct FontManager;
struct IconManager;
class ImageManager;
class ThemeManager;
namespace detail::storage { class IStorageSystem; }
namespace devSystems { class DevTimingRecorder; class MemorySampleSink; namespace tooling { struct DevTreeSnapshot; } }
}

namespace FlowUi::devSystems::tooling {

class DevSchemaRegistry;

/** @brief Lazy, observation-only catalogues over borrowed app managers. Query spans and entry strings expire when their cache or source manager changes; retain texture leases only for displayed resources. */
class DevCatalogues {
public:
	/** @brief Construct empty caches with no connected managers. */
	DevCatalogues() = default;
	/** @brief Release catalogue caches; connected managers remain caller-owned. */
	~DevCatalogues() = default;
	DevCatalogues(const DevCatalogues&) = delete;
	DevCatalogues& operator=(const DevCatalogues&) = delete;

	/** @brief Attach borrowed manager/registry pointers and invalidate dependent caches; dependencies must outlive subsequent queries. */
	void bindManagers(::FlowUi::detail::storage::IStorageSystem* storage, ThemeManager* themes,
					  ActionManager* actions, FontManager* fonts, IconManager* icons,
					  ImageManager* images, DevSchemaRegistry* schemaRegistry) noexcept;
	/** @brief Attach a borrowed timing recorder; null disables optional query attribution. */
	void setDevTimingRecorder(devSystems::DevTimingRecorder* recorder) noexcept {
		timingRecorder_ = recorder;
	}

	/** @brief Borrow the lazily refreshed image catalogue; queryFailed(Images) distinguishes failure from an empty result. */
	[[nodiscard]] std::span<const DevImageCatalogEntry> queryImages() noexcept;
	/** @brief Borrow the lazily refreshed icon catalogue; queryFailed(Icons) distinguishes failure from an empty result. */
	[[nodiscard]] std::span<const DevIconCatalogEntry> queryIcons() noexcept;
	/** @brief Borrow the lazily refreshed atlas catalogue; texture previews require an inspection lease while displayed. */
	[[nodiscard]] std::span<const DevAtlasCatalogEntry> queryAtlases() noexcept;
	/** @brief Borrow the lazily refreshed font catalogue; source strings remain owned by their managers. */
	[[nodiscard]] std::span<const DevFontCatalogEntry> queryFonts() noexcept;
	/** @brief Borrow callable-free action metadata; querying never invokes actions or captures their resources. */
	[[nodiscard]] std::span<const DevActionCatalogEntry> queryActions() noexcept;
	/** @brief Borrow reflected variant metadata; payload pointers expire on source theme mutation. */
	[[nodiscard]] std::span<const DevThemeCatalogEntry> queryThemes() noexcept;
	/** @brief Borrow definition metadata and optional completed-tree instance counts; currentTree is borrowed for the cache lifetime. */
	[[nodiscard]] std::span<const DevElementCatalogEntry> queryElements(
		const tooling::DevTreeSnapshot* currentTree) noexcept;
	/** @brief Borrow cached source revision counters until the next refresh. */
	[[nodiscard]] const DevCatalogueRevisions& revisions() const noexcept {
		return cachedRevisions_;
	}
	/** @brief Return the source revision used by the current image cache. */
	[[nodiscard]] std::uint64_t imageRevision() const noexcept { return imageRevision_; }
	/** @brief Return the source revision used by the current icon cache. */
	[[nodiscard]] std::uint64_t iconRevision() const noexcept { return iconRevision_; }
	/** @brief Return the source revision used by the current action cache. */
	[[nodiscard]] std::uint64_t actionRevision() const noexcept { return actionRevision_; }
	/** Distinguish a failed observation from a successfully observed empty collection. */
	[[nodiscard]] bool queryFailed(DevCatalogueQuery query) const noexcept {
		return (failedQueries_ & (uint32_t{1} << static_cast<uint8_t>(query))) != 0;
	}

	/** @brief Retain the catalogue's current inspection texture bindings; an unavailable/empty texture set yields an empty lease. */
	[[nodiscard]] DevCatalogLease acquireInspectionLease() noexcept;
	/** Retain only the exact visible or focused texture set supplied by the interface. */
	[[nodiscard]] DevCatalogLease acquireInspectionLease(std::span<const TextureHandle> textures) noexcept;
	/** @brief Mark every observation cache stale; source application resources remain unchanged. */
	void invalidateAllCaches() noexcept;
	/** @brief Return retained catalogue backing storage in bytes. */
	[[nodiscard]] std::size_t memoryFootprintBytes() const noexcept;

private:
	void rebuildImageCache();
	void rebuildIconCache();
	void rebuildAtlasCache();
	void rebuildFontCache();
	void rebuildActionCache();
	void rebuildThemeCache();
	void rebuildElementCache(const tooling::DevTreeSnapshot* currentTree);

	::FlowUi::detail::storage::IStorageSystem* storage_ = nullptr;
	ThemeManager* themeManager_ = nullptr;
	ActionManager* actionManager_ = nullptr;
	FontManager* fontManager_ = nullptr;
	IconManager* iconManager_ = nullptr;
	ImageManager* imageManager_ = nullptr;
	DevSchemaRegistry* schemaRegistry_ = nullptr;
	devSystems::DevTimingRecorder* timingRecorder_ = nullptr;
	DevCatalogueRevisions cachedRevisions_{};
	std::uint64_t imageRevision_ = UINT64_MAX;
	std::uint64_t iconRevision_ = UINT64_MAX;
	std::uint64_t atlasRevision_ = UINT64_MAX;
	std::uint64_t actionRevision_ = UINT64_MAX;
	std::uint64_t themeRevision_ = UINT64_MAX;
	uint32_t failedQueries_ = 0;
	const tooling::DevTreeSnapshot* cachedElementTree_ = nullptr;
	std::vector<DevImageCatalogEntry> cachedImages_{};
	std::vector<DevIconCatalogEntry> cachedIcons_{};
	std::vector<DevAtlasCatalogEntry> cachedAtlases_{};
	std::vector<DevFontCatalogEntry> cachedFonts_{};
	std::vector<DevActionCatalogEntry> cachedActions_{};
	std::vector<DevThemeCatalogEntry> cachedThemes_{};
	std::vector<DevElementCatalogEntry> cachedElements_{};
};

} // namespace FlowUi::devSystems::tooling

#endif
