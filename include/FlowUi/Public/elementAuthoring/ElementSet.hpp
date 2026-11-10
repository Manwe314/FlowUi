#pragma once

#include <type_traits>

#include "FlowUi/Public/elementAuthoring/ElementConcepts.hpp"

namespace FlowUi {


/** @brief Zero-data compile-time catalog of element definition types. */
template <FlowElement... Elements>
class ElementSet {};

/**
 * @brief Deduce a reusable element-type catalog from definition objects.
 *
 * The arguments participate only in type deduction; ElementSet stores no
 * object addresses or runtime data.
 */
template <typename... Elements>
	requires ((FlowElement<std::remove_cvref_t<Elements>>) && ...)
[[nodiscard]] constexpr auto elementSet(const Elements&...) noexcept {
	return ElementSet<std::remove_cvref_t<Elements>...>{};
}


} // namespace FlowUi
