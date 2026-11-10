#pragma once

#include "FlowUi/Private/IdentityHash.hpp"
#include <cstdint>
#include <string_view>

/* Internal hashing domain for supported application action identities. */
namespace FlowUi::detail::app_action_id {

inline constexpr uint64_t kAppActionDomain = 0x6ac12f9d84e753b1ull;
[[nodiscard]] constexpr uint64_t make(std::string_view name) noexcept {
	if (name.empty()) return 0;
	return detail::identity_hash::reserveZero(detail::identity_hash::combine(
		kAppActionDomain,
		detail::identity_hash::hashBytes(name)));
}

} // namespace FlowUi::detail::app_action_id
