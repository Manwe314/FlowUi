#pragma once

#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOWUI_INCLUDE_ICON_MANAGER
#include "FlowUi/Public/managers/IconManager.hpp"
#endif

namespace FlowUi::FSEL::standard_icons {

/** Semantic resource key for the numeric increment icon. */
inline constexpr std::string_view kIncrementKey = "fsel/increment";
/** Semantic resource key for the numeric decrement icon. */
inline constexpr std::string_view kDecrementKey = "fsel/decrement";
/** Semantic resource key for the open combo box disclosure icon. */
inline constexpr std::string_view kComboBoxOpenKey = "fsel/combo-box/open";
/** Semantic resource key for the closed combo box disclosure icon. */
inline constexpr std::string_view kComboBoxClosedKey = "fsel/combo-box/closed";

// Minimal valid placeholders. Applications may replace the SVG artwork while
// retaining these semantic keys.
/** Default increment SVG artwork; applications may replace its semantic resource key. */
inline constexpr std::string_view kIncrementSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M8 3v10M3 8h10" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round"/>
</svg>
)svg";

/** Default decrement SVG artwork. */
inline constexpr std::string_view kDecrementSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M3 8h10" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round"/>
</svg>
)svg";

/** Default open disclosure SVG artwork. */
inline constexpr std::string_view kComboBoxOpenSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M3 10l5-5 5 5" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

/** Default closed disclosure SVG artwork. */
inline constexpr std::string_view kComboBoxClosedSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M3 6l5 5 5-5" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

#if FLOWUI_INCLUDE_ICON_MANAGER
/**
 * Register missing standard icons without replacing application-provided keys.
 * Registration failures are ignored; controls fall back to empty references or
 * their built-in disclosure rendering when artwork is unavailable.
 * @param icons Application icon manager that owns the registered SVG resources.
 */
inline void registerStandardIcons(IconManager& icons) {
	if (!icons.contains(kIncrementKey)) {
		(void)icons.registerSvg(kIncrementKey, kIncrementSvg);
	}
	if (!icons.contains(kDecrementKey)) {
		(void)icons.registerSvg(kDecrementKey, kDecrementSvg);
	}
	if (!icons.contains(kComboBoxOpenKey)) {
		(void)icons.registerSvg(kComboBoxOpenKey, kComboBoxOpenSvg);
	}
	if (!icons.contains(kComboBoxClosedKey)) {
		(void)icons.registerSvg(kComboBoxClosedKey, kComboBoxClosedSvg);
	}
}
#endif

} // namespace FlowUi::FSEL::standard_icons
