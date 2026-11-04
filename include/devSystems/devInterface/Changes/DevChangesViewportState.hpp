#pragma once
#include <cstdint>

namespace FlowUi::devSystems::interface_elements::changes_ui {
/** Preserve a hidden panel's requested offset while recreated text layout settles. */
struct ViewportState {
	float requested_offset = 0;
	uint8_t settling_frames = 0;
};
} // namespace FlowUi::devSystems::interface_elements::changes_ui
