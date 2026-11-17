#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace FlowUi::defaults {

/** Borrowed, tightly packed RGBA8 pixels with static storage lifetime. */
struct TexturePixels {
	/** Borrowed contiguous RGBA8 bytes; missing_texture() returns static storage. */
	std::span<const std::byte> bytes{};
	/** Pixel width of the tightly packed image. */
	uint32_t width = 0;
	/** Pixel height of the tightly packed image. */
	uint32_t height = 0;
};

/** Return the opaque 2x2 purple/cyan missing-texture checkerboard.
 * @par Example
 * @code{.cpp}
 * const FlowUi::defaults::TexturePixels pixels = FlowUi::defaults::missing_texture();
 * const auto bytes = pixels.bytes; // Static, borrowed RGBA8 storage.
 * @endcode
 */
[[nodiscard]] TexturePixels missing_texture() noexcept;

} // namespace FlowUi::defaults
