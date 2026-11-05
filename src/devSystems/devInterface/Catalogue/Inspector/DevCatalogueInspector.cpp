#include "devSystems/devInterface/Catalogue/Inspector/DevCatalogueInspector.hpp"

#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Catalogue/DevCatalogueFocus.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include "internal/Text/CurrentTextShaper.hpp"
#include "managers/FontManager.hpp"
#include <algorithm>

namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = diagnostics_ui;
using Context = DevCatalogueInspector::BuildContext;

/** Wrap long technical tokens at measured UTF-8 boundaries inside the assigned column. */
void text(Context& context, std::string_view label,
		  Clay_Color color = interface_theme::kTextSecondary, uint16_t size = 13) {
	const auto bounds = Clay_GetElementData(context.clayID());
	const float width = std::max(40.0f, (bounds.found ? bounds.boundingBox.width : 320.0f) - 30);
	const auto* face = context.params.app ? context.params.app->fonts().getFontById(0) : nullptr;
	if (!face) {
		ui::text(context, label, color, size);
		return;
	}
	const ::FlowUi::detail::text::CurrentTextShaper shaper;
	Clay_TextElementConfig configuration{};
	configuration.fontSize = size;
	const auto metrics =
		shaper.shape({.text = "M", .pointsToPixelsScale = 1, .fontSize = size}, *face);
	const float resolved_height =
		context.uiManager.inputContentElement(configuration).layout.sizing.height.size.minMax.min;
	const float scale = metrics.lineHeight > 0 ? resolved_height / metrics.lineHeight : 1;
	std::string display;
	display.reserve(label.size() + label.size() / 20);
	size_t offset = 0;
	while (offset < label.size()) {
		if (label[offset] == ' ' || label[offset] == '\n' || label[offset] == '\t' ||
			label[offset] == '\r') {
			display += label[offset++];
			continue;
		}
		const auto separator = label.find_first_of(" \n\t\r", offset);
		const auto end = separator == std::string_view::npos ? label.size() : separator;
		const auto word = label.substr(offset, end - offset);
		const auto layout =
			shaper.shape({.text = word, .pointsToPixelsScale = 1, .fontSize = size}, *face);
		size_t prefix = 0;
		float origin = 0;
		for (const auto& cluster : layout.clusters) {
			if ((cluster.xEnd - origin) * scale > width && cluster.startByte > prefix) {
				display += word.substr(prefix, cluster.startByte - prefix);
				display += '\n';
				prefix = cluster.startByte;
				origin = cluster.xStart;
			}
		}
		display += word.substr(prefix);
		offset = end;
	}
	ui::text(context, display, color, size);
}

void fact(Context& context, uint64_t identity, std::string_view label, std::string_view value) {
	auto declaration = ui::column(3);
	declaration.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	declaration.layout.padding = {0, 0, 4, 8};
	declaration.border = {.color = interface_theme::kBorderSubtle, .width = {0, 0, 0, 1, 0}};
	CLAY(context.clayID(Keyed("fact", identity)), declaration) {
		text(context, label, interface_theme::kTextMuted, 11);
		// Preserve the entire value with display-only wrapping.
		auto content = ui::column(0);
		content.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		CLAY(context.clayID(Keyed("value", identity)), content)
		text(context, value.empty() ? "Unknown" : value, interface_theme::kTextCanvas, 12);
	}
}
void toggle_section(void* target, uint64_t, uint64_t) {
	auto& expanded = *static_cast<bool*>(target);
	expanded = !expanded;
}

template <class Content>
void section(Context& context, LocalElementName identity, std::string_view label, bool& expanded,
			 Content&& build_content) {
	auto section = ui::column(0);
	section.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	CLAY(context.clayID(identity), section) {
		const auto header = Keyed("section-header", identity.token);
		context.uiManager.createElement(ui::kControl, header)
			.setParameters({.label = std::string(label),
							.icon = expanded ? diagnostics_icons::Icon::Expanded
											 : diagnostics_icons::Icon::Collapsed,
							.activate = toggle_section,
							.target = &expanded,
							.section_header = true})
			.setDevInternalCapture(true)
			.draw();
		catalogue_focus::register_element(context, context.params.interfaceState->catalogue,
										  ui::Control::definitionId, header);
		if (expanded) {
			auto content = ui::column(8);
			content.layout.sizing.width = CLAY_SIZING_PERCENT(1);
			content.layout.padding = {12, 12, 10, 12};
			CLAY(context.clayID(Keyed("section-content", identity.token)), content)
			build_content();
		}
	}
}
} // namespace

void DevCatalogueInspector::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->catalogue;
	auto& local = context.state();
	auto root = ui::column(0);
	root.layout.sizing = {CLAY_SIZING_PERCENT(1), CLAY_SIZING_PERCENT(1)};
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.clip.scrollInputDisabled = true;
	CLAY(context.clayID(), root) {
		auto title = ui::row(36);
		title.layout.padding = {12, 12, 0, 0};
		title.backgroundColor = interface_theme::kDepth2Ink;
		CLAY(context.clayID("title"), title)
		ui::text(context, "Inspector", interface_theme::kTextCanvas, 14);
		auto body = ui::column(0);
		body.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		body.layout.sizing.height = CLAY_SIZING_GROW(0);
		const auto viewport = context.clayID("scroll-body");
		const auto scroll = Clay_GetScrollContainerData(viewport);
		if (scroll.found && scroll.scrollPosition)
			state.inspector_scroll = scroll.scrollPosition->y;
		body.clip = {.vertical = true, .childOffset = {0, state.inspector_scroll}};
		CLAY(viewport, body) {
			const catalogue_focus::ViewportScope focus_viewport(state, viewport);
			const auto domain = static_cast<DevCatalogueDomain>(state.domain);
			const auto* asset = selected_catalogue_asset(state);
			if (state.selected_missing) {
				section(context, "registration-section", "Registration",
						local.registration_expanded, [&] {
							text(context, catalogue_domain_name(domain),
								 interface_theme::kAccentCurrent, 12);
							text(context, state.selected_name, interface_theme::kTextCanvas, 15);
							text(context, "Asset no longer registered",
								 interface_theme::kStatusAmber);
							text(context, "Its last identity is retained. Select All to return to "
										  "the current inventory.");
						});
			} else if (asset) {
				section(context, "registration-section", "Registration",
						local.registration_expanded, [&] {
							text(context, catalogue_domain_name(domain),
								 interface_theme::kAccentCurrent, 12);
							text(context, asset->name, interface_theme::kTextCanvas, 15);
							text(context, asset->status, interface_theme::kAccentSeaGlass, 12);
							fact(context, 0, "Group", asset->group);
							fact(context, 1, "Source", asset->source);
							fact(context, 2, "Observation",
								 "Revision " + std::to_string(state.revision));
						});
				if (!asset->facts.empty())
					section(context, "rendering-section", "Rendering Data",
							local.rendering_expanded, [&] {
								uint64_t identity = 3;
								for (const auto& [label, value] : asset->facts)
									fact(context, identity++, label, value);
							});
				if (!asset->detail.empty() || domain == DevCatalogueDomain::Icons)
					section(context, "preview-section", "Preview Capability",
							local.preview_expanded, [&] {
								if (!asset->detail.empty())
									text(context, asset->detail, interface_theme::kTextSecondary,
										 12);
								if (domain == DevCatalogueDomain::Icons) {
									uint64_t identity = 3 + asset->facts.size();
									fact(context, identity++, "Cached sizes",
										 std::to_string(asset->icons.size()));
									const auto owned =
										std::ranges::count(state.preview_rasters, asset->name,
														   &DevCataloguePreviewRaster::icon_name);
									fact(context, identity, "Created for Catalogue preview",
										 std::to_string(owned));
									text(context,
										 "Previews sample exact cached atlas regions. Explicit "
										 "raster requests and cleanup are in the Workbench.",
										 interface_theme::kTextMuted, 11);
								}
							});
			} else {
				section(context, "overview-section", "Inventory Overview", local.overview_expanded,
						[&] {
							text(context, catalogue_domain_name(domain),
								 interface_theme::kAccentCurrent, 12);
							text(context, "Registered assets", interface_theme::kTextCanvas, 16);
							fact(context, 0, "Logical assets",
								 std::to_string(state.counts[state.domain]));
							fact(context, 1, "Matching filter",
								 std::to_string(state.visible_assets.size()));
							fact(context, 2, "Scope",
								 domain == DevCatalogueDomain::Elements ||
										 domain == DevCatalogueDomain::Shortcuts ||
										 domain == DevCatalogueDomain::Viewports
									 ? "Application window " +
										   std::to_string(
											   context.params.interfaceState->selectedWindowId)
									 : "Application shared resources");
							if (domain == DevCatalogueDomain::Icons) {
								std::size_t cached_sizes = 0;
								for (const auto& item : state.assets)
									if (item.domain == domain)
										cached_sizes += item.icons.size();
								fact(context, 3, "Cached raster variants",
									 std::to_string(cached_sizes));
								fact(context, 4, "Preview-owned raster variants",
									 std::to_string(state.preview_rasters.size()));
							}
							text(context, state.domain_status[state.domain],
								 interface_theme::kTextSecondary, 12);
						});
				section(context, "scope-section", "Inventory Scope", local.scope_expanded, [&] {
					text(context,
						 "Registered definitions and resources observable through supported "
						 "managers and schemas. Unregistered source files and failed registrations "
						 "without a retained record are outside this inventory.",
						 interface_theme::kTextSecondary, 12);
					text(context,
						 "Choose a card or a browser row for its registration, rendering data, and "
						 "preview capabilities.",
						 interface_theme::kTextMuted, 12);
				});
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
