#include "FlowUi/Public/defaults/FselIcons.hpp"

#if FLOWUI_INCLUDE_ICON_MANAGER
namespace FlowUi::FSEL::standard_icons {

void registerStandardIcons(IconManager& icons) {
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

} // namespace FlowUi::FSEL::standard_icons
#endif
