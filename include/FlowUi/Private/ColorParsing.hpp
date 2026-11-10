#pragma once

#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "clay.h"

namespace FlowUi {

namespace detail {

constexpr Clay_Color decodeFlowColor(std::string_view hexRgba) {
	if ((hexRgba.size() != 7 && hexRgba.size() != 9) || hexRgba[0] != '#') {
		throw std::invalid_argument("Flow_Color expects #RRGGBB or #RRGGBBAA.");
	}

	const auto decodeHexNibble = [](char c) -> uint8_t {
		if (c >= '0' && c <= '9')
			return static_cast<uint8_t>(c - '0');
		if (c >= 'a' && c <= 'f')
			return static_cast<uint8_t>(10 + (c - 'a'));
		if (c >= 'A' && c <= 'F')
			return static_cast<uint8_t>(10 + (c - 'A'));
		throw std::invalid_argument("Flow_Color received invalid hex digit.");
	};

	const auto decodeHexByte = [&](std::size_t index) -> float {
		const uint8_t hi = decodeHexNibble(hexRgba[index]);
		const uint8_t lo = decodeHexNibble(hexRgba[index + 1]);
		return static_cast<float>((hi << 4) | lo);
	};

	return Clay_Color{
		decodeHexByte(1),
		decodeHexByte(3),
		decodeHexByte(5),
		hexRgba.size() == 9 ? decodeHexByte(7) : 255.0f,
	};
}

} // namespace detail

} // namespace FlowUi
