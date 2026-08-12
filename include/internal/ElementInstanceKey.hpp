#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "FlowUi/ElementID.hpp"

namespace FlowUi::detail::element {

/** Numeric-only key stored by persistent element-state infrastructure. */
struct ElementInstanceKey {
	uint64_t value = 0;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	friend constexpr bool operator==(
		ElementInstanceKey,
		ElementInstanceKey) noexcept = default;
};

struct ElementInstanceKeyHash {
	[[nodiscard]] constexpr std::size_t operator()(ElementInstanceKey key) const noexcept {
		return static_cast<std::size_t>(key.value);
	}
};

[[nodiscard]] constexpr ElementInstanceKey toInstanceKey(FlowElementID id) noexcept {
	return ElementInstanceKey{.value = id.value};
}

[[nodiscard]] constexpr ElementInstanceKey toInstanceKey(GlobalFlowID id) noexcept {
	return ElementInstanceKey{.value = id.value};
}

[[nodiscard]] constexpr ElementInstanceKey toInstanceKey(FlowElementPartID id) noexcept {
	return ElementInstanceKey{.value = id.value};
}

static_assert(sizeof(ElementInstanceKey) == sizeof(uint64_t));
static_assert(std::is_trivially_copyable_v<ElementInstanceKey>);

} // namespace FlowUi::detail::element
