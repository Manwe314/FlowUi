#pragma once

#include "FlowUi/Public/managerStructs/FontTypes.hpp"

namespace FlowUi::defaults {

/**
 * Create the disk-independent emergency face: one filled rectangle at U+FFFD.
 * Other non-whitespace codepoints use that glyph; spaces/tabs retain synthetic advances.
 * Its borrowed baked bytes have static lifetime and require no runtime font baking.
 * This face signals unavailable text content and does not provide readable lettering.
 */
[[nodiscard]] FontFaceCreateInfo missing_font();

} // namespace FlowUi::defaults
