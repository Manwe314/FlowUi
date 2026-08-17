#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "managers/structs/InputFieldManagerStructs.hpp"

namespace FlowUi::detail::text {

struct SingleLineStorage {
	std::string text{};
};

struct MultiLineStorage {
	static constexpr size_t TargetChunkBytes = 4096;

	std::vector<std::string> chunks{};
	std::vector<size_t> lineStarts{0};
	size_t byteSize = 0;
};

using FieldStorage = std::variant<SingleLineStorage, MultiLineStorage>;

[[nodiscard]] FieldStorage makeFieldStorage(TextFieldMode mode, std::string_view initialText);
[[nodiscard]] TextFieldMode storageMode(const FieldStorage& storage) noexcept;
[[nodiscard]] size_t byteCount(const FieldStorage& storage) noexcept;
[[nodiscard]] bool empty(const FieldStorage& storage) noexcept;
[[nodiscard]] std::optional<std::string_view> contiguous(const FieldStorage& storage) noexcept;
[[nodiscard]] std::string copy(const FieldStorage& storage);
[[nodiscard]] std::string copy(const FieldStorage& storage, TextRange range);
void forEachChunk(
	const FieldStorage& storage,
	TextRange range,
	const std::function<void(std::string_view)>& visitor);
[[nodiscard]] bool equals(const FieldStorage& storage, std::string_view text);
[[nodiscard]] char byteAt(const FieldStorage& storage, size_t byteOffset) noexcept;
[[nodiscard]] size_t clampUtf8Boundary(const FieldStorage& storage, size_t byteOffset) noexcept;
[[nodiscard]] size_t nextUtf8Codepoint(const FieldStorage& storage, size_t byteOffset) noexcept;
[[nodiscard]] size_t previousUtf8Codepoint(const FieldStorage& storage, size_t byteOffset) noexcept;

void replace(FieldStorage& storage, TextRange range, std::string_view insertedText);
[[nodiscard]] bool migrate(FieldStorage& storage, TextFieldMode targetMode);

[[nodiscard]] size_t lineCount(const FieldStorage& storage) noexcept;
[[nodiscard]] size_t lineFromByte(const FieldStorage& storage, size_t byteOffset) noexcept;
[[nodiscard]] TextRange lineRange(const FieldStorage& storage, size_t lineIndex) noexcept;

} // namespace FlowUi::detail::text
