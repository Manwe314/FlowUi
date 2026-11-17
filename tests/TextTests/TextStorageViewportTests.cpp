#include <algorithm>
#include <concepts>
#include <cstdlib>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "TestHarness.hpp"
#include "FlowUi/Private/inputService/InputFieldManagerState.hpp"
#include "FlowUi/Private/textLayoutService/TextLineBreaker.hpp"
#include "FlowUi/Private/textLayoutService/TextStorage.hpp"
#include "FlowUi/Public/managers/InputFieldManager.hpp"

namespace {
thread_local std::ptrdiff_t allocationsBeforeFailure = -1;
}

// Exercise the actual standard-container allocation paths without production hooks.
void* operator new(std::size_t bytes) {
	if (allocationsBeforeFailure >= 0 && allocationsBeforeFailure-- == 0) {
		allocationsBeforeFailure = -1;
		throw std::bad_alloc{};
	}
	if (void* allocation = std::malloc(bytes == 0 ? 1 : bytes)) return allocation;
	throw std::bad_alloc{};
}

void operator delete(void* allocation) noexcept { std::free(allocation); }
void operator delete(void* allocation, std::size_t) noexcept { std::free(allocation); }

namespace {

using namespace FlowUi;
namespace text = FlowUi::detail::text;

template <typename Config>
concept HasLegacyNewlineFlag = requires(Config config) { config.allowNewline; };

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

void testAllocationFailurePreservesMultilineDocument() {
	std::string original(text::MultiLineStorage::TargetChunkBytes * 8u, 'a');
	for (size_t offset = 79; offset < original.size(); offset += 80) original[offset] = '\n';
	const TextRange range{2u * text::MultiLineStorage::TargetChunkBytes + 17u,
		4u * text::MultiLineStorage::TargetChunkBytes + 31u};
	const std::string insertion(text::MultiLineStorage::TargetChunkBytes * 3u, 'b');
	std::string expected = original;
	expected.replace(range.startByte, range.endByte - range.startByte, insertion);
	size_t injectedFailures = 0;
	bool completed = false;
	for (std::ptrdiff_t checkpoint = 0; checkpoint < 64; ++checkpoint) {
		text::FieldStorage storage = text::makeFieldStorage(TextFieldMode::MultiLine, original);
		const auto originalLineStarts = std::get<text::MultiLineStorage>(storage).lineStarts;
		allocationsBeforeFailure = checkpoint;
		try {
			text::replace(storage, range, insertion);
			allocationsBeforeFailure = -1;
			FLOWUI_CHECK(text::copy(storage) == expected);
			completed = true;
			break;
		} catch (const std::bad_alloc&) {
			allocationsBeforeFailure = -1;
			++injectedFailures;
			FLOWUI_CHECK(text::byteCount(storage) == original.size());
			FLOWUI_CHECK(text::copy(storage) == original);
			FLOWUI_CHECK(std::get<text::MultiLineStorage>(storage).lineStarts == originalLineStarts);
		}
	}
	FLOWUI_CHECK(injectedFailures > 0);
	FLOWUI_CHECK(completed);
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
	static_assert(!std::is_convertible_v<FieldTextView, std::string_view>);
	static_assert(!std::is_constructible_v<std::string, FieldTextView>);
	static_assert(!HasLegacyNewlineFlag<FieldConfig>);
	static_assert(std::same_as<decltype(std::declval<const FieldTextView&>().forEachChunk(
		std::declval<TextRange>(), std::declval<const TextChunkVisitor&>())), void>);
	static_assert(!noexcept(std::declval<const FieldTextView&>().forEachChunk(
		std::declval<TextRange>(), std::declval<const TextChunkVisitor&>())));
	static_assert(std::is_same_v<decltype(FieldQueryResult{}.visibleLines),
		std::span<const VisibleTextLine>>);
	static_assert(requires(InputFieldManager& manager, FieldHandle handle,
		const FieldTextSpanSubmission& span) {
		{ manager.submitTextSpan(handle, span) } -> std::same_as<bool>;
	});
	static_assert(!noexcept(std::declval<InputFieldManager&>().submitTextSpan(
		std::declval<FieldHandle>(), std::declval<const FieldTextSpanSubmission&>())));
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
	runner.run("failed multiline edit preserves chunks and line index", testAllocationFailurePreservesMultilineDocument);
	runner.run("soft wrapping breaks only at layout clusters", testClusterLineBreaking);
	runner.run("multiline API and manager boundary are explicit", testPublicAndConsumerBoundary);
	return runner.finish();
}
