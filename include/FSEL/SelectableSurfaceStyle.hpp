#pragma once

#include <cstdint>
#include <optional>

#include "FSEL/Theme.hpp"

namespace FlowUi::FSEL {

/** Optional appearance overrides for one interaction state; empty values inherit the active theme. */
struct SelectableSurfaceStateOverrides {
	/** Surface background color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	/** Border color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> borderColor = std::nullopt;
};

/** Layout and appearance shared by selectable surface based elements. */
struct SelectableSurfaceStyle {
	/** Clay width and height sizing rules for the root element. */
	Clay_Sizing sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};

	/**
	 * Inset between the root border and its children, in layout pixels. Empty inherits the active
	 * theme.
	 */
	std::optional<Clay_Padding> padding = std::nullopt;
	/** Alignment of children inside the available content area. Empty inherits the active theme. */
	std::optional<Clay_ChildAlignment> childAlignment = std::nullopt;
	/** Axis and order used to arrange children. Empty inherits the active theme. */
	std::optional<Clay_LayoutDirection> layoutDirection = std::nullopt;
	/** Space between adjacent content items, in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> contentGap = std::nullopt;
	/** Per-edge border widths in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	/** Corner radii in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;

	/** Appearance when enabled and idle. Empty colors inherit the theme. */
	SelectableSurfaceStateOverrides idleOverrides{};
	/** Appearance while hovered. Empty colors inherit the theme. */
	SelectableSurfaceStateOverrides hoveredOverrides{};
	/** Appearance during an armed pointer press. Empty colors inherit the theme. */
	SelectableSurfaceStateOverrides pressedOverrides{};
	/** Appearance when selected. Empty colors inherit the theme. */
	SelectableSurfaceStateOverrides selectedOverrides{};
	/** Appearance when interaction is disabled. Empty colors inherit the theme. */
	SelectableSurfaceStateOverrides disabledOverrides{};
	/** Appearance when selected and disabled. Empty colors inherit the theme. */
	SelectableSurfaceStateOverrides selectedDisabledOverrides{};
};

} // namespace FlowUi::FSEL
