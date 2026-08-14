#pragma once

#include <cstdint>
#include <optional>

#include "FSEL/Theme.hpp"

namespace FlowUi::FSEL {

struct SelectableSurfaceStateOverrides {
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	std::optional<Clay_Color> borderColor = std::nullopt;
};

/** Layout and appearance shared by selectable surface based elements. */
struct SelectableSurfaceStyle {
	Clay_Sizing sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};

	std::optional<Clay_Padding> padding = std::nullopt;
	std::optional<Clay_ChildAlignment> childAlignment = std::nullopt;
	std::optional<Clay_LayoutDirection> layoutDirection = std::nullopt;
	std::optional<uint16_t> contentGap = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;

	SelectableSurfaceStateOverrides idleOverrides{};
	SelectableSurfaceStateOverrides hoveredOverrides{};
	SelectableSurfaceStateOverrides pressedOverrides{};
	SelectableSurfaceStateOverrides selectedOverrides{};
	SelectableSurfaceStateOverrides disabledOverrides{};
	SelectableSurfaceStateOverrides selectedDisabledOverrides{};
};

} // namespace FlowUi::FSEL
