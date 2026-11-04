#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devTooling/catalogue/DevElementPreviews.hpp"
#include "managers/IconManager.hpp"
#include <cassert>
#include <memory>

namespace {
struct NoncopyableParameters {
	std::unique_ptr<int> value = std::make_unique<int>(7);
};
struct DefaultProbe {
	using Parameters = NoncopyableParameters;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("catalogue.test.noncopyable");
	static inline unsigned builds = 0;
	static void buildElement(FlowUi::ElementBuildContext<DefaultProbe>& context) {
		builds += static_cast<unsigned>(*context.params.value);
	}
};
struct ConstructProbe {
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("catalogue.test.construct");
	static Clay_ElementDeclaration constructElement(FlowUi::ElementBuildContext<ConstructProbe>&) {
		return Clay_ElementDeclaration{
			.layout = {.sizing = {CLAY_SIZING_FIXED(20), CLAY_SIZING_FIXED(20)}}};
	}
};
struct ColorTheme {
	float value = 1;
};
struct SpacingTheme {
	float value = 2;
};

struct NestedProbe {
	static constexpr auto definitionId = FlowUi::DefinitionID("catalogue.test.nested");
	static inline unsigned logic_calls = 0;
	static void runLogic(FlowUi::ElementInteractionContext<NestedProbe>&) { ++logic_calls; }
	static void buildElement(FlowUi::ElementBuildContext<NestedProbe>& context) {
		CLAY(context.clayID(),
			 Clay_ElementDeclaration{
				 .layout = {.sizing = {CLAY_SIZING_FIXED(20), CLAY_SIZING_FIXED(20)}}}) {}
	}
};
struct ParentProbe {
	static constexpr auto definitionId = FlowUi::DefinitionID("catalogue.test.parent");
	static inline unsigned logic_calls = 0;
	static void runLogic(FlowUi::ElementInteractionContext<ParentProbe>&) { ++logic_calls; }
	static void buildElement(FlowUi::ElementBuildContext<ParentProbe>& context) {
		CLAY(context.clayID(),
			 Clay_ElementDeclaration{
				 .layout = {.sizing = {CLAY_SIZING_FIXED(30), CLAY_SIZING_FIXED(30)}}}) {
			context.uiManager.createElement(NestedProbe{}, "child").draw();
		}
	}
};

template <class Theme>
[[nodiscard]] FlowUi::ThemeManager::DevThemePayloadView payload(const Theme& theme) noexcept {
	return {
		.type = FlowUi::detail::typeHash<Theme>(), .payload = &theme, .payloadSize = sizeof(Theme)};
}

void registration_preserves_admission_and_noncopyable_defaults() {
	FlowUi::devMode::DevSchemaRegistry schemas;
	schemas.registerElementPreview<DefaultProbe>();
	assert(schemas.elementPreviews().size() == 1);
	assert(!schemas.elementPreviews().front().admitted);
	const auto discovery_revision = schemas.previewRevision();
	FlowUi::devMode::register_element_catalogue(
		schemas, "Custom set", FlowUi::ElementSet<DefaultProbe, ConstructProbe>{}, true);
	assert(schemas.elementPreviews().size() == 2);
	assert(schemas.previewRevision() > discovery_revision);
	assert(schemas.elementPreviews().front().admitted);
	assert(schemas.elementPreviews().front().has_draw);
	assert(!schemas.elementPreviews().front().has_construct);
	assert(schemas.elementPreviews().back().has_construct);
	assert(!schemas.elementPreviews().back().has_draw);
	const auto admitted_revision = schemas.previewRevision();
	schemas.registerElementPreview<DefaultProbe>();
	assert(schemas.elementPreviews().front().admitted);
	assert(schemas.previewRevision() == admitted_revision);
	assert(DefaultProbe::builds == 0);
	assert(schemas.publishPendingAtSafePoint());
	assert(schemas.view()->findElement(DefaultProbe::definitionId));
}

void theme_and_passive_scopes_restore_nested_resolution(FlowUi::UiManager& ui) {
	const ColorTheme outer{11}, inner{22};
	const SpacingTheme spacing{33};
	assert(!ui.isDevPassivePreview());
	{
		const FlowUi::UiManager::DevThemeScope outer_scope(ui, payload(outer));
		const FlowUi::UiManager::DevPassivePreviewScope passive(ui);
		assert(ui.isDevPassivePreview());
		assert(ui.isDevInternalScope());
		assert(&ui.theme<ColorTheme>() == &outer);
		{
			const FlowUi::UiManager::DevThemeScope inner_scope(ui, payload(inner));
			const FlowUi::UiManager::DevThemeScope other_type(ui, payload(spacing));
			const FlowUi::UiManager::DevPassivePreviewScope nested(ui);
			assert(&ui.theme<ColorTheme>() == &inner);
			assert(&ui.theme<SpacingTheme>() == &spacing);
		}
		assert(&ui.theme<ColorTheme>() == &outer);
		assert(ui.isDevPassivePreview());
	}
	assert(!ui.isDevPassivePreview());
	assert(!ui.isDevInternalScope());
}

void verify_live_preview_scopes() {
	FlowUi::AppConfig config;
	config.window.title = "Catalogue passive preview verification";
	config.window.width = 320;
	config.window.height = 240;
	auto app = FlowUi::makeApplication(config);
	assert(app.themes().registerTheme(ColorTheme{1}));
	theme_and_passive_scopes_restore_nested_resolution(app.ui());
	assert(app.ui().theme<ColorTheme>().value == 1);
	assert(app.themes().getActiveTheme<ColorTheme>().value == 1);
#if FLOWUI_INCLUDE_ICON_MANAGER
	assert(!app.icons().devRasterize("", 16, 16));
	assert(!app.icons().devRasterize("catalogue-test/unregistered", 16, 16));
	constexpr std::string_view icon_name = "catalogue-test/logical-icon";
	assert(app.icons().registerSvg(
		icon_name,
		R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect width="16" height="16" fill="#ef8530"/></svg>)"));
	const auto existing = app.icons().devRasterize(icon_name, 16, 16);
	assert(existing && existing->newlyRasterized);
	const auto reused = app.icons().devRasterize(icon_name, 16, 16);
	assert(reused && !reused->newlyRasterized && reused->texture == existing->texture);
	FlowUi::devSystems::DevInterfaceState interface_state;
	FlowUi::devSystems::service_catalogue(app, interface_state, true);
	auto& catalogue = interface_state.catalogue;
	const auto find_icon = [&]() {
		return std::ranges::find(catalogue.assets, icon_name,
								 &FlowUi::devSystems::DevCatalogueAsset::name);
	};
	assert(find_icon() != catalogue.assets.end());
	const auto identity = find_icon()->identity;
	FlowUi::devSystems::select_catalogue_asset(catalogue, identity);
	assert(catalogue.preview_rasters.empty() && find_icon()->icons.size() == 1);
	catalogue.pending = FlowUi::devSystems::DevCatalogueCommand::RasterizeIcon;
	catalogue.pending_identity = identity;
	catalogue.pending_size = 24;
	FlowUi::devSystems::service_catalogue(app, interface_state, true);
	assert(catalogue.preview_rasters.size() == 1 && find_icon()->icons.size() == 2);
	const auto preview = catalogue.preview_rasters.front().texture;
	catalogue.pending = FlowUi::devSystems::DevCatalogueCommand::RasterizeIcon;
	catalogue.pending_size = 16;
	FlowUi::devSystems::service_catalogue(app, interface_state, true);
	assert(catalogue.preview_rasters.size() == 1);
	catalogue.pending = FlowUi::devSystems::DevCatalogueCommand::RemovePreviewRasters;
	catalogue.pending_identity = 0;
	FlowUi::devSystems::service_catalogue(app, interface_state, true);
	assert(catalogue.preview_rasters.empty() && find_icon()->icons.size() == 1);
	assert(find_icon()->icons.front().atlasTexture == existing->texture);
	assert(app.icons().contains(icon_name));
	const auto already_removed = app.icons().devRemoveRaster(preview);
	assert(already_removed && !*already_removed);
	const auto replacement = app.icons().devRasterize(icon_name, 24, 24);
	assert(replacement && replacement->newlyRasterized && replacement->texture != preview);
	const auto stale_removal = app.icons().devRemoveRaster(preview);
	assert(stale_removal && !*stale_removal);
	const auto removed_replacement = app.icons().devRemoveRaster(replacement->texture);
	assert(removed_replacement && *removed_replacement);
#endif
	assert(app.beginFrame());
	app.ui().createElement(ParentProbe{}, "normal").draw();
	assert(ParentProbe::logic_calls == 1 && NestedProbe::logic_calls == 1);
	{
		const FlowUi::UiManager::DevPassivePreviewScope passive(app.ui());
		app.ui().createElement(ParentProbe{}, "passive").draw();
	}
	assert(ParentProbe::logic_calls == 1 && NestedProbe::logic_calls == 1);
	assert(app.endFrame());
	assert(app.drawFrame());
}
} // namespace
int main(int argument_count, char**) {
	registration_preserves_admission_and_noncopyable_defaults();
	if (argument_count > 1)
		verify_live_preview_scopes();
}
