#pragma once

#include <algorithm>
#include <cmath>

namespace FlowUi::FSEL::detail::scroll_indicator {

struct ThumbGeometry {
	bool visible = false;
	float maximumScroll = 0.0f;
	float extent = 0.0f;
	float offset = 0.0f;
};

/** Pure axis math shared by scrollable FSEL surfaces and indicators. */
[[nodiscard]] inline ThumbGeometry calculate(
	float viewportExtent,
	float contentExtent,
	float scrollOffset,
	float trackExtent,
	float minimumThumbExtent) noexcept {
	if (!std::isfinite(viewportExtent) || !std::isfinite(contentExtent) ||
		!std::isfinite(scrollOffset) || !std::isfinite(trackExtent) ||
		!std::isfinite(minimumThumbExtent) || viewportExtent <= 0.0f ||
		contentExtent <= viewportExtent || trackExtent <= 0.0f) {
		return {};
	}

	ThumbGeometry result{};
	result.visible = true;
	result.maximumScroll = contentExtent - viewportExtent;
	result.extent = std::clamp(
		trackExtent * viewportExtent / contentExtent,
		std::clamp(minimumThumbExtent, 0.0f, trackExtent),
		trackExtent);
	const float normalizedScroll = std::clamp(
		-scrollOffset / result.maximumScroll,
		0.0f,
		1.0f);
	result.offset = normalizedScroll * (trackExtent - result.extent);
	return result;
}

} // namespace FlowUi::FSEL::detail::scroll_indicator
