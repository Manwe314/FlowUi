#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace FlowUi::detail::text {

struct DecodedCodepoint {
	uint32_t value = 0;
	size_t startByte = 0;
	size_t endByte = 0;
};

inline bool decodeNextUtf8(
	std::string_view text,
	size_t& byteOffset,
	DecodedCodepoint& decoded) noexcept {
	if (byteOffset >= text.size()) return false;
	decoded.startByte = byteOffset;
	const auto* bytes = reinterpret_cast<const uint8_t*>(text.data());
	const uint8_t first = bytes[byteOffset];
	if (first < 0x80u) {
		decoded.value = first;
		decoded.endByte = ++byteOffset;
		return true;
	}
	const auto continuation = [&](size_t index) {
		return index < text.size() ? bytes[index] : uint8_t{0};
	};
	if ((first & 0xE0u) == 0xC0u && byteOffset + 1 < text.size()) {
		const uint8_t c1 = continuation(byteOffset + 1);
		if ((c1 & 0xC0u) == 0x80u) {
			decoded.value = ((first & 0x1Fu) << 6u) | (c1 & 0x3Fu);
			byteOffset += 2;
			decoded.endByte = byteOffset;
			return true;
		}
	} else if ((first & 0xF0u) == 0xE0u && byteOffset + 2 < text.size()) {
		const uint8_t c1 = continuation(byteOffset + 1);
		const uint8_t c2 = continuation(byteOffset + 2);
		if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u) {
			decoded.value = ((first & 0x0Fu) << 12u) | ((c1 & 0x3Fu) << 6u) | (c2 & 0x3Fu);
			byteOffset += 3;
			decoded.endByte = byteOffset;
			return true;
		}
	} else if ((first & 0xF8u) == 0xF0u && byteOffset + 3 < text.size()) {
		const uint8_t c1 = continuation(byteOffset + 1);
		const uint8_t c2 = continuation(byteOffset + 2);
		const uint8_t c3 = continuation(byteOffset + 3);
		if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u && (c3 & 0xC0u) == 0x80u) {
			decoded.value = ((first & 0x07u) << 18u) | ((c1 & 0x3Fu) << 12u) |
				((c2 & 0x3Fu) << 6u) | (c3 & 0x3Fu);
			byteOffset += 4;
			decoded.endByte = byteOffset;
			return true;
		}
	}
	decoded.value = 0xFFFDu;
	decoded.endByte = ++byteOffset;
	return true;
}

} // namespace FlowUi::detail::text
