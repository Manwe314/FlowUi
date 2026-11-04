#include "devSystems/devInterface/Catalogue/Inspector/DevCatalogueInspector.hpp"

#if FLOW_UI_DEV_MODE
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
} // namespace

void DevCatalogueInspector::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->catalogue;
	auto root = ui::column(9);
	root.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	root.layout.padding = {14, 14, 14, 14};
	root.layout.sizing.height = CLAY_SIZING_GROW(0);
	const auto scroll = Clay_GetScrollContainerData(context.clayID());
	if (scroll.found && scroll.scrollPosition)
		state.inspector_scroll = scroll.scrollPosition->y;
	root.clip = {.vertical = true, .childOffset = {0, state.inspector_scroll}};
	CLAY(context.clayID(), root) {
		const auto domain = static_cast<DevCatalogueDomain>(state.domain);
		text(context, catalogue_domain_name(domain), interface_theme::kAccentCurrent, 12);
		const auto* asset = selected_catalogue_asset(state);
		if (state.selected_missing) {
			text(context, state.selected_name, interface_theme::kTextCanvas, 15);
			text(context, "Asset no longer registered", interface_theme::kStatusAmber);
			text(context,
				 "Its last identity is retained. Select All to return to the current inventory.");
		} else if (asset) {
			text(context, asset->name, interface_theme::kTextCanvas, 15);
			text(context, asset->status, interface_theme::kAccentSeaGlass, 12);
			fact(context, 0, "Group", asset->group);
			fact(context, 1, "Source", asset->source);
			fact(context, 2, "Observation", "Revision " + std::to_string(state.revision));
			uint64_t identity = 3;
			for (const auto& [label, value] : asset->facts)
				fact(context, identity++, label, value);
			if (!asset->detail.empty()) {
				text(context, "PREVIEW CAPABILITY", interface_theme::kTextMuted, 11);
				text(context, asset->detail, interface_theme::kTextSecondary, 12);
			}
			if (domain == DevCatalogueDomain::Icons) {
				fact(context, identity++, "Cached sizes", std::to_string(asset->icons.size()));
				const auto owned = std::ranges::count(state.preview_rasters, asset->name,
													  &DevCataloguePreviewRaster::icon_name);
				fact(context, identity++, "Created for Catalogue preview", std::to_string(owned));
				text(context,
					 "Previews sample exact cached atlas regions. Explicit raster requests and "
					 "cleanup are in the Workbench.",
					 interface_theme::kTextMuted, 11);
			}
		} else {
			text(context, "Registered assets", interface_theme::kTextCanvas, 16);
			fact(context, 0, "Logical assets", std::to_string(state.counts[state.domain]));
			fact(context, 1, "Matching filter", std::to_string(state.visible_assets.size()));
			fact(context, 2, "Scope",
				 domain == DevCatalogueDomain::Elements ||
						 domain == DevCatalogueDomain::Shortcuts ||
						 domain == DevCatalogueDomain::Viewports
					 ? "Application window " +
						   std::to_string(context.params.interfaceState->selectedWindowId)
					 : "Application shared resources");
			if (domain == DevCatalogueDomain::Icons) {
				std::size_t cached_sizes = 0;
				for (const auto& item : state.assets)
					if (item.domain == domain)
						cached_sizes += item.icons.size();
				fact(context, 3, "Cached raster variants", std::to_string(cached_sizes));
				fact(context, 4, "Preview-owned raster variants",
					 std::to_string(state.preview_rasters.size()));
			}
			text(context, state.domain_status[state.domain], interface_theme::kTextSecondary, 12);
			text(context, "INVENTORY SCOPE", interface_theme::kTextMuted, 11);
			text(context,
				 "Registered definitions and resources observable through supported managers "
				 "and schemas. Unregistered source files and failed registrations without a "
				 "retained record are outside this inventory.",
				 interface_theme::kTextSecondary, 12);
			text(context,
				 "Choose a card or a browser row for its registration, rendering data, and "
				 "preview capabilities.",
				 interface_theme::kTextMuted, 12);
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
