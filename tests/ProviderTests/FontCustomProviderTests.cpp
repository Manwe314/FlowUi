#include "DistinctFontMsdfProvider.hpp"
#include "TestHarness.hpp"
#include <array>

namespace {
void testIndependentProvider() {
	using namespace FlowUi;
	using namespace FlowUi::detail::font;
	test::DistinctFontMsdfProvider provider;
	const std::array source{std::byte{1}, std::byte{2}, std::byte{3}};
	const auto baked = test::requireValue(
		provider.bake(FontBakeRequest{.sourceBytes = source, .faceName = "distinct"}));
	FLOWUI_CHECK(baked.loadedGlyphCount == 95u);
	FLOWUI_CHECK(baked.asset.variants.front().glyphs.back().advanceX > 0.8f);
	auto bytes = test::requireValue(provider.encodeBaked(baked.asset));
	const auto decoded = test::requireValue(provider.loadBaked(bytes));
	FLOWUI_CHECK(decoded.atlas.pixels == baked.asset.atlas.pixels);
	FLOWUI_CHECK(decoded.variants.front().glyphs.size() == 95u);
	FLOWUI_CHECK(decoded.variants.front().name == "distinct");
	FLOWUI_CHECK(decoded.variants.front().kerningPairs.front().advance == -0.05f);
	FLOWUI_CHECK(!provider.loadBaked(source));
	bytes.pop_back();
	FLOWUI_CHECK(!provider.loadBaked(bytes));
	FLOWUI_CHECK(test::DistinctFontMsdfProvider::bakeCalls >= 1u);
	FLOWUI_CHECK(test::DistinctFontMsdfProvider::encodeCalls >= 1u);
	FLOWUI_CHECK(test::DistinctFontMsdfProvider::loadCalls >= 3u);
}
} // namespace
int main() {
	FlowUi::test::Runner runner;
	runner.run("independent font provider serializes owned data without vendor libraries",
			   testIndependentProvider);
	return runner.finish();
}
