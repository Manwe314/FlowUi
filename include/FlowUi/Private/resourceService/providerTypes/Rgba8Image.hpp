#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace FlowUi::detail {

/** Owned top-down, tightly packed RGBA8 rows. Icons use straight alpha;
 * font atlases contain linear distance samples and preserve the MTSDF alpha channel. */
struct Rgba8Image {
	std::vector<std::byte> pixels{};
	uint32_t width = 0;
	uint32_t height = 0;
};

/** Checked byte count; invalid dimensions do not yield a usable image. */
struct Rgba8ByteSize {
	size_t value = 0;
	bool valid = false;
};

/** Check nonzero dimensions and all row/image arithmetic before allocation. */
[[nodiscard]] constexpr Rgba8ByteSize rgba8ByteSize(uint32_t width, uint32_t height) noexcept {
	if (width == 0 || height == 0)
		return {};
	constexpr size_t maximum = std::numeric_limits<size_t>::max();
	if (static_cast<uintmax_t>(width) > maximum / 4u)
		return {};
	const size_t rowBytes = static_cast<size_t>(width) * 4u;
	if (static_cast<uintmax_t>(height) > maximum / rowBytes)
		return {};
	return {rowBytes * static_cast<size_t>(height), true};
}

static_assert(rgba8ByteSize(2, 3).valid && rgba8ByteSize(2, 3).value == 24);
static_assert(!rgba8ByteSize(0, 3).valid);

} // namespace FlowUi::detail
