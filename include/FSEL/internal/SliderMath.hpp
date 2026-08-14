#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

namespace FlowUi::FSEL::detail::slider {

struct Bounds {
	double lower = 0.0;
	double upper = 1.0;
};

[[nodiscard]] inline Bounds normalizeBounds(double minimum, double maximum) {
	if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
		return {};
	}
	const auto [lower, upper] = std::minmax(minimum, maximum);
	return {.lower = lower, .upper = upper};
}

[[nodiscard]] inline double clampValue(double value, Bounds bounds) {
	if (!std::isfinite(value)) {
		return bounds.lower;
	}
	return std::clamp(value, bounds.lower, bounds.upper);
}

[[nodiscard]] inline double normalizedRatio(double value, Bounds bounds) {
	const double range = bounds.upper - bounds.lower;
	if (!(range > 0.0) || !std::isfinite(range)) {
		return 0.0;
	}
	return std::clamp(
		(clampValue(value, bounds) - bounds.lower) / range,
		0.0,
		1.0);
}

[[nodiscard]] inline double valueFromRatio(double ratio, Bounds bounds) {
	if (!std::isfinite(ratio)) {
		ratio = 0.0;
	}
	return bounds.lower +
		std::clamp(ratio, 0.0, 1.0) * (bounds.upper - bounds.lower);
}

/** Snap to an interval anchored at the lower bound. Empty/non-positive means continuous. */
[[nodiscard]] inline double snapValue(
	double value,
	Bounds bounds,
	std::optional<double> roundingStep) {
	const double clamped = clampValue(value, bounds);
	if (!roundingStep.has_value() || !std::isfinite(*roundingStep) ||
		*roundingStep <= 0.0 || bounds.upper <= bounds.lower) {
		return clamped;
	}

	if (clamped == bounds.lower || clamped == bounds.upper) {
		return clamped;
	}
	const double snapped = bounds.lower +
		std::round((clamped - bounds.lower) / *roundingStep) * *roundingStep;
	return clampValue(snapped, bounds);
}

[[nodiscard]] inline float clampRoundness(float roundness) {
	if (!std::isfinite(roundness)) {
		return 0.0f;
	}
	return std::clamp(roundness, 0.0f, 1.0f);
}

} // namespace FlowUi::FSEL::detail::slider
