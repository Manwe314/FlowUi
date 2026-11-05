#include "devSystems/devInterface/Catalogue/DevCatalogueModel.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "managers/IconManager.hpp"
#include "managers/ShortcutManager.hpp"
#include "managers/UiManager.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <unordered_map>

namespace FlowUi::devSystems {
namespace {
using Domain = DevCatalogueDomain;
constexpr size_t domain_index(Domain domain) noexcept {
	return static_cast<size_t>(domain);
}
uint64_t identity(Domain domain, std::string_view key) noexcept {
	uint64_t result = 1469598103934665603ull ^ static_cast<uint64_t>(domain);
	for (const auto character : key)
		result = (result ^ static_cast<unsigned char>(character)) * 1099511628211ull;
	return result ? result : 1;
}
std::string namespace_group(std::string_view name, std::string_view fallback) {
	auto separator = name.find_last_of("/:");
	if (separator != std::string_view::npos && separator > 0 && name[separator] == ':' &&
		name[separator - 1] == ':')
		--separator;
	return separator == std::string_view::npos ? std::string(fallback)
											   : std::string(name.substr(0, separator));
}
[[nodiscard]] bool group_contains(std::string_view selected_group,
								  std::string_view asset_group) noexcept {
	if (selected_group.empty() || asset_group == selected_group)
		return true;
	if (!asset_group.starts_with(selected_group))
		return false;
	const auto descendant = asset_group.substr(selected_group.size());
	return descendant.starts_with('/') || descendant.starts_with("::");
}
bool matches(std::string_view text, std::string_view search) noexcept {
	return std::ranges::search(
			   text, search,
			   [](char left, char right) {
				   return std::tolower(static_cast<unsigned char>(left)) ==
						  std::tolower(static_cast<unsigned char>(right));
			   }).begin() != text.end() ||
		   search.empty();
}
void fact(DevCatalogueAsset& asset, std::string label, std::string value) {
	asset.facts.emplace_back(std::move(label), std::move(value));
}
std::string dimensions(uint32_t width, uint32_t height) {
	return std::to_string(width) + " x " + std::to_string(height);
}
[[nodiscard]] std::string display_type_name(std::string_view name) {
	if (const auto marker = name.find("T = "); marker != std::string_view::npos) {
		name.remove_prefix(marker + 4);
		name = name.substr(0, name.find_first_of(";]"));
	}
	if (name.starts_with("struct "))
		name.remove_prefix(7);
	if (name.starts_with("class "))
		name.remove_prefix(6);
	if (name.starts_with("FlowUi::"))
		name.remove_prefix(8);
	return std::string(name);
}
std::string resource_status(devMode::DevResourceStatus status) {
	switch (status) {
	case devMode::DevResourceStatus::Unloaded:
		return "Unloaded";
	case devMode::DevResourceStatus::Loading:
		return "Loading";
	case devMode::DevResourceStatus::Ready:
		return "Ready";
	case devMode::DevResourceStatus::Failed:
		return "Failed";
	case devMode::DevResourceStatus::Retiring:
		return "Retiring";
	}
	return "Unknown";
}
DevCatalogueAsset& append(std::vector<DevCatalogueAsset>& assets, Domain domain, std::string name,
						  std::string group, std::string_view stable = {}) {
	assets.emplace_back();
	auto& asset = assets.back();
	asset.identity = identity(domain, stable.empty() ? std::string_view(name) : stable);
	asset.domain = domain;
	asset.name = std::move(name);
	asset.group = std::move(group);
	return asset;
}
std::string shortcut_chord(const ShortcutChord& chord) {
	std::string result;
	if (chord.ctrl)
		result += "Ctrl+";
	if (chord.shift)
		result += "Shift+";
	if (chord.alt)
		result += "Alt+";
	if (chord.super)
		result += "Super+";
	if (chord.key >= 32 && chord.key <= 126)
		result += static_cast<char>(chord.key);
	else
		result += "Key " + std::to_string(chord.key);
	return result;
}
void collect(App& app, DevInterfaceState& interface_state) {
	auto& state = interface_state.catalogue;
	auto& catalogues = app.devTooling().catalogues();
	state.domain_status.fill({});
	const auto theme_status = state.theme_model.refresh(app);
	if (!theme_status)
		state.domain_status[domain_index(Domain::Themes)] =
			"Some theme tokens could not be observed";
	std::vector<DevCatalogueAsset> assets;
	assets.reserve(state.assets.capacity());
	const auto window = interface_state.selectedWindowId;
	if (state.observed_window != InvalidWindowId && state.observed_window != window) {
		const auto* selected = selected_catalogue_asset(state);
		if (selected &&
			(selected->domain == Domain::Shortcuts || selected->domain == Domain::Viewports)) {
			state.selected_identity = 0;
			state.selected_name.clear();
			state.selected_missing = false;
			state.domains[state.domain].group.clear();
		}
		state.frozen_viewport = {};
		state.frozen_viewport_identity = 0;
	}
	state.observed_window = window;
	const auto* tree = app.hasWindow(window) ? &app.ui(window).devTreeSnapshot() : nullptr;
	for (const auto& entry : catalogues.queryElements(tree)) {
		auto& asset = append(assets, Domain::Elements, std::string(entry.definitionName),
							 "Registered definitions", std::to_string(entry.definition.value));
		asset.element = entry;
		asset.element.definitionName = {};
		asset.element.headerSourceFile = {};
		asset.source = entry.headerSourceFile;
		asset.status = "Registered definition";
		const auto adapters = app.devTooling().schemas().elementPreviews();
		const auto adapter = std::ranges::find(
			adapters, entry.definition, &devMode::DevSchemaRegistry::ElementPreview::definition);
		if (adapter != adapters.end()) {
			if (!adapter->set_name.empty())
				asset.group = adapter->set_name;
			asset.status =
				adapter->admitted ? "Passive default preview" : "Default preview not admitted";
		} else
			asset.status = "Preview adapter unavailable";
		asset.detail = "Effective defaults; preview requires a typed, admitted adapter.";
		fact(asset, "Definition", std::to_string(entry.definition.value));
		fact(asset, "Observed instances in selected window",
			 std::to_string(entry.activeInstanceCount));
		fact(asset, "Reflected parameter fields", std::to_string(entry.reflectedFieldCount));
		fact(asset, "Parameters type", std::to_string(entry.parametersTypeId));
		fact(asset, "State type", std::to_string(entry.stateTypeId));
	}
	for (const auto& entry : catalogues.queryThemes()) {
		const auto stable =
			std::to_string(entry.themeTypeId) + "/" + std::string(entry.variantName);
		auto& asset = append(assets, Domain::Themes, std::string(entry.variantName),
							 display_type_name(entry.typeName), stable);
		asset.theme = entry;
		asset.theme.typeName = {};
		asset.theme.variantName = {};
		asset.theme.payloadPointer = nullptr;
		asset.status = entry.isActive ? "Active variant" : "Registered variant";
		asset.generation = entry.revision;
		fact(asset, "Variant", std::string(entry.variantName));
		fact(asset, "Theme type", display_type_name(entry.typeName));
		fact(asset, "Revision", std::to_string(entry.revision));
		fact(asset, "Payload bytes", std::to_string(entry.payloadSizeBytes));
		fact(asset, "Reflected fields", std::to_string(entry.reflectedFieldCount));
	}
	std::unordered_map<std::string, size_t> logical_icons;
	for (const auto& entry : catalogues.queryIcons()) {
		const std::string name(entry.iconName);
		auto found = logical_icons.find(name);
		if (found == logical_icons.end()) {
			const auto index = assets.size();
			append(assets, Domain::Icons, name, namespace_group(name, "Registered icons"));
			found = logical_icons.emplace(name, index).first;
		}
		auto& asset = assets[found->second];
		if (entry.atlasTexture) {
			auto raster = entry;
			raster.iconName = {};
			raster.iconKey.name = {};
			asset.icons.emplace_back(raster);
		}
	}
	for (const auto& [name, index] : logical_icons) {
		auto& asset = assets[index];
		std::ranges::sort(asset.icons, {}, &devMode::DevIconCatalogEntry::targetWidth);
		if (!asset.icons.empty())
			asset.icon = asset.icons.front();
		asset.status = asset.icons.empty() ? "Registered · no cached raster"
										   : std::to_string(asset.icons.size()) + " cached sizes";
		asset.detail = "Only existing app rasters are displayed. Rasterize a preset size "
					   "explicitly to add a preview.";
		fact(asset, "Logical key", name);
		fact(asset, "Cached raster variants", std::to_string(asset.icons.size()));
		for (const auto& raster : asset.icons)
			fact(asset, dimensions(raster.targetWidth, raster.targetHeight),
				 "Page " + std::to_string(raster.atlasLayer) + " · texture " +
					 std::to_string(raster.atlasTexture.packed()));
	}
	for (const auto& entry : catalogues.queryFonts()) {
		const auto stable = std::to_string(entry.familyHandle) + "/" +
							std::to_string(entry.fontHandle) + "/" + std::to_string(entry.weight) +
							(entry.isItalic ? "i" : "n");
		auto& asset =
			append(assets, Domain::Fonts,
				   std::string(entry.faceName.empty() ? entry.familyName : entry.faceName) + " · " +
					   std::to_string(entry.weight) + (entry.isItalic ? " Italic" : ""),
				   std::string(entry.familyName), stable);
		asset.font = entry;
		asset.font.familyName = {};
		asset.font.faceName = {};
		asset.font.sourcePath = {};
		asset.source = entry.sourcePath;
		asset.status = std::to_string(entry.loadedGlyphCount) + " loaded glyphs";
		asset.detail = "Exact registered face. Loaded coverage is distinct from the original "
					   "font's complete coverage.";
		fact(asset, "Font ID", std::to_string(entry.fontHandle));
		fact(asset, "Family ID", std::to_string(entry.familyHandle));
		fact(asset, "Baked pixel size", std::to_string(entry.emSizePoints));
		fact(asset, "Ascender (design units)", std::to_string(entry.ascent));
		fact(asset, "Descender (design units)", std::to_string(entry.descent));
		fact(asset, "Line gap (design units)", std::to_string(entry.lineGap));
		fact(asset, "Atlas layer", std::to_string(entry.atlasLayer));
	}
	for (const auto& entry : catalogues.queryImages()) {
		auto& asset = append(assets, Domain::Images, std::string(entry.key.name),
							 namespace_group(entry.key.name, "Registered images"));
		asset.image = entry;
		asset.image.key.name = {};
		asset.image.sourcePathOrDebugName = {};
		asset.source = entry.sourcePathOrDebugName;
		asset.status = resource_status(entry.status);
		asset.generation = entry.textureHandle.packed();
		fact(asset, "Intrinsic texels", dimensions(entry.width, entry.height));
		fact(asset, "Uploaded Vulkan format", std::to_string(entry.formatVulkan));
		fact(asset, "Reported GPU allocation bytes", std::to_string(entry.gpuMemoryBytes));
		fact(asset, "Texture generation", std::to_string(entry.textureHandle.packed()));
		fact(asset, "Representation", "Decoded RGBA8 sRGB");
	}
	for (const auto& entry : catalogues.queryActions()) {
		const auto kind = entry.kind == devMode::DevActionKind::AppActionBinding
							  ? "Application bindings"
							  : "UI recipes";
		const auto stable = std::string(kind) + "/" + std::to_string(entry.stableId);
		auto& asset = append(assets, Domain::Actions,
							 entry.debugName.empty() ? std::to_string(entry.stableId)
													 : std::string(entry.debugName),
							 kind, stable);
		asset.action = entry;
		asset.action.debugName = {};
		asset.action.definitionSource = {};
		asset.action.actionId.debugName = {};
		asset.source = entry.definitionSource;
		asset.status = entry.isBound ? "Bound" : "Unbound";
		asset.detail = "Contract observation only. Browsing never invokes this action.";
		fact(asset, "Stable identity", std::to_string(entry.stableId));
		fact(asset, "Kind", kind);
		fact(asset, "Availability flags", std::to_string(entry.availabilityFlags));
		fact(asset, "Reconstructable", entry.isReconstructable ? "Yes" : "No");
		fact(asset, "Observed lifetime invocations", std::to_string(entry.lifetimeInvocations));
		fact(asset, "Callable type hash", std::to_string(entry.callableTypeHash));
		fact(asset, "Result type hash", std::to_string(entry.resultTypeHash));
	}
	if (app.hasWindow(window)) {
		struct ShortcutContext {
			std::vector<DevCatalogueAsset>* assets;
			WindowId window;
		} shortcut_context{&assets, window};
		const auto observed = app.ui(window).shortcuts().visitDevShortcuts(
			&shortcut_context, [](void* target, const ShortcutManager::DevShortcutView& entry) {
				const auto& context = *static_cast<ShortcutContext*>(target);
				const auto scope = entry.scope == ShortcutScope::FocusedInput ? "Focused input"
								   : entry.scope == ShortcutScope::FocusedElement
									   ? "Focused element"
									   : "Global";
				const auto stable = std::to_string(context.window) + "/" +
									std::to_string(entry.epoch) + "/" + std::to_string(entry.id);
				auto& asset = append(*context.assets, Domain::Shortcuts,
									 shortcut_chord(entry.chord), scope, stable);
				asset.shortcut = entry;
				asset.shortcut.action.debugName = {};
				asset.window = context.window;
				asset.status = entry.callback ? "Callback · handled result is conditional"
											  : "Application action";
				asset.detail = "Focused input → focused element → global; descending priority, "
							   "then registration order. Modifiers match exactly.";
				fact(asset, "Window", std::to_string(context.window));
				fact(asset, "Registration",
					 std::to_string(entry.id) + " · epoch " + std::to_string(entry.epoch));
				fact(asset, "Scope", scope);
				fact(asset, "Priority", std::to_string(entry.priority));
				fact(asset, "Registration order", std::to_string(entry.order));
				fact(asset, "Trigger",
					 entry.chord.trigger == ShortcutTrigger::Press	   ? "Press"
					 : entry.chord.trigger == ShortcutTrigger::Release ? "Release"
																	   : "Down");
				fact(asset, "Handling",
					 entry.handling == ShortcutHandling::Consume ? "Consume if handled"
																 : "Pass through");
				fact(asset, "Target",
					 entry.callback ? "Callback"
									: "App action " + std::to_string(entry.action.value));
				return true;
			});
		if (!observed)
			state.domain_status[domain_index(Domain::Shortcuts)] =
				"Shortcut manager observation unavailable";
	}
	for (const auto& entry : app.devViewportSnapshot(window)) {
		const auto stable = std::to_string(window) + "/" + entry.key;
		auto& asset =
			append(assets, Domain::Viewports, entry.key, "Selected application window", stable);
		asset.generation = entry.generation;
		asset.window = window;
		asset.viewport = {.texture = entry.capture_texture,
						  .source_texture = entry.capture_texture,
						  .frame = entry.captured_frame,
						  .generation = entry.captured_generation,
						  .submission = entry.captured_submission,
						  .width = entry.captured_width,
						  .height = entry.captured_height,
						  .format = entry.captured_format,
						  .supported = entry.capture_supported,
						  .pending = entry.capture_pending,
						  .failed = entry.capture_failed};
		asset.status = entry.capture_texture
						   ? "Captured frame " + std::to_string(entry.captured_frame)
					   : entry.capture_failed	  ? "Capture failed"
					   : !entry.capture_supported ? "Output format is not supported for capture"
					   : entry.capture_pending	  ? "Waiting for a natural source render"
												  : "Not yet captured";
		asset.detail = "Immutable completed output. Capture waits for the source's natural render; "
					   "browsing never invokes its callback or changes its extent.";
		fact(asset, "Window", std::to_string(window));
		fact(asset, "Observed extent", dimensions(entry.width, entry.height));
		fact(asset, "Resource generation", std::to_string(entry.generation));
		fact(asset, "Vulkan format", std::to_string(entry.format));
		fact(asset, "Render callback", entry.has_callback ? "Present" : "Absent");
		fact(asset, "Clear each render", entry.clear_every_frame ? "Yes" : "No");
		if (entry.capture_texture) {
			fact(asset, "Captured frame", std::to_string(entry.captured_frame));
			fact(asset, "Captured generation", std::to_string(entry.captured_generation));
			fact(asset, "Completed copy submission", std::to_string(entry.captured_submission));
			fact(asset, "Captured extent", dimensions(entry.captured_width, entry.captured_height));
		}
	}
	for (const auto& entry : catalogues.queryAtlases()) {
		const bool icon = entry.kind == devMode::DevAtlasKind::IconVectorAtlas;
		const uint32_t page_count = icon ? 1u : std::max(1u, entry.allocatedRegions);
		for (uint32_t page = 0; page < page_count; ++page) {
			const auto page_index = icon ? entry.pageIndex : page;
			const auto stable = std::string(icon ? "icon/" : "font/") + std::to_string(page_index);
			auto& asset = append(assets, Domain::Atlases,
								 std::string(entry.debugName) + (icon ? " · page " : " · layer ") +
									 std::to_string(page_index),
								 icon ? "Icon pages" : "Font layers", stable);
			asset.atlas = entry;
			asset.atlas.pageIndex = page_index;
			asset.generation = entry.atlasTexture.packed() ^ entry.atlasImageView.packed();
			asset.atlas.atlasKey.name = {};
			asset.atlas.debugName = {};
			asset.status = "Storage observation";
			asset.detail = icon ? "Icon atlas region map; pages are storage, not individual icons."
								: "MSDF storage requires font reconstruction; raw color previews "
								  "are unavailable.";
			fact(asset, "Page dimensions", dimensions(entry.width, entry.height));
			fact(asset, "Layers", std::to_string(entry.layerCount));
			fact(asset, icon ? "Page" : "Layer", std::to_string(page_index));
			if (!icon) {
				const auto owner =
					std::ranges::find_if(assets, [page_index](const auto& candidate) {
						return candidate.domain == Domain::Fonts &&
							   candidate.font.atlasLayer == page_index;
					});
				if (owner != assets.end()) {
					asset.font = owner->font;
					fact(asset, "Font owner", owner->name);
				}
				fact(asset, "Allocation accounting", "Shared atlas image across font layers");
			}
			fact(asset, icon ? "Allocated regions" : "Used layers",
				 std::to_string(entry.allocatedRegions));
			fact(asset, "Reported allocation bytes", std::to_string(entry.gpuMemoryBytes));
			fact(asset, icon ? "Occupied area ratio" : "Used layer ratio",
				 std::to_string(entry.occupancyRatio));
		}
	}
	static constexpr std::array query_domains{Domain::Images,  Domain::Icons,	Domain::Atlases,
											  Domain::Fonts,   Domain::Actions, Domain::Themes,
											  Domain::Elements};
	for (size_t query_index = 0; query_index < query_domains.size(); ++query_index) {
		if (!catalogues.queryFailed(static_cast<devMode::DevCatalogueQuery>(query_index)))
			continue;
		const auto domain = query_domains[query_index];
		state.domain_status[domain_index(domain)] = "Observation failed; previous data retained";
		for (const auto& previous : state.assets)
			if (previous.domain == domain)
				assets.emplace_back(previous);
	}
#if !FLOWUI_INCLUDE_ICON_MANAGER
	state.domain_status[domain_index(Domain::Icons)] = "Icon manager is disabled in this build";
#endif
	std::ranges::sort(assets, [](const auto& left, const auto& right) {
		if (left.domain != right.domain)
			return left.domain < right.domain;
		if (left.group != right.group)
			return left.group < right.group;
		if (left.name != right.name)
			return left.name < right.name;
		return left.identity < right.identity;
	});
	state.assets = std::move(assets);
	std::erase_if(state.preview_rasters, [&](const auto& owned) {
		return std::ranges::none_of(state.assets, [&](const auto& asset) {
			return asset.domain == Domain::Icons && asset.name == owned.icon_name &&
				   std::ranges::any_of(asset.icons, [&](const auto& raster) {
					   return raster.atlasTexture == owned.texture;
				   });
		});
	});
	state.counts.fill(0);
	for (const auto& asset : state.assets)
		++state.counts[domain_index(asset.domain)];
	state.selected_missing = state.selected_identity && !selected_catalogue_asset(state);
	++state.revision;
	state.initialized = true;
	rebuild_catalogue_filter(state);
}

void service_icon_command(App& app, DevCatalogueState& state) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	if (state.pending == DevCatalogueCommand::RasterizeIcon) {
		const auto found =
			std::ranges::find(state.assets, state.pending_identity, &DevCatalogueAsset::identity);
		if (found == state.assets.end() || found->domain != Domain::Icons) {
			state.message = "Icon is no longer registered";
			state.message_is_error = true;
			return;
		}
		const auto size = static_cast<uint32_t>(state.pending_size);
		if (std::ranges::find(catalogue_icon_steps, size) == catalogue_icon_steps.end()) {
			state.message = "Choose a supported raster step";
			state.message_is_error = true;
			return;
		}
		const auto cached = std::ranges::find_if(found->icons, [size](const auto& entry) {
			return entry.targetWidth == size && entry.targetHeight == size;
		});
		if (cached != found->icons.end()) {
			state.message = "This raster size is already cached";
			return;
		}
		state.preview_rasters.reserve(state.preview_rasters.size() + 1);
		DevCataloguePreviewRaster owned{.icon_name = found->name};
		const auto result = app.icons().devRasterize(found->name, size, size);
		if (!result) {
			state.message = "Could not rasterize the requested preview";
			state.message_is_error = true;
			return;
		}
		if (result->newlyRasterized) {
			owned.texture = result->texture;
			owned.width = result->targetWidth;
			owned.height = result->targetHeight;
			state.preview_rasters.emplace_back(std::move(owned));
		}
		state.message =
			result->newlyRasterized
				? "Created preview raster " + dimensions(result->targetWidth, result->targetHeight)
				: "This raster size is already cached";
	} else if (state.pending == DevCatalogueCommand::RemovePreviewRasters) {
		size_t removed = 0;
		std::erase_if(state.preview_rasters, [&](const auto& owned) {
			if (state.pending_identity &&
				identity(Domain::Icons, owned.icon_name) != state.pending_identity)
				return false;
			const auto result = app.icons().devRemoveRaster(owned.texture);
			if (!result) {
				state.message_is_error = true;
				return false;
			}
			if (*result)
				++removed;
			return true;
		});
		state.message = "Removed " + std::to_string(removed) + " preview rasters";
	}
#else
	state.message = "Icon manager is disabled in this build";
	state.message_is_error = true;
#endif
}
[[nodiscard]] bool release_viewport_request(App& app,
											DevCatalogueViewportRequest& request) noexcept {
	if (request.retiring_texture) {
		const auto released = app.release_dev_viewport_capture_texture(request.retiring_texture);
		if (!released)
			return false;
		request.retiring_texture = {};
	}
	if (request.observation.texture) {
		const auto released = app.release_dev_viewport_capture_texture(request.observation.texture);
		if (!released)
			return false;
		request.observation.texture = {};
	}
	app.release_dev_viewport_capture(request.window, request.key);
	return true;
}
void release_viewport_requests(App& app, DevCatalogueState& state) noexcept {
	std::erase_if(state.viewport_requests,
				  [&](auto& request) { return release_viewport_request(app, request); });
	state.visible_viewports.clear();
	state.frozen_viewport = {};
	state.frozen_viewport_identity = 0;
}

void service_viewport_requests(App& app, DevCatalogueState& state, bool refresh) {
	if (state.domain != static_cast<uint64_t>(Domain::Viewports)) {
		release_viewport_requests(app, state);
		return;
	}
	if (const auto* selected = selected_catalogue_asset(state);
		selected && selected->domain == Domain::Viewports) {
		state.visible_viewports.assign(1, selected->identity);
	} else {
		state.frozen_viewport = {};
		state.frozen_viewport_identity = 0;
		if (state.visible_viewports.size() > 16)
			state.visible_viewports.resize(16);
	}
	std::erase_if(state.viewport_requests, [&](auto& request) {
		const bool visible = std::ranges::find(state.visible_viewports, request.identity) !=
							 state.visible_viewports.end();
		const bool registered = std::ranges::any_of(state.assets, [&](const auto& asset) {
			return asset.identity == request.identity && asset.domain == Domain::Viewports;
		});
		return (!visible || !registered) && release_viewport_request(app, request);
	});
	state.viewport_requests.reserve(state.viewport_requests.size() +
									state.visible_viewports.size());
	const bool follow = state.domains[state.domain].follow_observed_output;
	for (const auto identity : state.visible_viewports) {
		const auto found = std::ranges::find(state.assets, identity, &DevCatalogueAsset::identity);
		if (found == state.assets.end() || found->domain != Domain::Viewports ||
			!found->viewport.supported)
			continue;
		auto owned = std::ranges::find(state.viewport_requests, identity,
									   &DevCatalogueViewportRequest::identity);
		if (owned == state.viewport_requests.end()) {
			state.viewport_requests.emplace_back(DevCatalogueViewportRequest{
				.key = found->name, .window = found->window, .identity = identity});
			owned = std::prev(state.viewport_requests.end());
		}
		bool can_replace = true;
		if (owned->retiring_texture) {
			const auto released = app.release_dev_viewport_capture_texture(owned->retiring_texture);
			if (released)
				owned->retiring_texture = {};
			else
				can_replace = false;
		}
		const auto backend = found->viewport;
		const bool explicit_request = state.pending == DevCatalogueCommand::CaptureViewport &&
									  state.pending_identity == identity;
		if (explicit_request) {
			owned->refresh_pending = true;
			owned->refresh_after_submission = backend.submission;
		}
		const bool request_output =
			explicit_request || (follow && refresh) || !backend.source_texture;
		if (request_output && !backend.pending && (explicit_request || !backend.failed)) {
			const auto requested = app.request_dev_viewport_capture(found->window, found->name);
			if (!requested) {
				state.message = "Could not request a completed viewport capture";
				state.message_is_error = true;
			}
		}
		const bool new_output =
			backend.source_texture && backend.source_texture != owned->observation.source_texture;
		const bool refreshed_output =
			owned->refresh_pending && backend.submission > owned->refresh_after_submission;
		if (can_replace && backend.source_texture &&
			(!owned->observation.texture || (new_output && (follow || refreshed_output)))) {
			const auto retained =
				app.retain_dev_viewport_capture(found->window, found->name, backend.source_texture);
			if (retained) {
				bool accepted = true;
				if (owned->observation.texture) {
					const auto released =
						app.release_dev_viewport_capture_texture(owned->observation.texture);
					if (!released) {
						accepted = false;
						const auto rollback = app.release_dev_viewport_capture_texture(*retained);
						if (!rollback)
							owned->retiring_texture = *retained;
						state.message = "Could not retire the previous viewport preview";
						state.message_is_error = true;
					}
				}
				if (accepted) {
					owned->observation = backend;
					owned->observation.texture = *retained;
					if (backend.submission > owned->refresh_after_submission)
						owned->refresh_pending = false;
				}
			} else {
				state.message = "Completed viewport output changed before it could be retained";
				state.message_is_error = true;
			}
		}
		if (owned->observation.texture) {
			found->viewport = owned->observation;
			found->viewport.pending = backend.pending || owned->refresh_pending;
			found->viewport.failed = backend.failed;
			found->status = "Captured frame " + std::to_string(found->viewport.frame);
			for (auto& [label, value] : found->facts) {
				if (label == "Captured frame")
					value = std::to_string(found->viewport.frame);
				else if (label == "Captured generation")
					value = std::to_string(found->viewport.generation);
				else if (label == "Completed copy submission")
					value = std::to_string(found->viewport.submission);
				else if (label == "Captured extent")
					value = dimensions(found->viewport.width, found->viewport.height);
			}
		} else
			found->viewport.texture = {};
	}
}

} // namespace

std::string_view catalogue_domain_name(Domain domain) noexcept {
	static constexpr std::array names{"Elements", "Themes",	   "Icons",		"Fonts",  "Images",
									  "Actions",  "Shortcuts", "Viewports", "Atlases"};
	const auto index = domain_index(domain);
	return index < names.size() ? names[index] : "Catalogue";
}
void select_catalogue_domain(DevCatalogueState& state, Domain domain) noexcept {
	state.domain = std::min<uint64_t>(static_cast<uint64_t>(domain), 8);
	state.selected_identity = 0;
	state.selected_name.clear();
	state.selected_missing = false;
	state.domains[state.domain].group.clear();
	state.dirty = true;
}
void select_catalogue_asset(DevCatalogueState& state, uint64_t selected) {
	const auto found = std::ranges::find(state.assets, selected, &DevCatalogueAsset::identity);
	if (found == state.assets.end())
		return;
	if (state.selected_identity != selected) {
		state.frozen_viewport = {};
		state.frozen_viewport_identity = 0;
	}
	state.domain = static_cast<uint64_t>(found->domain);
	if (state.selected_identity != selected && found->domain == Domain::Icons) {
		state.domains[state.domain].raster_identity = 0;
		state.domains[state.domain].page = 0;
	}
	state.selected_identity = selected;
	state.selected_name = found->name;
	state.selected_missing = false;
	state.reveal_selection = true;
	state.dirty = true;
	state.domains[state.domain].fit = true;
	state.domains[state.domain].zoom = 1;
	state.domains[state.domain].pan_x = state.domains[state.domain].pan_y = 0;
	rebuild_catalogue_filter(state);
}
const DevCatalogueAsset* selected_catalogue_asset(const DevCatalogueState& state) noexcept {
	const auto found =
		std::ranges::find(state.assets, state.selected_identity, &DevCatalogueAsset::identity);
	return found == state.assets.end() ? nullptr : &*found;
}
void rebuild_catalogue_filter(DevCatalogueState& state) {
	state.domain = std::min<uint64_t>(state.domain, 8);
	const auto& view = state.domains[state.domain];
	state.visible_assets.clear();
	state.visible_assets.reserve(state.assets.size());
	for (size_t index = 0; index < state.assets.size(); ++index) {
		const auto& asset = state.assets[index];
		if (static_cast<uint64_t>(asset.domain) != state.domain)
			continue;
		if (!group_contains(view.group, asset.group))
			continue;
		if (!matches(asset.name, view.search) && !matches(asset.group, view.search))
			continue;
		state.visible_assets.emplace_back(index);
	}
	state.selected_outside_filter =
		state.selected_identity && !state.selected_missing &&
		std::ranges::none_of(state.visible_assets, [&](size_t index) {
			return state.assets[index].identity == state.selected_identity;
		});
	state.dirty = false;
}
void service_catalogue(App& app, DevInterfaceState& interface_state, bool visible) {
	auto& state = interface_state.catalogue;
	if (!visible && state.pending == DevCatalogueCommand::None) {
		release_viewport_requests(app, state);
		state.lease = {};
		return;
	}
	const auto now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
											   std::chrono::steady_clock::now().time_since_epoch())
											   .count());
	const bool explicit_request = state.pending != DevCatalogueCommand::None;
	const bool refresh =
		explicit_request || !state.initialized || now - state.last_refresh_ns > 250000000ull;
	if (state.pending == DevCatalogueCommand::RasterizeIcon ||
		state.pending == DevCatalogueCommand::RemovePreviewRasters) {
		state.message_is_error = false;
		try {
			service_icon_command(app, state);
		} catch (...) {
			state.message = "Preview request could not allocate its owned bookkeeping";
			state.message_is_error = true;
		}
	}
	if (refresh) {
		try {
			collect(app, interface_state);
			state.last_refresh_ns = now;
		} catch (...) {
			state.message = "Catalogue observation failed; previous observations retained";
			state.message_is_error = true;
		}
	}
	service_viewport_requests(app, state, refresh);
	state.pending = DevCatalogueCommand::None;
	if (state.dirty)
		rebuild_catalogue_filter(state);
	std::vector<TextureHandle> textures;
	textures.reserve(state.visible_textures.size() + 16);
	for (const auto texture : state.visible_textures)
		textures.emplace_back(texture);
	const auto retain_asset = [&](const DevCatalogueAsset& asset) {
		if (asset.image.textureHandle)
			textures.emplace_back(asset.image.textureHandle);
		if (asset.icon.atlasTexture)
			textures.emplace_back(asset.icon.atlasTexture);
		if (asset.viewport.texture)
			textures.emplace_back(asset.viewport.texture);
		if (asset.domain == Domain::Icons && state.selected_identity)
			for (const auto& raster : asset.icons)
				textures.emplace_back(raster.atlasTexture);
	};
	if (state.frozen_viewport.texture)
		textures.emplace_back(state.frozen_viewport.texture);
	if (const auto* selected = selected_catalogue_asset(state))
		retain_asset(*selected);
	else if (state.visible_textures.empty())
		for (size_t index = 0; index < std::min<size_t>(state.visible_assets.size(), 24); ++index)
			retain_asset(state.assets[state.visible_assets[index]]);
	state.lease = app.devTooling().catalogues().acquireInspectionLease(textures);
}
} // namespace FlowUi::devSystems
#endif
