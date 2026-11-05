#include "devSystems/devInterface/Catalogue/Workbench/DevCatalogueWorkbench.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Catalogue/DevCatalogueFocus.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "internal/Text/CurrentTextShaper.hpp"
#include "internal/Text/TextAnalysis.hpp"
#include "managers/FontManager.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace FlowUi::devSystems::interface_elements {
namespace {
using Context = DevCatalogueWorkbench::BuildContext;
using Domain = DevCatalogueDomain;
namespace ui = diagnostics_ui;
enum Action : uint64_t {
	Select = 1,
	All,
	Fit,
	Native,
	ZoomOut,
	ZoomIn,
	Background,
	Remove,
	View,
	RasterStep,
	ShortcutContext,
	ShortcutTransition,
	PreviousPage,
	NextPage,
	Refresh,
	GlyphSelect,
	Tint,
	NextRaster,
	HostSize,
	FollowViewport,
	CaptureViewport,
	ImageFrame,
	Raster = 100
};
struct AssetCard {
	struct Parameters {
		Clay_ElementId actions_id{}, selection_focus_id{};
		DevCatalogueState* catalogue = nullptr;
		uint64_t identity = 0;
		float width = 200;
	};
	using State = FSEL::detail::selectable_surface::State;
	using BuildContext = ElementBuildContext<AssetCard>;
	using InteractionContext = ElementInteractionContext<AssetCard>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev.catalogue.asset-card");
	static constexpr std::string_view debugName = "Catalogue Asset Card";
	static constexpr bool isDevInternal = true;
	static void onPressed(InteractionContext& context) {
		const auto actions = context.params.actions_id.id
								 ? Clay_GetElementData(context.params.actions_id)
								 : Clay_ElementData{};
		const auto& input = context.uiManager.getCurrentFrameInput();
		const bool action_pressed =
			actions.found && input.mouseX >= actions.boundingBox.x &&
			input.mouseX < actions.boundingBox.x + actions.boundingBox.width &&
			input.mouseY >= actions.boundingBox.y &&
			input.mouseY < actions.boundingBox.y + actions.boundingBox.height;
		FSEL::detail::selectable_surface::onPressed(context, !action_pressed);
	}
	static void onHovered(InteractionContext& context) {
		context.uiManager.requestCursor(CursorType::PointingHand, 4);
	}
	static void onReleased(InteractionContext& context) {
		if (FSEL::detail::selectable_surface::onReleased(context, true) &&
			context.params.catalogue) {
			select_catalogue_asset(*context.params.catalogue, context.params.identity);
			if (context.params.selection_focus_id.id &&
				!context.uiManager.inputFields().hasPrimaryFieldFocus() &&
				!context.uiManager.popups().has_open_popups())
				context.uiManager.shortcuts().setFocusedElement(context.params.selection_focus_id);
		}
	}
	static void runLogic(InteractionContext& context) {
		FSEL::detail::selectable_surface::runLogic(context, true);
		const auto& current = context.uiManager.getCurrentFrameInput();
		const auto& previous = context.uiManager.getPreviousFrameInput();
		if (context.params.catalogue && !context.uiManager.inputFields().hasPrimaryFieldFocus() &&
			!context.uiManager.popups().has_open_popups() &&
			context.uiManager.shortcuts().focusedElement().id == context.clayID().id &&
			((current.keyDown[GLFW_KEY_ENTER] && !previous.keyDown[GLFW_KEY_ENTER]) ||
			 (current.keyDown[GLFW_KEY_SPACE] && !previous.keyDown[GLFW_KEY_SPACE]))) {
			select_catalogue_asset(*context.params.catalogue, context.params.identity);
			if (context.params.selection_focus_id.id)
				context.uiManager.shortcuts().setFocusedElement(context.params.selection_focus_id);
		}
	}
	[[nodiscard]] static Clay_ElementDeclaration constructElement(BuildContext& context) {
		auto card = ui::card(5);
		card.layout.sizing = {CLAY_SIZING_FIXED(context.params.width), CLAY_SIZING_GROW(0)};
		card.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
		if (context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID()))
			card.backgroundColor = interface_theme::kHoverSurface;
		if (context.uiManager.shortcuts().focusedElement().id == context.clayID().id)
			card.border.color = interface_theme::kAccentCurrent;
		return card;
	}
};
inline constexpr AssetCard kAssetCard{};
void activate(void* target, uint64_t action, uint64_t value) {
	auto& state = *static_cast<DevCatalogueState*>(target);
	auto& view = state.domains[state.domain];
	if (action == GlyphSelect)
		view.selected_glyph = value;
	if (action == Select)
		select_catalogue_asset(state, value);
	if (action == FollowViewport) {
		view.follow_observed_output = !view.follow_observed_output;
		state.frozen_viewport = {};
		state.frozen_viewport_identity = 0;
	}
	if (action == CaptureViewport) {
		state.pending = DevCatalogueCommand::CaptureViewport;
		state.pending_identity = value;
		state.frozen_viewport = {};
		state.frozen_viewport_identity = 0;
	}
	if (action == All) {
		state.selected_identity = 0;
		state.selected_missing = false;
		view.group.clear();
		state.dirty = true;
	}
	if (action == Fit) {
		view.fit = true;
		view.zoom = 1;
		view.pan_x = view.pan_y = 0;
	}
	if (action == Native) {
		view.fit = false;
		view.zoom = 1;
		view.pan_x = view.pan_y = 0;
	}
	if (action == ZoomOut || action == ZoomIn) {
		view.fit = false;
		view.zoom = std::clamp(view.zoom * (action == ZoomIn ? 1.25f : .8f), .05f, 32.0f);
	}
	if (action == Background)
		view.background = (view.background + 1) % 3;
	if (action == Remove) {
		state.pending = DevCatalogueCommand::RemovePreviewRasters;
		state.pending_identity = value;
	}
	if (action == View) {
		view.view = value;
		if (state.domain == static_cast<uint64_t>(Domain::Fonts) ||
			state.domain == static_cast<uint64_t>(Domain::Icons))
			view.page = 0;
		view.workbench_scroll = 0;
	}
	if (action == RasterStep)
		view.raster_step = (view.raster_step + 1) % catalogue_icon_steps.size();
	if (action == ShortcutContext)
		view.shortcut_context = (view.shortcut_context + 1) % 4;
	if (action == ShortcutTransition)
		view.shortcut_transition = (view.shortcut_transition + 1) % 3;
	if (action == PreviousPage && view.page)
		--view.page;
	if (action == NextPage)
		++view.page;
	if (action == Refresh)
		state.pending = DevCatalogueCommand::Refresh;
	if (action == Tint)
		view.tint = !view.tint;
	if (action == HostSize)
		view.page = (view.page + 1) % 3;
	if (action == ImageFrame)
		view.page = (view.page + 1) % 3;
	if (action == NextRaster) {
		const auto* selected = selected_catalogue_asset(state);
		if (selected && !selected->icons.empty()) {
			view.page = (view.page + 1) % selected->icons.size();
			view.raster_identity = selected->icons[view.page].atlasTexture.packed();
		}
	}
	if (action >= Raster && action < Raster + catalogue_icon_steps.size()) {
		state.pending = DevCatalogueCommand::RasterizeIcon;
		state.pending_identity = value;
		state.pending_size = catalogue_icon_steps[action - Raster];
	}
}
template <class Identity>
void control_button(Context& context, Identity key, std::string label, uint64_t action,
					uint64_t value, bool enabled = true, std::string tooltip = {}) {
	const auto element = ::FlowUi::detail::element_id::resolveLocal(
		context.id, ui::Control::definitionId, key.token);
	context.uiManager.createElement(ui::kControl, key)
		.withID(element)
		.setParameters({.label = std::move(label),
						.tooltip = std::move(tooltip),
						.activate = activate,
						.target = &context.params.interfaceState->catalogue,
						.action = action,
						.value = value,
						.enabled = enabled})
		.setDevInternalCapture(true)
		.draw();
	catalogue_focus::register_element(context, context.params.interfaceState->catalogue,
									  ui::Control::definitionId, key, enabled);
}
void button(Context& context, LocalElementName key, std::string label, uint64_t action,
			uint64_t value = 0, bool enabled = true, std::string tooltip = {}) {
	control_button(context, key, std::move(label), action, value, enabled, std::move(tooltip));
}
void button(Context& context, IndexedElementName key, std::string label, uint64_t action,
			uint64_t value = 0, bool enabled = true, std::string tooltip = {}) {
	control_button(context, key, std::move(label), action, value, enabled, std::move(tooltip));
}
[[nodiscard]] Clay_Color background(uint64_t mode) noexcept {
	return mode == 1 ? Clay_Color{238, 240, 242, 255} : interface_theme::kDepth0Keel;
}
[[nodiscard]] float framebuffer_scale(Context& context) {
	const auto framebuffer = context.params.app->framebufferSize(context.uiManager.windowId());
	const auto layout = Clay_GetLayoutDimensions();
	return layout.width > 0 ? std::max(.01f, static_cast<float>(framebuffer.first) / layout.width)
							: 1;
}
void picture(Context& context, IndexedElementName key, TextureRef texture, float width,
			 float height) {
	if (!texture.handle || texture.sourceWidth <= 0 || texture.sourceHeight <= 0)
		return;
	context.params.interfaceState->catalogue.visible_textures.emplace_back(texture.handle);
	const auto scale = std::min({width / texture.sourceWidth, height / texture.sourceHeight,
								 1.0f / framebuffer_scale(context)});
	if (!std::isfinite(scale) || scale <= 0)
		return;
	Clay_ElementDeclaration image{};
	image.layout.sizing = {CLAY_SIZING_FIXED(texture.sourceWidth * scale),
						   CLAY_SIZING_FIXED(texture.sourceHeight * scale)};
	image.image.imageData = context.uiManager.imageData(texture);
	if (texture.tintEnabled)
		image.backgroundColor = interface_theme::kAccentSeaGlass;
	CLAY(context.clayID(key), image) {}
}
[[nodiscard]] TextureRef icon_texture(const devMode::DevIconCatalogEntry& icon,
									  bool tint = false) noexcept {
	return TextureRef{.handle = icon.atlasTexture,
					  .uv0x = icon.atlasUv.x,
					  .uv0y = icon.atlasUv.y,
					  .uv1x = icon.atlasUv.x + icon.atlasUv.width,
					  .uv1y = icon.atlasUv.y + icon.atlasUv.height,
					  .fitMode = TextureFitMode::Contain,
					  .sourceWidth = static_cast<int32_t>(icon.targetWidth),
					  .sourceHeight = static_cast<int32_t>(icon.targetHeight),
					  .tintEnabled = tint};
}
void specimen_text(Context& context, const DevCatalogueAsset& asset, std::string_view text,
				   uint16_t size) {
	Clay_TextElementConfig configuration{};
	configuration.fontId = asset.font.fontHandle;
	configuration.fontSize = size;
	configuration.textColor = interface_theme::kTextCanvas;
	configuration.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY_TEXT(context.uiManager.toClayString(text), CLAY_TEXT_CONFIG(configuration));
}
[[nodiscard]] const devMode::DevSchemaRegistry::ElementPreview*
element_adapter(Context& context, const DevCatalogueAsset& asset) noexcept {
	const auto adapters = context.params.app->devTooling().schemas().elementPreviews();
	const auto found = std::ranges::find(adapters, asset.element.definition,
										 &devMode::DevSchemaRegistry::ElementPreview::definition);
	return found == adapters.end() ? nullptr : &*found;
}
void element_preview(Context& context, const DevCatalogueAsset& asset, bool focused) {
	const auto* adapter = element_adapter(context, asset);
	if (!adapter || !adapter->admitted || !adapter->draw) {
		ui::text(context, asset.status, interface_theme::kTextSecondary, 11);
		return;
	}
	const auto& view =
		context.params.interfaceState->catalogue.domains[static_cast<size_t>(Domain::Elements)];
	const bool construct = (view.view == 1 && adapter->has_construct) || !adapter->has_draw;
	adapter->draw(context.uiManager, asset.identity ^ (focused ? 0xb3f1d501ull : 0xa9e40683ull),
				  construct);
}
[[nodiscard]] const theme_editor::ThemePresentation*
theme_presentation(Context& context, const DevCatalogueAsset& asset) noexcept {
	if (!context.params.interfaceState->catalogue.theme_observation_available)
		return nullptr;
	const auto& model = context.params.interfaceState->catalogue.theme_model;
	for (const auto& view : model.catalogue())
		if (view.type == asset.theme.themeTypeId && view.variant == asset.name)
			return model.find(view);
	return nullptr;
}
void theme_tokens(Context& context, const DevCatalogueAsset& asset, size_t maximum) {
	const auto* presentation = theme_presentation(context, asset);
	if (!presentation || presentation->fields.empty()) {
		ui::text(context, "Reflection unavailable; registered scenes may still be available",
				 interface_theme::kTextMuted, 11);
		return;
	}
	const auto schema = context.params.app->devTooling().schemas().view();
	for (size_t index = 0; index < std::min(maximum, presentation->fields.size()); ++index) {
		const auto& field = presentation->fields[index];
		auto token = ui::row(25);
		token.layout.sizing.height = CLAY_SIZING_FIT(25);
		CLAY(context.clayID(Keyed("token", asset.identity ^ (index + 1))), token) {
			const auto* type =
				schema && field.field ? schema->type(field.field->valueType) : nullptr;
			if (type && type->id == ::FlowUi::detail::typeHash<Clay_Color>() && field.live) {
				Clay_ElementDeclaration swatch{};
				swatch.layout.sizing = {CLAY_SIZING_FIXED(18), CLAY_SIZING_FIXED(18)};
				swatch.backgroundColor = *static_cast<const Clay_Color*>(field.live);
				CLAY(context.clayID(Keyed("swatch", asset.identity ^ (index + 1))), swatch) {}
			}
			ui::text(context, field.path + "  " + presentation->values[index].live_display,
					 interface_theme::kTextSecondary, 11);
		}
	}
}
void theme_components(Context& context, const DevCatalogueAsset& asset) {
	const auto* presentation = theme_presentation(context, asset);
	if (!presentation || !presentation->source.payload) {
		ui::text(context, "Variant payload is unavailable");
		return;
	}
	UiManager::DevThemeScope theme_scope(context.uiManager, presentation->source);
	UiManager::DevPassivePreviewScope passive(context.uiManager);
	bool rendered = false;
	context.params.app->themes().visitDevPreviews(asset.theme.themeTypeId, [&](const auto& scene) {
		if (!scene.uses_local_theme && !presentation->source.active) {
			ui::text(context,
					 scene.debug_name + ": inactive preview requires the local theme resolver",
					 interface_theme::kTextMuted, 11);
			return;
		}
		ui::text(context, scene.debug_name, interface_theme::kTextCanvas, 12);
		scene.draw(*context.params.app, context.uiManager);
		rendered = true;
	});
	if (!rendered && asset.theme.themeTypeId == ::FlowUi::detail::typeHash<FSEL::FSELTheme>()) {
		draw_fsel_theme_specimens(context, *context.params.app, context.state().specimens);
		rendered = true;
	}
	if (!rendered)
		ui::text(context, "No scene registered for this theme type");
}
void icon_controls(Context& context, const DevCatalogueAsset& asset, bool compact) {
	auto& state = context.params.interfaceState->catalogue;
	const auto step = std::min<size_t>(state.domains[state.domain].raster_step, 6);
	if (compact) {
		CLAY(context.clayID(Keyed("card-controls", asset.identity)), ui::row(28)) {
			button(context, Keyed("rasterize-card", asset.identity),
				   "Rasterize " + std::to_string(catalogue_icon_steps[step]), Raster + step,
				   asset.identity);
			if (std::ranges::any_of(state.preview_rasters, [&](const auto& owned) {
					return owned.icon_name == asset.name;
				}))
				button(context, Keyed("cleanup-card", asset.identity), "Remove", Remove,
					   asset.identity, true,
					   "Remove only rasters explicitly created for Catalogue previews");
		}
		return;
	}
	for (size_t first = 0; first < catalogue_icon_steps.size(); first += 4)
		CLAY(context.clayID(Keyed("raster-steps", first)), ui::row(30)) {
			for (size_t index = first; index < std::min(first + 4, catalogue_icon_steps.size());
				 ++index)
				button(context, Keyed("rasterize", index),
					   std::to_string(catalogue_icon_steps[index]) + "px", Raster + index,
					   asset.identity, true,
					   "Explicitly create this physical raster size only if missing");
		}
	const auto owned = std::ranges::count(state.preview_rasters, asset.name,
										  &DevCataloguePreviewRaster::icon_name);
	button(context, "remove-icon-previews",
		   "Remove preview rasters (" + std::to_string(owned) + ")", Remove, asset.identity,
		   owned > 0);
}
void thumbnail(Context& context, const DevCatalogueAsset& asset, float width) {
	auto stage = ui::column(0);
	stage.layout.sizing = {CLAY_SIZING_FIXED(std::max(1.0f, width - 22)), CLAY_SIZING_FIXED(104)};
	stage.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
	stage.backgroundColor = interface_theme::kDepth0Keel;
	stage.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
	CLAY(context.clayID(Keyed("thumbnail", asset.identity)), stage) {
		switch (asset.domain) {
		case Domain::Icons:
			if (asset.icon.atlasTexture)
				picture(context, Keyed("icon", asset.identity),
						icon_texture(asset.icon, context.params.interfaceState->catalogue
													 .domains[static_cast<size_t>(Domain::Icons)]
													 .tint),
						width - 20, 92);
			else
				ui::text(context, "No cached raster", interface_theme::kTextMuted, 11);
			break;
		case Domain::Images:
			if (asset.image.status == devMode::DevResourceStatus::Ready)
				picture(context, Keyed("image", asset.identity),
						TextureRef{.handle = asset.image.textureHandle,
								   .sourceWidth = static_cast<int32_t>(asset.image.width),
								   .sourceHeight = static_cast<int32_t>(asset.image.height)},
						width - 20, 92);
			else
				ui::text(context, asset.status);
			break;
		case Domain::Fonts:
			specimen_text(context, asset, "Il1 O0 012345\nQuick brown fox", 16);
			break;
		case Domain::Elements:
			element_preview(context, asset, false);
			break;
		case Domain::Themes:
			theme_tokens(context, asset, 3);
			break;
		case Domain::Actions:
			ui::text(context, asset.action.isBound ? "BOUND" : "UNBOUND",
					 asset.action.isBound ? interface_theme::kStatusGreen
										  : interface_theme::kStatusAmber,
					 16);
			ui::text(context,
					 std::to_string(asset.action.lifetimeInvocations) + " observed invocations",
					 interface_theme::kTextSecondary, 11);
			break;
		case Domain::Shortcuts:
			ui::text(context, asset.name, interface_theme::kAccentSeaGlass, 16);
			break;
		case Domain::Viewports:
			context.params.interfaceState->catalogue.visible_viewports.emplace_back(asset.identity);
			if (asset.viewport.texture)
				picture(context, Keyed("viewport-thumbnail", asset.identity),
						TextureRef{.handle = asset.viewport.texture,
								   .sourceWidth = static_cast<int32_t>(asset.viewport.width),
								   .sourceHeight = static_cast<int32_t>(asset.viewport.height)},
						width - 20, 92);
			else
				ui::text(context, asset.status, interface_theme::kTextMuted, 11);
			break;
		case Domain::Atlases:
			ui::text(context,
					 std::to_string(asset.atlas.width) + " x " + std::to_string(asset.atlas.height),
					 interface_theme::kAccentSeaGlass, 16);
			ui::text(context, std::to_string(asset.atlas.allocatedRegions) + " regions",
					 interface_theme::kTextSecondary, 11);
			break;
		case Domain::Count:
			break;
		}
	}
}
[[nodiscard]] std::string card_label(Context& context, std::string_view label, float width) {
	const auto* face = context.params.app->fonts().getFontById(0);
	if (!face)
		return std::string(label);
	const ::FlowUi::detail::text::CurrentTextShaper shaper;
	const auto layout =
		shaper.shape({.text = label, .pointsToPixelsScale = 1, .fontSize = 13}, *face);
	Clay_TextElementConfig configuration{};
	configuration.fontSize = 13;
	const float height =
		context.uiManager.inputContentElement(configuration).layout.sizing.height.size.minMax.min;
	const float scale = layout.lineHeight > 0 ? height / layout.lineHeight : 1;
	if (layout.measuredWidth * scale <= width)
		return std::string(label);
	const auto suffix =
		shaper.shape({.text = "...", .pointsToPixelsScale = 1, .fontSize = 13}, *face);
	const float available = width - suffix.measuredWidth * scale;
	size_t prefix = 0;
	for (const auto& cluster : layout.clusters) {
		if (cluster.xEnd * scale > available)
			break;
		prefix = cluster.endByte;
	}
	return prefix ? std::string(label.substr(0, prefix)) + "..." : std::string("...");
}
void gallery(Context& context, float width, float height) {
	auto& state = context.params.interfaceState->catalogue;
	auto& view = state.domains[state.domain];
	const size_t columns =
		static_cast<size_t>(std::clamp(std::floor((width - 20) / 208), 1.0f, 6.0f));
	const size_t row_count = (state.visible_assets.size() + columns - 1) / columns;
	constexpr float row_height = 242;
	const auto viewport = context.clayID("gallery-scroll");
	auto scroll = Clay_GetScrollContainerData(viewport);
	if (scroll.found && scroll.scrollPosition && context.state().last_domain != state.domain)
		scroll.scrollPosition->y = view.workbench_scroll;
	if (scroll.found && scroll.scrollPosition)
		view.workbench_scroll = scroll.scrollPosition->y;
	const auto first_row = std::min(
		row_count,
		static_cast<size_t>(std::max(0.0f, std::floor(-view.workbench_scroll / row_height))));
	const auto end_row =
		std::min(row_count, first_row + static_cast<size_t>(std::ceil(height / row_height)) + 2);
	auto surface = ui::column(0);
	surface.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	surface.layout.sizing.height = CLAY_SIZING_GROW(0);
	surface.clip = {.vertical = true, .childOffset = {0, view.workbench_scroll}};
	CLAY(viewport, surface) {
		const catalogue_focus::ViewportScope focus_viewport(state, viewport);
		const auto spacer = [&](IndexedElementName key, float extent) {
			if (extent <= 0)
				return;
			Clay_ElementDeclaration declaration{};
			declaration.layout.sizing = {CLAY_SIZING_FIXED(1), CLAY_SIZING_FIXED(extent)};
			CLAY(context.clayID(key), declaration) {}
		};
		spacer(Keyed("gallery-spacer", 0), first_row * row_height);
		for (size_t row_index = first_row; row_index < end_row; ++row_index) {
			auto row = ui::row(row_height);
			row.layout.padding = {10, 10, 8, 8};
			row.layout.childGap = 10;
			CLAY(context.clayID(Keyed("gallery-row", row_index)), row) {
				for (size_t column = 0; column < columns; ++column) {
					const auto index = row_index * columns + column;
					if (index >= state.visible_assets.size())
						break;
					const auto& asset = state.assets[state.visible_assets[index]];
					const float card_width = (width - 20 - (columns - 1) * 10) / columns;
					context.uiManager.createElement(kAssetCard, Keyed("asset-card", asset.identity))
						.setParameters({.actions_id = asset.domain == Domain::Icons
														  ? context.clayID(Keyed("card-controls",
																				 asset.identity))
														  : Clay_ElementId{},
										.selection_focus_id = context.clayID("focused-scroll"),
										.catalogue = &state,
										.identity = asset.identity,
										.width = std::max(140.0f, card_width)})
						.setDevInternalCapture(true)
						.construct();
					catalogue_focus::register_element(context, state, AssetCard::definitionId,
													  Keyed("asset-card", asset.identity));
					{
						thumbnail(context, asset, card_width);
						auto title = ui::row(24);
						title.clip = {.horizontal = true, .scrollInputDisabled = true};
						const auto separator = asset.name.find_last_of("/:");
						const std::string_view display_name =
							separator != std::string::npos && (asset.domain == Domain::Icons ||
															   asset.domain == Domain::Images)
								? std::string_view(asset.name).substr(separator + 1)
								: std::string_view(asset.name);
						CLAY(context.clayID(Keyed("card-title", asset.identity)), title)
						ui::text(context, card_label(context, display_name, card_width - 24),
								 interface_theme::kTextCanvas, 13);
						auto status = ui::column(0);
						status.layout.sizing = {CLAY_SIZING_FIXED(std::max(1.0f, card_width - 22)),
												CLAY_SIZING_FIXED(34)};
						CLAY(context.clayID(Keyed("card-status", asset.identity)), status)
						ui::text(context, asset.status, interface_theme::kTextSecondary, 11);
						if (asset.domain == Domain::Icons)
							icon_controls(context, asset, true);
					}
					context.uiManager.drawConstructed();
					const auto card_identity = ::FlowUi::detail::element_id::resolveLocal(
						context.id, AssetCard::definitionId,
						Keyed("asset-card", asset.identity).token);
					if (context.uiManager.getPreviousFramesInteraction().isHovered(
							context.uiManager.toClayEID(card_identity)))
						ui::pointer_tooltip(context, Keyed("asset-tooltip", asset.identity),
											asset.name + "\n" + asset.status);
				}
			}
		}
		spacer(Keyed("gallery-spacer", 1), (row_count - end_row) * row_height);
	}
	if (state.visible_assets.empty())
		ui::text(context,
				 view.search.empty() ? "No observable registrations in this collection"
									 : "No assets match this filter",
				 interface_theme::kTextMuted);
}
void canvas(Context& context, const DevCatalogueAsset& asset, float width, float height,
			TextureRef texture) {
	auto& state = context.params.interfaceState->catalogue;
	auto& view = state.domains[state.domain];
	if (!texture.handle || texture.sourceWidth <= 0 || texture.sourceHeight <= 0) {
		ui::text(context, asset.status);
		return;
	}
	state.visible_textures.emplace_back(texture.handle);
	const auto canvas_id = context.clayID("asset-canvas");
	const auto bounds = Clay_GetElementData(canvas_id);
	const auto& current = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	const bool hovered = bounds.found && current.mouseX >= bounds.boundingBox.x &&
						 current.mouseY >= bounds.boundingBox.y &&
						 current.mouseX < bounds.boundingBox.x + bounds.boundingBox.width &&
						 current.mouseY < bounds.boundingBox.y + bounds.boundingBox.height;
	const float native_scale = 1 / framebuffer_scale(context);
	const float fit_scale =
		std::min({width / texture.sourceWidth, height / texture.sourceHeight, native_scale});
	float scale = view.fit ? fit_scale : view.zoom * native_scale;
	if (hovered && current.ctrl && current.scrollY != 0) {
		const float previous_scale = scale;
		view.fit = false;
		view.zoom =
			std::clamp((scale / native_scale) * std::pow(1.015f, current.scrollY), .05f, 32.0f);
		scale = view.zoom * native_scale;
		const float pointer_x = current.mouseX - bounds.boundingBox.x - width / 2,
					pointer_y = current.mouseY - bounds.boundingBox.y - height / 2;
		view.pan_x = pointer_x - (pointer_x - view.pan_x) * scale / previous_scale;
		view.pan_y = pointer_y - (pointer_y - view.pan_y) * scale / previous_scale;
	}
	if (hovered && current.mouseDown[0] && previous.mouseDown[0] && !view.fit) {
		view.pan_x += current.mouseX - previous.mouseX;
		view.pan_y += current.mouseY - previous.mouseY;
	}
	const float drawn_width = texture.sourceWidth * scale,
				drawn_height = texture.sourceHeight * scale;
	view.pan_x = std::clamp(view.pan_x, -std::max(0.0f, (drawn_width - width) / 2),
							std::max(0.0f, (drawn_width - width) / 2));
	view.pan_y = std::clamp(view.pan_y, -std::max(0.0f, (drawn_height - height) / 2),
							std::max(0.0f, (drawn_height - height) / 2));
	auto stage = ui::column(0);
	stage.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(height)};
	stage.backgroundColor = background(view.background);
	stage.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
	CLAY(canvas_id, stage) {
		if (view.background == 0) {
			const size_t rows = std::min<size_t>(64, static_cast<size_t>(std::ceil(height / 24))),
						 columns = std::min<size_t>(64, static_cast<size_t>(std::ceil(width / 24)));
			for (size_t row_index = 0; row_index < rows; ++row_index) {
				auto row = ui::row(24);
				row.layout.childGap = 0;
				CLAY(context.clayID(Keyed("checker-row", row_index)), row) {
					for (size_t column = 0; column < columns; ++column) {
						Clay_ElementDeclaration tile{};
						tile.layout.sizing = {CLAY_SIZING_FIXED(24), CLAY_SIZING_FIXED(24)};
						tile.backgroundColor = (row_index + column) % 2
												   ? Clay_Color{29, 41, 48, 255}
												   : Clay_Color{40, 54, 63, 255};
						CLAY(context.clayID(Keyed("checker-tile", row_index * 64 + column)), tile) {
						}
					}
				}
			}
		}
		Clay_ElementDeclaration image{};
		image.layout.sizing = {CLAY_SIZING_FIXED(drawn_width), CLAY_SIZING_FIXED(drawn_height)};
		image.floating.attachTo = CLAY_ATTACH_TO_PARENT;
		image.floating.clipTo = CLAY_CLIP_TO_ATTACHED_PARENT;
		image.floating.offset = {(width - drawn_width) / 2 + view.pan_x,
								 (height - drawn_height) / 2 + view.pan_y};
		image.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
		image.image.imageData = context.uiManager.imageData(texture);
		if (texture.tintEnabled)
			image.backgroundColor = interface_theme::kAccentSeaGlass;
		image.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
		CLAY(context.clayID("canvas-image"), image) {
			if (asset.domain == Domain::Atlases && view.view == 1) {
				size_t region_index = 0;
				for (const auto& owner : state.assets) {
					if (owner.domain != Domain::Icons)
						continue;
					for (const auto& raster : owner.icons) {
						if (raster.atlasLayer != asset.atlas.pageIndex || region_index >= 2048)
							continue;
						Clay_ElementDeclaration region{};
						region.layout.sizing = {
							CLAY_SIZING_FIXED(raster.atlasUv.width * drawn_width),
							CLAY_SIZING_FIXED(raster.atlasUv.height * drawn_height)};
						region.floating.attachTo = CLAY_ATTACH_TO_PARENT;
						region.floating.clipTo = CLAY_CLIP_TO_ATTACHED_PARENT;
						region.floating.offset = {raster.atlasUv.x * drawn_width,
												  raster.atlasUv.y * drawn_height};
						region.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
						region.border = {.color = interface_theme::kAccentSeaGlass,
										 .width = {1, 1, 1, 1, 0}};
						CLAY(context.clayID(Keyed("atlas-region", region_index++)), region) {}
					}
				}
			}
		}
	}
	ui::text(context,
			 std::to_string(texture.sourceWidth) + " x " + std::to_string(texture.sourceHeight) +
				 " texels · " + std::to_string(static_cast<int>(scale * 100)) + "% layout · " +
				 std::to_string(static_cast<int>(scale / native_scale * 100)) +
				 "% physical · Ctrl+wheel zoom · drag to pan",
			 interface_theme::kTextMuted, 11);
}
[[nodiscard]] std::string unicode_name(uint32_t codepoint) {
	char buffer[16]{};
	std::snprintf(buffer, sizeof(buffer), "U+%04X", codepoint);
	return buffer;
}
[[nodiscard]] std::string glyph_text(uint32_t codepoint) {
	if (codepoint < 32 || codepoint == 127 || codepoint > 0x10ffff ||
		(codepoint >= 0xd800 && codepoint <= 0xdfff))
		return {};
	std::string result;
	result.reserve(4);
	if (codepoint < 0x80)
		result += static_cast<char>(codepoint);
	else if (codepoint < 0x800) {
		result += static_cast<char>(0xc0 | (codepoint >> 6));
		result += static_cast<char>(0x80 | (codepoint & 0x3f));
	} else if (codepoint < 0x10000) {
		result += static_cast<char>(0xe0 | (codepoint >> 12));
		result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
		result += static_cast<char>(0x80 | (codepoint & 0x3f));
	} else {
		result += static_cast<char>(0xf0 | (codepoint >> 18));
		result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
		result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
		result += static_cast<char>(0x80 | (codepoint & 0x3f));
	}
	return result;
}
void font_focus(Context& context, const DevCatalogueAsset& asset) {
	auto& state = context.params.interfaceState->catalogue;
	auto& view = state.domains[state.domain];
	FSEL::TextInputParameters input{};
	input.value = &state.specimen_text;
	input.maxBytes = 1024;
	input.fontSize = 12;
	input.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30)};
	context.uiManager.createElement(FSEL::kTextInput, "font-sample")
		.setParameters(input)
		.setDevInternalCapture(true)
		.draw();
	catalogue_focus::register_element(context, state, FSEL::TextInput::definitionId,
									  LocalElementName{"font-sample"}, true, true);
	const auto* face = context.params.app->fonts().getFontById(asset.font.fontHandle);
	const auto* variant = face ? face->defaultVariant() : nullptr;
	if (!variant) {
		ui::text(context, "Loaded glyph metadata unavailable");
		return;
	}
	std::vector<uint32_t> missing;
	missing.reserve(state.specimen_text.size());
	size_t byte_offset = 0;
	::FlowUi::detail::text::DecodedCodepoint decoded{};
	while (::FlowUi::detail::text::decodeNextUtf8(state.specimen_text, byte_offset, decoded)) {
		if (decoded.value == '\n' || decoded.value == '\r' || decoded.value == '\t')
			continue;
		if (!variant->unicodeToGlyphIndex.contains(decoded.value))
			missing.emplace_back(decoded.value);
	}
	std::ranges::sort(missing);
	missing.erase(std::unique(missing.begin(), missing.end()), missing.end());
	std::string coverage =
		"Loaded coverage: " + std::to_string(variant->unicodeToGlyphIndex.size()) + " codepoints";
	if (missing.empty())
		coverage += " · sample covered";
	else {
		coverage += " · fallback for ";
		for (size_t index = 0; index < std::min<size_t>(missing.size(), 12); ++index)
			coverage += (index ? ", " : "") + unicode_name(missing[index]);
		if (missing.size() > 12)
			coverage += " and " + std::to_string(missing.size() - 12) + " more";
	}
	ui::text(context, coverage,
			 missing.empty() ? interface_theme::kTextMuted : interface_theme::kStatusAmber, 11);
	if (view.view == 0) {
		for (const uint16_t size : {10, 12, 14, 16, 20, 24, 32, 48, 64}) {
			ui::text(context, std::to_string(size) + " pt", interface_theme::kTextMuted, 11);
			specimen_text(context, asset, state.specimen_text, size);
		}
	} else if (view.view == 1) {
		specimen_text(
			context, asset,
			state.specimen_text +
				"\n\nThe same face is rendered through the application's real text layout. Il1 O0 "
				"0123456789; punctuation, wrapping and fallback are visible here.",
			16);
	} else {
		auto filter = input;
		filter.value = &view.glyph_search;
		filter.placeholder = "Find a character or U+0041";
		filter.maxBytes = 64;
		const auto old_search = view.glyph_search;
		context.uiManager.createElement(FSEL::kTextInput, "glyph-search")
			.setParameters(filter)
			.setDevInternalCapture(true)
			.draw();
		catalogue_focus::register_element(context, state, FSEL::TextInput::definitionId,
										  LocalElementName{"glyph-search"}, true, true);
		if (old_search != view.glyph_search)
			view.page = 0;
		std::vector<uint32_t> codepoints;
		codepoints.reserve(variant->unicodeToGlyphIndex.size());
		for (const auto& [codepoint, glyph_index] : variant->unicodeToGlyphIndex) {
			if (glyph_index >= variant->glyphs.size())
				continue;
			const auto label = unicode_name(codepoint);
			const auto text = glyph_text(codepoint);
			const auto search =
				std::ranges::search(label, view.glyph_search, [](char left, char right) {
					return std::tolower(static_cast<unsigned char>(left)) ==
						   std::tolower(static_cast<unsigned char>(right));
				});
			if (view.glyph_search.empty() || text == view.glyph_search ||
				search.begin() != label.end())
				codepoints.emplace_back(codepoint);
		}
		std::ranges::sort(codepoints);
		const auto parent = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
		const size_t columns = static_cast<size_t>(std::clamp(
			std::floor((parent.found ? parent.boundingBox.width : 480) / 88), 1.0f, 8.0f));
		const size_t per_page = columns * 4;
		const size_t pages = std::max<size_t>(1, (codepoints.size() + per_page - 1) / per_page);
		view.page = std::min<uint64_t>(view.page, pages - 1);
		CLAY(context.clayID("glyph-pages"), ui::row()) {
			button(context, "glyph-previous", "Previous", PreviousPage, 0, view.page > 0);
			ui::text(context,
					 "Page " + std::to_string(view.page + 1) + " / " + std::to_string(pages),
					 interface_theme::kTextMuted, 11);
			button(context, "glyph-next", "Next", NextPage, 0, view.page + 1 < pages);
		}
		for (size_t row_start = view.page * per_page;
			 row_start < std::min(codepoints.size(), (view.page + 1) * per_page);
			 row_start += columns) {
			auto row = ui::row(94);
			row.layout.childGap = 4;
			CLAY(context.clayID(Keyed("glyph-row", row_start)), row) {
				for (size_t index = row_start;
					 index < std::min(row_start + columns, codepoints.size()); ++index) {
					const auto codepoint = codepoints[index];
					context.uiManager.createElement(ui::kControl, Keyed("choose-glyph", codepoint))
						.setParameters({.activate = activate,
										.target = &state,
										.action = GlyphSelect,
										.value = codepoint,
										.selected = view.selected_glyph == codepoint})
						.setDevInternalCapture(true)
						.construct();
					catalogue_focus::register_element(context, state, ui::Control::definitionId,
													  Keyed("choose-glyph", codepoint));
					auto cell = ui::column(2);
					cell.layout.sizing = {CLAY_SIZING_FIXED(58), CLAY_SIZING_FIXED(82)};
					cell.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
					cell.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
					CLAY(context.clayID(Keyed("glyph-cell", codepoint)), cell) {
						specimen_text(context, asset, glyph_text(codepoint), 26);
						ui::text(context, unicode_name(codepoint), interface_theme::kTextMuted, 10);
					}
					context.uiManager.drawConstructed();
				}
			}
		}
		if (codepoints.empty())
			ui::text(context, "No loaded codepoint matches this filter");
		const auto selected =
			variant->unicodeToGlyphIndex.find(static_cast<uint32_t>(view.selected_glyph));
		if (selected != variant->unicodeToGlyphIndex.end() &&
			selected->second < variant->glyphs.size()) {
			const auto& glyph = variant->glyphs[selected->second];
			CLAY(context.clayID("selected-glyph"), ui::card()) {
				ui::text(context,
						 unicode_name(static_cast<uint32_t>(view.selected_glyph)) +
							 " · exact face " + std::to_string(asset.font.fontHandle),
						 interface_theme::kTextCanvas, 13);
				specimen_text(context, asset,
							  glyph_text(static_cast<uint32_t>(view.selected_glyph)), 64);
				ui::text(context,
						 "Design units · EM " + std::to_string(variant->emSize) + " · ascender " +
							 std::to_string(variant->ascender) + " · descender " +
							 std::to_string(variant->descender),
						 interface_theme::kTextSecondary, 11);
				ui::text(context,
						 "Advance " + std::to_string(glyph.advanceX) + " · ink width " +
							 std::to_string(glyph.planeRight - glyph.planeLeft) + " · bearing " +
							 std::to_string(glyph.planeLeft),
						 interface_theme::kTextSecondary, 11);
				ui::text(context,
						 "Atlas layer " + std::to_string(face->atlasLayer) + " · region [" +
							 std::to_string(glyph.imageLeft) + ", " +
							 std::to_string(glyph.imageBottom) + ", " +
							 std::to_string(glyph.imageRight) + ", " +
							 std::to_string(glyph.imageTop) + "] source texels",
						 interface_theme::kTextMuted, 11);
			}
		}
	}
	ui::text(context,
			 "Current text pipeline: codepoints, pair kerning and fallback. General complex-script "
			 "shaping is unavailable.",
			 interface_theme::kTextMuted, 11);
}
void shortcut_focus(Context& context, const DevCatalogueAsset& asset) {
	auto& state = context.params.interfaceState->catalogue;
	const auto& view = state.domains[state.domain];
	static constexpr std::array contexts{"No focus", "Input focus", "Element focus",
										 "Input + element focus"};
	static constexpr std::array transitions{"Press", "Release", "Down"};
	CLAY(context.clayID("shortcut-experiment"), ui::column()) {
		button(context, "shortcut-context", contexts[view.shortcut_context], ShortcutContext);
		button(context, "shortcut-transition", transitions[view.shortcut_transition],
			   ShortcutTransition);
	}
	auto chord = asset.shortcut.chord;
	chord.trigger = static_cast<ShortcutTrigger>(view.shortcut_transition);
	std::vector<const DevCatalogueAsset*> candidates;
	for (const auto& candidate : state.assets)
		if (candidate.domain == Domain::Shortcuts && candidate.window == asset.window &&
			ShortcutManager::devMatches(candidate.shortcut, chord, view.shortcut_context & 1,
										view.shortcut_context & 2))
			candidates.emplace_back(&candidate);
	std::ranges::sort(candidates, [](const auto* left, const auto* right) {
		return ShortcutManager::devPrecedes(left->shortcut, right->shortcut);
	});
	ui::text(context, "Hypothetical candidate order", interface_theme::kTextCanvas, 14);
	if (candidates.empty())
		ui::text(context, "No eligible registration for this exact chord and transition");
	for (const auto* candidate : candidates) {
		CLAY(context.clayID(Keyed("candidate", candidate->identity)), ui::card()) {
			ui::text(context, candidate->group + " → " + candidate->name,
					 interface_theme::kAccentSeaGlass);
			ui::text(context,
					 "Priority " + std::to_string(candidate->shortcut.priority) + " · order " +
						 std::to_string(candidate->shortcut.order),
					 interface_theme::kTextSecondary, 11);
			ui::text(context,
					 candidate->shortcut.handling == ShortcutHandling::PassThrough
						 ? "Continues after action; outcome unknown"
						 : "Stops if handled; otherwise continues",
					 interface_theme::kTextMuted, 11);
		}
	}
	ui::text(context,
			 "Pure matching and ordering share dispatcher rules. No input, focus, callback or "
			 "action is changed.",
			 interface_theme::kTextMuted, 11);
}
void focused(Context& context, const DevCatalogueAsset& asset, float width, float height) {
	auto& state = context.params.interfaceState->catalogue;
	auto& view = state.domains[state.domain];
	if (asset.domain == Domain::Viewports) {
		state.visible_viewports.emplace_back(asset.identity);
		if (!view.follow_observed_output && !asset.viewport.pending && asset.viewport.texture &&
			state.frozen_viewport_identity != asset.identity) {
			state.frozen_viewport = asset.viewport;
			state.frozen_viewport_identity = asset.identity;
		}
		const auto& observation =
			!view.follow_observed_output && state.frozen_viewport_identity == asset.identity
				? state.frozen_viewport
				: asset.viewport;
		if (observation.texture) {
			canvas(context, asset, width, std::max(100.0f, height - 72),
				   TextureRef{.handle = observation.texture,
							  .sourceWidth = static_cast<int32_t>(observation.width),
							  .sourceHeight = static_cast<int32_t>(observation.height)});
			ui::text(
				context,
				std::string(view.follow_observed_output ? "Follow observed output" : "Frozen") +
					" · source frame " + std::to_string(observation.frame) + " · generation " +
					std::to_string(observation.generation),
				interface_theme::kTextMuted, 11);
		} else
			ui::text(context, asset.status, interface_theme::kTextSecondary, 13);
		if (asset.viewport.pending)
			ui::text(context, "Waiting for the next natural source render",
					 interface_theme::kTextMuted, 11);
		ui::text(context, asset.detail, interface_theme::kTextMuted, 11);
	} else if (asset.domain == Domain::Images) {
		if (asset.image.status != devMode::DevResourceStatus::Ready) {
			ui::text(context, asset.status);
			return;
		}
		CLAY(context.clayID("image-presentation"), ui::row(30)) {
			button(context, "image-canvas", "Canvas", View, 0);
			button(context, "image-layout-fit", "Layout fit", View, view.view ? view.view : 1);
		}
		if (view.view == 0) {
			canvas(context, asset, width, std::max(140.0f, height - 80),
				   TextureRef{.handle = asset.image.textureHandle,
							  .sourceWidth = static_cast<int32_t>(asset.image.width),
							  .sourceHeight = static_cast<int32_t>(asset.image.height)});
		} else {
			CLAY(context.clayID("image-fit-modes"), ui::row(30)) {
				button(context, "image-contain", "Contain", View, 1);
				button(context, "image-cover", "Cover", View, 2);
				button(context, "image-stretch", "Stretch", View, 3);
			}
			static constexpr std::array<Clay_Dimensions, 3> frame_sizes{
				{{240, 160}, {320, 120}, {180, 240}}};
			const auto preset = frame_sizes[view.page % frame_sizes.size()];
			const float frame_width = std::min(width, preset.width);
			button(context, "image-frame-size",
				   "Frame " + std::to_string(static_cast<int>(frame_width)) + " x " +
					   std::to_string(static_cast<int>(preset.height)),
				   ImageFrame);
			ui::text(context,
					 view.view == 1
						 ? "Contain · entire image visible; this layout demonstration may upscale"
					 : view.view == 2 ? "Cover · cropped to fill the frame"
									  : "Stretch · aspect ratio is distorted to fill the frame",
					 interface_theme::kTextSecondary, 12);
			auto frame = ui::column(0);
			frame.layout.sizing = {CLAY_SIZING_FIXED(frame_width),
								   CLAY_SIZING_FIXED(preset.height)};
			frame.backgroundColor = background(view.background);
			frame.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
			frame.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
			CLAY(context.clayID("layout-fit-frame"), frame) {
				if (view.background == 0) {
					for (size_t row_index = 0;
						 row_index < static_cast<size_t>(std::ceil(preset.height / 24));
						 ++row_index) {
						auto row = ui::row(24);
						row.layout.childGap = 0;
						CLAY(context.clayID(Keyed("fit-checker-row", row_index)), row) {
							for (size_t column = 0;
								 column < static_cast<size_t>(std::ceil(frame_width / 24));
								 ++column) {
								Clay_ElementDeclaration tile{};
								tile.layout.sizing = {CLAY_SIZING_FIXED(24), CLAY_SIZING_FIXED(24)};
								tile.backgroundColor = (row_index + column) % 2
														   ? Clay_Color{29, 41, 48, 255}
														   : Clay_Color{40, 54, 63, 255};
								CLAY(context.clayID(
										 Keyed("fit-checker-tile", row_index * 16 + column)),
									 tile) {}
							}
						}
					}
				}
				Clay_ElementDeclaration image{};
				image.layout.sizing = {CLAY_SIZING_FIXED(frame_width),
									   CLAY_SIZING_FIXED(preset.height)};
				image.floating.attachTo = CLAY_ATTACH_TO_PARENT;
				image.floating.clipTo = CLAY_CLIP_TO_ATTACHED_PARENT;
				image.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
				const auto fit = view.view == 1	  ? TextureFitMode::Contain
								 : view.view == 2 ? TextureFitMode::Cover
												  : TextureFitMode::Stretch;
				image.image.imageData = context.uiManager.imageData(
					TextureRef{.handle = asset.image.textureHandle,
							   .fitMode = fit,
							   .sourceWidth = static_cast<int32_t>(asset.image.width),
							   .sourceHeight = static_cast<int32_t>(asset.image.height)});
				state.visible_textures.emplace_back(asset.image.textureHandle);
				CLAY(context.clayID("layout-fit-image"), image) {}
			}
		}
	} else if (asset.domain == Domain::Icons) {
		ui::text(context,
				 view.tint ? "Tint enabled · exact cached rasters"
						   : "Source colors · exact cached rasters",
				 interface_theme::kTextCanvas);
		if (asset.icons.empty())
			ui::text(context, "Registered · no cached raster. Choose an explicit step below to "
							  "create a preview.");
		if (view.view == 1 && !asset.icons.empty()) {
			if (!view.raster_identity)
				view.raster_identity =
					asset.icons[std::min<size_t>(view.page, asset.icons.size() - 1)]
						.atlasTexture.packed();
			const auto found = std::ranges::find_if(asset.icons, [&](const auto& raster) {
				return raster.atlasTexture.packed() == view.raster_identity;
			});
			if (found != asset.icons.end())
				canvas(context, asset, width, std::max(140.0f, height - 180),
					   icon_texture(*found, view.tint));
			else
				ui::text(context, "Cached raster no longer available",
						 interface_theme::kStatusAmber);
			button(context, "next-cached-raster", "Next cached size", NextRaster, 0,
				   asset.icons.size() > 1);
		} else
			for (const auto& raster : asset.icons) {
				const float row_height =
					std::max(44.0f, static_cast<float>(raster.targetHeight) + 8);
				CLAY(context.clayID(Keyed("cached-size", raster.atlasTexture.packed())),
					 ui::row(row_height)) {
					ui::text(context,
							 std::to_string(raster.targetWidth) + " x " +
								 std::to_string(raster.targetHeight) + " px",
							 interface_theme::kTextMuted, 11);
					for (uint64_t mode = 1; mode <= 2; ++mode) {
						auto comparison = ui::row(row_height);
						comparison.layout.sizing.width = CLAY_SIZING_FIXED(
							std::min(width / 3, static_cast<float>(raster.targetWidth) + 24));
						comparison.backgroundColor = background(mode);
						comparison.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
						CLAY(context.clayID(
								 Keyed("icon-background", raster.atlasTexture.packed() ^ mode)),
							 comparison)
						picture(context,
								Keyed("cached-raster", raster.atlasTexture.packed() ^ mode),
								icon_texture(raster, view.tint), width / 3 - 10,
								static_cast<float>(raster.targetHeight));
					}
				}
			}
		ui::text(context, "Rasterize at a physical pixel step", interface_theme::kTextSecondary,
				 12);
		icon_controls(context, asset, false);
	} else if (asset.domain == Domain::Fonts)
		font_focus(context, asset);
	else if (asset.domain == Domain::Elements) {
		ui::text(context, "Effective defaults · passive specimen · bounded host",
				 interface_theme::kTextSecondary, 11);
		static constexpr std::array<Clay_Dimensions, 3> host_sizes{
			{{360, 220}, {480, 320}, {240, 160}}};
		const auto host_size = host_sizes[view.page % host_sizes.size()];
		auto stage = ui::column();
		stage.layout.sizing = {CLAY_SIZING_FIXED(std::min(width, host_size.width)),
							   CLAY_SIZING_FIXED(host_size.height)};
		stage.layout.padding = {12, 12, 12, 12};
		stage.backgroundColor = background(view.background);
		stage.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
		stage.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
		CLAY(context.clayID("element-stage"), stage) element_preview(context, asset, true);
		ui::text(context,
				 "Empty text, absent borrowed values and unpainted layouts keep their actual "
				 "defaults. The host outline shows available space.",
				 interface_theme::kTextMuted, 11);
	} else if (asset.domain == Domain::Themes) {
		if (view.view == 0)
			theme_tokens(context, asset, 10000);
		else
			theme_components(context, asset);
	} else if (asset.domain == Domain::Shortcuts)
		shortcut_focus(context, asset);
	else if (asset.domain == Domain::Atlases &&
			 asset.atlas.kind == devMode::DevAtlasKind::IconVectorAtlas &&
			 asset.atlas.atlasTexture) {
		canvas(context, asset, width, std::max(140.0f, height - 45),
			   TextureRef{.handle = asset.atlas.atlasTexture,
						  .sourceWidth = static_cast<int32_t>(asset.atlas.width),
						  .sourceHeight = static_cast<int32_t>(asset.atlas.height)});
		ui::text(context, "Verified icon owners on this page", interface_theme::kTextCanvas, 13);
		size_t owner_count = 0;
		for (const auto& owner : state.assets) {
			if (owner.domain != Domain::Icons ||
				!std::ranges::any_of(owner.icons, [&](const auto& raster) {
					return raster.atlasLayer == asset.atlas.pageIndex;
				}))
				continue;
			if (owner_count++ >= 64) {
				ui::text(context, "Additional owners are available in Icons",
						 interface_theme::kTextMuted, 11);
				break;
			}
			button(context, Keyed("atlas-owner", owner.identity), owner.name, Select,
				   owner.identity);
		}
	} else if (asset.domain == Domain::Atlases &&
			   asset.atlas.kind == devMode::DevAtlasKind::FontMsdfAtlas) {
		ui::text(context, "MSDF reconstruction · layer " + std::to_string(asset.atlas.pageIndex),
				 interface_theme::kTextCanvas, 16);
		bool found_owner = false;
		for (const auto& owner : state.assets) {
			if (owner.domain != Domain::Fonts || owner.font.atlasLayer != asset.atlas.pageIndex)
				continue;
			found_owner = true;
			specimen_text(context, owner, "Aa Bb Il1 O0 0123456789", 40);
			button(context, Keyed("font-atlas-owner", owner.identity), "Open face: " + owner.name,
				   Select, owner.identity);
		}
		if (!found_owner)
			ui::text(context, "No registered face currently owns this layer");
		ui::text(context,
				 "Reconstruction uses the registered face's real MSDF renderer. Raw font atlas "
				 "sampling is unavailable through the current texture binding.",
				 interface_theme::kTextMuted, 11);
	} else
		CLAY(context.clayID("contract"), ui::card()) {
			ui::text(context, asset.status, interface_theme::kAccentSeaGlass, 17);
			ui::text(context, asset.detail);
			for (const auto& fact : asset.facts)
				ui::text(context, fact.first + "  " + fact.second, interface_theme::kTextSecondary,
						 12);
		}
}
} // namespace
void DevCatalogueWorkbench::buildElement(BuildContext& context) {
	if (!context.params.app || !context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->catalogue;
	if (state.domain == static_cast<uint64_t>(Domain::Themes))
		state.theme_observation_available =
			static_cast<bool>(state.theme_model.refresh(*context.params.app));
	if (state.dirty)
		rebuild_catalogue_filter(state);
	state.visible_textures.clear();
	state.visible_viewports.clear();
	auto& local = context.state();
	auto& view = state.domains[state.domain];
	const auto* selected = selected_catalogue_asset(state);
	const auto parent = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const float width = std::max(180.0f, parent.found ? parent.boundingBox.width - 24 : 480),
				height = std::max(180.0f, parent.found ? parent.boundingBox.height - 145 : 400);
	auto root = ui::column(8);
	root.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	root.layout.sizing.height = CLAY_SIZING_PERCENT(1);
	root.layout.padding = {12, 12, 10, 10};
	root.clip = {.horizontal = true, .scrollInputDisabled = true};
	CLAY(context.clayID(), root) {
		auto title = ui::row(32);
		title.clip = {.horizontal = true, .scrollInputDisabled = true};
		CLAY(context.clayID("title"), title) {
			button(context, "all",
				   std::string(catalogue_domain_name(static_cast<Domain>(state.domain))), All);
			ui::text(context,
					 selected			  ? "/ " + selected->name
					 : view.group.empty() ? "/ All"
										  : "/ " + view.group,
					 interface_theme::kTextCanvas, 14);
		}
		auto toolbar = ui::row(30);
		toolbar.clip = {.horizontal = true, .scrollInputDisabled = true};
		CLAY(context.clayID("toolbar"), toolbar) {
			if (state.domain == static_cast<uint64_t>(Domain::Viewports)) {
				button(context, "viewport-follow",
					   view.follow_observed_output ? "Follow observed" : "Frozen", FollowViewport,
					   0, true, "Follow copies output only when the source naturally renders");
				if (selected)
					button(context, "viewport-capture", "Capture next output", CaptureViewport,
						   selected->identity, selected->viewport.supported);
			}
			if (selected && ((selected->domain == Domain::Images && view.view == 0) ||
							 selected->domain == Domain::Atlases)) {
				button(context, "fit", "Fit", Fit);
				button(context, "native", "1:1", Native);
				button(context, "zoom-out", "-", ZoomOut);
				button(context, "zoom-in", "+", ZoomIn);
			}
			if (selected && selected->domain == Domain::Atlases &&
				selected->atlas.kind == devMode::DevAtlasKind::IconVectorAtlas)
				button(context, "atlas-overlay", view.view == 1 ? "Hide regions" : "Show regions",
					   View, view.view == 1 ? 0 : 1);
			if (state.domain == static_cast<uint64_t>(Domain::Icons)) {
				button(context, "icon-tint", view.tint ? "Tint" : "Source", Tint);
				if (selected) {
					button(context, "icon-ladder", "Cached sizes", View, 0);
					button(context, "icon-exact", "Inspect raster", View, 1);
				}
			}
			if (selected && selected->domain == Domain::Themes) {
				button(context, "tokens", "Tokens", View, 0);
				button(context, "components", "Components", View, 1);
			}
			if (selected && selected->domain == Domain::Fonts) {
				button(context, "specimen", "Specimen", View, 0);
				button(context, "paragraph", "Paragraph", View, 1);
				button(context, "glyphs", "Glyph metrics", View, 2);
			}
			if (selected && selected->domain == Domain::Elements) {
				static constexpr std::array host_labels{"Host 360 x 220", "Host 480 x 320",
														"Host 240 x 160"};
				button(context, "element-host-size", host_labels[view.page % host_labels.size()],
					   HostSize);
				const auto* adapter = element_adapter(context, *selected);
				if (adapter && adapter->has_draw && adapter->has_construct) {
					button(context, "draw", "Draw", View, 0);
					button(context, "construct", "Construct", View, 1);
				}
			}
			if (selected && selected->domain != Domain::Icons &&
				selected->domain != Domain::Viewports)
				button(context, "background",
					   view.background == 0	  ? "Checkerboard"
					   : view.background == 1 ? "Light"
											  : "Dark",
					   Background);
			else if (!selected)
				button(context, "refresh", "Refresh", Refresh);
		}
		if (state.domain == static_cast<uint64_t>(Domain::Icons)) {
			CLAY(context.clayID("icon-toolbar-secondary"), ui::row(30)) {
				button(context, "raster-step",
					   "Step " +
						   std::to_string(
							   catalogue_icon_steps[std::min<size_t>(view.raster_step, 6)]) +
						   "px",
					   RasterStep);
				if (selected) {
					button(context, "background",
						   view.background == 0	  ? "Checker"
						   : view.background == 1 ? "Light"
												  : "Dark",
						   Background);
					if (view.view == 1 && width >= 460) {
						button(context, "fit", "Fit", Fit);
						button(context, "native", "1:1", Native);
						button(context, "zoom-out", "-", ZoomOut);
						button(context, "zoom-in", "+", ZoomIn);
					}
				} else
					button(context, "remove-all-previews", "Remove preview rasters", Remove, 0,
						   !state.preview_rasters.empty());
			}
			if (selected && view.view == 1 && width < 460)
				CLAY(context.clayID("icon-toolbar-zoom"), ui::row(30)) {
					button(context, "fit", "Fit", Fit);
					button(context, "native", "1:1", Native);
					button(context, "zoom-out", "-", ZoomOut);
					button(context, "zoom-in", "+", ZoomIn);
				}
		}
		if (selected && selected->domain == Domain::Viewports)
			CLAY(context.clayID("viewport-toolbar-view"), ui::row(30)) {
				button(context, "fit", "Fit", Fit);
				button(context, "native", "1:1", Native);
				button(context, "zoom-out", "-", ZoomOut);
				button(context, "zoom-in", "+", ZoomIn);
				button(context, "background",
					   view.background == 0	  ? "Checker"
					   : view.background == 1 ? "Light"
											  : "Dark",
					   Background);
			}
		if (state.selected_outside_filter)
			ui::text(context, "Selected asset is outside this filter",
					 interface_theme::kStatusAmber, 11);
		if (state.selected_missing)
			ui::text(context, "Asset no longer registered: " + state.selected_name,
					 interface_theme::kStatusAmber);
		else if (selected) {
			const auto scroll_id = context.clayID("focused-scroll");
			auto scroll = Clay_GetScrollContainerData(scroll_id);
			if (scroll.found && scroll.scrollPosition &&
				(local.last_domain != state.domain ||
				 local.last_selection != state.selected_identity))
				scroll.scrollPosition->y = 0;
			auto surface = ui::column(10);
			surface.layout.sizing.width = CLAY_SIZING_PERCENT(1);
			surface.layout.sizing.height = CLAY_SIZING_GROW(0);
			surface.clip = {.vertical = true,
							.childOffset = scroll.found && scroll.scrollPosition
											   ? *scroll.scrollPosition
											   : Clay_Vector2{}};
			catalogue_focus::register_target(context, state, scroll_id);
			CLAY(scroll_id, surface) {
				const catalogue_focus::ViewportScope focus_viewport(state, scroll_id);
				focused(context, *selected, width, height);
			}
		} else
			gallery(context, width, height);
		if (!state.message.empty())
			ui::text(context, state.message,
					 state.message_is_error ? interface_theme::kStatusAmber
											: interface_theme::kAccentSeaGlass,
					 11);
		ui::text(context,
				 std::to_string(state.visible_assets.size()) + " / " +
					 std::to_string(state.counts[state.domain]) +
					 " registered assets · observation " + std::to_string(state.revision),
				 interface_theme::kTextMuted, 11);
	}
	local.last_domain = state.domain;
	local.last_selection = state.selected_identity;
}
} // namespace FlowUi::devSystems::interface_elements
#endif
