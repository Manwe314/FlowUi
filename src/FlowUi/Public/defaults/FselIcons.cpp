#include "FlowUi/Public/defaults/FselIcons.hpp"

#if FLOWUI_INCLUDE_ICON_MANAGER
namespace FlowUi::FSEL::standard_icons {

void registerStandardIcons(IconManager& icons) noexcept {
	const auto registerMissingIcon = [&icons](std::string_view iconKey, std::string_view svgSource) {
		const auto containsIcon = icons.contains(iconKey);
		if (containsIcon && !*containsIcon)
			(void)icons.registerSvg(iconKey, svgSource);
	};
	registerMissingIcon(kIncrementKey, kIncrementSvg);
	registerMissingIcon(kDecrementKey, kDecrementSvg);
	registerMissingIcon(kComboBoxOpenKey, kComboBoxOpenSvg);
	registerMissingIcon(kComboBoxClosedKey, kComboBoxClosedSvg);
}

} // namespace FlowUi::FSEL::standard_icons
#endif
