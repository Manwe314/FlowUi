#include "FlowUi/Private/resourceService/iconRasterProvider/IconRasterCalculations.hpp"
#include "FlowUi/Private/resourceService/iconRasterProvider/providers/plutoSvg/PlutoSvgIconRasterProvider.hpp"
#include "FlowUi/Private/resourceService/iconRasterProvider/providers/plutoSvg/PlutoSvgPixelConversions.hpp"
#include "TestHarness.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using namespace FlowUi;
using namespace FlowUi::detail;
using namespace FlowUi::detail::icon;

static_assert(IconRasterProvider<PlutoSvgIconRasterProvider>);
static_assert(!std::is_default_constructible_v<PlutoSvgIconRasterProvider::Document>);
static_assert(straightChannel(255, 0) == 0);
static_assert(straightChannel(255, 1) == 255);
static_assert(rgbaFromArgb(0xff123456u) ==
			  std::array{std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0xff}});

[[nodiscard]] std::span<const std::byte> bytes(std::string_view source) noexcept {
	return std::as_bytes(std::span{source.data(), source.size()});
}

void testOwnedDocumentMoveAndExactRaster() {
	PlutoSvgIconRasterProvider provider;
	std::string source =
		R"(<svg xmlns="http://www.w3.org/2000/svg" width="12" height="6"><rect width="12" height="6" fill="#123456"/></svg>)";
	auto document = test::requireValue(provider.parse(bytes(source)));
	source.assign(source.size(), 'x');
	source.clear();
	source.shrink_to_fit();
	auto moved = std::move(document);
	const auto dimensions = provider.intrinsicSize(moved);
	FLOWUI_CHECK(dimensions.width == 12 && dimensions.height == 6);
	const auto extents = provider.contentSize(moved);
	FLOWUI_CHECK(extents.width == 12 && extents.height == 6);
	const auto raster = test::requireValue(provider.rasterize(moved, {24, 12}));
	FLOWUI_CHECK(raster.width == 24 && raster.height == 12);
	FLOWUI_CHECK(raster.pixels.size() == 24u * 12u * 4u);
	for (size_t pixelIndex = 0; pixelIndex < raster.pixels.size(); pixelIndex += 4) {
		FLOWUI_CHECK(raster.pixels[pixelIndex] == std::byte{0x12});
		FLOWUI_CHECK(raster.pixels[pixelIndex + 1] == std::byte{0x34});
		FLOWUI_CHECK(raster.pixels[pixelIndex + 2] == std::byte{0x56});
		FLOWUI_CHECK(raster.pixels[pixelIndex + 3] == std::byte{255});
	}
}

void testStraightAlphaAndTransparentPixels() {
	PlutoSvgIconRasterProvider provider;
	const auto document = test::requireValue(provider.parse(bytes(
		R"(<svg xmlns="http://www.w3.org/2000/svg" width="4" height="2"><rect width="2" height="2" fill="#ff8040" fill-opacity="0.5"/></svg>)")));
	const auto raster = test::requireValue(provider.rasterize(document, {4, 2}));
	const auto value = [&](size_t byteIndex) {
		return std::to_integer<unsigned>(raster.pixels[byteIndex]);
	};
	FLOWUI_CHECK(value(0) == 255);
	FLOWUI_CHECK(value(1) >= 126 && value(1) <= 129);
	FLOWUI_CHECK(value(2) >= 62 && value(2) <= 65);
	FLOWUI_CHECK(value(3) >= 127 && value(3) <= 128);
	for (size_t channelIndex = 8; channelIndex < 16; ++channelIndex)
		FLOWUI_CHECK(raster.pixels[channelIndex] == std::byte{0});
	// Straight-alpha samples retain color before the renderer multiplies by coverage.
	FLOWUI_CHECK(value(0) > value(3));
}

void testPaddedNativeRowsAndBounds() {
	std::array<std::byte, 16> nativePixels{};
	const uint32_t firstPixel = 0x80804020u;
	const uint32_t secondPixel = 0xff123456u;
	std::memcpy(nativePixels.data(), &firstPixel, sizeof(firstPixel));
	std::memcpy(nativePixels.data() + 8, &secondPixel, sizeof(secondPixel));
	nativePixels[4] = std::byte{0xee};
	nativePixels[12] = std::byte{0xdd};
	std::array<std::byte, 8> output{};
	FLOWUI_CHECK(copyNativeArgbRows(nativePixels, 8, 1, 2, output));
	FLOWUI_CHECK(
		(output == std::array{std::byte{255}, std::byte{127}, std::byte{63}, std::byte{128},
							  std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{255}}));
	FLOWUI_CHECK(!copyNativeArgbRows(nativePixels, 3, 1, 2, output));
	FLOWUI_CHECK(!copyNativeArgbRows(std::span{nativePixels}.first(9), 8, 1, 2, output));
	FLOWUI_CHECK(
		!copyNativeArgbRows(nativePixels, std::numeric_limits<size_t>::max(), 1, 2, output));
	FLOWUI_CHECK(!copyNativeArgbRows(nativePixels, 8, 1, 0, output));
}

void testCoreFitAndPadding() {
	const auto landscape = fitIconRasterExtent(31, 31, {12, 6}, {});
	FLOWUI_CHECK(landscape.valid && landscape.width == 31 && landscape.height == 16);
	const auto portrait = fitIconRasterExtent(31, 31, {6, 12}, {});
	FLOWUI_CHECK(portrait.valid && portrait.width == 16 && portrait.height == 31);
	const auto extents = fitIconRasterExtent(30, 20, {}, {12, 6});
	FLOWUI_CHECK(extents.valid && extents.width == 30 && extents.height == 15);
	const auto unknown = fitIconRasterExtent(30, 20, {}, {});
	FLOWUI_CHECK(unknown.valid && unknown.width == 30 && unknown.height == 20);
	const auto emptyRequest = fitIconRasterExtent(0, 0, {}, {});
	FLOWUI_CHECK(emptyRequest.valid && emptyRequest.width == 1 && emptyRequest.height == 1);
	FLOWUI_CHECK(
		!fitIconRasterExtent(30, 20, {std::numeric_limits<float>::infinity(), 2}, {}).valid);
	FLOWUI_CHECK(!fitIconRasterExtent(30, 20, {}, {-1, 2}).valid);
	FLOWUI_CHECK(!paddedIconExtent(UINT32_MAX, 2, 2).valid);
	FLOWUI_CHECK(!paddedIconExtent(2, 2, UINT32_MAX).valid);
}

void testParseAndDimensionFailures() {
	PlutoSvgIconRasterProvider provider;
	const auto empty = provider.parse({});
	FLOWUI_CHECK(!empty && empty.error().code == ErrorCode::IconSourceInvalid);
	FLOWUI_CHECK(empty.error().site == ErrorSite::IconParseSource);
	const auto corrupt = provider.parse(bytes("not an svg"));
	FLOWUI_CHECK(!corrupt && corrupt.error().code == ErrorCode::IconSourceInvalid);
	auto document = test::requireValue(provider.parse(bytes(
		R"(<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1"><rect width="1" height="1"/></svg>)")));
	const auto zero = provider.rasterize(document, {0, 1});
	FLOWUI_CHECK(!zero && zero.error().code == ErrorCode::IconRasterInvalid);
	const auto oversized = provider.rasterize(document, {std::numeric_limits<uint32_t>::max(), 1});
	FLOWUI_CHECK(!oversized && oversized.error().code == ErrorCode::IconRasterInvalid);
	auto moved = std::move(document);
	FLOWUI_CHECK(provider.intrinsicSize(document).width == 0);
	FLOWUI_CHECK(!provider.rasterize(document, {1, 1}));
	FLOWUI_CHECK(provider.rasterize(moved, {1, 1}));
}
} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("owned SVG source survives caller release and document move",
			   testOwnedDocumentMoveAndExactRaster);
	runner.run("native colored transparency produces straight-alpha RGBA",
			   testStraightAlphaAndTransparentPixels);
	runner.run("native padded strides and buffer bounds", testPaddedNativeRowsAndBounds);
	runner.run("core contain fit, fallback dimensions and checked padding", testCoreFitAndPadding);
	runner.run("SVG parse and dimension errors stay structured", testParseAndDimensionFailures);
	return runner.finish();
}
