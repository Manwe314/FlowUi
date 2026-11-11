#include "FlowUi/Public/defaults/InputDefaults.hpp"

#include <algorithm>

namespace FlowUi::defaults {

InputFieldOverlayStyle input_field_overlay_style(
	const InputManagerConfig& config) noexcept {
	return {
		.caretShape = InputCaretShape::Bar,
		.caretThicknessPx = config.caretWidthPx,
		.caretBlockWidthPx = std::max(config.caretWidthPx, 8.0f),
		.caretHeightOverflowTopPx = config.caretHeightOverflowTopPx,
		.caretHeightOverflowBottomPx = config.caretHeightOverflowBottomPx,
		.caretColor = config.caretColor,
		.selectionBoxColor = config.highlightBoxColor,
		.selectedTextColor = config.highlightedTextColor,
	};
}

} // namespace FlowUi::defaults
