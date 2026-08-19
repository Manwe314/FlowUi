#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "FSEL/Numeric.hpp"

namespace FlowUi::FSEL::detail::numeric_text {

enum class ParseStatus : uint8_t {
	Complete = 0,
	Incomplete,
	Overflow,
	Invalid,
};

template<NumericValueType T>
struct ParseResult {
	ParseStatus status = ParseStatus::Invalid;
	T value{};
	bool negative = false;
};

template<NumericValueType T>
struct ClassifiedValue {
	NumericTextStatus status = NumericTextStatus::Incomplete;
	std::optional<T> candidate = std::nullopt;
};

template<NumericValueType T>
[[nodiscard]] std::string sanitize(
	std::string_view text,
	bool allowScientificInput) {
	std::string result;
	result.reserve(text.size());
	bool hasDecimalPoint = false;
	bool hasExponent = false;
	bool hasMantissaDigit = false;
	bool exponentMayTakeSign = false;

	for (const char character : text) {
		if (character >= '0' && character <= '9') {
			result.push_back(character);
			if (!hasExponent) {
				hasMantissaDigit = true;
			}
			exponentMayTakeSign = false;
			continue;
		}
		if (character == '-' && result.empty() &&
			(std::signed_integral<T> || std::floating_point<T>)) {
			result.push_back(character);
			continue;
		}
		if constexpr (std::floating_point<T>) {
			if (character == '.' && !hasDecimalPoint && !hasExponent) {
				result.push_back(character);
				hasDecimalPoint = true;
				continue;
			}
			if (allowScientificInput && (character == 'e' || character == 'E') &&
				!hasExponent && hasMantissaDigit) {
				result.push_back(character);
				hasExponent = true;
				exponentMayTakeSign = true;
				continue;
			}
			if (allowScientificInput && (character == '+' || character == '-') &&
				exponentMayTakeSign) {
				result.push_back(character);
				exponentMayTakeSign = false;
				continue;
			}
		}
	}
	return result;
}

inline bool isIncompleteFloatingText(std::string_view text) {
	if (text.empty() || text == "-" || text == "." || text == "-.") {
		return true;
	}
	const char last = text.back();
	if (last == 'e' || last == 'E') {
		return true;
	}
	if ((last == '+' || last == '-') && text.size() >= 2u) {
		const char previous = text[text.size() - 2u];
		return previous == 'e' || previous == 'E';
	}
	return false;
}

template<NumericValueType T>
[[nodiscard]] ParseResult<T> parse(std::string_view text) {
	ParseResult<T> result{};
	result.negative = !text.empty() && text.front() == '-';
	if (text.empty() || text == "-" ||
		(std::floating_point<T> && isIncompleteFloatingText(text))) {
		result.status = ParseStatus::Incomplete;
		return result;
	}

	T value{};
	const char* const begin = text.data();
	const char* const end = begin + text.size();
	std::from_chars_result parsed{};
	if constexpr (std::integral<T>) {
		parsed = std::from_chars(begin, end, value, 10);
	} else {
		parsed = std::from_chars(begin, end, value, std::chars_format::general);
	}
	if (parsed.ec == std::errc::result_out_of_range) {
		result.status = ParseStatus::Overflow;
		return result;
	}
	if (parsed.ec != std::errc{} || parsed.ptr != end ||
		!detail::numeric::isFinite(value)) {
		result.status = ParseStatus::Invalid;
		return result;
	}
	result.status = ParseStatus::Complete;
	result.value = value;
	return result;
}

template<NumericValueType T>
[[nodiscard]] ClassifiedValue<T> classify(
	const ParseResult<T>& parsed,
	detail::numeric::Bounds<T> bounds) {
	if (!bounds.valid) {
		return {.status = NumericTextStatus::ConfigurationError};
	}
	switch (parsed.status) {
	case ParseStatus::Incomplete:
	case ParseStatus::Invalid:
		return {.status = NumericTextStatus::Incomplete};
	case ParseStatus::Overflow:
		return {
			.status = NumericTextStatus::Overflow,
			.candidate = parsed.negative ? bounds.lower : bounds.upper,
		};
	case ParseStatus::Complete:
		break;
	}
	if (parsed.value < bounds.lower) {
		return {
			.status = NumericTextStatus::BelowMinimum,
			.candidate = bounds.lower,
		};
	}
	if (parsed.value > bounds.upper) {
		return {
			.status = NumericTextStatus::AboveMaximum,
			.candidate = bounds.upper,
		};
	}
	return {
		.status = NumericTextStatus::Complete,
		.candidate = parsed.value,
	};
}

struct FormattedValue {
	std::array<char, 128> bytes{};
	size_t size = 0;

	[[nodiscard]] std::string_view view() const noexcept {
		return std::string_view(bytes.data(), size);
	}
};

inline std::chars_format toCharsFormat(NumericFloatNotation notation) {
	switch (notation) {
	case NumericFloatNotation::Fixed:
		return std::chars_format::fixed;
	case NumericFloatNotation::Scientific:
		return std::chars_format::scientific;
	case NumericFloatNotation::General:
	default:
		return std::chars_format::general;
	}
}

inline void trimTrailingZeros(FormattedValue& formatted) {
	const std::string_view text = formatted.view();
	const size_t exponent = text.find_first_of("eE");
	const size_t mantissaEnd = exponent == std::string_view::npos
		? text.size()
		: exponent;
	const size_t decimal = text.substr(0, mantissaEnd).find('.');
	if (decimal == std::string_view::npos) {
		return;
	}

	size_t trimmedEnd = mantissaEnd;
	while (trimmedEnd > decimal + 1u &&
		formatted.bytes[trimmedEnd - 1u] == '0') {
		--trimmedEnd;
	}
	if (trimmedEnd == decimal + 1u) {
		trimmedEnd = decimal;
	}
	if (trimmedEnd == mantissaEnd) {
		return;
	}
	if (exponent != std::string_view::npos) {
		const size_t exponentBytes = formatted.size - exponent;
		std::memmove(
			formatted.bytes.data() + trimmedEnd,
			formatted.bytes.data() + exponent,
			exponentBytes);
		formatted.size = trimmedEnd + exponentBytes;
	} else {
		formatted.size = trimmedEnd;
	}
}

template<NumericValueType T>
[[nodiscard]] FormattedValue format(
	T value,
	const NumericFormatOptions& options = {}) {
	FormattedValue result{};
	if constexpr (std::floating_point<T>) {
		if (options.canonicalizeNegativeZero && value == T{}) {
			value = T{};
		}
	}

	std::to_chars_result formatted{};
	if constexpr (std::integral<T>) {
		formatted = std::to_chars(
			result.bytes.data(),
			result.bytes.data() + result.bytes.size(),
			value,
			10);
	} else {
		const std::chars_format notation = toCharsFormat(options.notation);
		if (options.precision.has_value() && *options.precision >= 0) {
			formatted = std::to_chars(
				result.bytes.data(),
				result.bytes.data() + result.bytes.size(),
				value,
				notation,
				*options.precision);
		} else {
			formatted = std::to_chars(
				result.bytes.data(),
				result.bytes.data() + result.bytes.size(),
				value,
				notation);
		}
	}
	if (formatted.ec != std::errc{}) {
		return result;
	}
	result.size = static_cast<size_t>(formatted.ptr - result.bytes.data());
	if constexpr (std::floating_point<T>) {
		if (options.trimTrailingZeros) {
			trimTrailingZeros(result);
		}
	}
	return result;
}

} // namespace FlowUi::FSEL::detail::numeric_text
