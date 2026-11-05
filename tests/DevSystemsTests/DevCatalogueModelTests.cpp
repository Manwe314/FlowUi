#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Catalogue/DevCatalogueModel.hpp"
#include <algorithm>
#include <array>
#include <cassert>

namespace {
using namespace FlowUi;
using namespace FlowUi::devSystems;

void verify_pinned_selection_and_domain_filters() {
	DevCatalogueState state;
	state.assets.emplace_back(DevCatalogueAsset{.name = "toolbar/save",
												.group = "toolbar",
												.identity = 101,
												.domain = DevCatalogueDomain::Icons});
	state.assets.emplace_back(DevCatalogueAsset{.name = "toolbar/open",
												.group = "toolbar",
												.identity = 102,
												.domain = DevCatalogueDomain::Icons});
	state.assets.emplace_back(DevCatalogueAsset{.name = "Portrait",
												.group = "photos",
												.identity = 103,
												.domain = DevCatalogueDomain::Images});
	select_catalogue_domain(state, DevCatalogueDomain::Icons);
	rebuild_catalogue_filter(state);
	assert(state.visible_assets.size() == 2);
	select_catalogue_asset(state, 101);
	auto& icons = state.domains[static_cast<size_t>(DevCatalogueDomain::Icons)];
	icons.search = "OPEN";
	rebuild_catalogue_filter(state);
	assert(state.visible_assets.size() == 1);
	assert(state.assets[state.visible_assets.front()].identity == 102);
	assert(state.selected_identity == 101 && state.selected_outside_filter);
	assert(selected_catalogue_asset(state)->name == "toolbar/save");
	std::reverse(state.assets.begin(), state.assets.end());
	rebuild_catalogue_filter(state);
	assert(selected_catalogue_asset(state)->identity == 101);
	icons.selector_scroll = -75;
	icons.zoom = 2;
	select_catalogue_domain(state, DevCatalogueDomain::Images);
	assert(state.selected_identity == 0);
	state.domains[state.domain].search = "photos";
	rebuild_catalogue_filter(state);
	assert(state.visible_assets.size() == 1);
	select_catalogue_domain(state, DevCatalogueDomain::Icons);
	rebuild_catalogue_filter(state);
	assert(icons.search == "OPEN" && icons.selector_scroll == -75 && icons.zoom == 2);
	assert(state.selected_identity == 0 && state.visible_assets.size() == 1);
	select_catalogue_asset(state, 101);
	std::erase_if(state.assets, [](const auto& asset) { return asset.identity == 101; });
	assert(selected_catalogue_asset(state) == nullptr);
	assert(state.selected_identity == 101 && state.selected_name == "toolbar/save");
}

void verify_group_filter_and_explicit_raster_ownership() {
	DevCatalogueState state;
	state.assets.emplace_back(DevCatalogueAsset{
		.name = "save", .group = "toolbar", .identity = 11, .domain = DevCatalogueDomain::Icons});
	state.assets.emplace_back(DevCatalogueAsset{
		.name = "warning", .group = "status", .identity = 12, .domain = DevCatalogueDomain::Icons});
	select_catalogue_domain(state, DevCatalogueDomain::Icons);
	state.domains[state.domain].group = "status";
	rebuild_catalogue_filter(state);
	assert(state.visible_assets.size() == 1 &&
		   state.assets[state.visible_assets.front()].identity == 12);
	assert(state.preview_rasters.empty());
	select_catalogue_asset(state, 11);
	assert(state.preview_rasters.empty());
	assert(state.pending == DevCatalogueCommand::None);
	assert(state.selected_outside_filter);
	select_catalogue_domain(state, DevCatalogueDomain::Icons);
	assert(state.domains[state.domain].group.empty());
	assert(state.selected_identity == 0);
}

void verify_pure_shortcut_dispatch_trace() {
	ShortcutManager::DevShortcutView global{.chord = {.key = 67, .ctrl = true},
											.order = 1,
											.priority = 1000,
											.scope = ShortcutScope::Global};
	auto local = global;
	local.scope = ShortcutScope::FocusedInput;
	local.priority = 1;
	local.order = 2;
	assert(ShortcutManager::devMatches(global, global.chord, false, false));
	assert(!ShortcutManager::devMatches(local, global.chord, false, true));
	assert(ShortcutManager::devMatches(local, global.chord, true, false));
	assert(ShortcutManager::devPrecedes(local, global));
	auto shifted = global.chord;
	shifted.shift = true;
	assert(!ShortcutManager::devMatches(global, shifted, true, true));
	auto released = global.chord;
	released.trigger = ShortcutTrigger::Release;
	assert(!ShortcutManager::devMatches(global, released, true, true));
	auto later = global;
	later.order = 2;
	assert(ShortcutManager::devPrecedes(global, later));
	auto higher_priority = later;
	higher_priority.priority = 1001;
	assert(ShortcutManager::devPrecedes(higher_priority, global));
}
void verify_group_descendant_filter() {
	DevCatalogueState state;
	constexpr std::array<std::string_view, 8> groups{
		"toolbar",		  "toolbar/editor",		   "toolbar/editor/tools",
		"toolbar::menus", "toolbar::menus::popup", "toolbarish",
		"toolbar-other",  "toolbar:legacy"};
	state.assets.reserve(groups.size());
	for (size_t group_index = 0; group_index < groups.size(); ++group_index)
		state.assets.emplace_back(DevCatalogueAsset{.name = std::string(groups[group_index]),
													.group = std::string(groups[group_index]),
													.identity = group_index + 1,
													.domain = DevCatalogueDomain::Icons});
	select_catalogue_domain(state, DevCatalogueDomain::Icons);
	auto& icons = state.domains[state.domain];
	icons.group = "toolbar";
	rebuild_catalogue_filter(state);
	assert(state.visible_assets.size() == 5);
	for (size_t visible_index = 0; visible_index < state.visible_assets.size(); ++visible_index)
		assert(state.assets[state.visible_assets[visible_index]].identity == visible_index + 1);
	icons.group = "toolbar/editor";
	rebuild_catalogue_filter(state);
	assert(state.visible_assets.size() == 2);
	assert(state.assets[state.visible_assets[0]].identity == 2);
	assert(state.assets[state.visible_assets[1]].identity == 3);
	icons.group = "toolbar::menus";
	rebuild_catalogue_filter(state);
	assert(state.visible_assets.size() == 2);
	assert(state.assets[state.visible_assets[0]].identity == 4);
	assert(state.assets[state.visible_assets[1]].identity == 5);
	icons.search = "popup";
	rebuild_catalogue_filter(state);
	assert(state.visible_assets.size() == 1);
	assert(state.assets[state.visible_assets.front()].identity == 5);
	icons.search.clear();
	icons.group.clear();
	rebuild_catalogue_filter(state);
	assert(state.visible_assets.size() == groups.size());
}
} // namespace
int main() {
	verify_pinned_selection_and_domain_filters();
	verify_group_filter_and_explicit_raster_ownership();
	verify_group_descendant_filter();
	verify_pure_shortcut_dispatch_trace();
}
