#pragma once
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontProviderTypes.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
namespace FlowUi::detail::font {
/** Validate CPU invariants shared by every selected provider before catalog publication.
 * An out-of-range fallback is permitted here and repaired to index zero by core. */
[[nodiscard]] Status validateFontCpuAsset(const FontCpuAsset& asset) noexcept;
} // namespace FlowUi::detail::font
