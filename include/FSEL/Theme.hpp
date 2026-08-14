#pragma once

#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/App.hpp"

namespace FlowUi::FSEL {

struct FSELBoxTheme {
	Clay_Color backgroundColor = Flow_Color("#00000000");
	Clay_Color borderColor = Flow_Color("#00000000");
	Clay_BorderWidth borderWidth = {0, 0, 0, 0, 0};
	Clay_CornerRadius cornerRadius = {0, 0, 0, 0};
};

struct FSELSplitterHandleTheme {
	Clay_Color backgroundColor = Flow_Color("#00000000");
	Clay_Color hoverColor = Flow_Color("#ffffff18");
	Clay_Color draggingColor = Flow_Color("#ffffff30");
	Clay_Color borderColor = Flow_Color("#00000000");
	Clay_BorderWidth borderWidth = {0, 0, 0, 0, 0};
	Clay_CornerRadius cornerRadius = {0, 0, 0, 0};
	CursorType horizontalResizeCursor = CursorType::ResizeHorizontal;
	CursorType verticalResizeCursor = CursorType::ResizeVertical;
	uint8_t cursorPriority = 100;
};

struct FSELButtonStateTheme {
	Clay_Color backgroundColor = Flow_Color("#00000000");
	Clay_Color labelColor = Flow_Color("#ffffffff");
	Clay_Color iconColor = Flow_Color("#ffffffff");
	Clay_Color borderColor = Flow_Color("#00000000");
};

struct FSELButtonTheme {
	FSELButtonStateTheme idle = {
		.backgroundColor = Flow_Color("#252526ff"),
		.labelColor = Flow_Color("#ffffffff"),
		.iconColor = Flow_Color("#ffffffff"),
		.borderColor = Flow_Color("#454545ff"),
	};
	FSELButtonStateTheme hovered = {
		.backgroundColor = Flow_Color("#3e3e42ff"),
		.labelColor = Flow_Color("#ffffffff"),
		.iconColor = Flow_Color("#ffffffff"),
		.borderColor = Flow_Color("#5a5a5eff"),
	};
	FSELButtonStateTheme pressed = {
		.backgroundColor = Flow_Color("#094771ff"),
		.labelColor = Flow_Color("#ffffffff"),
		.iconColor = Flow_Color("#ffffffff"),
		.borderColor = Flow_Color("#007accff"),
	};
	FSELButtonStateTheme disabled = {
		.backgroundColor = Flow_Color("#252526ff"),
		.labelColor = Flow_Color("#6c6c6cff"),
		.iconColor = Flow_Color("#6c6c6cff"),
		.borderColor = Flow_Color("#3e3e42ff"),
	};

	Clay_Padding padding = CLAY_PADDING_ALL(10);
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	Clay_LayoutDirection layoutDirection = CLAY_LEFT_TO_RIGHT;
	uint16_t contentGap = 8;
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);

	FontFamilyId labelFontFamily = 0;
	uint32_t labelFontWeight = 400;
	FontStyle labelFontStyle = FontStyle::Normal;
	uint16_t labelFontSize = 16;
	Clay_TextElementConfigWrapMode labelWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment labelAlignment = CLAY_TEXT_ALIGN_CENTER;
	float iconSize = 18.0f;

	CursorType cursor = CursorType::PointingHand;
	uint8_t cursorPriority = 10;
};

struct FSELCheckboxStateTheme {
	Clay_Color backgroundColor = Flow_Color("#252526ff");
	Clay_Color borderColor = Flow_Color("#5a5a5eff");
	Clay_Color iconColor = Flow_Color("#ffffffff");
};

struct FSELCheckboxValueTheme {
	FSELCheckboxStateTheme idle{};
	FSELCheckboxStateTheme hovered = {
		.backgroundColor = Flow_Color("#2d2d30ff"),
		.borderColor = Flow_Color("#6a6a6eff"),
	};
	FSELCheckboxStateTheme pressed = {
		.backgroundColor = Flow_Color("#3e3e42ff"),
		.borderColor = Flow_Color("#7a7a7eff"),
	};
	FSELCheckboxStateTheme disabled = {
		.backgroundColor = Flow_Color("#252526ff"),
		.borderColor = Flow_Color("#3e3e42ff"),
		.iconColor = Flow_Color("#6c6c6cff"),
	};
};

struct FSELCheckboxTheme {
	FSELCheckboxValueTheme unchecked{};
	FSELCheckboxValueTheme checked = {
		.idle = {
			.backgroundColor = Flow_Color("#007accff"),
			.borderColor = Flow_Color("#007accff"),
		},
		.hovered = {
			.backgroundColor = Flow_Color("#1c97eaff"),
			.borderColor = Flow_Color("#1c97eaff"),
		},
		.pressed = {
			.backgroundColor = Flow_Color("#005fb8ff"),
			.borderColor = Flow_Color("#005fb8ff"),
		},
		.disabled = {
			.backgroundColor = Flow_Color("#094771ff"),
			.borderColor = Flow_Color("#094771ff"),
			.iconColor = Flow_Color("#6c6c6cff"),
		},
	};

	float size = 20.0f;
	float iconSize = 14.0f;
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);
	CursorType cursor = CursorType::PointingHand;
	uint8_t cursorPriority = 10;
};

struct FSELSwitchStateTheme {
	Clay_Color trackColor = Flow_Color("#3e3e42ff");
	Clay_Color trackBorderColor = Flow_Color("#5a5a5eff");
	Clay_Color knobColor = Flow_Color("#c8c8c8ff");
	Clay_Color knobBorderColor = Flow_Color("#00000000");
};

struct FSELSwitchValueTheme {
	FSELSwitchStateTheme idle{};
	FSELSwitchStateTheme hovered = {
		.trackColor = Flow_Color("#4a4a4eff"),
		.trackBorderColor = Flow_Color("#6a6a6eff"),
		.knobColor = Flow_Color("#ffffffff"),
	};
	FSELSwitchStateTheme pressed = {
		.trackColor = Flow_Color("#2d2d30ff"),
		.trackBorderColor = Flow_Color("#5a5a5eff"),
		.knobColor = Flow_Color("#ffffffff"),
	};
	FSELSwitchStateTheme disabled = {
		.trackColor = Flow_Color("#2d2d30ff"),
		.trackBorderColor = Flow_Color("#3e3e42ff"),
		.knobColor = Flow_Color("#6c6c6cff"),
	};
};

struct FSELSwitchTheme {
	FSELSwitchValueTheme off{};
	FSELSwitchValueTheme on = {
		.idle = {
			.trackColor = Flow_Color("#007accff"),
			.trackBorderColor = Flow_Color("#007accff"),
			.knobColor = Flow_Color("#ffffffff"),
		},
		.hovered = {
			.trackColor = Flow_Color("#1c97eaff"),
			.trackBorderColor = Flow_Color("#1c97eaff"),
			.knobColor = Flow_Color("#ffffffff"),
		},
		.pressed = {
			.trackColor = Flow_Color("#005fb8ff"),
			.trackBorderColor = Flow_Color("#005fb8ff"),
			.knobColor = Flow_Color("#ffffffff"),
		},
		.disabled = {
			.trackColor = Flow_Color("#094771ff"),
			.trackBorderColor = Flow_Color("#094771ff"),
			.knobColor = Flow_Color("#6c6c6cff"),
		},
	};

	float trackWidth = 40.0f;
	float trackHeight = 22.0f;
	uint16_t knobInset = 2;
	float roundness = 1.0f;
	Clay_BorderWidth trackBorderWidth = {1, 1, 1, 1, 0};
	Clay_BorderWidth knobBorderWidth = {0, 0, 0, 0, 0};
	CursorType cursor = CursorType::PointingHand;
	uint8_t cursorPriority = 10;
};

struct FSELSliderStateTheme {
	Clay_Color trackColor = Flow_Color("#3e3e42ff");
	Clay_Color trackBorderColor = Flow_Color("#5a5a5eff");
	Clay_Color fillColor = Flow_Color("#007accff");
	Clay_Color fillBorderColor = Flow_Color("#007accff");
	Clay_Color thumbColor = Flow_Color("#c8c8c8ff");
	Clay_Color thumbBorderColor = Flow_Color("#00000000");
};

struct FSELSliderTheme {
	FSELSliderStateTheme idle{};
	FSELSliderStateTheme hovered = {
		.trackColor = Flow_Color("#4a4a4eff"),
		.trackBorderColor = Flow_Color("#6a6a6eff"),
		.fillColor = Flow_Color("#1c97eaff"),
		.fillBorderColor = Flow_Color("#1c97eaff"),
		.thumbColor = Flow_Color("#ffffffff"),
	};
	FSELSliderStateTheme dragging = {
		.trackColor = Flow_Color("#2d2d30ff"),
		.trackBorderColor = Flow_Color("#5a5a5eff"),
		.fillColor = Flow_Color("#005fb8ff"),
		.fillBorderColor = Flow_Color("#005fb8ff"),
		.thumbColor = Flow_Color("#ffffffff"),
	};
	FSELSliderStateTheme disabled = {
		.trackColor = Flow_Color("#2d2d30ff"),
		.trackBorderColor = Flow_Color("#3e3e42ff"),
		.fillColor = Flow_Color("#094771ff"),
		.fillBorderColor = Flow_Color("#094771ff"),
		.thumbColor = Flow_Color("#6c6c6cff"),
	};

	float length = 140.0f;
	float hitThickness = 20.0f;
	float trackThickness = 6.0f;
	float thumbLength = 12.0f;
	float thumbThickness = 18.0f;
	float trackRoundness = 1.0f;
	float thumbRoundness = 1.0f;
	Clay_BorderWidth trackBorderWidth = {0, 0, 0, 0, 0};
	Clay_BorderWidth fillBorderWidth = {0, 0, 0, 0, 0};
	Clay_BorderWidth thumbBorderWidth = {0, 0, 0, 0, 0};
	CursorType cursor = CursorType::PointingHand;
	CursorType draggingCursor = CursorType::Grabbing;
	uint8_t cursorPriority = 10;
};

struct FSELSelectableSurfaceStateTheme {
	Clay_Color backgroundColor = Flow_Color("#00000000");
	Clay_Color borderColor = Flow_Color("#00000000");
};

struct FSELSelectableSurfaceTheme {
	FSELSelectableSurfaceStateTheme idle{};
	FSELSelectableSurfaceStateTheme hovered = {
		.backgroundColor = Flow_Color("#ffffff12"),
	};
	FSELSelectableSurfaceStateTheme pressed = {
		.backgroundColor = Flow_Color("#ffffff20"),
	};
	FSELSelectableSurfaceStateTheme selected = {
		.backgroundColor = Flow_Color("#094771ff"),
		.borderColor = Flow_Color("#007accff"),
	};
	FSELSelectableSurfaceStateTheme disabled{};
	FSELSelectableSurfaceStateTheme selectedDisabled = {
		.backgroundColor = Flow_Color("#09477180"),
		.borderColor = Flow_Color("#007acc80"),
	};

	Clay_Padding padding = CLAY_PADDING_ALL(8);
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	Clay_LayoutDirection layoutDirection = CLAY_LEFT_TO_RIGHT;
	uint16_t contentGap = 8;
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);
};

struct FSELTheme {
	FSELBoxTheme boxTheme{};
	FSELSplitterHandleTheme splitterHandleTheme{};
	FSELButtonTheme buttonTheme{};
	FSELCheckboxTheme checkboxTheme{};
	FSELSwitchTheme switchTheme{};
	FSELSliderTheme sliderTheme{};
	FSELSelectableSurfaceTheme selectableSurfaceTheme{};
};

} // namespace FlowUi::FSEL
