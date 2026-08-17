#include "TestHarness.hpp"

#include <cmath>
#include <deque>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include "internal/ManagerStorage/FontCatalogController.hpp"
#include "internal/Text/TextLayoutService.hpp"

namespace {

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
	runner.run("text consumers depend only on TextLayoutService", testConsumersOnlyUseTextLayoutService);
	return runner.finish();
}
