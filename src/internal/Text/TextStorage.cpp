#include "internal/Text/TextStorage.hpp"

#include <algorithm>
#include <cstring>

namespace FlowUi::detail::text {
namespace {

bool isContinuation(char value) noexcept {
	return (static_cast<unsigned char>(value) & 0xC0u) == 0x80u;
}

void appendChunked(std::vector<std::string>& chunks, std::string_view text) {
	while (!text.empty()) {
		if (!chunks.empty() && chunks.back().size() < MultiLineStorage::TargetChunkBytes) {
			const size_t amount = std::min(
				text.size(), MultiLineStorage::TargetChunkBytes - chunks.back().size());
			chunks.back().append(text.substr(0, amount));
			text.remove_prefix(amount);
			continue;
		}
		const size_t amount = std::min(text.size(), MultiLineStorage::TargetChunkBytes);
		chunks.emplace_back(text.substr(0, amount));
		text.remove_prefix(amount);
	}
}

void rebuildLineStarts(MultiLineStorage& storage) {
	storage.lineStarts.clear();
	storage.lineStarts.push_back(0);
	size_t absolute = 0;
	for (const std::string& chunk : storage.chunks) {
		for (size_t i = 0; i < chunk.size(); ++i) {
			if (chunk[i] == '\n') storage.lineStarts.push_back(absolute + i + 1u);
		}
		absolute += chunk.size();
	}
}

} // namespace

FieldStorage makeFieldStorage(TextFieldMode mode, std::string_view initialText) {
	if (mode == TextFieldMode::SingleLine) return SingleLineStorage{std::string(initialText)};
	MultiLineStorage multiline{};
	multiline.byteSize = initialText.size();
	appendChunked(multiline.chunks, initialText);
	rebuildLineStarts(multiline);
	return multiline;
}

TextFieldMode storageMode(const FieldStorage& storage) noexcept {
	return std::holds_alternative<SingleLineStorage>(storage)
		? TextFieldMode::SingleLine
		: TextFieldMode::MultiLine;
}

size_t byteCount(const FieldStorage& storage) noexcept {
	if (const auto* single = std::get_if<SingleLineStorage>(&storage)) return single->text.size();
	return std::get<MultiLineStorage>(storage).byteSize;
}

bool empty(const FieldStorage& storage) noexcept {
	return byteCount(storage) == 0;
}

std::optional<std::string_view> contiguous(const FieldStorage& storage) noexcept {
	if (const auto* single = std::get_if<SingleLineStorage>(&storage)) return std::string_view(single->text);
	return std::nullopt;
}

std::string copy(const FieldStorage& storage) {
	return copy(storage, TextRange{0, byteCount(storage)});
}

std::string copy(const FieldStorage& storage, TextRange range) {
	const size_t size = byteCount(storage);
	range.startByte = std::min(range.startByte, size);
	range.endByte = std::clamp(range.endByte, range.startByte, size);
	if (const auto* single = std::get_if<SingleLineStorage>(&storage)) {
		return single->text.substr(range.startByte, range.endByte - range.startByte);
	}
	std::string result;
	result.reserve(range.endByte - range.startByte);
	forEachChunk(storage, range, [&](std::string_view chunk) { result.append(chunk); });
	return result;
}

void forEachChunk(
	const FieldStorage& storage,
	TextRange range,
	const std::function<void(std::string_view)>& visitor) {
	if (!visitor) return;
	const size_t size = byteCount(storage);
	range.startByte = std::min(range.startByte, size);
	range.endByte = std::clamp(range.endByte, range.startByte, size);
	if (range.endByte == range.startByte) return;
	if (const auto* single = std::get_if<SingleLineStorage>(&storage)) {
		visitor(std::string_view(single->text).substr(
			range.startByte, range.endByte - range.startByte));
		return;
	}

	const auto& multiline = std::get<MultiLineStorage>(storage);
	size_t absolute = 0;
	for (const std::string& chunk : multiline.chunks) {
		const size_t chunkEnd = absolute + chunk.size();
		if (chunkEnd <= range.startByte) {
			absolute = chunkEnd;
			continue;
		}
		if (absolute >= range.endByte) break;
		const size_t localStart = range.startByte > absolute ? range.startByte - absolute : 0u;
		const size_t localEnd = std::min(chunk.size(), range.endByte - absolute);
		visitor(std::string_view(chunk).substr(localStart, localEnd - localStart));
		absolute = chunkEnd;
	}
}

bool equals(const FieldStorage& storage, std::string_view text) {
	if (byteCount(storage) != text.size()) return false;
	if (const auto value = contiguous(storage)) return *value == text;
	size_t cursor = 0;
	bool equal = true;
	forEachChunk(storage, TextRange{0, text.size()}, [&](std::string_view chunk) {
		if (!equal || std::memcmp(chunk.data(), text.data() + cursor, chunk.size()) != 0) {
			equal = false;
		}
		cursor += chunk.size();
	});
	return equal;
}

char byteAt(const FieldStorage& storage, size_t byteOffset) noexcept {
	if (byteOffset >= byteCount(storage)) return '\0';
	if (const auto* single = std::get_if<SingleLineStorage>(&storage)) return single->text[byteOffset];
	const auto& multiline = std::get<MultiLineStorage>(storage);
	for (const std::string& chunk : multiline.chunks) {
		if (byteOffset < chunk.size()) return chunk[byteOffset];
		byteOffset -= chunk.size();
	}
	return '\0';
}

size_t clampUtf8Boundary(const FieldStorage& storage, size_t byteOffset) noexcept {
	const size_t size = byteCount(storage);
	size_t clamped = std::min(byteOffset, size);
	while (clamped > 0 && clamped < size && isContinuation(byteAt(storage, clamped))) --clamped;
	return clamped;
}

size_t nextUtf8Codepoint(const FieldStorage& storage, size_t byteOffset) noexcept {
	const size_t size = byteCount(storage);
	size_t cursor = clampUtf8Boundary(storage, byteOffset);
	if (cursor >= size) return size;
	++cursor;
	while (cursor < size && isContinuation(byteAt(storage, cursor))) ++cursor;
	return cursor;
}

size_t previousUtf8Codepoint(const FieldStorage& storage, size_t byteOffset) noexcept {
	size_t cursor = clampUtf8Boundary(storage, byteOffset);
	if (cursor == 0) return 0;
	--cursor;
	while (cursor > 0 && isContinuation(byteAt(storage, cursor))) --cursor;
	return cursor;
}

void replace(FieldStorage& storage, TextRange range, std::string_view insertedText) {
	const size_t size = byteCount(storage);
	range.startByte = std::min(range.startByte, size);
	range.endByte = std::clamp(range.endByte, range.startByte, size);
	if (auto* single = std::get_if<SingleLineStorage>(&storage)) {
		single->text.replace(range.startByte, range.endByte - range.startByte, insertedText);
		return;
	}

	auto& multiline = std::get<MultiLineStorage>(storage);
	std::vector<size_t> nextLineStarts;
	nextLineStarts.reserve(multiline.lineStarts.size() + 4u);
	for (const size_t lineStart : multiline.lineStarts) {
		if (lineStart <= range.startByte) nextLineStarts.push_back(lineStart);
	}
	for (size_t i = 0; i < insertedText.size(); ++i) {
		if (insertedText[i] == '\n') nextLineStarts.push_back(range.startByte + i + 1u);
	}
	const size_t removedBytes = range.endByte - range.startByte;
	for (const size_t lineStart : multiline.lineStarts) {
		if (lineStart <= range.endByte) continue;
		nextLineStarts.push_back(insertedText.size() >= removedBytes
			? lineStart + (insertedText.size() - removedBytes)
			: lineStart - (removedBytes - insertedText.size()));
	}
	std::sort(nextLineStarts.begin(), nextLineStarts.end());
	nextLineStarts.erase(
		std::unique(nextLineStarts.begin(), nextLineStarts.end()),
		nextLineStarts.end());
	if (nextLineStarts.empty() || nextLineStarts.front() != 0u) nextLineStarts.insert(nextLineStarts.begin(), 0u);

	struct ChunkLocation { size_t index = 0; size_t offset = 0; };
	const auto locate = [&](size_t position) {
		size_t absolute = 0;
		for (size_t i = 0; i < multiline.chunks.size(); ++i) {
			const size_t chunkEnd = absolute + multiline.chunks[i].size();
			if (position < chunkEnd) return ChunkLocation{i, position - absolute};
			absolute = chunkEnd;
		}
		return ChunkLocation{multiline.chunks.size(), 0};
	};
	const ChunkLocation start = locate(range.startByte);
	const ChunkLocation end = locate(range.endByte);

	std::vector<std::string> next;
	next.reserve(multiline.chunks.size() + insertedText.size() / MultiLineStorage::TargetChunkBytes + 2u);
	for (size_t i = 0; i < start.index; ++i) next.push_back(std::move(multiline.chunks[i]));
	if (start.index < multiline.chunks.size() && start.offset > 0u) {
		appendChunked(next, std::string_view(multiline.chunks[start.index]).substr(0, start.offset));
	}
	appendChunked(next, insertedText);
	size_t firstUntouchedChunk = end.index;
	if (end.index < multiline.chunks.size() && end.offset > 0u) {
		appendChunked(next, std::string_view(multiline.chunks[end.index]).substr(end.offset));
		firstUntouchedChunk = end.index + 1u;
	}
	for (size_t i = firstUntouchedChunk; i < multiline.chunks.size(); ++i) {
		next.push_back(std::move(multiline.chunks[i]));
	}
	multiline.chunks = std::move(next);
	multiline.byteSize = size - (range.endByte - range.startByte) + insertedText.size();
	multiline.lineStarts = std::move(nextLineStarts);
}

bool migrate(FieldStorage& storage, TextFieldMode targetMode) {
	if (storageMode(storage) == targetMode) return true;
	if (targetMode == TextFieldMode::SingleLine) {
		bool hasNewline = false;
		forEachChunk(storage, TextRange{0, byteCount(storage)}, [&](std::string_view chunk) {
			hasNewline = hasNewline || chunk.find('\n') != std::string_view::npos;
		});
		if (hasNewline) return false;
		storage = SingleLineStorage{copy(storage)};
		return true;
	}
	storage = makeFieldStorage(TextFieldMode::MultiLine, std::get<SingleLineStorage>(storage).text);
	return true;
}

size_t lineCount(const FieldStorage& storage) noexcept {
	if (std::holds_alternative<SingleLineStorage>(storage)) return 1;
	return std::get<MultiLineStorage>(storage).lineStarts.size();
}

size_t lineFromByte(const FieldStorage& storage, size_t byteOffset) noexcept {
	if (std::holds_alternative<SingleLineStorage>(storage)) return 0;
	const auto& multiline = std::get<MultiLineStorage>(storage);
	byteOffset = std::min(byteOffset, multiline.byteSize);
	const auto it = std::upper_bound(multiline.lineStarts.begin(), multiline.lineStarts.end(), byteOffset);
	return it == multiline.lineStarts.begin()
		? 0u
		: static_cast<size_t>(std::distance(multiline.lineStarts.begin(), it) - 1);
}

TextRange lineRange(const FieldStorage& storage, size_t lineIndex) noexcept {
	const size_t size = byteCount(storage);
	if (std::holds_alternative<SingleLineStorage>(storage)) return TextRange{0, size};
	const auto& multiline = std::get<MultiLineStorage>(storage);
	if (lineIndex >= multiline.lineStarts.size()) return TextRange{size, size};
	const size_t start = multiline.lineStarts[lineIndex];
	size_t end = lineIndex + 1u < multiline.lineStarts.size()
		? multiline.lineStarts[lineIndex + 1u]
		: size;
	if (end > start && byteAt(storage, end - 1u) == '\n') --end;
	return TextRange{start, end};
}

} // namespace FlowUi::detail::text
