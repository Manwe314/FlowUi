#pragma once

#include <cstdint>

namespace FlowUi::detail::platform {
/** Revision of the source-versioned phase-1 Vulkan platform contract. */
inline constexpr std::uint32_t platformContractRevision = 1u;
/** Runtime initialization settings; native window settings are supplied per window. */
struct PlatformConfig {};
} // namespace FlowUi::detail::platform
