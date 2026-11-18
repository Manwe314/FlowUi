#pragma once

#include <cstdint>

#include "FlowUi/Private/resourceService/providerTypes/Rgba8Image.hpp"

namespace FlowUi::detail::icon {

/** Positive authored or content dimensions; zero represents unavailable dimensions. */
struct IconDimensions {
	float width = 0.0f;
	float height = 0.0f;
};

/** Exact fitted raster dimensions, independent of core cache bucketing and atlas placement. */
struct IconRasterRequest {
	uint32_t width = 0;
	uint32_t height = 0;
};

} // namespace FlowUi::detail::icon
