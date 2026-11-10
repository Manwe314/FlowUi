#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include <clay.h>

namespace FlowUi::FSEL::detail::combo_box {

/* Space reserved by the trigger or an option row after fitting its decoration. */
struct ContentLayout {
	Clay_Padding padding{};
	float icon_size = 0.0f;
	float content_height = 0.0f;
	float label_width = 0.0f;
	uint16_t gap = 0;
	bool show_leading_icon = false;
};

/* Fit padding, icons and gaps without stealing the disclosure's reserved space. */
[[nodiscard]] inline ContentLayout fit_content(float width, float height, Clay_Padding padding,
											   float requested_icon_size, uint16_t requested_gap,
											   bool leading_icon, bool disclosure,
											   float text_height = 0.0f) noexcept {
	width = std::isfinite(width) ? std::max(width, 0.0f) : std::numeric_limits<float>::max();
	height = std::isfinite(height) ? std::max(height, 0.0f) : std::numeric_limits<float>::max();
	const auto fit_pair = [](uint16_t& leading, uint16_t& trailing, float extent) {
		const float total = static_cast<float>(leading) + trailing;
		if (total > extent && total > 0.0f) {
			leading = static_cast<uint16_t>(extent * static_cast<float>(leading) / total);
			trailing = static_cast<uint16_t>(std::max(0.0f, extent - leading));
		}
	};
	fit_pair(padding.left, padding.right, width);
	fit_pair(padding.top, padding.bottom,
			 std::max(0.0f, height - std::clamp(text_height, 0.0f, height)));
	const float content_width = std::max(0.0f, width - padding.left - padding.right);
	const float content_height = std::max(0.0f, height - padding.top - padding.bottom);
	const float icon_size =
		std::clamp(std::isfinite(requested_icon_size) ? requested_icon_size : 0.0f, 0.0f,
				   std::min(content_height, content_width));
	const bool show_leading =
		leading_icon && icon_size > 0.0f &&
		(!disclosure || content_width >= 2.0f * icon_size + 2.0f * requested_gap + 8.0f);
	const unsigned icon_count = static_cast<unsigned>(disclosure) + show_leading;
	const float gap_budget =
		icon_count ? std::max(0.0f, content_width - icon_count * icon_size) / icon_count : 0.0f;
	const auto gap = static_cast<uint16_t>(std::min<float>(requested_gap, gap_budget));
	return {.padding = padding,
			.icon_size = icon_size,
			.content_height = content_height,
			.label_width = std::max(0.0f, content_width - icon_count * (icon_size + gap)),
			.gap = gap,
			.show_leading_icon = show_leading};
}

/* Resolve explicit dimensions before using retained layout data for grow/percent controls. */
[[nodiscard]] inline float resolved_extent(Clay_SizingAxis sizing, float retained,
										   float fallback) noexcept {
	if (sizing.type == CLAY__SIZING_TYPE_FIXED)
		return sizing.size.minMax.min;
	return retained > 0.0f ? retained : fallback;
}

} // namespace FlowUi::FSEL::detail::combo_box
