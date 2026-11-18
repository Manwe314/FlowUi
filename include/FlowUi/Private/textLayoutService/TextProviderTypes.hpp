#pragma once

#include "FlowUi/Private/textLayoutService/TextLayoutRecords.hpp"

namespace FlowUi::detail::text {

/** Borrowed synchronous input to the selected CPU text algorithm.
 * Results own their arrays. Glyph indices address the resolved face's default
 * variant, never source glyph IDs. Byte ranges are half-open input UTF-8 offsets.
 * Revision one uses source-ordered horizontal LTR clusters and caret positions;
 * soft wrapping and editing remain core responsibilities. Geometry-disabled
 * calls retain measurements, hard lines, clusters and carets with no glyphs.
 * Atlas identity, distance sampling and UVs are finalized by core.
 */
struct TextProviderInput {
	std::string_view text{};
	float pointsToPixelsScale = 96.0f / 72.0f;
	uint16_t fontSize = 0;
	uint16_t letterSpacing = 0;
	uint8_t tabWidth = 4;
	bool includeGlyphGeometry = false;
};

/** Preserve the basic provider's effective EM scaling without moving font policy. */
[[nodiscard]] constexpr float textEmPixels(const TextProviderInput& input,
										   float defaultPixels) noexcept {
	return input.fontSize > 0 ? static_cast<float>(input.fontSize) * input.pointsToPixelsScale
							  : defaultPixels;
}

/** Convert metric units with the legacy minimum denominator. */
[[nodiscard]] constexpr float textEmScale(float pixels, float emSize) noexcept {
	return pixels / (emSize > 1.0e-6f ? emSize : 1.0e-6f);
}

} // namespace FlowUi::detail::text
