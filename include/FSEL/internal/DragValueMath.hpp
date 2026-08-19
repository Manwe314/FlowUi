#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace FlowUi::FSEL::detail::drag_value {

[[nodiscard]] inline bool validPixelsPerStep(float pixelsPerStep) noexcept {
	return std::isfinite(pixelsPerStep) && pixelsPerStep > 0.0f;
}

[[nodiscard]] inline bool crossedThreshold(
	float horizontalDelta,
	float threshold) noexcept {
	if (!std::isfinite(horizontalDelta) || !std::isfinite(threshold)) {
		return false;
	}
	return std::abs(horizontalDelta) >= std::max(threshold, 0.0f);
}

[[nodiscard]] inline int64_t stepCountForDelta(
	float horizontalDelta,
	float pixelsPerStep) noexcept {
	if (!std::isfinite(horizontalDelta) ||
		!validPixelsPerStep(pixelsPerStep)) {
		return 0;
	}
	const double steps = static_cast<double>(horizontalDelta) /
		static_cast<double>(pixelsPerStep);
	constexpr double minimum = static_cast<double>(
		std::numeric_limits<int64_t>::lowest());
	constexpr double maximum = static_cast<double>(
		std::numeric_limits<int64_t>::max());
	if (steps <= minimum) {
		return std::numeric_limits<int64_t>::lowest();
	}
	if (steps >= maximum) {
		return std::numeric_limits<int64_t>::max();
	}
	return static_cast<int64_t>(steps);
}

} // namespace FlowUi::FSEL::detail::drag_value
