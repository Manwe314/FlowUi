#pragma once

#include "FlowUi/BuildConfig.hpp"

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

namespace FlowUi::devMode {

class DevSchemaRegistry;

class DevCatalogues {
public:
	DevCatalogues() = default;
	~DevCatalogues() = default;
	DevCatalogues(const DevCatalogues&) = delete;
	DevCatalogues& operator=(const DevCatalogues&) = delete;

	void bindManagers(
		detail::storage::IStorageSystem* storage,
		ThemeManager* themes,
		ActionManager* actions,
		FontManager* fonts,
		IconManager* icons,
		ImageManager* images,
		DevSchemaRegistry* schemaRegistry) noexcept;
	void setDevTimingRecorder(devSystems::DevTimingRecorder* recorder) noexcept {
		timingRecorder_ = recorder;
	}

	[[nodiscard]] std::span<const DevImageCatalogEntry> queryImages() noexcept;
	[[nodiscard]] std::span<const DevIconCatalogEntry> queryIcons() noexcept;
	[[nodiscard]] std::span<const DevAtlasCatalogEntry> queryAtlases() noexcept;
	[[nodiscard]] std::span<const DevFontCatalogEntry> queryFonts() noexcept;
	[[nodiscard]] std::span<const DevActionCatalogEntry> queryActions() noexcept;
	[[nodiscard]] std::span<const DevThemeCatalogEntry> queryThemes() noexcept;
	[[nodiscard]] std::span<const DevElementCatalogEntry> queryElements(
		const devSystems::tooling::DevTreeSnapshot* currentTree) noexcept;

	[[nodiscard]] DevCatalogLease acquireInspectionLease() noexcept;
	void invalidateAllCaches() noexcept;
	[[nodiscard]] std::size_t memoryFootprintBytes() const noexcept;

private:
	void rebuildImageCache();
	void rebuildIconCache();
	void rebuildAtlasCache();
	void rebuildFontCache();
	void rebuildActionCache();
	void rebuildThemeCache();
	void rebuildElementCache(const devSystems::tooling::DevTreeSnapshot* currentTree);

	detail::storage::IStorageSystem* storage_ = nullptr;
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
	const devSystems::tooling::DevTreeSnapshot* cachedElementTree_ = nullptr;
	std::vector<DevImageCatalogEntry> cachedImages_{};
	std::vector<DevIconCatalogEntry> cachedIcons_{};
	std::vector<DevAtlasCatalogEntry> cachedAtlases_{};
	std::vector<DevFontCatalogEntry> cachedFonts_{};
	std::vector<DevActionCatalogEntry> cachedActions_{};
	std::vector<DevThemeCatalogEntry> cachedThemes_{};
	std::vector<DevElementCatalogEntry> cachedElements_{};
};

} // namespace FlowUi::devMode

#endif
