#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "FlowUi/ElementID.hpp"

namespace FlowUi::detail::input_field {

/** Separates element-backed fields from explicitly named input resources. */
enum class InputFieldKeyDomain : uint8_t {
	Element = 1,
	Resource = 2,
};

/** Numeric-only persistent key used by one window's InputFieldManager. */
struct InputFieldKey {
	uint64_t value = 0;
	InputFieldKeyDomain domain = InputFieldKeyDomain::Element;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return value != 0;
	}

	friend constexpr bool operator==(
		InputFieldKey,
		InputFieldKey) noexcept = default;
};

struct InputFieldKeyHash {
	[[nodiscard]] constexpr std::size_t operator()(InputFieldKey key) const noexcept {
		uint64_t value = key.value;
		value ^= static_cast<uint64_t>(key.domain) * 0x9e3779b97f4a7c15ull;
		value ^= value >> 30u;
		value *= 0xbf58476d1ce4e5b9ull;
		value ^= value >> 27u;
		value *= 0x94d049bb133111ebull;
		value ^= value >> 31u;
		return static_cast<std::size_t>(value);
	}
};

[[nodiscard]] constexpr InputFieldKey toInputFieldKey(FlowElementID id) noexcept {
	return {.value = id.value, .domain = InputFieldKeyDomain::Element};
}

[[nodiscard]] constexpr InputFieldKey toInputFieldKey(GlobalFlowID id) noexcept {
	return {.value = id.value, .domain = InputFieldKeyDomain::Element};
}

[[nodiscard]] constexpr InputFieldKey toInputFieldKey(FlowElementPartID id) noexcept {
	return {.value = id.value, .domain = InputFieldKeyDomain::Element};
}

[[nodiscard]] constexpr InputFieldKey resourceInputFieldKey(uint64_t value) noexcept {
	return {.value = value, .domain = InputFieldKeyDomain::Resource};
}

static_assert(std::is_trivially_copyable_v<InputFieldKey>);

} // namespace FlowUi::detail::input_field
