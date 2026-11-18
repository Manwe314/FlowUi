#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuAssetValidation.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/providers/msdfAtlas/MsdfAtlasFontProvider.hpp"
#include "TestHarness.hpp"
#include <artery-font/std-artery-font.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
using namespace FlowUi;
using namespace FlowUi::detail::font;
using ArteryFont = artery_font::StdArteryFont<float>;
constexpr unsigned char pngPixels[] = {
	137, 80,  78, 71,  13,	10, 26,	 10, 0,	  0,   0,	13, 73,	 72,  68, 82, 0,  0,  0,
	2,	 0,	  0,  0,   1,	8,	6,	 0,	 0,	  0,   244, 34, 127, 138, 0,  0,  0,  17, 73,
	68,	 65,  84, 120, 156, 99, 248, 95, 111, 223, 32,	24, 125, 154, 1,  0,  18, 95, 3,
	117, 118, 51, 130, 17,	0,	0,	 0,	 0,	  73,  69,	78, 68,	 174, 66, 96, 130};
struct Writer {
	std::vector<std::byte> bytes;
	bool failed = false;
};
int writeBytes(const void* source, int length, void* context) noexcept {
	auto& writer = *static_cast<Writer*>(context);
	if (length < 0 || writer.failed)
		return 0;
	try {
		const auto* begin = static_cast<const std::byte*>(source);
		writer.bytes.insert(writer.bytes.end(), begin, begin + length);
		return length;
	} catch (...) {
		writer.failed = true;
		return 0;
	}
}
[[nodiscard]] ArteryFont fixture() {
	ArteryFont font{};
	font.variants = artery_font::StdList<ArteryFont::Variant>(1);
	auto& variant = font.variants[0];
	variant = ArteryFont::Variant{};
	variant.codepointType = artery_font::CP_UNICODE;
	variant.imageType = artery_font::IMAGE_MTSDF;
	variant.flags = 0x123u;
	variant.weight = 450;
	variant.fallbackGlyph = 99;
	variant.metrics.fontSize = 18.0f;
	variant.metrics.distanceRange = 6.0f;
	variant.metrics.distanceRangeMiddle = 0.25f;
	variant.metrics.emSize = 1.0f;
	variant.metrics.ascender = 0.8f;
	variant.metrics.descender = -0.2f;
	variant.metrics.lineHeight = 1.2f;
	variant.name = artery_font::StdString("fixture", 7);
	variant.glyphs = artery_font::StdList<artery_font::Glyph<float>>(1);
	variant.glyphs[0] = {65, 0, {-0.1f, -0.2f, 0.9f, 1.0f}, {0.0f, 0.0f, 2.0f, 1.0f}, {1.1f, 0.0f}};
	variant.kernPairs = artery_font::StdList<artery_font::KernPair<float>>(1);
	variant.kernPairs[0] = {65, 86, {-0.1f, 0.0f}};
	font.images = artery_font::StdList<ArteryFont::Image>(1);
	auto& image = font.images[0];
	image = ArteryFont::Image{};
	image.width = 2u;
	image.height = 1u;
	image.channels = 4u;
	image.pixelFormat = artery_font::PIXEL_UNSIGNED8;
	image.imageType = artery_font::IMAGE_MTSDF;
	image.encoding = artery_font::IMAGE_PNG;
	image.data = artery_font::StdByteArray(sizeof(pngPixels));
	std::memcpy(static_cast<unsigned char*>(image.data), pngPixels, sizeof(pngPixels));
	return font;
}
[[nodiscard]] std::vector<std::byte> encode(const ArteryFont& font) {
	Writer writer;
	FLOWUI_CHECK(artery_font::encode<writeBytes>(font, &writer));
	FLOWUI_CHECK(!writer.failed);
	return writer.bytes;
}
void testPngAndRoundTrip() {
	MsdfAtlasFontProvider provider;
	auto decoded = test::requireValue(provider.loadBaked(encode(fixture())));
	const std::vector<std::byte> expected{std::byte{255}, std::byte{127}, std::byte{63},
										  std::byte{128}, std::byte{17},  std::byte{91},
										  std::byte{203}, std::byte{0}};
	FLOWUI_CHECK(decoded.atlas.pixels == expected);
	const auto& variant = decoded.variants.front();
	FLOWUI_CHECK(variant.flags == 0x123 && variant.weight == 450 &&
				 variant.fallbackGlyphIndex == 99);
	FLOWUI_CHECK(variant.name == "fixture" && variant.distanceRangeMiddle == 0.25f);
	FLOWUI_CHECK(variant.glyphs.front().codepoint == 65 &&
				 variant.glyphs.front().sourceImageIndex == 0);
	FLOWUI_CHECK(variant.glyphs.front().planeLeft == -0.1f &&
				 variant.kerningPairs.front().advance == -0.1f);
	auto reloaded =
		test::requireValue(provider.loadBaked(test::requireValue(provider.encodeBaked(decoded))));
	FLOWUI_CHECK(reloaded.atlas.pixels == decoded.atlas.pixels);
	FLOWUI_CHECK(reloaded.variants.front().glyphs.front().advanceX ==
				 variant.glyphs.front().advanceX);
	FLOWUI_CHECK(reloaded.variants.front().fallbackGlyphIndex == 99);
}
void testRawPaddingOriginAndMsdf() {
	MsdfAtlasFontProvider provider;
	auto font = fixture();
	auto& image = font.images[0];
	image.height = 2u;
	image.channels = 3u;
	image.imageType = font.variants[0].imageType = artery_font::IMAGE_MSDF;
	image.encoding = artery_font::IMAGE_RAW_BINARY;
	image.rawBinaryFormat.rowLength = 8u;
	image.rawBinaryFormat.orientation = artery_font::ORIENTATION_BOTTOM_UP;
	const unsigned char rows[]{1, 2, 3, 4, 5, 6, 88, 88, 7, 8, 9, 10, 11, 12, 99, 99};
	image.data = artery_font::StdByteArray(sizeof(rows));
	std::memcpy(static_cast<unsigned char*>(image.data), rows, sizeof(rows));
	auto decoded = test::requireValue(provider.loadBaked(encode(font)));
	const std::vector<std::byte> expected{
		std::byte{7},  std::byte{8},   std::byte{9}, std::byte{255}, std::byte{10}, std::byte{11},
		std::byte{12}, std::byte{255}, std::byte{1}, std::byte{2},	 std::byte{3},	std::byte{255},
		std::byte{4},  std::byte{5},   std::byte{6}, std::byte{255}};
	FLOWUI_CHECK(decoded.atlas.pixels == expected);
	FLOWUI_CHECK(decoded.fieldKind == FontDistanceFieldKind::Msdf);
}
void testInvalidFormatsAndPayloads() {
	MsdfAtlasFontProvider provider;
	FLOWUI_CHECK(!provider.loadBaked({}));
	auto bytes = encode(fixture());
	bytes.resize(bytes.size() / 2u);
	FLOWUI_CHECK(!provider.loadBaked(bytes));
	auto font = fixture();
	font.images[0].imageType = font.variants[0].imageType = artery_font::IMAGE_SDF;
	const auto unsupported = provider.loadBaked(encode(font));
	FLOWUI_CHECK(!unsupported && unsupported.error().code == ErrorCode::AssetFormatUnsupported);
	font = fixture();
	font.variants[0].codepointType = artery_font::CP_INDEXED;
	FLOWUI_CHECK(!provider.loadBaked(encode(font)));
	font = fixture();
	font.variants[0].glyphs[0].image = 1u;
	FLOWUI_CHECK(!provider.loadBaked(encode(font)));
	font = fixture();
	font.images[0].encoding = artery_font::IMAGE_RAW_BINARY;
	font.images[0].channels = 1u;
	FLOWUI_CHECK(!provider.loadBaked(encode(font)));
}
void testExistingBakedAsset() {
	std::ifstream input(std::filesystem::path(FLOWUI_PROVIDER_ASSET_DIRECTORY) / "Inter.arfont",
						std::ios::binary);
	std::vector<char> raw{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
	FLOWUI_CHECK(!raw.empty());
	MsdfAtlasFontProvider provider;
	auto decoded = test::requireValue(provider.loadBaked(std::as_bytes(std::span(raw))));
	FLOWUI_CHECK(decoded.variants.front().glyphs.size() > 90u);
	auto reloaded =
		test::requireValue(provider.loadBaked(test::requireValue(provider.encodeBaked(decoded))));
	FLOWUI_CHECK(reloaded.atlas.pixels == decoded.atlas.pixels);
	FLOWUI_CHECK(reloaded.variants.front().glyphs.size() == decoded.variants.front().glyphs.size());
}
#if FLOWUI_CONFIG_DEFAULT_FONT_BAKING
void testRuntimeAndOfflineProfiles() {
	std::ifstream input(std::filesystem::path(FLOWUI_PROVIDER_ASSET_DIRECTORY) /
							"Inter-VariableFont_opsz,wght.ttf",
						std::ios::binary);
	const std::vector<char> bytes{std::istreambuf_iterator<char>(input),
								  std::istreambuf_iterator<char>()};
	MsdfAtlasFontProvider provider;
	FontBakeRequest request{.sourceBytes = std::as_bytes(std::span(bytes)),
							.faceName = "runtime",
							.pageWidth = 512u,
							.pageHeight = 512u};
	const auto runtime = test::requireValue(provider.bake(request));
	FLOWUI_CHECK(runtime.requestedCodepointCount == 95u && runtime.loadedGlyphCount > 90u);
	FLOWUI_CHECK(runtime.asset.atlas.width == 512u && runtime.asset.atlas.height == 512u);
	FLOWUI_CHECK(runtime.asset.variants.front().fontSizePx == 18.0f);
	request.pageWidth = request.pageHeight = 0u;
	request.pixelSize = 64.0;
	request.pixelRange = 8.0;
	request.charsetMode = FontCharsetMode::OfflineUi;
	const auto offline = test::requireValue(provider.bake(request));
	FLOWUI_CHECK(offline.requestedCodepointCount > runtime.requestedCodepointCount);
	FLOWUI_CHECK(offline.asset.atlas.width % 4u == 0u &&
				 offline.asset.atlas.width == offline.asset.atlas.height);
	FLOWUI_CHECK(offline.asset.variants.front().distanceRange == 8.0f);
	FLOWUI_CHECK(offline.asset.variants.front().fallbackGlyphIndex == 0u);
	FLOWUI_CHECK(offline.asset.metadata.empty() && offline.asset.variants.front().metadata.empty());
	auto reloaded = test::requireValue(
		provider.loadBaked(test::requireValue(provider.encodeBaked(offline.asset))));
	FLOWUI_CHECK(reloaded.atlas.pixels == offline.asset.atlas.pixels);
	request.pageWidth = 16;
	request.pageHeight = 0;
	FLOWUI_CHECK(!provider.bake(request));
	request.pageWidth = request.pageHeight = 16;
	FLOWUI_CHECK(!provider.bake(request));
	request.pageWidth = request.pageHeight = 0;
	request.charsetMode = FontCharsetMode::FileSpecification;
	request.charsetSpecification = "[0x41,0x43]";
	FLOWUI_CHECK(provider.bake(request));
	request.charsetSpecification = "@include \"unresolved.txt\"";
	FLOWUI_CHECK(!provider.bake(request));
	request.charsetSpecification = "not a valid charset [";
	FLOWUI_CHECK(!provider.bake(request));
	request.charsetMode = FontCharsetMode::RuntimeAscii;
	request.pixelSize = std::numeric_limits<double>::max();
	FLOWUI_CHECK(!provider.bake(request));
	request.sourceBytes = {};
	FLOWUI_CHECK(!provider.bake(request));
}
#else
void testUnavailableBaking() {
	MsdfAtlasFontProvider provider;
	const auto unavailable = provider.bake({});
	FLOWUI_CHECK(!unavailable && unavailable.error().code == ErrorCode::FontBakeUnavailable);
}
#endif
} // namespace
int main() {
	FlowUi::test::Runner runner;
	runner.run("PNG distance samples and Artery round-trip preserve channels and metrics",
			   testPngAndRoundTrip);
	runner.run("raw atlas rows preserve padding, origin and MSDF opaque alpha",
			   testRawPaddingOriginAndMsdf);
	runner.run("font codec rejects corrupt, indexed and incompatible distance formats",
			   testInvalidFormatsAndPayloads);
	runner.run("existing Inter asset CPU decoding and raw round-trip", testExistingBakedAsset);
#if FLOWUI_CONFIG_DEFAULT_FONT_BAKING
	runner.run("runtime and offline font bake profiles execute real MSDF generation",
			   testRuntimeAndOfflineProfiles);
#else
	runner.run("disabled CPU baking returns the supported absence error", testUnavailableBaking);
#endif
	return runner.finish();
}
