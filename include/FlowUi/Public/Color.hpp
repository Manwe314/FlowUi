#pragma once

#include <cstddef>
#include <string_view>

#include "FlowUi/Private/ColorParsing.hpp"
#include "clay.h"

namespace FlowUi {

/**
 * @brief Convert a literal \#RRGGBB or \#RRGGBBAA color into a Clay_Color at compile time.
 *
 * Example:
 * @code{.cpp}
 * Clay_Color color = FlowUi::Flow_Color("\#fff3e8ff");
 * Clay_Color opaque = FlowUi::Flow_Color("\#fff3e8");
 * @endcode
 *
 * Invalid literals are rejected during compilation. Six-digit RGB input receives
 * an implicit `ff` alpha channel.
 *
 * @param hexRgba String literal containing a \#RRGGBB or \#RRGGBBAA hex color code.
 * @return Clay_Color decoded from the hex color input.
 */
template <std::size_t Size>
[[nodiscard]] consteval Clay_Color Flow_Color(const char (&hexRgba)[Size]) {
	return detail::decodeFlowColor(std::string_view{hexRgba, Size - 1u});
}

/**
 * @brief Convert a runtime \#RRGGBB or \#RRGGBBAA string into a Clay_Color.
 *
 * @param hexRgba Borrowed hex input, consumed synchronously.
 * @return RGBA color with components in Clay's 0..255 range.
 * @throws std::invalid_argument if the string is not valid \#RRGGBB or \#RRGGBBAA.
 */
[[nodiscard]] Clay_Color Flow_Color(std::string_view hexRgba);

} // namespace FlowUi
