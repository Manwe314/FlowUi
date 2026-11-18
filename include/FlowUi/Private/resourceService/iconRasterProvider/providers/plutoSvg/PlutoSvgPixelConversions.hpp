#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include "FlowUi/Private/resourceService/providerTypes/Rgba8Image.hpp"

namespace FlowUi::detail::icon {

/** Preserve integer truncation while clamping malformed premultiplied components. */
[[nodiscard]] constexpr uint8_t straightChannel(uint32_t premultiplied, uint32_t alpha) noexcept {
	if (alpha == 0) return 0;
	if (alpha == 255) return static_cast<uint8_t>(std::min(premultiplied, 255u));
	return static_cast<uint8_t>(std::min(premultiplied * 255u / alpha, 255u));
}

/** Convert a native ARGB32 word, independent of the host byte order. */
[[nodiscard]] constexpr std::array<std::byte, 4> rgbaFromArgb(uint32_t pixel) noexcept {
	const uint32_t alpha = (pixel >> 24u) & 255u;
	return {std::byte{straightChannel((pixel >> 16u) & 255u, alpha)},
		std::byte{straightChannel((pixel >> 8u) & 255u, alpha)},
		std::byte{straightChannel(pixel & 255u, alpha)}, std::byte{static_cast<uint8_t>(alpha)}};
}

/** Convert checked native rows into the tight output, ignoring any native stride padding. */
[[nodiscard]] inline bool copyNativeArgbRows(std::span<const std::byte> nativePixels,
											 size_t strideBytes, uint32_t width, uint32_t height,
											 std::span<std::byte> rgbaPixels) noexcept {
	const auto imageBytes = rgba8ByteSize(width, height);
	if (!imageBytes.valid || rgbaPixels.size() != imageBytes.value)
		return false;
	const size_t rowBytes = static_cast<size_t>(width) * 4u;
	if (strideBytes < rowBytes || strideBytes > std::numeric_limits<size_t>::max() / height ||
		nativePixels.size() < static_cast<size_t>(height - 1u) * strideBytes + rowBytes)
		return false;
	for (uint32_t rowIndex = 0; rowIndex < height; ++rowIndex) {
		const std::byte* sourceRow =
			nativePixels.data() + static_cast<size_t>(rowIndex) * strideBytes;
		std::byte* destinationRow = rgbaPixels.data() + static_cast<size_t>(rowIndex) * rowBytes;
		for (uint32_t columnIndex = 0; columnIndex < width; ++columnIndex) {
			uint32_t nativePixel = 0;
			std::memcpy(&nativePixel, sourceRow + static_cast<size_t>(columnIndex) * 4u,
						sizeof(nativePixel));
			const auto channels = rgbaFromArgb(nativePixel);
			std::memcpy(destinationRow + static_cast<size_t>(columnIndex) * 4u, channels.data(),
						channels.size());
		}
	}
	return true;
}

static_assert(rgbaFromArgb(0x80804020u) ==
	std::array{std::byte{255}, std::byte{127}, std::byte{63}, std::byte{128}});
static_assert(rgbaFromArgb(0u) == std::array<std::byte, 4>{});

} // namespace FlowUi::detail::icon
