#include "TestHarness.hpp"

#include <cmath>
#include <deque>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/textLayoutService/TextLayoutService.hpp"
#include "FlowUi/Private/textLayoutService/TextLayoutValidation.hpp"
#include "FlowUi/Private/textLayoutService/providers/basic/BasicTextLayoutProvider.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"

namespace {

static_assert(FlowUi::detail::text::textEmPixels({.pointsToPixelsScale = 2, .fontSize = 10}, 3) ==
			  20);
static_assert(FlowUi::detail::text::textEmScale(20, 10) == 2);
static_assert(FlowUi::detail::text::textAtlasCoordinate(10, .01f) == 10 * .01f);
static_assert(FlowUi::detail::text::textAtlasCoordinate(-10, .01f) == 0);
static_assert(FlowUi::detail::text::textAtlasCoordinate(200, .01f) == 1);

bool near(float a, float b) {
	return std::fabs(a - b) < 0.0001f;
}

struct FontFixture {
	std::deque<FlowUi::Font::FontFaceData> fonts{};
	FlowUi::detail::manager_storage::FontFrameView view{};

	FontFixture() {
		FlowUi::Font::FontVariantData variant{};
		variant.fontSizePx = 10.0f;
		variant.distanceRange = 4.0f;
		variant.emSize = 10.0f;
		variant.ascender = 8.0f;
		variant.descender = -2.0f;
		variant.lineHeight = 12.0f;
		variant.glyphs = {
			FlowUi::Font::GlyphData{
				.codepoint = '?',
				.planeRight = 5.0f,
				.planeTop = 8.0f,
				.imageRight = 5.0f,
				.imageTop = 8.0f,
				.advanceX = 6.0f,
			},
			FlowUi::Font::GlyphData{
				.codepoint = 'A',
				.planeRight = 5.0f,
				.planeTop = 8.0f,
				.imageLeft = 10.0f,
				.imageRight = 15.0f,
				.imageTop = 8.0f,
				.advanceX = 6.0f,
			},
			FlowUi::Font::GlyphData{
				.codepoint = 'V',
				.planeRight = 5.0f,
				.planeTop = 8.0f,
				.imageLeft = 20.0f,
				.imageRight = 25.0f,
				.imageTop = 8.0f,
				.advanceX = 7.0f,
			},
			FlowUi::Font::GlyphData{
				.codepoint = ' ',
				.advanceX = 3.0f,
			},
		};
		variant.unicodeToGlyphIndex = {{'?', 0}, {'A', 1}, {'V', 2}, {' ', 3}};
		variant.kerningPairs.emplace(
			FlowUi::Font::FontVariantData::kerningKey('A', 'V'),
			-1.0f);

		FlowUi::Font::FontFaceData face{};
		face.id = 0;
		face.atlasLayer = 3;
		face.atlasWidth = 100;
		face.atlasHeight = 100;
		face.sourceAtlasWidth = 50;
		face.sourceAtlasHeight = 50;
		face.variants.push_back(std::move(variant));
		fonts.push_back(std::move(face));
		view.fonts = &fonts;
		view.fontCount = fonts.size();
		view.publicationRevision = 7;
	}
};

void testCurrentLayoutCompatibilityRecords() {
	FontFixture fixture;
	FlowUi::detail::text::TextLayoutService service;
	const auto& layout = service.layout(FlowUi::detail::text::TextLayoutRequest{
		.text = "AV \t\nA",
		.fontView = &fixture.view,
		.fontId = 0,
		.pointsToPixelsScale = 1.0f,
		.fontSize = 10,
		.letterSpacing = 1,
		.includeGlyphGeometry = true,
	});
	FLOWUI_CHECK(layout.success);
	FLOWUI_CHECK(near(layout.measuredWidth, 11.0f));
	FLOWUI_CHECK(near(layout.measuredAdvance, 31.0f));
	FLOWUI_CHECK(near(layout.lineHeight, 12.0f));
	FLOWUI_CHECK(layout.atlasLayer == 3);
	FLOWUI_CHECK(near(layout.distanceRangePx, 4.0f));
	FLOWUI_CHECK(layout.glyphs.size() == 3);
	FLOWUI_CHECK(layout.clusters.size() == 6);
	FLOWUI_CHECK(layout.lines.size() == 2);
	FLOWUI_CHECK(layout.caretStops.size() == 7);
	FLOWUI_CHECK(layout.glyphs[1].clusterStartByte == 1);
	FLOWUI_CHECK(layout.glyphs[1].clusterEndByte == 2);
	FLOWUI_CHECK(near(layout.glyphs[0].x, 0.0f));
	FLOWUI_CHECK(near(layout.glyphs[1].x, 6.0f));
	FLOWUI_CHECK(near(layout.glyphs[0].y, 0.0f));
	FLOWUI_CHECK(near(layout.glyphs[0].width, 5.0f));
	FLOWUI_CHECK(near(layout.glyphs[0].height, 8.0f));
	FLOWUI_CHECK(near(layout.glyphs[0].u0, 0.1f));
	FLOWUI_CHECK(near(layout.glyphs[0].u1, 0.15f));
	FLOWUI_CHECK(near(layout.glyphs[0].v0, 0.42f));
	FLOWUI_CHECK(near(layout.glyphs[0].v1, 0.5f));
	FLOWUI_CHECK(layout.lines[0].endByte == 4);
	FLOWUI_CHECK(layout.lines[1].startByte == 5);
}

void testCacheUsesCompleteLayoutConfiguration() {
	FontFixture fixture;
	FlowUi::detail::text::TextLayoutService service;
	FlowUi::detail::text::TextLayoutRequest request{
		.text = "AV",
		.fontView = &fixture.view,
		.fontId = 0,
		.pointsToPixelsScale = 1.0f,
		.fontSize = 10,
	};
	const auto* first = &service.layout(request);
	const auto* repeated = &service.layout(request);
	FLOWUI_CHECK(first == repeated);
	FLOWUI_CHECK(service.cacheEntryCount() == 1);

	request.letterSpacing = 1;
	(void)service.layout(request);
	FLOWUI_CHECK(service.cacheEntryCount() == 2);
	request.tabWidth = 8;
	(void)service.layout(request);
	FLOWUI_CHECK(service.cacheEntryCount() == 3);
	fixture.view.publicationRevision = 8;
	(void)service.layout(request);
	FLOWUI_CHECK(service.cacheEntryCount() == 1);
	request.includeGlyphGeometry = true;
	(void)service.layout(request);
	FLOWUI_CHECK(service.cacheEntryCount() == 2);
	FLOWUI_CHECK(service.cacheBytes() <= FlowUi::detail::text::TextLayoutService::MaxCacheBytes);
}

void testWhitespaceMalformedUtf8AndLineCompatibility() {
	FontFixture fixture;
	FlowUi::detail::text::TextLayoutService service;
	const auto& face = fixture.fonts.front();
	auto empty = service.layout(FlowUi::detail::text::TextProviderInput{}, face);
	FLOWUI_CHECK(empty.success && empty.glyphs.empty() && empty.clusters.empty());
	FLOWUI_CHECK(empty.lines.size() == 1 && empty.caretStops.size() == 1);
	FLOWUI_CHECK(empty.lines[0].startByte == 0 && empty.lines[0].endByte == 0);
	const auto whitespace = service.layout({.text = " \t\xC2\xA0\n",
											.pointsToPixelsScale = 1.0f,
											.fontSize = 10,
											.includeGlyphGeometry = true},
										   face);
	FLOWUI_CHECK(whitespace.success && whitespace.glyphs.empty());
	FLOWUI_CHECK(near(whitespace.measuredWidth, 18.0f));
	FLOWUI_CHECK(near(whitespace.measuredAdvance, 18.0f));
	FLOWUI_CHECK(whitespace.lines.size() == 2 && whitespace.clusters.size() == 4);
	FLOWUI_CHECK(whitespace.lines[1].startByte == 5 && whitespace.lines[1].endByte == 5);
	FLOWUI_CHECK(whitespace.clusters[2].startByte == 2 && whitespace.clusters[2].endByte == 4);
	const auto malformed = service.layout({.text = "A\xFF\xC2",
										   .pointsToPixelsScale = 1.0f,
										   .fontSize = 10,
										   .includeGlyphGeometry = true},
										  face);
	FLOWUI_CHECK(malformed.success && malformed.glyphs.size() == 3);
	FLOWUI_CHECK(malformed.glyphs[1].codepoint == 0xfffd &&
				 malformed.glyphs[2].codepoint == 0xfffd);
	FLOWUI_CHECK(malformed.glyphs[1].glyphIndex == 0 && malformed.glyphs[2].glyphIndex == 0);
	FLOWUI_CHECK(malformed.clusters[2].startByte == 2 && malformed.clusters[2].endByte == 3);
	const auto decodedOverlong = service.layout({.text = "\xC1\x81",
												 .pointsToPixelsScale = 1.0f,
												 .fontSize = 10,
												 .includeGlyphGeometry = true},
												face);
	FLOWUI_CHECK(decodedOverlong.success && decodedOverlong.clusters.size() == 1);
	FLOWUI_CHECK(decodedOverlong.glyphs[0].codepoint == 'A');
	const auto overlongNewline = service.layout({.text = "A\xC0\x8A" "V", .pointsToPixelsScale = 1.0f,
		.fontSize = 10, .includeGlyphGeometry = true}, face);
	FLOWUI_CHECK(overlongNewline.success && overlongNewline.lines.size() == 2);
	FLOWUI_CHECK(overlongNewline.lines[0].endByte == 1 && overlongNewline.lines[1].startByte == 3);
	FLOWUI_CHECK(overlongNewline.clusters[1].startByte == 1 && overlongNewline.clusters[1].endByte == 3);
}

void testScalingNegativeBearingsAndGeometryAgreement() {
	FontFixture fixture;
	fixture.fonts.front().variants[0].glyphs[1].planeLeft = -2.0f;
	FlowUi::detail::text::TextLayoutService service;
	const auto& face = fixture.fonts.front();
	auto geometry = service.layout({.text = "AV\nA",
									.pointsToPixelsScale = 2.0f,
									.fontSize = 10,
									.letterSpacing = 1,
									.includeGlyphGeometry = true},
								   face);
	const auto measured = service.layout(
		{.text = "AV\nA", .pointsToPixelsScale = 2.0f, .fontSize = 10, .letterSpacing = 1}, face);
	FLOWUI_CHECK(geometry.success && measured.success && measured.glyphs.empty());
	FLOWUI_CHECK(near(geometry.glyphs[0].x, -4.0f));
	FLOWUI_CHECK(near(geometry.glyphs[0].width, 14.0f));
	FLOWUI_CHECK(near(geometry.glyphs[2].y, geometry.glyphs[0].y));
	FLOWUI_CHECK(near(geometry.measuredWidth, measured.measuredWidth));
	FLOWUI_CHECK(near(geometry.measuredAdvance, measured.measuredAdvance));
	FLOWUI_CHECK(near(geometry.lineHeight, measured.lineHeight));
	FLOWUI_CHECK(geometry.clusters.size() == measured.clusters.size());
	for (size_t clusterIndex = 0; clusterIndex < geometry.clusters.size(); ++clusterIndex) {
		FLOWUI_CHECK(
			near(geometry.clusters[clusterIndex].xStart, measured.clusters[clusterIndex].xStart));
		FLOWUI_CHECK(
			near(geometry.clusters[clusterIndex].xEnd, measured.clusters[clusterIndex].xEnd));
		FLOWUI_CHECK(measured.clusters[clusterIndex].firstGlyph == 0 &&
					 measured.clusters[clusterIndex].glyphCount == 0);
	}
	FLOWUI_CHECK(near(geometry.emPixels, 20.0f) && near(geometry.lineHeight, 24.0f));
}

void testCacheEvictionUncachedAndFailureLifetime() {
	FontFixture fixture;
	FlowUi::detail::text::TextLayoutService service;
	FlowUi::detail::text::TextLayoutRequest request{.fontView = &fixture.view};
	for (size_t entryIndex = 0; entryIndex < 300; ++entryIndex) {
		const std::string text = "A" + std::to_string(entryIndex);
		request.text = text;
		FLOWUI_CHECK(service.layout(request).success);
	}
	FLOWUI_CHECK(service.cacheEntryCount() == service.MaxCacheEntries);
	FLOWUI_CHECK(service.cacheBytes() <= service.MaxCacheBytes);
	service.clear();
	FLOWUI_CHECK(service.cacheEntryCount() == 0 && service.cacheBytes() == 0);
	std::string oversized(service.MaxCachedTextBytes + 1, 'A');
	request.text = oversized;
	FLOWUI_CHECK(service.layout(request).success);
	FLOWUI_CHECK(service.cacheEntryCount() == 0);
	const std::string byteExpensive(40000, 'A');
	request.text = byteExpensive;
	request.includeGlyphGeometry = true;
	FLOWUI_CHECK(service.layout(request).success);
	FLOWUI_CHECK(service.cacheEntryCount() == 0);
	request.text = "AV";
	request.fontSize = 10;
	request.pointsToPixelsScale = std::numeric_limits<float>::quiet_NaN();
	const auto& invalidScale = service.layout(request);
	FLOWUI_CHECK(!invalidScale.success && invalidScale.glyphs.empty() &&
				 invalidScale.clusters.empty());
	FLOWUI_CHECK(service.cacheEntryCount() == 0);
	request.pointsToPixelsScale = 1;
	request.fontView = nullptr;
	FLOWUI_CHECK(!service.layout(request).success);
	FLOWUI_CHECK(service.cacheEntryCount() == 0);
}

void testCoreFinalizationRejectsInvalidProviderRecords() {
	FontFixture fixture;
	FlowUi::detail::text::BasicTextLayoutProvider provider;
	const auto& face = fixture.fonts.front();
	const FlowUi::detail::text::TextProviderInput input{.text = "AV", .includeGlyphGeometry = true};
	auto valid = provider.layout(input, face);
	FLOWUI_CHECK(valid && valid->success);
	valid->atlasLayer = 987;
	valid->distanceRangePx = 123;
	valid->glyphs[0].codepoint = 'V';
	valid->glyphs[0].u0 = std::numeric_limits<float>::quiet_NaN();
	const auto finalized = FlowUi::detail::text::finalizeTextLayout(input, face, *valid);
	FLOWUI_CHECK(finalized);
	FLOWUI_CHECK(valid->atlasLayer == face.atlasLayer && near(valid->distanceRangePx, 4));
	FLOWUI_CHECK(near(valid->glyphs[0].u0, .1f));
	for (unsigned defect = 0; defect < 6; ++defect) {
		auto corrupt = provider.layout(input, face);
		FLOWUI_CHECK(corrupt);
		switch (defect) {
		case 0:
			corrupt->glyphs[0].glyphIndex = 999;
			break;
		case 1:
			corrupt->glyphs[0].width = std::numeric_limits<float>::infinity();
			break;
		case 2:
			corrupt->clusters[0].glyphCount = 999;
			break;
		case 3:
			corrupt->caretStops[1].x = -1;
			break;
		case 4:
			corrupt->lines[0].endByte = 999;
			break;
		case 5:
			corrupt->success = false;
			break;
		}
		const auto rejected = FlowUi::detail::text::finalizeTextLayout(input, face, *corrupt);
		FLOWUI_CHECK(!rejected);
		FLOWUI_CHECK(rejected.error().code == FlowUi::ErrorCode::AssetPayloadInvalid);
		FLOWUI_CHECK(rejected.error().site == FlowUi::ErrorSite::TextLayoutCompute);
	}
}

void testConsumersOnlyUseTextLayoutService() {
	for (const char* path : {
		FLOWUI_UI_MANAGER_SOURCE,
		FLOWUI_INPUT_FIELD_MANAGER_SOURCE,
		FLOWUI_UI_RENDERER_SOURCE,
	}) {
		std::ifstream source(path, std::ios::binary);
		FLOWUI_CHECK(source.good());
		const std::string contents(
			(std::istreambuf_iterator<char>(source)),
			std::istreambuf_iterator<char>());
		FLOWUI_CHECK(contents.find("TextLayoutService") != std::string::npos);
		FLOWUI_CHECK(contents.find("TextLayoutEngine") == std::string::npos);
		FLOWUI_CHECK(contents.find("LayoutTextLine") == std::string::npos);
		FLOWUI_CHECK(contents.find("CurrentTextShaper") == std::string::npos);
	}
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("current LTR layout produces backend-neutral records", testCurrentLayoutCompatibilityRecords);
	runner.run("layout cache keys include font generation and configuration", testCacheUsesCompleteLayoutConfiguration);
	runner.run("whitespace, malformed UTF8 and hard-line compatibility",
			   testWhitespaceMalformedUtf8AndLineCompatibility);
	runner.run("scaling, negative bearings and geometry-disabled agreement",
			   testScalingNegativeBearingsAndGeometryAgreement);
	runner.run("cache eviction, oversized results and failure lifetime",
			   testCacheEvictionUncachedAndFailureLifetime);
	runner.run("core validates provider ranges and owns atlas finalization",
			   testCoreFinalizationRejectsInvalidProviderRecords);
	runner.run("text consumers depend only on TextLayoutService", testConsumersOnlyUseTextLayoutService);
	return runner.finish();
}
