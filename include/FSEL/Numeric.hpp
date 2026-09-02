#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

#include "managers/structs/ActionManagerStructs.hpp"

namespace FlowUi::FSEL {

template<typename T>
concept NumericValueType =
	(std::integral<T> || std::floating_point<T>) &&
	!std::same_as<std::remove_cv_t<T>, bool> &&
	!std::same_as<std::remove_cv_t<T>, char> &&
	!std::same_as<std::remove_cv_t<T>, signed char> &&
	!std::same_as<std::remove_cv_t<T>, unsigned char> &&
	!std::same_as<std::remove_cv_t<T>, wchar_t> &&
	!std::same_as<std::remove_cv_t<T>, char8_t> &&
	!std::same_as<std::remove_cv_t<T>, char16_t> &&
	!std::same_as<std::remove_cv_t<T>, char32_t>;

/** Shared begin/change/commit/cancel contract for numeric controls. */
struct NumericEditActions {
	ActionCall onBegin{};
	ActionCall onChanged{};
	ActionCall onCommit{};
	ActionCall onCancel{};
};

enum class NumericTextSyncPolicy : uint8_t {
	Live = 0,
	OnCommit,
};

enum class NumericTextBoundsPolicy : uint8_t {
	SoftClamp = 0,
	ClampTextImmediately,
};

enum class NumericTextStatus : uint8_t {
	Complete = 0,
	Incomplete,
	BelowMinimum,
	AboveMaximum,
	Overflow,
	ConfigurationError,
};

enum class NumericFloatNotation : uint8_t {
	General = 0,
	Fixed,
	Scientific,
};

/** Locale-independent formatting and floating text grammar policy. */
struct NumericFormatOptions {
	NumericFloatNotation notation = NumericFloatNotation::General;
	std::optional<int> precision = 10;
	bool trimTrailingZeros = true;
	bool canonicalizeNegativeZero = true;
	bool allowScientificInput = false;
};

template<NumericValueType T>
struct NumericEditSession {
	bool active = false;
	bool valueChanged = false;
	bool hasLastValidValue = false;
	T beginValue{};
	T lastValidValue{};
};

namespace detail::numeric {

template<NumericValueType T>
struct Bounds {
	T lower = std::numeric_limits<T>::lowest();
	T upper = std::numeric_limits<T>::max();
	bool valid = true;
};

template<NumericValueType T>
[[nodiscard]] constexpr bool isFinite(T value) noexcept {
	if constexpr (std::floating_point<T>) {
		return std::isfinite(value);
	}
	return true;
}

template<NumericValueType T>
[[nodiscard]] constexpr Bounds<T> resolveBounds(
	std::optional<T> minimum,
	std::optional<T> maximum) noexcept {
	Bounds<T> result{
		.lower = minimum.value_or(std::numeric_limits<T>::lowest()),
		.upper = maximum.value_or(std::numeric_limits<T>::max()),
	};
	result.valid = isFinite(result.lower) && isFinite(result.upper) &&
		result.lower <= result.upper;
	return result;
}

template<NumericValueType T>
[[nodiscard]] constexpr T clampValue(T value, Bounds<T> bounds) noexcept {
	return std::clamp(value, bounds.lower, bounds.upper);
}

template<NumericValueType T>
[[nodiscard]] constexpr bool validStep(T step) noexcept {
	return isFinite(step) && step > T{};
}

template<NumericValueType T>
[[nodiscard]] T stepValue(
	T value,
	T step,
	int direction,
	Bounds<T> bounds) noexcept {
	if (!bounds.valid) {
		return value;
	}
	if (!validStep(step) || direction == 0) {
		return clampValue(value, bounds);
	}

	value = clampValue(value, bounds);
	if (direction > 0) {
		if (value >= bounds.upper) {
			return bounds.upper;
		}
		if constexpr (std::integral<T>) {
			const T nativeMaximum = std::numeric_limits<T>::max();
			const T candidate = value > nativeMaximum - step
				? nativeMaximum
				: static_cast<T>(value + step);
			return std::min(candidate, bounds.upper);
		} else {
			const T candidate = static_cast<T>(value + step);
			return !isFinite(candidate)
				? bounds.upper
				: std::min(candidate, bounds.upper);
		}
	}

	if (value <= bounds.lower) {
		return bounds.lower;
	}
	if constexpr (std::integral<T>) {
		if constexpr (std::unsigned_integral<T>) {
			const T candidate = value < step ? T{} : static_cast<T>(value - step);
			return std::max(candidate, bounds.lower);
		} else {
			const T nativeMinimum = std::numeric_limits<T>::lowest();
			const T candidate = value < nativeMinimum + step
				? nativeMinimum
				: static_cast<T>(value - step);
			return std::max(candidate, bounds.lower);
		}
	} else {
		const T candidate = static_cast<T>(value - step);
		return !isFinite(candidate)
			? bounds.lower
			: std::max(candidate, bounds.lower);
	}
}

/** Apply a signed number of native steps without overflowing intermediate values. */
template<NumericValueType T>
[[nodiscard]] T offsetBySteps(
	T value,
	T step,
	int64_t stepCount,
	Bounds<T> bounds) noexcept {
	if (!bounds.valid || !validStep(step) || stepCount == 0) {
		return bounds.valid ? clampValue(value, bounds) : value;
	}

	const bool increasing = stepCount > 0;
	const uint64_t magnitude = stepCount < 0
		? static_cast<uint64_t>(-(stepCount + 1)) + 1u
		: static_cast<uint64_t>(stepCount);
	value = clampValue(value, bounds);

	if constexpr (std::floating_point<T>) {
		const long double signedMagnitude = increasing
			? static_cast<long double>(magnitude)
			: -static_cast<long double>(magnitude);
		const long double candidate = static_cast<long double>(value) +
			static_cast<long double>(step) * signedMagnitude;
		if (!std::isfinite(candidate)) {
			return increasing ? bounds.upper : bounds.lower;
		}
		return static_cast<T>(std::clamp(
			candidate,
			static_cast<long double>(bounds.lower),
			static_cast<long double>(bounds.upper)));
	} else {
		using Unsigned = std::make_unsigned_t<T>;
		static_assert(sizeof(Unsigned) <= sizeof(uint64_t));
		const Unsigned unsignedStep = static_cast<Unsigned>(step);
		const Unsigned distance = increasing
			? static_cast<Unsigned>(bounds.upper) - static_cast<Unsigned>(value)
			: static_cast<Unsigned>(value) - static_cast<Unsigned>(bounds.lower);
		if (distance == 0u) {
			return increasing ? bounds.upper : bounds.lower;
		}
		const Unsigned requiredSteps = static_cast<Unsigned>(
			distance / unsignedStep +
			(distance % unsignedStep == 0u ? 0u : 1u));
		if (magnitude >= static_cast<uint64_t>(requiredSteps)) {
			return increasing ? bounds.upper : bounds.lower;
		}
		const Unsigned offset = static_cast<Unsigned>(
			unsignedStep * static_cast<Unsigned>(magnitude));

		if constexpr (std::unsigned_integral<T>) {
			return increasing
				? static_cast<T>(value + offset)
				: static_cast<T>(value - offset);
		} else {
			auto fromNegativeMagnitude = [](Unsigned negativeMagnitude) {
				if (negativeMagnitude == 0u) {
					return T{};
				}
				return static_cast<T>(
					-static_cast<T>(negativeMagnitude - 1u) - 1);
			};

			if (increasing) {
				if (value >= T{}) {
					return static_cast<T>(
						value + static_cast<T>(offset));
				}
				const Unsigned negativeMagnitude =
					static_cast<Unsigned>(-(value + 1)) + 1u;
				if (offset < negativeMagnitude) {
					return fromNegativeMagnitude(
						negativeMagnitude - offset);
				}
				return static_cast<T>(offset - negativeMagnitude);
			}

			if (value <= T{}) {
				const Unsigned negativeMagnitude =
					static_cast<Unsigned>(-(value + 1)) + 1u;
				return fromNegativeMagnitude(negativeMagnitude + offset);
			}
			const Unsigned positiveValue = static_cast<Unsigned>(value);
			if (offset <= positiveValue) {
				return static_cast<T>(positiveValue - offset);
			}
			return fromNegativeMagnitude(offset - positiveValue);
		}
	}
}

template<NumericValueType T>
void beginSession(NumericEditSession<T>& session, T value) noexcept {
	if (session.active) {
		return;
	}
	session.active = true;
	session.valueChanged = false;
	session.hasLastValidValue = true;
	session.beginValue = value;
	session.lastValidValue = value;
}

template<NumericValueType T>
void resetSession(NumericEditSession<T>& session) noexcept {
	session = NumericEditSession<T>{};
}

} // namespace detail::numeric
} // namespace FlowUi::FSEL
