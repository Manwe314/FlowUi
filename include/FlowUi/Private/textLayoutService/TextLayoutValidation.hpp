#pragma once

#include "FlowUi/Private/textLayoutService/TextProviderTypes.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"

namespace FlowUi::detail::text {

/** Validate a provider result and assign core-owned atlas metadata/UVs in place. */
[[nodiscard]] Status finalizeTextLayout(const TextProviderInput& input,
										const Font::FontFaceData& fontFace,
										TextLayoutResult& result) noexcept;

/** Pure legacy atlas-coordinate mapping, shared by every selected provider. */
[[nodiscard]] constexpr float textAtlasCoordinate(float coordinate,
												  float inverseDimension) noexcept {
	const float value = coordinate * inverseDimension;
	return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

} // namespace FlowUi::detail::text
