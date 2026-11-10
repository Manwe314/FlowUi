#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FlowUi/Private/actionService/AppActionIdentity.hpp"
#include "FlowUi/Public/BuildConfig.hpp"

namespace FlowUi {

/** @addtogroup flowui_action_manager
 * @{
 */

/**
 * Stable authored application-action identity. Create with AppAction(); value-initialize to obtain an
 * invalid ID. Developer labels do not affect equality or hashing.
 */
struct AppActionID {
	/** Stable nonzero hashed identity, or zero for an invalid action. */
	uint64_t value;
#if FLOW_UI_DEV_MODE
	/** Borrowed literal label available in developer builds. */
	std::string_view debugName;
#endif

	/** Return whether the identity is nonzero; does not test whether it is bound. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	/** Compare numeric identities, ignoring developer labels. */
	[[nodiscard]] friend constexpr bool operator==(AppActionID lhs, AppActionID rhs) noexcept {
		return lhs.value == rhs.value;
	}
};

/** Sentinel for an absent application action. */
inline constexpr AppActionID InvalidAppActionID{};


/** Hash a stable authored literal into an application-action identity at compile time.
 * @param name Nonempty action name; developer builds borrow its literal storage.
 * @return Nonzero action ID for binding and invocation. */
template <std::size_t N>
[[nodiscard]] consteval AppActionID AppAction(const char (&name)[N]) noexcept {
	const std::string_view view = detail::identity_hash::literalView(name);
	return AppActionID{
		.value = detail::app_action_id::make(view),
#if FLOW_UI_DEV_MODE
		.debugName = view,
#endif
	};
}

/** Container hash adapter for application-action identity; ignores developer labels. */
struct AppActionIDHash {
	/** Hash the numeric action identity for associative containers. */
	[[nodiscard]] constexpr std::size_t operator()(AppActionID id) const noexcept {
		return static_cast<std::size_t>(id.value ^ (id.value >> 32u));
	}
};

/** @} */

} // namespace FlowUi
