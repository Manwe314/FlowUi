#pragma once

#include <cstdint>
#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"

namespace FlowUi {

/** Strong identity of a Flow element definition. */
struct FlowDefinitionID {
	/** Stable numeric identity; zero is invalid. The surrounding type determines its scope. */
	uint64_t value = 0;

	/** Return whether the identity is nonzero; this does not validate registration. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	/** Compare definition identity values. */
	[[nodiscard]] friend constexpr bool operator==(FlowDefinitionID,
												   FlowDefinitionID) noexcept = default;
};

/** Resolved identity of one local Flow element instance or named Clay node.
 * Resolve against the same parent/definition path each frame to retain state.
 * WindowId supplies the outer storage and interaction scope. */
struct FlowElementID {
	/** Stable numeric identity; zero is invalid. The surrounding type determines its scope. */
	uint64_t value = 0;
#if FLOW_UI_DEV_MODE
	/** Borrowed developer label excluded from equality; backing storage must outlive retained uses. */
	std::string_view debugName{};
#endif

	/** Return whether the identity is nonzero; this does not validate registration. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	/** Compare numeric local identities, ignoring developer labels. */
	[[nodiscard]] friend constexpr bool operator==(FlowElementID lhs, FlowElementID rhs) noexcept {
		return lhs.value == rhs.value;
	}
};

/** Definition-scoped, parent-independent named identity. The same numeric ID
 * still denotes separate instances in different windows. Obtain with Global(). */
struct GlobalFlowID {
	/** Stable numeric identity; zero is invalid. The surrounding type determines its scope. */
	uint64_t value = 0;
#if FLOW_UI_DEV_MODE
	/** Borrowed developer label excluded from equality; backing storage must outlive retained uses. */
	std::string_view debugName{};
#endif

	/** Return whether the identity is nonzero; this does not validate registration. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	/** Compare numeric global identities, ignoring developer labels. */
	[[nodiscard]] friend constexpr bool operator==(GlobalFlowID lhs, GlobalFlowID rhs) noexcept {
		return lhs.value == rhs.value;
	}
};

/** Definition-time declaration of one semantic part owned by an element. */
struct FlowElementPart {
	/** Definition-time part token; zero is invalid. */
	uint64_t token = 0;
#if FLOW_UI_DEV_MODE
	/** Borrowed developer label excluded from equality; backing storage must outlive retained uses. */
	std::string_view debugName{};
#endif

	/** Return whether the identity is nonzero; this does not validate registration. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return token != 0;
	}

	/** Compare declaration tokens, ignoring developer labels. */
	[[nodiscard]] friend constexpr bool operator==(FlowElementPart lhs,
												   FlowElementPart rhs) noexcept {
		return lhs.token == rhs.token;
	}
};

/** Semantic part declaration bound to one concrete owner element instance. */
struct FlowElementPartID {
	/** Stable numeric identity; zero is invalid. The surrounding type determines its scope. */
	uint64_t value = 0;
#if FLOW_UI_DEV_MODE
	/** Borrowed developer label excluded from equality; backing storage must outlive retained uses. */
	std::string_view debugName{};
#endif

	/** Return whether the identity is nonzero; this does not validate registration. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	/** Compare resolved part identities, ignoring developer labels. */
	[[nodiscard]] friend constexpr bool operator==(FlowElementPartID lhs,
												   FlowElementPartID rhs) noexcept {
		return lhs.value == rhs.value;
	}
};

/** Sentinel for an absent element definition. */
inline constexpr FlowDefinitionID InvalidFlowDefinitionID{};
/** Sentinel for an absent local element instance. */
inline constexpr FlowElementID InvalidFlowElementID{};
/** Sentinel for an absent global element identity. */
inline constexpr GlobalFlowID InvalidGlobalFlowID{};
/** Sentinel for an absent semantic part declaration. */
inline constexpr FlowElementPart InvalidFlowElementPart{};
/** Sentinel for an absent resolved semantic part. */
inline constexpr FlowElementPartID InvalidFlowElementPartID{};

} // namespace FlowUi
