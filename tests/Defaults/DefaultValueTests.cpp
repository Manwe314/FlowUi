#include "TestHarness.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

#include "FSEL/StandardIcons.hpp"
#include "FlowUi/Public/FlowUiTheme.hpp"
#include "FlowUi/Public/defaults/InputDefaults.hpp"
#include "FlowUi/Public/defaults/TextureDefaults.hpp"

#if FLOWUI_INCLUDE_ICON_MANAGER
#include <plutosvg.h>
#endif

namespace {

void test_missing_texture_pixels() {
	const auto texture = FlowUi::defaults::missing_texture();
	FLOWUI_CHECK(texture.width == 2);
	FLOWUI_CHECK(texture.height == 2);
	FLOWUI_CHECK(texture.bytes.size() == 16);

	constexpr std::array<std::byte, 4> purple{
		std::byte{128}, std::byte{0}, std::byte{255}, std::byte{255}};
	constexpr std::array<std::byte, 4> cyan{
		std::byte{0}, std::byte{255}, std::byte{255}, std::byte{255}};
	FLOWUI_CHECK(std::ranges::equal(texture.bytes.subspan(0, 4), purple));
	FLOWUI_CHECK(std::ranges::equal(texture.bytes.subspan(4, 4), cyan));
	FLOWUI_CHECK(std::ranges::equal(texture.bytes.subspan(8, 4), cyan));
	FLOWUI_CHECK(std::ranges::equal(texture.bytes.subspan(12, 4), purple));
	FLOWUI_CHECK(FlowUi::defaults::missing_texture().bytes.data() == texture.bytes.data());
}

void test_theme_presets() {
	const auto dark = FlowUi::FlowUiTheme::dark();
	const auto light = FlowUi::FlowUiTheme::light();
	FLOWUI_CHECK(dark.background.r == 30.0f / 255.0f);
	FLOWUI_CHECK(dark.textPrimary.r == 1.0f);
	FLOWUI_CHECK(light.background.r == 1.0f);
	FLOWUI_CHECK(light.background.a == 1.0f);
	FLOWUI_CHECK(light.textPrimary.r == 0.0f);
	FLOWUI_CHECK(light.textPrimary.a == 1.0f);
	FLOWUI_CHECK(light.border.r == 204.0f / 255.0f);
	FLOWUI_CHECK(light.primary.r == dark.primary.r);
	FLOWUI_CHECK(light.primary.g == dark.primary.g);
	FLOWUI_CHECK(light.defaultFontFamily == dark.defaultFontFamily);
	FLOWUI_CHECK(light.spacingMd == dark.spacingMd);
}

void test_input_overlay_defaults_follow_configuration() {
	FlowUi::InputManagerConfig config{};
	config.caretWidthPx = 3.0f;
	config.caretHeightOverflowTopPx = 4.0f;
	config.caretHeightOverflowBottomPx = 5.0f;
	config.caretColor = {1.0f, 2.0f, 3.0f, 4.0f};
	config.highlightBoxColor = {5.0f, 6.0f, 7.0f, 8.0f};
	config.highlightedTextColor = {9.0f, 10.0f, 11.0f, 12.0f};

	const auto style = FlowUi::defaults::input_field_overlay_style(config);
	FLOWUI_CHECK(style.caretShape == FlowUi::InputCaretShape::Bar);
	FLOWUI_CHECK(style.caretThicknessPx == 3.0f);
	FLOWUI_CHECK(style.caretBlockWidthPx == 8.0f);
	FLOWUI_CHECK(style.caretHeightOverflowTopPx == 4.0f);
	FLOWUI_CHECK(style.caretHeightOverflowBottomPx == 5.0f);
	FLOWUI_CHECK(style.caretColor.r == config.caretColor.r);
	FLOWUI_CHECK(style.caretColor.a == config.caretColor.a);
	FLOWUI_CHECK(style.selectionBoxColor.g == config.highlightBoxColor.g);
	FLOWUI_CHECK(style.selectionBoxColor.a == config.highlightBoxColor.a);
	FLOWUI_CHECK(style.selectedTextColor.b == config.highlightedTextColor.b);
	FLOWUI_CHECK(style.selectedTextColor.a == config.highlightedTextColor.a);
	FLOWUI_CHECK(style.caretBlinkPeriodSeconds == 1.0);
	FLOWUI_CHECK(style.caretBlinkVisibleSeconds == 0.5);

	config.caretWidthPx = 12.0f;
	FLOWUI_CHECK(FlowUi::defaults::input_field_overlay_style(config).caretBlockWidthPx == 12.0f);
}

#if FLOWUI_INCLUDE_ICON_MANAGER
void test_default_fsel_svgs_rasterize() {
	using namespace FlowUi::FSEL::standard_icons;
	constexpr std::array<std::string_view, 4> icon_sources{
		kIncrementSvg, kDecrementSvg, kComboBoxOpenSvg, kComboBoxClosedSvg};

	for (const auto source : icon_sources) {
		const std::unique_ptr<plutosvg_document_t, decltype(&plutosvg_document_destroy)> document(
			plutosvg_document_load_from_data(source.data(), static_cast<int>(source.size()),
											-1.0f, -1.0f, nullptr, nullptr),
			&plutosvg_document_destroy);
		FLOWUI_CHECK(document != nullptr);
		const std::unique_ptr<plutovg_surface_t, decltype(&plutovg_surface_destroy)> surface(
			plutosvg_document_render_to_surface(document.get(), nullptr, 16, 16, nullptr,
											  nullptr, nullptr),
			&plutovg_surface_destroy);
		FLOWUI_CHECK(surface != nullptr);
		FLOWUI_CHECK(plutovg_surface_get_width(surface.get()) == 16);
		FLOWUI_CHECK(plutovg_surface_get_height(surface.get()) == 16);
		const auto pixels = std::span(
			plutovg_surface_get_data(surface.get()),
			static_cast<size_t>(plutovg_surface_get_stride(surface.get()) * 16));
		FLOWUI_CHECK(std::ranges::any_of(pixels, [](unsigned char channel) { return channel != 0; }));
	}
}
#endif

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("missing texture has opaque purple/cyan diagonal cells", test_missing_texture_pixels);
	runner.run("dark and light theme presets", test_theme_presets);
	runner.run("input overlay defaults follow configuration", test_input_overlay_defaults_follow_configuration);
#if FLOWUI_INCLUDE_ICON_MANAGER
	runner.run("default FSEL SVG artwork rasterizes", test_default_fsel_svgs_rasterize);
#endif
	return runner.finish();
}
