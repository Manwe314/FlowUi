#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOWUI_INCLUDE_ICON_MANAGER
#include "managers/IconManager.hpp"
#endif

namespace FlowUi::FSEL::standard_icons {

inline constexpr std::string_view kIncrementKey = "fsel/increment";
inline constexpr std::string_view kDecrementKey = "fsel/decrement";

// Minimal valid placeholders. Applications may replace the SVG artwork while
// retaining these semantic keys.
inline constexpr std::string_view kIncrementSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M8 3v10M3 8h10" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round"/>
</svg>
)svg";

inline constexpr std::string_view kDecrementSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M3 8h10" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round"/>
</svg>
)svg";

#if FLOWUI_INCLUDE_ICON_MANAGER
inline void registerStandardIcons(IconManager& icons) {
	if (!icons.contains(kIncrementKey)) {
		(void)icons.registerSvg(kIncrementKey, kIncrementSvg);
	}
	if (!icons.contains(kDecrementKey)) {
		(void)icons.registerSvg(kDecrementKey, kDecrementSvg);
	}
}
#endif

} // namespace FlowUi::FSEL::standard_icons
