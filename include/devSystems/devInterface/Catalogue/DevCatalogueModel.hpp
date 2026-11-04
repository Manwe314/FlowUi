#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Inspect/DevThemeEditorModel.hpp"
#include "devSystems/devTooling/catalogue/DevCatalogLease.hpp"
#include "devSystems/devTooling/catalogue/DevCatalogTypes.hpp"
#include "managers/ShortcutManager.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems {
struct DevInterfaceState;

enum class DevCatalogueDomain : uint8_t {
	Elements,
	Themes,
	Icons,
	Fonts,
	Images,
	Actions,
	Shortcuts,
	Viewports,
	Atlases,
	Count
};
enum class DevCatalogueCommand : uint8_t {
	None,
	Refresh,
	RasterizeIcon,
	RemovePreviewRasters,
	CaptureViewport
};

/** Copied metadata for an immutable, completed viewport image. */
struct DevCatalogueViewportObservation {
	TextureHandle texture{}, source_texture{};
	uint64_t frame = 0, generation = 0, submission = 0;
	uint32_t width = 0, height = 0, format = 0;
	bool supported = false, pending = false, failed = false;
};

/** Visible capture subscriptions are owned by DevInterface. */
struct DevCatalogueViewportRequest {
	std::string key{};
	DevCatalogueViewportObservation observation{};
	TextureHandle retiring_texture{};
	WindowId window = InvalidWindowId;
	uint64_t identity = 0;
	uint64_t refresh_after_submission = 0;
	bool refresh_pending = false;
};

/** Owned display record: no manager-owned string views or payload pointers survive refresh. */
struct DevCatalogueAsset {
	std::vector<std::pair<std::string, std::string>> facts{};
	std::vector<devMode::DevIconCatalogEntry> icons{};
	std::string name{}, group{}, source{}, status{}, detail{};
	devMode::DevImageCatalogEntry image{};
	devMode::DevIconCatalogEntry icon{};
	devMode::DevAtlasCatalogEntry atlas{};
	devMode::DevFontCatalogEntry font{};
	devMode::DevActionCatalogEntry action{};
	devMode::DevThemeCatalogEntry theme{};
	devMode::DevElementCatalogEntry element{};
	ShortcutManager::DevShortcutView shortcut{};
	DevCatalogueViewportObservation viewport{};
	WindowId window = InvalidWindowId;
	uint64_t identity = 0, generation = 0;
	DevCatalogueDomain domain = DevCatalogueDomain::Elements;
};

/** Viewing settings are local to each domain and never modify the registered asset. */
struct DevCatalogueDomainState {
	std::string search{}, group{}, glyph_search{};
	std::vector<std::string> collapsed_groups{};
	std::vector<uint64_t> expanded_icons{};
	float selector_scroll = 0, workbench_scroll = 0, zoom = 1, pan_x = 0, pan_y = 0;
	uint64_t background = 0, view = 0, raster_step = 3, page = 0;
	uint64_t raster_identity = 0;
	uint64_t selected_glyph = UINT32_MAX;
	uint64_t shortcut_context = 0, shortcut_transition = 0;
	bool fit = true, tint = false;
	bool follow_observed_output = false;
};

/** Exact rasters created by explicit Catalogue requests; existing app rasters are never owned. */
struct DevCataloguePreviewRaster {
	std::string icon_name{};
	TextureHandle texture{};
	uint32_t width = 0, height = 0;
};

/** Submitted keyboard targets are independent of optional developer tree capture. */
struct DevCatalogueFocusTarget {
	Clay_ElementId identity{}, viewport{};
	FlowElementID text_input{};
};

/** DevInterface owns selection, copied observations and preview-resource bookkeeping. */
struct DevCatalogueState {
	std::vector<DevCatalogueAsset> assets{};
	std::vector<size_t> visible_assets{};
	std::vector<DevCataloguePreviewRaster> preview_rasters{};
	std::vector<TextureHandle> visible_textures{};
	std::vector<DevCatalogueFocusTarget> focus_targets{};
	std::vector<uint64_t> visible_viewports{};
	std::vector<DevCatalogueViewportRequest> viewport_requests{};
	interface_elements::theme_editor::ThemeEditorModel theme_model{};
	devMode::DevCatalogLease lease{};
	std::array<DevCatalogueDomainState, 9> domains{};
	std::array<size_t, 9> counts{};
	std::array<std::string, 9> domain_status{};
	std::string selected_name{}, message{},
		specimen_text = "Il1 O0 0123456789 — The quick brown fox.";
	uint64_t domain = 0, selected_identity = 0, pending_identity = 0, pending_size = 24;
	uint64_t last_refresh_ns = 0, revision = 0, focused_identity = 0, focused_domain = 0;
	uint64_t inspector_section = 0;
	float domain_height = 290, inspector_scroll = 0;
	Clay_ElementId registration_viewport{};
	DevCatalogueViewportObservation frozen_viewport{};
	uint64_t frozen_viewport_identity = 0;
	WindowId observed_window = InvalidWindowId;
	DevCatalogueCommand pending = DevCatalogueCommand::None;
	bool initialized = false, selected_missing = false, selected_outside_filter = false;
	bool dirty = true, message_is_error = false, keyboard_active = false, domain_focus = true;
	bool reveal_selection = false, inspector_drawer = false, selector_drawer = false;
	bool theme_observation_available = true;
};

inline constexpr std::array<uint32_t, 7> catalogue_icon_steps{12, 16, 20, 24, 32, 48, 64};
/** Human-readable stable domain label. */
[[nodiscard]] std::string_view catalogue_domain_name(DevCatalogueDomain domain) noexcept;
/** Open the domain overview, retaining its own filter and view settings. */
void select_catalogue_domain(DevCatalogueState& state, DevCatalogueDomain domain) noexcept;
/** Select the same canonical leaf from either the browser or a gallery card. */
void select_catalogue_asset(DevCatalogueState& state, uint64_t identity);
/** Recompute the filtered, stable-sorted overview indices without manager access. */
void rebuild_catalogue_filter(DevCatalogueState& state);
/** The selected copied observation, or null for overview/removal. */
[[nodiscard]] const DevCatalogueAsset*
selected_catalogue_asset(const DevCatalogueState& state) noexcept;
/** Apply explicit requests and publish copied observations outside Clay construction. */
void service_catalogue(App& app, DevInterfaceState& interface_state, bool visible);
} // namespace FlowUi::devSystems
#endif
