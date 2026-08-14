#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"
#include "internal/IdentityHash.hpp"

namespace FlowUi {

/** @addtogroup flowui_action_manager
 * @{
 */

struct AppActionID {
	uint64_t value;
#if FLOW_UI_DEV_MODE
	std::string_view debugName;
#endif

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	friend constexpr bool operator==(AppActionID lhs, AppActionID rhs) noexcept {
		return lhs.value == rhs.value;
	}
};

inline constexpr AppActionID InvalidAppActionID{};

namespace detail::app_action_id {

inline constexpr uint64_t kAppActionDomain = 0x6ac12f9d84e753b1ull;
[[nodiscard]] constexpr uint64_t make(std::string_view name) noexcept {
	if (name.empty()) return 0;
	return detail::identity_hash::reserveZero(detail::identity_hash::combine(
		kAppActionDomain,
		detail::identity_hash::hashBytes(name)));
}

} // namespace detail::app_action_id

template <std::size_t N>
[[nodiscard]] consteval AppActionID AppAction(const char (&name)[N]) {
	const std::string_view view = detail::identity_hash::literalView(name);
	return AppActionID{
		.value = detail::app_action_id::make(view),
#if FLOW_UI_DEV_MODE
		.debugName = view,
#endif
	};
}

struct AppActionIDHash {
	[[nodiscard]] constexpr std::size_t operator()(AppActionID id) const noexcept {
		return static_cast<std::size_t>(id.value ^ (id.value >> 32u));
	}
};

/** @} */

} // namespace FlowUi
