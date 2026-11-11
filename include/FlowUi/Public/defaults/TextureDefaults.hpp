#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace FlowUi::defaults {

/** Borrowed, tightly packed RGBA8 pixels with static storage lifetime. */
struct TexturePixels {
	std::span<const std::byte> bytes{};
	uint32_t width = 0;
	uint32_t height = 0;
};

/** Return the opaque 2x2 purple/cyan missing-texture checkerboard. */
[[nodiscard]] TexturePixels missing_texture() noexcept;

} // namespace FlowUi::defaults
