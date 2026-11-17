#pragma once

#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"

namespace FlowUi::defaults {

/**
 * Create the disk-independent emergency face: one filled rectangle at U+FFFD.
 * Other non-whitespace codepoints use that glyph; spaces/tabs retain synthetic advances.
 * Its borrowed baked bytes have static lifetime and require no runtime font baking.
 * This face signals unavailable text content and does not provide readable lettering.
 * Returns AllocationFailed if the owned face metadata cannot be allocated.
 * @par Example
 * @code{.cpp}
 * auto result = FlowUi::defaults::missing_font();
 * if (!result)
 *     throw FlowUi::FlowUiException(result.error());
 * FlowUi::FontFaceCreateInfo face = std::move(*result);
 * @endcode
 */
[[nodiscard]] Result<FontFaceCreateInfo> missing_font() noexcept;

} // namespace FlowUi::defaults
