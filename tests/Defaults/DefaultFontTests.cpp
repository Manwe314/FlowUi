#include "TestHarness.hpp"

#include "FlowUi.hpp"
#include "FlowUi/Private/textLayoutService/CurrentTextShaper.hpp"
#include "FlowUi/Public/Resources.hpp"
#include "FlowUi/Public/defaults/FontDefaults.hpp"
#include "FlowUi/Public/defaults/FselIcons.hpp"
#include "FlowUi/Public/managers/FontManager.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <artery-font/std-artery-font.h>

namespace {

[[nodiscard]] int read_payload(void* destination, int length, void* user_data) noexcept {
	if (length <= 0) return 0;
	auto& remaining = *static_cast<std::span<const std::byte>*>(user_data);
	const size_t byte_count = std::min(remaining.size(), static_cast<size_t>(length));
	if (byte_count != 0) std::memcpy(destination, remaining.data(), byte_count);
	remaining = remaining.subspan(byte_count);
	return static_cast<int>(byte_count);
}

void test_payload() {
	const auto face = FlowUi::defaults::missing_font();
	FLOWUI_CHECK(face.path.empty());
	FLOWUI_CHECK(!face.baked_data.empty());
	FLOWUI_CHECK(face.baked_data.data() == FlowUi::defaults::missing_font().baked_data.data());
	auto remaining = face.baked_data;
	artery_font::StdArteryFont<float> decoded{};
	FLOWUI_CHECK(artery_font::decode<read_payload>(decoded, &remaining));
	FLOWUI_CHECK(remaining.empty());
	FLOWUI_CHECK(decoded.variants.length() == 1);
	FLOWUI_CHECK(decoded.images.length() == 1);
	const auto& variant = decoded.variants[0];
	FLOWUI_CHECK(variant.glyphs.length() == 1);
	FLOWUI_CHECK(variant.fallbackGlyph == 0);
	FLOWUI_CHECK(variant.codepointType == artery_font::CP_UNICODE);
	FLOWUI_CHECK(variant.glyphs[0].codepoint == 0xfffd);
	FLOWUI_CHECK(variant.glyphs[0].advance.h > 0.0f);
	FLOWUI_CHECK(variant.metrics.lineHeight > 0.0f);
	FLOWUI_CHECK(variant.metrics.distanceRange == 4.0f);
	const auto& image = decoded.images[0];
	FLOWUI_CHECK(image.encoding == artery_font::IMAGE_RAW_BINARY);
	FLOWUI_CHECK(image.imageType == artery_font::IMAGE_SDF);
	FLOWUI_CHECK(image.pixelFormat == artery_font::PIXEL_UNSIGNED8);
	FLOWUI_CHECK(image.width == 8 && image.height == 12 && image.channels == 1);
	FLOWUI_CHECK(image.data.length() == 96);
	for (uint32_t row = 0; row < image.height; ++row) {
		for (uint32_t column = 0; column < image.width; ++column) {
			const auto sample = image.data[static_cast<int>(row * image.width + column)];
			const bool inside_rectangle = column >= 2 && column < 6 && row >= 2 && row < 10;
			FLOWUI_CHECK((sample > 127) == inside_rectangle);
		}
	}
}

class ShaderOnlyResources {
public:
	ShaderOnlyResources() {
		const auto shader = FlowUi::locate_resource("shaders/flowui_ui_solid.vert.spv");
		FLOWUI_CHECK(shader);
		directory_ = std::filesystem::temp_directory_path() /
			("flowui-default-font-" + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(directory_ / "shaders");
		std::filesystem::copy(shader->parent_path(), directory_ / "shaders",
			std::filesystem::copy_options::recursive |
			std::filesystem::copy_options::overwrite_existing);
	}
	~ShaderOnlyResources() {
		std::error_code error;
		std::filesystem::remove_all(directory_, error);
	}
	[[nodiscard]] FlowUi::ResourceConfig config() const {
		return {.roots = {directory_}, .use_platform_defaults = false, .use_build_directory = false};
	}

private:
	std::filesystem::path directory_{};
};

struct FontRecoveryCapture {
	uint32_t fallback_count = 0;
	uint32_t skipped_count = 0;
};

void capture_recovery(void* user_data, const FlowUi::ErrorEventView& event) noexcept {
	if (event.error.code != FlowUi::ErrorCode::DefaultFontUnavailable) return;
	auto& capture = *static_cast<FontRecoveryCapture*>(user_data);
	if (event.resolution == FlowUi::ErrorResolution::UsedFallback) ++capture.fallback_count;
	if (event.resolution == FlowUi::ErrorResolution::Skipped) ++capture.skipped_count;
}

void test_loaded_face(const FlowUi::Font::FontFaceData& face) {
	const auto* variant = face.defaultVariant();
	FLOWUI_CHECK(variant && variant->glyphs.size() == 1);
	FLOWUI_CHECK(variant->unicodeToGlyphIndex.at(0xfffd) == 0);
	FLOWUI_CHECK(variant->fallbackGlyphIndex == 0);
	FLOWUI_CHECK(face.sourceAtlasWidth == 8 && face.sourceAtlasHeight == 12);
	const FlowUi::detail::text::CurrentTextShaper shaper{};
	const auto shaped = shaper.shape({
		.text = "A\xEF\xBF\xBD\xE7\x95\x8C",
		.fontSize = 16,
		.includeGlyphGeometry = true,
	}, face);
	FLOWUI_CHECK(shaped.success && shaped.glyphs.size() == 3);
	for (const auto& glyph : shaped.glyphs) {
		FLOWUI_CHECK(glyph.glyphIndex == 0);
		FLOWUI_CHECK(glyph.width > 0.0f && glyph.height > 0.0f);
		FLOWUI_CHECK(glyph.u0 < glyph.u1 && glyph.v0 < glyph.v1);
		FLOWUI_CHECK(std::isfinite(glyph.x) && std::isfinite(glyph.y));
	}
	const auto whitespace = shaper.shape({
		.text = " \t\xC2\xA0\n",
		.fontSize = 16,
		.includeGlyphGeometry = true,
	}, face);
	FLOWUI_CHECK(whitespace.success && whitespace.glyphs.empty());
	FLOWUI_CHECK(whitespace.lines.size() == 2);
	FLOWUI_CHECK(whitespace.measuredAdvance > 0.0f && whitespace.lineHeight > 0.0f);
}

#if COMPILE_FSELI && FLOWUI_INCLUDE_ICON_MANAGER
void test_fsel_icons(FlowUi::IconManager& icons) {
	namespace standard_icons = FlowUi::FSEL::standard_icons;
	for (const auto key : {standard_icons::kIncrementKey, standard_icons::kDecrementKey,
		standard_icons::kComboBoxOpenKey, standard_icons::kComboBoxClosedKey}) {
		FLOWUI_CHECK(icons.contains(key));
	}
	const auto removed = icons.remove(standard_icons::kIncrementKey);
	FLOWUI_CHECK(removed && *removed);
	const auto installed = icons.registerSvg(standard_icons::kIncrementKey,
		R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 8 8"><rect width="8" height="8" fill="#fff"/></svg>)svg");
	FLOWUI_CHECK(installed && *installed);
	const auto custom_texture = icons.textureRef(standard_icons::kIncrementKey);
	FLOWUI_CHECK(custom_texture.handle);
#if FLOW_UI_DEV_MODE
	const auto custom_revision = icons.devRevision();
#endif
	standard_icons::registerStandardIcons(icons);
	FLOWUI_CHECK(icons.textureRef(standard_icons::kIncrementKey).handle == custom_texture.handle);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(icons.devRevision() == custom_revision);
#endif
}
#endif

void test_startup(FlowUi::DefaultFontFailurePolicy policy, bool configured_memory = false) {
	const ShaderOnlyResources resources{};
	FlowUi::AppConfig config{};
	config.resources = resources.config();
	FLOWUI_CHECK(!FlowUi::locate_resource("fonts/Inter.arfont", config.resources));
	if (configured_memory) {
		config.ui.defaultFontFamily.faces = {FlowUi::defaults::missing_font()};
	} else {
		config.ui.defaultFontFamily.faces.front().path = "missing-user-font.arfont";
	}
	config.ui.fontAtlasSize = 64;
	config.window.width = 160;
	config.window.height = 120;
	config.errors.policy.defaultFont = policy;
	FontRecoveryCapture capture{};
	config.errors.observer.mode = FlowUi::ErrorReportingMode::SinkOnly;
	config.errors.observer.sink = {.userData = &capture, .callback = &capture_recovery};
	if (policy == FlowUi::DefaultFontFailurePolicy::FailAppImmediately && !configured_memory) {
		bool failed_as_requested = false;
		try {
			auto application = FlowUi::makeApplication(config);
			FLOWUI_CHECK(application.fonts().getFontById(0) == nullptr);
		} catch (const FlowUi::FlowUiException& exception) {
			failed_as_requested = exception.error().code == FlowUi::ErrorCode::DefaultFontUnavailable;
		}
		FLOWUI_CHECK(failed_as_requested);
		FLOWUI_CHECK(capture.fallback_count == 0 && capture.skipped_count == 0);
		return;
	}

	auto application = FlowUi::makeApplication(config);
	FLOWUI_CHECK(capture.fallback_count == (configured_memory ? 0u : 1u));
	FLOWUI_CHECK(capture.skipped_count == 0);
	const auto* default_face = application.fonts().getFontById(0);
	FLOWUI_CHECK(default_face);
	test_loaded_face(*default_face);
#if COMPILE_FSELI && FLOWUI_INCLUDE_ICON_MANAGER
	test_fsel_icons(application.icons());
#endif

	FlowUi::FontFamilyCreateInfo memory_family{};
	memory_family.name = "Borrowed font bytes";
	memory_family.faces.clear();
	const auto family = application.fonts().createFamily(memory_family);
	FLOWUI_CHECK(family);
	FlowUi::FontId loaded_font = std::numeric_limits<FlowUi::FontId>::max();
	{
		auto face = FlowUi::defaults::missing_font();
		std::vector<std::byte> owned_bytes(face.baked_data.begin(), face.baked_data.end());
		face.baked_data = owned_bytes;
		face.path = "nonexistent-path-ignored-for-memory.arfont";
		const auto added = application.fonts().addFamilyFace(*family, face);
		FLOWUI_CHECK(added);
		loaded_font = *added;
	}
	const auto* memory_face = application.fonts().getFontById(loaded_font);
	FLOWUI_CHECK(memory_face);
	test_loaded_face(*memory_face);
	const std::array<std::byte, 4> corrupt_bytes{};
	auto corrupt_face = FlowUi::defaults::missing_font();
	corrupt_face.baked_data = corrupt_bytes;
	const auto rejected = application.fonts().addFamilyFace(*family, corrupt_face);
	FLOWUI_CHECK(!rejected && rejected.error().code == FlowUi::ErrorCode::AssetReadFailed);
	FLOWUI_CHECK(application.fonts().resolveFont(*family) == loaded_font);
	FLOWUI_CHECK(application.beginFrame());
	CLAY_TEXT(CLAY_STRING("A missing font"), CLAY_TEXT_CONFIG({
		.textColor = {255, 255, 255, 255}, .fontSize = 16,
	}));
	FLOWUI_CHECK(application.endFrame());
	FLOWUI_CHECK(application.drawFrame());
}

} // namespace

int main(int argument_count, char** arguments) {
	FlowUi::test::Runner runner{};
	const std::string_view mode = argument_count > 1 ? arguments[1] : "--startup";
	if (mode == "--payload") {
		runner.run("embedded font payload", test_payload);
	} else {
		const bool configured_memory = mode == "--configured-memory";
		const auto policy = mode == "--strict"
			? FlowUi::DefaultFontFailurePolicy::TryFallbackThenFailApp
			: mode == "--immediate" || configured_memory
				? FlowUi::DefaultFontFailurePolicy::FailAppImmediately
				: FlowUi::DefaultFontFailurePolicy::TryFallbackThenDisableText;
		runner.run("disk-independent font startup and memory registration", [policy, configured_memory] {
			test_startup(policy, configured_memory);
		});
	}
	return runner.finish();
}
