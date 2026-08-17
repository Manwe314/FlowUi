#include <algorithm>
#include <concepts>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

#include "TestHarness.hpp"
#include "internal/ManagerStorage/InputFieldManagerState.hpp"
#include "internal/Text/TextLineBreaker.hpp"
#include "internal/Text/TextStorage.hpp"
#include "managers/InputFieldManager.hpp"

namespace {

using namespace FlowUi;
namespace text = FlowUi::detail::text;

std::string readFile(const char* path) {
	std::ifstream input(path, std::ios::binary);
	std::ostringstream stream;
	stream << input.rdbuf();
	return stream.str();
}

void testStorageStrategiesAndViews() {
	text::FieldStorage single = text::makeFieldStorage(TextFieldMode::SingleLine, "small");
	FLOWUI_CHECK(text::storageMode(single) == TextFieldMode::SingleLine);
	FLOWUI_CHECK(text::contiguous(single).value_or("") == "small");
	FLOWUI_CHECK(text::lineCount(single) == 1u);

	std::string document;
	for (size_t i = 0; i < 20000; ++i) document += "line value\n";
	text::FieldStorage multiline = text::makeFieldStorage(TextFieldMode::MultiLine, document);
	FLOWUI_CHECK(text::storageMode(multiline) == TextFieldMode::MultiLine);
	FLOWUI_CHECK(!text::contiguous(multiline).has_value());
	FLOWUI_CHECK(std::get<text::MultiLineStorage>(multiline).chunks.size() > 1u);
	FLOWUI_CHECK(text::lineCount(multiline) == 20001u);
	FLOWUI_CHECK(text::lineRange(multiline, 10000u).endByte -
		text::lineRange(multiline, 10000u).startByte == 10u);

	const TextRange visibleRange = text::lineRange(multiline, 10000u);
	size_t visitedBytes = 0;
	text::forEachChunk(multiline, visibleRange, [&](std::string_view chunk) {
		visitedBytes += chunk.size();
	});
	FLOWUI_CHECK(visitedBytes == 10u);
	FLOWUI_CHECK(text::copy(multiline, visibleRange) == "line value");

	const size_t lineCountBefore = text::lineCount(multiline);
	text::replace(multiline, visibleRange, "first\nsecond");
	FLOWUI_CHECK(!text::contiguous(multiline).has_value());
	FLOWUI_CHECK(text::lineCount(multiline) == lineCountBefore + 1u);
	FLOWUI_CHECK(text::copy(multiline, text::lineRange(multiline, 10000u)) == "first");
	FLOWUI_CHECK(text::copy(multiline, text::lineRange(multiline, 10001u)) == "second");
	FLOWUI_CHECK(!text::migrate(multiline, TextFieldMode::SingleLine));
}

void testUtf8AndCrossChunkEdits() {
	std::string document(text::MultiLineStorage::TargetChunkBytes - 1u, 'a');
	document += "é\nlast";
	text::FieldStorage storage = text::makeFieldStorage(TextFieldMode::MultiLine, document);
	const size_t codepoint = text::MultiLineStorage::TargetChunkBytes - 1u;
	FLOWUI_CHECK(text::nextUtf8Codepoint(storage, codepoint) == codepoint + 2u);
	FLOWUI_CHECK(text::previousUtf8Codepoint(storage, codepoint + 2u) == codepoint);
	FLOWUI_CHECK(text::clampUtf8Boundary(storage, codepoint + 1u) == codepoint);

	text::replace(storage, TextRange{codepoint, codepoint + 3u}, "x\ny\n");
	FLOWUI_CHECK(text::lineCount(storage) == 3u);
	FLOWUI_CHECK(text::copy(storage, text::lineRange(storage, 1u)) == "y");
	FLOWUI_CHECK(text::copy(storage, text::lineRange(storage, 2u)) == "last");
}

void testClusterLineBreaking() {
	text::TextLayoutResult layout{};
	layout.success = true;
	for (size_t i = 0; i < 6; ++i) {
		layout.clusters.push_back(text::TextLayoutCluster{
			.startByte = i,
			.endByte = i + 1u,
			.xStart = static_cast<float>(i) * 10.0f,
			.xEnd = static_cast<float>(i + 1u) * 10.0f,
		});
	}
	const std::vector<TextRange> lines = text::breakVisualLines("ab cd!", layout, 31.0f);
	FLOWUI_CHECK(lines.size() == 2u);
	FLOWUI_CHECK(lines[0].startByte == 0u);
	FLOWUI_CHECK(lines[0].endByte == 3u);
	FLOWUI_CHECK(lines[1].startByte == 3u);
	FLOWUI_CHECK(lines[1].endByte == 6u);
}

void testPublicAndConsumerBoundary() {
	static_assert(std::is_same_v<decltype(FieldQueryResult{}.text), FieldTextView>);
	static_assert(std::is_same_v<decltype(FieldQueryResult{}.visibleLines),
		std::span<const VisibleTextLine>>);
	static_assert(requires(InputFieldManager& manager, FieldHandle handle,
		const FieldTextSpanSubmission& span) {
		{ manager.submitTextSpan(handle, span) } -> std::same_as<bool>;
	});
	FLOWUI_CHECK(std::holds_alternative<text::SingleLineStorage>(
		detail::manager_storage::InputFieldState{}.storage));

	const std::string source = readFile(FLOWUI_INPUT_FIELD_MANAGER_SOURCE);
	const std::string stateHeader = readFile(FLOWUI_INPUT_FIELD_STATE_SOURCE);
	FLOWUI_CHECK(source.find("submittedTextSpans") != std::string::npos);
	FLOWUI_CHECK(source.find("TextLayoutResult") != std::string::npos);
	FLOWUI_CHECK(source.find("materializeVisibleLines") != std::string::npos);
	FLOWUI_CHECK(source.find("moveCaretsVertical") != std::string::npos);
	FLOWUI_CHECK(stateHeader.find("FieldStorage storage") != std::string::npos);
	FLOWUI_CHECK(stateHeader.find("std::string text{}") == std::string::npos);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("single and multiline storage remain distinct", testStorageStrategiesAndViews);
	runner.run("chunk boundaries preserve UTF-8 and newline lookup", testUtf8AndCrossChunkEdits);
	runner.run("soft wrapping breaks only at layout clusters", testClusterLineBreaking);
	runner.run("multiline API and manager boundary are explicit", testPublicAndConsumerBoundary);
	return runner.finish();
}
