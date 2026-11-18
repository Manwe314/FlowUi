#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "FlowUi/Private/resourceService/iconRasterProvider/IconProviderTypes.hpp"

namespace FlowUi::detail::icon {

/** Checked core extent; invalid output cannot be reserved in an atlas. */
struct IconRasterExtent {
	uint32_t width = 0;
	uint32_t height = 0;
	bool valid = false;
};

/** Add padding without overflowing the atlas's uint32_t coordinate vocabulary. */
[[nodiscard]] constexpr IconRasterExtent paddedIconExtent(uint32_t width, uint32_t height,
														  uint32_t padding) noexcept {
	if (width == 0 || height == 0)
		return {};
	const uint64_t paddedWidth = static_cast<uint64_t>(width) + static_cast<uint64_t>(padding) * 2u;
	const uint64_t paddedHeight =
		static_cast<uint64_t>(height) + static_cast<uint64_t>(padding) * 2u;
	if (paddedWidth > std::numeric_limits<uint32_t>::max() ||
		paddedHeight > std::numeric_limits<uint32_t>::max())
		return {};
	return {static_cast<uint32_t>(paddedWidth), static_cast<uint32_t>(paddedHeight), true};
}

/** Preserve the core contain-fit and llround rule independently of provider raster work. */
[[nodiscard]] inline IconRasterExtent fitIconRasterExtent(uint32_t requestedWidth,
														  uint32_t requestedHeight,
														  IconDimensions intrinsic,
														  IconDimensions content) noexcept {
	const uint32_t targetWidth = std::max(1u, requestedWidth);
	const uint32_t targetHeight = std::max(1u, requestedHeight);
	const auto finiteDimensions = [](IconDimensions dimensions) noexcept {
		return std::isfinite(dimensions.width) && std::isfinite(dimensions.height) &&
			   dimensions.width >= 0.0f && dimensions.height >= 0.0f;
	};
	if (!finiteDimensions(intrinsic))
		return {};
	if (!(intrinsic.width > 0.0f && intrinsic.height > 0.0f)) {
		if (!finiteDimensions(content))
			return {};
		intrinsic = content;
	}
	const bool hasDimensions = intrinsic.width > 0.0f && intrinsic.height > 0.0f;
	const double sourceWidth = hasDimensions ? static_cast<double>(intrinsic.width) : targetWidth;
	const double sourceHeight =
		hasDimensions ? static_cast<double>(intrinsic.height) : targetHeight;
	const double scale = std::min(static_cast<double>(targetWidth) / sourceWidth,
								  static_cast<double>(targetHeight) / sourceHeight);
	const double fittedWidth = sourceWidth * scale;
	const double fittedHeight = sourceHeight * scale;
	if (!std::isfinite(scale) || scale <= 0.0 || !std::isfinite(fittedWidth) ||
		!std::isfinite(fittedHeight) || fittedWidth < 0.0 || fittedHeight < 0.0 ||
		fittedWidth > std::numeric_limits<uint32_t>::max() ||
		fittedHeight > std::numeric_limits<uint32_t>::max())
		return {};
	return {std::clamp(static_cast<uint32_t>(std::llround(fittedWidth)), 1u, targetWidth),
			std::clamp(static_cast<uint32_t>(std::llround(fittedHeight)), 1u, targetHeight), true};
}

static_assert(paddedIconExtent(12, 6, 1).valid && paddedIconExtent(12, 6, 1).width == 14);
static_assert(!paddedIconExtent(UINT32_MAX, 1, 1).valid);

} // namespace FlowUi::detail::icon
