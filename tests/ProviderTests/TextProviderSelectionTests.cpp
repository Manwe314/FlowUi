#include "DeterministicTextLayoutProvider.hpp"
#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/textLayoutService/TextLayoutService.hpp"
#include "TestHarness.hpp"

#include <cmath>
#include <deque>

namespace {
using namespace FlowUi;
namespace text = detail::text;
using Provider = test::DeterministicTextLayoutProvider;
static_assert(std::derived_from<text::ActiveTextLayoutProvider, Provider>);

struct Fixture {
	std::deque<Font::FontFaceData> fonts;
	detail::manager_storage::FontFrameView view;
	text::TextLayoutService service;
	Fixture() {
		Font::FontVariantData variant;
		variant.fontSizePx = 10;
		variant.emSize = 10;
		variant.ascender = 8;
		variant.lineHeight = 12;
		variant.distanceRange = 4;
		variant.glyphs.emplace_back(Font::GlyphData{.codepoint = 'A',
													.planeRight = 5,
													.planeTop = 8,
													.imageLeft = 10,
													.imageRight = 15,
													.imageTop = 8,
													.advanceX = 6});
		variant.unicodeToGlyphIndex.emplace('A', 0);
		Font::FontFaceData face;
		face.atlasLayer = 3;
		face.atlasWidth = face.atlasHeight = 100;
		face.sourceAtlasWidth = face.sourceAtlasHeight = 50;
		face.variants.emplace_back(std::move(variant));
		fonts.emplace_back(std::move(face));
		view.fonts = &fonts;
		view.fontCount = 1;
		view.publicationRevision = 1;
		Provider::mode = Provider::Mode::Valid;
		Provider::calls = 0;
	}
	[[nodiscard]] text::TextLayoutRequest request() const noexcept {
		return {.text = "AA", .fontView = &view, .includeGlyphGeometry = true};
	}
};

void testCustomProviderDrivesFacadeCacheAndCoreFinalization() {
	Fixture fixture;
	auto request = fixture.request();
	const auto first = fixture.service.layout(request);
	FLOWUI_CHECK(first.success && Provider::calls == 1);
	FLOWUI_CHECK(first.glyphs.size() == 2 && std::fabs(first.measuredAdvance - 18) < .001f);
	FLOWUI_CHECK(std::fabs(first.glyphs[0].x - 2) < .001f);
	FLOWUI_CHECK(std::fabs(first.glyphs[1].x - 11) < .001f);
	FLOWUI_CHECK(first.glyphs[0].codepoint == 0);
	FLOWUI_CHECK(std::fabs(first.glyphs[0].u0 - .1f) < .001f);
	FLOWUI_CHECK(first.atlasLayer == 3 && first.distanceRangePx == 4);
	FLOWUI_CHECK(fixture.service.layout(request).success && Provider::calls == 1);
	request.text = "AAA";
	FLOWUI_CHECK(fixture.service.layout(request).success && Provider::calls == 2);
	++fixture.view.publicationRevision;
	FLOWUI_CHECK(fixture.service.layout(request).success && Provider::calls == 3);
	FLOWUI_CHECK(fixture.service.cacheEntryCount() == 1);
	fixture.service.clear();
	FLOWUI_CHECK(fixture.service.layout(request).success && Provider::calls == 4);
}

void testCustomFailuresRejectBeforeCachingAndClearUncachedData() {
	Fixture fixture;
	const auto request = fixture.request();
	for (const auto mode : {Provider::Mode::InvalidGlyph, Provider::Mode::InvalidRange,
							Provider::Mode::Nonfinite, Provider::Mode::FailureAllocation,
							Provider::Mode::FailureForeign, Provider::Mode::FailureKnown}) {
		fixture.service.clear();
		Provider::mode = mode;
		bool caught = false;
		try {
			(void)fixture.service.layout(request);
		} catch (const FlowUiException& error) {
			caught = true;
			const ErrorCode expected =
				mode == Provider::Mode::FailureAllocation ? ErrorCode::AllocationFailed
				: mode == Provider::Mode::FailureForeign  ? ErrorCode::ForeignExceptionObserved
														  : ErrorCode::AssetPayloadInvalid;
			FLOWUI_CHECK(error.error().code == expected);
			FLOWUI_CHECK(error.error().site == (mode == Provider::Mode::FailureKnown
													? ErrorSite::FontParse
													: ErrorSite::TextLayoutCompute));
			if (mode == Provider::Mode::FailureKnown)
				FLOWUI_CHECK(error.error().subject == 321);
		}
		FLOWUI_CHECK(caught && fixture.service.cacheEntryCount() == 0);
		Provider::mode = Provider::Mode::Valid;
		FLOWUI_CHECK(fixture.service.layout(request).success);
	}
	fixture.service.clear();
	Provider::mode = Provider::Mode::FailureAllocation;
	FLOWUI_CHECK_THROWS(fixture.service.layout(request));
	Provider::mode = Provider::Mode::Valid;
	auto missing = request;
	missing.fontView = nullptr;
	const auto& result = fixture.service.layout(missing);
	FLOWUI_CHECK(!result.success && result.glyphs.empty() && result.caretStops.empty());
}
} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("selected provider drives cache, positions and core atlas finalization",
			   testCustomProviderDrivesFacadeCacheAndCoreFinalization);
	runner.run("provider violations and known failures cannot publish cached data",
			   testCustomFailuresRejectBeforeCachingAndClearUncachedData);
	return runner.finish();
}
