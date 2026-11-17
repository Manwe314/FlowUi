#pragma once

/**
 * @file
 * @brief Literal/runtime colors and explicit Result-to-exception convenience functions.
 */

#include <cstddef>
#include <string_view>

#include "FlowUi/Private/ColorParsing.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include <type_traits>
#include <utility>
#include "clay.h"

/** Public FlowUi application, element-authoring and error-handling APIs. */
namespace FlowUi {

/**
 * @brief Convert a literal \#RRGGBB or \#RRGGBBAA color into a Clay_Color at compile time.
 *
 * Example:
 * @code{.cpp}
 * Clay_Color color = FlowUi::Flow_Color("#fff3e8ff");
 * Clay_Color opaque = FlowUi::Flow_Color("#fff3e8");
 * @endcode
 *
 * Invalid literals are rejected during compilation. Six-digit RGB input receives
 * an implicit `ff` alpha channel.
 *
 * @param hexRgba String literal containing a \#RRGGBB or \#RRGGBBAA hex color code.
 * @return Clay_Color decoded from the hex color input.
 */
template <std::size_t Size>
[[nodiscard]] consteval Clay_Color Flow_Color(const char (&hexRgba)[Size]) noexcept {
	return detail::decodeFlowColor(std::string_view{hexRgba, Size - 1u});
}

/**
 * @brief Convert a runtime \#RRGGBB or \#RRGGBBAA string into a Clay_Color.
 *
 * @param hexRgba Borrowed hex input, consumed synchronously.
 * @return RGBA color with components in Clay's 0..255 range, or ColorInvalid
 * when the runtime text is not valid \#RRGGBB or \#RRGGBBAA.
 * @par Example
 * @code{.cpp}
 * std::string_view setting = "#20242cff";
 * auto result = FlowUi::Flow_Color(setting);
 * if (!result)
 *     throw FlowUi::FlowUiException(result.error());
 * const Clay_Color color = *result;
 * @endcode
 */
[[nodiscard]] Result<Clay_Color> Flow_Color(std::string_view hexRgba) noexcept;

/**
 * @brief Borrow a successful result's value, or throw FlowUiException preserving its error.
 * @param result Checked operation result; the returned reference borrows its payload.
 * @return Mutable payload reference, valid while result and its value remain valid.
 * @throws FlowUiException when result contains an error. This does not report or resolve it.
 */
template <typename Value>
	requires (!std::is_void_v<Value>)
[[nodiscard]] Value& ValueOrThrow(Result<Value>& result) {
	if (!result)
		throw FlowUiException(result.error());
	return *result;
}

/** Borrow a successful const result's value; throw its original FlowUiError on failure. */
template <typename Value>
	requires (!std::is_void_v<Value>)
[[nodiscard]] const Value& ValueOrThrow(const Result<Value>& result) {
	if (!result)
		throw FlowUiException(result.error());
	return *result;
}

/**
 * @brief Move a successful temporary result's payload into an owned value.
 * @param result Temporary or explicitly moved operation result.
 * @return Owned payload; no reference into the destroyed temporary escapes.
 * @throws FlowUiException preserving the original error, or the payload's move exception.
 * @par Example
 * @code{.cpp}
 * const Clay_Color color = FlowUi::ValueOrThrow(FlowUi::Flow_Color(std::string_view{"#20242c"}));
 * @endcode
 */
template <typename Value>
	requires (!std::is_void_v<Value>)
[[nodiscard]] Value ValueOrThrow(Result<Value>&& result) {
	if (!result)
		throw FlowUiException(result.error());
	return std::move(*result);
}

/** Copy a const temporary's successful payload into an owned value, or throw its original error. */
template <typename Value>
	requires (!std::is_void_v<Value>)
[[nodiscard]] Value ValueOrThrow(const Result<Value>&& result) {
	if (!result)
		throw FlowUiException(result.error());
	return *result;
}

/**
 * @brief Require a successful no-value operation, otherwise throw its original FlowUiError.
 * @param status Operation completion status. Reporting and recovery remain with the caller.
 * @throws FlowUiException on failure; success returns normally without a payload.
 */
void ValueOrThrow(Status status);

} // namespace FlowUi
