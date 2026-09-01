#pragma once

#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/App.hpp"
#include "managers/structs/InputFieldManagerStructs.hpp"

namespace FlowUi::FSEL {

struct FSELBoxTheme {
	Clay_Color backgroundColor = Flow_Color("#00000000");
	Clay_Color borderColor = Flow_Color("#00000000");
	Clay_BorderWidth borderWidth = {0, 0, 0, 0, 0};
	Clay_CornerRadius cornerRadius = {0, 0, 0, 0};
};

struct FSELPopupSurfaceTheme {
	Clay_Color backgroundColor = Flow_Color("#252526ff");
	Clay_Color borderColor = Flow_Color("#454545ff");
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_Padding padding = CLAY_PADDING_ALL(10);
	uint16_t childGap = 6;
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_TOP,
	};
	Clay_LayoutDirection layoutDirection = CLAY_TOP_TO_BOTTOM;
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
		.backgroundColor = Flow_Color("#252526b8"),
		.labelColor = Flow_Color("#6c6c6cb8"),
		.iconColor = Flow_Color("#6c6c6cb8"),
		.borderColor = Flow_Color("#3e3e42b8"),
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
		.backgroundColor = Flow_Color("#252526b8"),
		.borderColor = Flow_Color("#3e3e42b8"),
		.iconColor = Flow_Color("#6c6c6cb8"),
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
			.backgroundColor = Flow_Color("#094771b8"),
			.borderColor = Flow_Color("#094771b8"),
			.iconColor = Flow_Color("#6c6c6cb8"),
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
		.trackColor = Flow_Color("#2d2d30b8"),
		.trackBorderColor = Flow_Color("#3e3e42b8"),
		.knobColor = Flow_Color("#6c6c6cb8"),
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
			.trackColor = Flow_Color("#094771b8"),
			.trackBorderColor = Flow_Color("#094771b8"),
			.knobColor = Flow_Color("#6c6c6cb8"),
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
		.trackColor = Flow_Color("#2d2d30b8"),
		.trackBorderColor = Flow_Color("#3e3e42b8"),
		.fillColor = Flow_Color("#094771b8"),
		.fillBorderColor = Flow_Color("#094771b8"),
		.thumbColor = Flow_Color("#6c6c6cb8"),
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

struct FSELProgressBarTheme {
	Clay_Color baseColor = Flow_Color("#2d2d30ff");
	Clay_Color borderColor = Flow_Color("#454545ff");
	Clay_Color fillColor = Flow_Color("#007accff");
	float length = 160.0f;
	float thickness = 12.0f;
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
};

struct FSELLabelTheme {
	Clay_Color textColor = Flow_Color("#ffffffff");
	FontFamilyId fontFamily = 0;
	uint32_t fontWeight = 400;
	FontStyle fontStyle = FontStyle::Normal;
	uint16_t fontSize = 14;
	uint16_t letterSpacing = 0;
	Clay_TextElementConfigWrapMode wrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment textAlignment = CLAY_TEXT_ALIGN_LEFT;
};

struct FSELTextFieldStateTheme {
	Clay_Color backgroundColor = Flow_Color("#1e1e1eff");
	Clay_Color textColor = Flow_Color("#f0f0f0ff");
	Clay_Color placeholderColor = Flow_Color("#858585ff");
	Clay_Color borderColor = Flow_Color("#454545ff");
};

struct FSELTextFieldTheme {
	FSELTextFieldStateTheme idle{};
	FSELTextFieldStateTheme hovered = {
		.backgroundColor = Flow_Color("#1e1e1eff"),
		.textColor = Flow_Color("#ffffffff"),
		.placeholderColor = Flow_Color("#909090ff"),
		.borderColor = Flow_Color("#666666ff"),
	};
	FSELTextFieldStateTheme focused = {
		.backgroundColor = Flow_Color("#1e1e1eff"),
		.textColor = Flow_Color("#ffffffff"),
		.placeholderColor = Flow_Color("#858585ff"),
		.borderColor = Flow_Color("#007accff"),
	};
	FSELTextFieldStateTheme readOnly = {
		.backgroundColor = Flow_Color("#252526ff"),
		.textColor = Flow_Color("#c8c8c8ff"),
		.placeholderColor = Flow_Color("#777777ff"),
		.borderColor = Flow_Color("#454545ff"),
	};
	FSELTextFieldStateTheme invalid = {
		.backgroundColor = Flow_Color("#1e1e1eff"),
		.textColor = Flow_Color("#ffffffff"),
		.placeholderColor = Flow_Color("#858585ff"),
		.borderColor = Flow_Color("#f14c4cff"),
	};
	FSELTextFieldStateTheme disabled = {
		.backgroundColor = Flow_Color("#252526b8"),
		.textColor = Flow_Color("#6c6c6cb8"),
		.placeholderColor = Flow_Color("#5f5f5fb8"),
		.borderColor = Flow_Color("#3e3e42b8"),
	};

	float width = 240.0f;
	float height = 34.0f;
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);

	FontFamilyId fontFamily = 0;
	uint32_t fontWeight = 400;
	FontStyle fontStyle = FontStyle::Normal;
	uint16_t fontSize = 14;
	uint16_t letterSpacing = 0;
	uint8_t tabWidth = 4;

	InputFieldOverlayStyle overlayStyle{};
	CursorType cursor = CursorType::IBeam;
	uint8_t cursorPriority = 20;
};

struct FSELNumberInputStepButtonStateTheme {
	Clay_Color backgroundColor = Flow_Color("#00000000");
	Clay_Color foregroundColor = Flow_Color("#c8c8c8ff");
};

struct FSELNumberInputTheme {
	FSELTextFieldTheme field{};
	FSELNumberInputStepButtonStateTheme stepIdle{};
	FSELNumberInputStepButtonStateTheme stepHovered = {
		.backgroundColor = Flow_Color("#ffffff12"),
		.foregroundColor = Flow_Color("#ffffffff"),
	};
	FSELNumberInputStepButtonStateTheme stepPressed = {
		.backgroundColor = Flow_Color("#007accff"),
		.foregroundColor = Flow_Color("#ffffffff"),
	};
	FSELNumberInputStepButtonStateTheme stepDisabled = {
		.backgroundColor = Flow_Color("#00000000"),
		.foregroundColor = Flow_Color("#6c6c6cb8"),
	};
	Clay_Color stepSeparatorColor = Flow_Color("#454545ff");
	float stepButtonWidth = 22.0f;
	float stepIconSize = 10.0f;
	uint16_t stepTextSize = 12;
	CursorType stepCursor = CursorType::PointingHand;
	uint8_t stepCursorPriority = 30;
};

struct FSELDragValueTheme {
	FSELTextFieldTheme field = [] {
		FSELTextFieldTheme theme{};
		theme.width = 160.0f;
		return theme;
	}();
	FSELTextFieldStateTheme dragging = {
		.backgroundColor = Flow_Color("#094771ff"),
		.textColor = Flow_Color("#ffffffff"),
		.placeholderColor = Flow_Color("#858585ff"),
		.borderColor = Flow_Color("#007accff"),
	};
	CursorType dragCursor = CursorType::ResizeHorizontal;
	CursorType editCursor = CursorType::IBeam;
	uint8_t cursorPriority = 30;
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
		.backgroundColor = Flow_Color("#09477168"),
		.borderColor = Flow_Color("#007acc68"),
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

struct FSELComboBoxStateTheme {
	Clay_Color backgroundColor = Flow_Color("#252526ff");
	Clay_Color textColor = Flow_Color("#ffffffff");
	Clay_Color iconColor = Flow_Color("#ffffffff");
	Clay_Color borderColor = Flow_Color("#454545ff");
};

struct FSELComboBoxOptionStateTheme {
	Clay_Color backgroundColor = Flow_Color("#00000000");
	Clay_Color textColor = Flow_Color("#ffffffff");
	Clay_Color iconColor = Flow_Color("#ffffffff");
};

struct FSELComboBoxTheme {
	FSELComboBoxStateTheme idle{};
	FSELComboBoxStateTheme hovered = {
		.backgroundColor = Flow_Color("#3e3e42ff"),
		.borderColor = Flow_Color("#5a5a5eff"),
	};
	FSELComboBoxStateTheme open = {
		.backgroundColor = Flow_Color("#094771ff"),
		.borderColor = Flow_Color("#007accff"),
	};
	FSELComboBoxStateTheme disabled = {
		.backgroundColor = Flow_Color("#252526b8"),
		.textColor = Flow_Color("#6c6c6cb8"),
		.iconColor = Flow_Color("#6c6c6cb8"),
		.borderColor = Flow_Color("#3e3e42b8"),
	};
	FSELComboBoxOptionStateTheme optionIdle{};
	FSELComboBoxOptionStateTheme optionHovered = {
		.backgroundColor = Flow_Color("#3e3e42ff"),
	};
	FSELComboBoxOptionStateTheme optionPressed = {
		.backgroundColor = Flow_Color("#094771ff"),
	};
	FSELComboBoxOptionStateTheme optionSelected = {
		.backgroundColor = Flow_Color("#094771a0"),
	};
	FSELComboBoxOptionStateTheme optionDisabled = {
		.textColor = Flow_Color("#6c6c6cb8"),
		.iconColor = Flow_Color("#6c6c6cb8"),
	};
	Clay_Color placeholderColor = Flow_Color("#858585ff");
	Clay_Color scrollTrackColor = Flow_Color("#ffffff12");
	Clay_Color scrollThumbColor = Flow_Color("#858585ff");
	Clay_Padding triggerPadding = {10, 8, 6, 6};
	Clay_Padding optionPadding = {8, 8, 4, 4};
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	float width = 240.0f;
	float height = 34.0f;
	float popupMaxHeight = 240.0f;
	float optionHeight = 32.0f;
	float iconSize = 16.0f;
	float scrollTrackWidth = 6.0f;
	float scrollThumbMinimum = 22.0f;
	uint16_t contentGap = 8;
	uint16_t optionGap = 2;
	uint16_t popupGap = 4;
	uint16_t scrollGap = 6;
	FontFamilyId fontFamily = 0;
	uint32_t fontWeight = 400;
	FontStyle fontStyle = FontStyle::Normal;
	uint16_t fontSize = 16;
	CursorType cursor = CursorType::PointingHand;
	uint8_t cursorPriority = 10;
};

struct FSELTheme {
	FSELBoxTheme boxTheme{};
	FSELSplitterHandleTheme splitterHandleTheme{};
	FSELButtonTheme buttonTheme{};
	FSELCheckboxTheme checkboxTheme{};
	FSELSwitchTheme switchTheme{};
	FSELSliderTheme sliderTheme{};
	FSELProgressBarTheme progressBarTheme{};
	FSELLabelTheme labelTheme{};
	FSELTextFieldTheme textInputTheme{};
	FSELNumberInputTheme numberInputTheme{};
	FSELDragValueTheme dragValueTheme{};
	FSELTextFieldTheme textAreaTheme = [] {
		FSELTextFieldTheme theme{};
		theme.width = 480.0f;
		theme.height = 220.0f;
		theme.padding = CLAY_PADDING_ALL(10);
		return theme;
	}();
	FSELSelectableSurfaceTheme selectableSurfaceTheme{};
	FSELPopupSurfaceTheme popupSurfaceTheme{};
	FSELComboBoxTheme comboBoxTheme{};
};

} // namespace FlowUi::FSEL
