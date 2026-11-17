#pragma once

#include <type_traits>

#include "FlowUi/Public/elementAuthoring/ElementConcepts.hpp"

namespace FlowUi {


/** @brief Zero-data compile-time catalog of element definition types. */
template <FlowElement... Elements>
class ElementSet {};

/**
 * @brief Deduce a zero-data catalog of element definition types. Definition objects and their addresses are not retained.
 *
 * @see docs/api/elements.md
 */
template <typename... Elements>
	requires ((FlowElement<std::remove_cvref_t<Elements>>) && ...)
[[nodiscard]] constexpr auto elementSet(const Elements&...) noexcept {
	return ElementSet<std::remove_cvref_t<Elements>...>{};
}


} // namespace FlowUi
