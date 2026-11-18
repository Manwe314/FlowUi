#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuAssetValidation.hpp"
#include "TestHarness.hpp"
#include <limits>

namespace {
using namespace FlowUi;
using namespace FlowUi::detail::font;
[[nodiscard]] FontCpuAsset validAsset() {
	FontCpuAsset asset;
	asset.atlas.width = 2u;
	asset.atlas.height = 2u;
	asset.atlas.pixels.resize(16u, std::byte{127});
	FontCpuVariant variant;
	variant.emSize = 1.0f;
	variant.fontSizePx = 18.0f;
	variant.distanceRange = 6.0f;
	variant.lineHeight = 1.2f;
	variant.glyphs.emplace_back(Font::GlyphData{.codepoint = 'A',
												.planeLeft = -0.1f,
												.planeBottom = -0.2f,
												.planeRight = 0.9f,
												.planeTop = 1.0f,
												.imageRight = 2.0f,
												.imageTop = 2.0f,
												.advanceX = 1.0f});
	variant.glyphs.emplace_back(Font::GlyphData{.codepoint = ' ', .advanceX = 0.3f});
	asset.variants.emplace_back(std::move(variant));
	return asset;
}
void testNormalizedContract() {
	auto asset = validAsset();
	FLOWUI_CHECK(validateFontCpuAsset(asset));
	asset.variants.front().fallbackGlyphIndex = 999u;
	FLOWUI_CHECK(validateFontCpuAsset(asset)); // Core compatibility repair owns this field.
	asset.atlas.pixels.pop_back();
	const auto invalidSize = validateFontCpuAsset(asset);
	FLOWUI_CHECK(!invalidSize && invalidSize.error().code == ErrorCode::AssetPayloadInvalid);
	asset = validAsset();
	asset.variants.front().glyphs.front().codepoint = 0xd800u;
	FLOWUI_CHECK(!validateFontCpuAsset(asset));
	asset = validAsset();
	asset.variants.front().glyphs.front().imageRight = 3.0f;
	FLOWUI_CHECK(!validateFontCpuAsset(asset));
	asset = validAsset();
	asset.variants.front().glyphs.front().sourceImageIndex = 1u;
	FLOWUI_CHECK(!validateFontCpuAsset(asset));
	asset = validAsset();
	asset.variants.front().ascender = std::numeric_limits<float>::quiet_NaN();
	FLOWUI_CHECK(!validateFontCpuAsset(asset));
	asset = validAsset();
	asset.fieldKind = static_cast<FontDistanceFieldKind>(255);
	FLOWUI_CHECK(!validateFontCpuAsset(asset));
	static_assert(legacyImageType(FontDistanceFieldKind::Msdf) == 6u);
	static_assert(legacyImageType(FontDistanceFieldKind::Mtsdf) == 7u);
}
} // namespace
int main() {
	FlowUi::test::Runner runner;
	runner.run("normalized CPU font contract rejects invalid output before publication",
			   testNormalizedContract);
	return runner.finish();
}
