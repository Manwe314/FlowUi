#pragma once

#include <cmath>

#include "FSEL/internal/BoundedValueMath.hpp"

namespace FlowUi::FSEL::detail::slider {

using bounded_value::Bounds;
using bounded_value::clampValue;
using bounded_value::normalizeBounds;
using bounded_value::normalizedRatio;
using bounded_value::snapValue;
using bounded_value::valueFromRatio;

[[nodiscard]] inline float clampRoundness(float roundness) {
	if (!std::isfinite(roundness)) {
		return 0.0f;
	}
	return std::clamp(roundness, 0.0f, 1.0f);
}

} // namespace FlowUi::FSEL::detail::slider
