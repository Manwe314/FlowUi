#pragma once

#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/App.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/schema/DevSchemaDescriptor.hpp"
#endif
#include "managers/structs/InputFieldManagerStructs.hpp"

namespace FlowUi::FSEL {

struct FSELBoxTheme {
	Clay_Color backgroundColor = Flow_Color("#00000000");
	Clay_Color borderColor = Flow_Color("#00000000");
	Clay_BorderWidth borderWidth = {0, 0, 0, 0, 0};
	Clay_CornerRadius cornerRadius = {0, 0, 0, 0};
};

struct FSELPopupSurfaceTheme {
	Clay_LayoutDirection layoutDirection = CLAY_TOP_TO_BOTTOM;
	Clay_Color backgroundColor = Flow_Color("#252526ff");
	Clay_Color borderColor = Flow_Color("#454545ff");
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_Padding padding = CLAY_PADDING_ALL(10);
	uint16_t childGap = 6;
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_TOP,
	};
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
	Clay_LayoutDirection layoutDirection = CLAY_LEFT_TO_RIGHT;

	FontFamilyId labelFontFamily = 0;
	Clay_TextElementConfigWrapMode labelWrapMode = CLAY_TEXT_WRAP_NONE;
	Clay_TextAlignment labelAlignment = CLAY_TEXT_ALIGN_CENTER;
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
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	uint32_t labelFontWeight = 400;
	FontStyle labelFontStyle = FontStyle::Normal;
	float iconSize = 18.0f;

	CursorType cursor = CursorType::PointingHand;
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};

	Clay_Padding padding = CLAY_PADDING_ALL(10);
	uint16_t contentGap = 8;
	uint16_t labelFontSize = 16;
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
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
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);

	float size = 20.0f;
	float iconSize = 14.0f;
	CursorType cursor = CursorType::PointingHand;
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
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
	float roundness = 1.0f;
	CursorType cursor = CursorType::PointingHand;
	Clay_BorderWidth trackBorderWidth = {1, 1, 1, 1, 0};
	Clay_BorderWidth knobBorderWidth = {0, 0, 0, 0, 0};
	uint16_t knobInset = 2;
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

	InputFieldOverlayStyle overlayStyle{};

	FontFamilyId fontFamily = 0;
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
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);

	float width = 240.0f;
	float height = 34.0f;
	uint32_t fontWeight = 400;
	FontStyle fontStyle = FontStyle::Normal;
	CursorType cursor = CursorType::IBeam;
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	uint16_t fontSize = 14;
	uint16_t letterSpacing = 0;
	uint8_t tabWidth = 4;
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
	FontFamilyId fontFamily = 0;
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
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	float width = 240.0f;
	float height = 34.0f;
	float popupMaxHeight = 240.0f;
	float optionHeight = 32.0f;
	float iconSize = 16.0f;
	float scrollTrackWidth = 6.0f;
	float scrollThumbMinimum = 22.0f;
	uint32_t fontWeight = 400;
	FontStyle fontStyle = FontStyle::Normal;
	CursorType cursor = CursorType::PointingHand;
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	Clay_Padding triggerPadding = {10, 8, 6, 6};
	Clay_Padding optionPadding = {8, 8, 4, 4};
	uint16_t contentGap = 8;
	uint16_t optionGap = 2;
	uint16_t popupGap = 4;
	uint16_t scrollGap = 6;
	uint16_t fontSize = 16;
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

#if FLOW_UI_DEV_MODE
FLOWUI_DEV_SCHEMA(FSELBoxTheme, FLOWUI_DEV_FIELD(FSELBoxTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELBoxTheme, borderColor),
				  FLOWUI_DEV_FIELD(FSELBoxTheme, borderWidth),
				  FLOWUI_DEV_FIELD(FSELBoxTheme, cornerRadius))

FLOWUI_DEV_SCHEMA(FSELPopupSurfaceTheme, FLOWUI_DEV_FIELD(FSELPopupSurfaceTheme, layoutDirection),
				  FLOWUI_DEV_FIELD(FSELPopupSurfaceTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELPopupSurfaceTheme, borderColor),
				  FLOWUI_DEV_FIELD(FSELPopupSurfaceTheme, cornerRadius),
				  FLOWUI_DEV_FIELD(FSELPopupSurfaceTheme, borderWidth),
				  FLOWUI_DEV_FIELD(FSELPopupSurfaceTheme, padding),
				  FLOWUI_DEV_FIELD(FSELPopupSurfaceTheme, childGap),
				  FLOWUI_DEV_FIELD(FSELPopupSurfaceTheme, childAlignment))

FLOWUI_DEV_SCHEMA(FSELSplitterHandleTheme,
				  FLOWUI_DEV_FIELD(FSELSplitterHandleTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELSplitterHandleTheme, hoverColor),
				  FLOWUI_DEV_FIELD(FSELSplitterHandleTheme, draggingColor),
				  FLOWUI_DEV_FIELD(FSELSplitterHandleTheme, borderColor),
				  FLOWUI_DEV_FIELD(FSELSplitterHandleTheme, borderWidth),
				  FLOWUI_DEV_FIELD(FSELSplitterHandleTheme, cornerRadius),
				  FLOWUI_DEV_FIELD(FSELSplitterHandleTheme, horizontalResizeCursor),
				  FLOWUI_DEV_FIELD(FSELSplitterHandleTheme, verticalResizeCursor),
				  FLOWUI_DEV_FIELD(FSELSplitterHandleTheme, cursorPriority))

FLOWUI_DEV_SCHEMA(FSELButtonStateTheme, FLOWUI_DEV_FIELD(FSELButtonStateTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELButtonStateTheme, labelColor),
				  FLOWUI_DEV_FIELD(FSELButtonStateTheme, iconColor),
				  FLOWUI_DEV_FIELD(FSELButtonStateTheme, borderColor))

FLOWUI_DEV_SCHEMA(
	FSELButtonTheme, FLOWUI_DEV_FIELD(FSELButtonTheme, layoutDirection),
	FLOWUI_DEV_FIELD(FSELButtonTheme, labelFontFamily,
					 FlowUi::devMode::DevFieldOptions{}
						 .withEditor(FlowUi::devMode::DevEditorKind::FontChoice)
						 .withChoiceDomain(FlowUi::devMode::DevChoiceDomain::FontFamily)),
	FLOWUI_DEV_FIELD(FSELButtonTheme, labelWrapMode),
	FLOWUI_DEV_FIELD(FSELButtonTheme, labelAlignment), FLOWUI_DEV_FIELD(FSELButtonTheme, idle),
	FLOWUI_DEV_FIELD(FSELButtonTheme, hovered), FLOWUI_DEV_FIELD(FSELButtonTheme, pressed),
	FLOWUI_DEV_FIELD(FSELButtonTheme, disabled), FLOWUI_DEV_FIELD(FSELButtonTheme, cornerRadius),
	FLOWUI_DEV_FIELD(FSELButtonTheme, labelFontWeight),
	FLOWUI_DEV_FIELD(FSELButtonTheme, labelFontStyle), FLOWUI_DEV_FIELD(FSELButtonTheme, iconSize),
	FLOWUI_DEV_FIELD(FSELButtonTheme, cursor), FLOWUI_DEV_FIELD(FSELButtonTheme, borderWidth),
	FLOWUI_DEV_FIELD(FSELButtonTheme, padding), FLOWUI_DEV_FIELD(FSELButtonTheme, contentGap),
	FLOWUI_DEV_FIELD(FSELButtonTheme, labelFontSize),
	FLOWUI_DEV_FIELD(FSELButtonTheme, childAlignment),
	FLOWUI_DEV_FIELD(FSELButtonTheme, cursorPriority))

FLOWUI_DEV_SCHEMA(FSELCheckboxStateTheme, FLOWUI_DEV_FIELD(FSELCheckboxStateTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELCheckboxStateTheme, borderColor),
				  FLOWUI_DEV_FIELD(FSELCheckboxStateTheme, iconColor))

FLOWUI_DEV_SCHEMA(FSELCheckboxValueTheme, FLOWUI_DEV_FIELD(FSELCheckboxValueTheme, idle),
				  FLOWUI_DEV_FIELD(FSELCheckboxValueTheme, hovered),
				  FLOWUI_DEV_FIELD(FSELCheckboxValueTheme, pressed),
				  FLOWUI_DEV_FIELD(FSELCheckboxValueTheme, disabled))

FLOWUI_DEV_SCHEMA(FSELCheckboxTheme, FLOWUI_DEV_FIELD(FSELCheckboxTheme, unchecked),
				  FLOWUI_DEV_FIELD(FSELCheckboxTheme, checked),
				  FLOWUI_DEV_FIELD(FSELCheckboxTheme, cornerRadius),
				  FLOWUI_DEV_FIELD(FSELCheckboxTheme, size),
				  FLOWUI_DEV_FIELD(FSELCheckboxTheme, iconSize),
				  FLOWUI_DEV_FIELD(FSELCheckboxTheme, cursor),
				  FLOWUI_DEV_FIELD(FSELCheckboxTheme, borderWidth),
				  FLOWUI_DEV_FIELD(FSELCheckboxTheme, cursorPriority))

FLOWUI_DEV_SCHEMA(FSELSwitchStateTheme, FLOWUI_DEV_FIELD(FSELSwitchStateTheme, trackColor),
				  FLOWUI_DEV_FIELD(FSELSwitchStateTheme, trackBorderColor),
				  FLOWUI_DEV_FIELD(FSELSwitchStateTheme, knobColor),
				  FLOWUI_DEV_FIELD(FSELSwitchStateTheme, knobBorderColor))

FLOWUI_DEV_SCHEMA(FSELSwitchValueTheme, FLOWUI_DEV_FIELD(FSELSwitchValueTheme, idle),
				  FLOWUI_DEV_FIELD(FSELSwitchValueTheme, hovered),
				  FLOWUI_DEV_FIELD(FSELSwitchValueTheme, pressed),
				  FLOWUI_DEV_FIELD(FSELSwitchValueTheme, disabled))

FLOWUI_DEV_SCHEMA(
	FSELSwitchTheme, FLOWUI_DEV_FIELD(FSELSwitchTheme, off), FLOWUI_DEV_FIELD(FSELSwitchTheme, on),
	FLOWUI_DEV_FIELD(FSELSwitchTheme, trackWidth), FLOWUI_DEV_FIELD(FSELSwitchTheme, trackHeight),
	FLOWUI_DEV_FIELD(FSELSwitchTheme, roundness), FLOWUI_DEV_FIELD(FSELSwitchTheme, cursor),
	FLOWUI_DEV_FIELD(FSELSwitchTheme, trackBorderWidth),
	FLOWUI_DEV_FIELD(FSELSwitchTheme, knobBorderWidth),
	FLOWUI_DEV_FIELD(FSELSwitchTheme, knobInset), FLOWUI_DEV_FIELD(FSELSwitchTheme, cursorPriority))

FLOWUI_DEV_SCHEMA(FSELSliderStateTheme, FLOWUI_DEV_FIELD(FSELSliderStateTheme, trackColor),
				  FLOWUI_DEV_FIELD(FSELSliderStateTheme, trackBorderColor),
				  FLOWUI_DEV_FIELD(FSELSliderStateTheme, fillColor),
				  FLOWUI_DEV_FIELD(FSELSliderStateTheme, fillBorderColor),
				  FLOWUI_DEV_FIELD(FSELSliderStateTheme, thumbColor),
				  FLOWUI_DEV_FIELD(FSELSliderStateTheme, thumbBorderColor))

FLOWUI_DEV_SCHEMA(FSELSliderTheme, FLOWUI_DEV_FIELD(FSELSliderTheme, idle),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, hovered),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, dragging),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, disabled),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, length),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, hitThickness),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, trackThickness),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, thumbLength),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, thumbThickness),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, trackRoundness),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, thumbRoundness),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, trackBorderWidth),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, fillBorderWidth),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, thumbBorderWidth),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, cursor),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, draggingCursor),
				  FLOWUI_DEV_FIELD(FSELSliderTheme, cursorPriority))

FLOWUI_DEV_SCHEMA(FSELProgressBarTheme, FLOWUI_DEV_FIELD(FSELProgressBarTheme, baseColor),
				  FLOWUI_DEV_FIELD(FSELProgressBarTheme, borderColor),
				  FLOWUI_DEV_FIELD(FSELProgressBarTheme, fillColor),
				  FLOWUI_DEV_FIELD(FSELProgressBarTheme, length),
				  FLOWUI_DEV_FIELD(FSELProgressBarTheme, thickness),
				  FLOWUI_DEV_FIELD(FSELProgressBarTheme, borderWidth),
				  FLOWUI_DEV_FIELD(FSELProgressBarTheme, cornerRadius))

FLOWUI_DEV_SCHEMA(
	FSELLabelTheme, FLOWUI_DEV_FIELD(FSELLabelTheme, textColor),
	FLOWUI_DEV_FIELD(FSELLabelTheme, fontFamily,
					 FlowUi::devMode::DevFieldOptions{}
						 .withEditor(FlowUi::devMode::DevEditorKind::FontChoice)
						 .withChoiceDomain(FlowUi::devMode::DevChoiceDomain::FontFamily)),
	FLOWUI_DEV_FIELD(FSELLabelTheme, fontWeight), FLOWUI_DEV_FIELD(FSELLabelTheme, fontStyle),
	FLOWUI_DEV_FIELD(FSELLabelTheme, fontSize), FLOWUI_DEV_FIELD(FSELLabelTheme, letterSpacing),
	FLOWUI_DEV_FIELD(FSELLabelTheme, wrapMode), FLOWUI_DEV_FIELD(FSELLabelTheme, textAlignment))

FLOWUI_DEV_SCHEMA(FSELTextFieldStateTheme,
				  FLOWUI_DEV_FIELD(FSELTextFieldStateTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELTextFieldStateTheme, textColor),
				  FLOWUI_DEV_FIELD(FSELTextFieldStateTheme, placeholderColor),
				  FLOWUI_DEV_FIELD(FSELTextFieldStateTheme, borderColor))

FLOWUI_DEV_SCHEMA(
	FSELTextFieldTheme, FLOWUI_DEV_FIELD(FSELTextFieldTheme, overlayStyle),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, fontFamily,
					 FlowUi::devMode::DevFieldOptions{}
						 .withEditor(FlowUi::devMode::DevEditorKind::FontChoice)
						 .withChoiceDomain(FlowUi::devMode::DevChoiceDomain::FontFamily)),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, idle), FLOWUI_DEV_FIELD(FSELTextFieldTheme, hovered),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, focused), FLOWUI_DEV_FIELD(FSELTextFieldTheme, readOnly),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, invalid), FLOWUI_DEV_FIELD(FSELTextFieldTheme, disabled),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, cornerRadius), FLOWUI_DEV_FIELD(FSELTextFieldTheme, width),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, height), FLOWUI_DEV_FIELD(FSELTextFieldTheme, fontWeight),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, fontStyle), FLOWUI_DEV_FIELD(FSELTextFieldTheme, cursor),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, borderWidth),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, padding), FLOWUI_DEV_FIELD(FSELTextFieldTheme, fontSize),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, letterSpacing),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, tabWidth),
	FLOWUI_DEV_FIELD(FSELTextFieldTheme, cursorPriority))

FLOWUI_DEV_SCHEMA(FSELNumberInputStepButtonStateTheme,
				  FLOWUI_DEV_FIELD(FSELNumberInputStepButtonStateTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELNumberInputStepButtonStateTheme, foregroundColor))

FLOWUI_DEV_SCHEMA(FSELNumberInputTheme, FLOWUI_DEV_FIELD(FSELNumberInputTheme, field),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepIdle),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepHovered),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepPressed),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepDisabled),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepSeparatorColor),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepButtonWidth),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepIconSize),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepTextSize),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepCursor),
				  FLOWUI_DEV_FIELD(FSELNumberInputTheme, stepCursorPriority))

FLOWUI_DEV_SCHEMA(FSELDragValueTheme, FLOWUI_DEV_FIELD(FSELDragValueTheme, field),
				  FLOWUI_DEV_FIELD(FSELDragValueTheme, dragging),
				  FLOWUI_DEV_FIELD(FSELDragValueTheme, dragCursor),
				  FLOWUI_DEV_FIELD(FSELDragValueTheme, editCursor),
				  FLOWUI_DEV_FIELD(FSELDragValueTheme, cursorPriority))

FLOWUI_DEV_SCHEMA(FSELSelectableSurfaceStateTheme,
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceStateTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceStateTheme, borderColor))

FLOWUI_DEV_SCHEMA(FSELSelectableSurfaceTheme, FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, idle),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, hovered),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, pressed),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, selected),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, disabled),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, selectedDisabled),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, padding),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, childAlignment),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, layoutDirection),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, contentGap),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, borderWidth),
				  FLOWUI_DEV_FIELD(FSELSelectableSurfaceTheme, cornerRadius))

FLOWUI_DEV_SCHEMA(FSELComboBoxStateTheme, FLOWUI_DEV_FIELD(FSELComboBoxStateTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELComboBoxStateTheme, textColor),
				  FLOWUI_DEV_FIELD(FSELComboBoxStateTheme, iconColor),
				  FLOWUI_DEV_FIELD(FSELComboBoxStateTheme, borderColor))

FLOWUI_DEV_SCHEMA(FSELComboBoxOptionStateTheme,
				  FLOWUI_DEV_FIELD(FSELComboBoxOptionStateTheme, backgroundColor),
				  FLOWUI_DEV_FIELD(FSELComboBoxOptionStateTheme, textColor),
				  FLOWUI_DEV_FIELD(FSELComboBoxOptionStateTheme, iconColor))

FLOWUI_DEV_SCHEMA(
	FSELComboBoxTheme,
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, fontFamily,
					 FlowUi::devMode::DevFieldOptions{}
						 .withEditor(FlowUi::devMode::DevEditorKind::FontChoice)
						 .withChoiceDomain(FlowUi::devMode::DevChoiceDomain::FontFamily)),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, idle), FLOWUI_DEV_FIELD(FSELComboBoxTheme, hovered),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, open), FLOWUI_DEV_FIELD(FSELComboBoxTheme, disabled),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, optionIdle),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, optionHovered),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, optionPressed),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, optionSelected),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, optionDisabled),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, placeholderColor),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, scrollTrackColor),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, scrollThumbColor),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, cornerRadius), FLOWUI_DEV_FIELD(FSELComboBoxTheme, width),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, height),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, popupMaxHeight),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, optionHeight),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, iconSize),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, scrollTrackWidth),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, scrollThumbMinimum),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, fontWeight), FLOWUI_DEV_FIELD(FSELComboBoxTheme, fontStyle),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, cursor), FLOWUI_DEV_FIELD(FSELComboBoxTheme, borderWidth),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, triggerPadding),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, optionPadding),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, contentGap), FLOWUI_DEV_FIELD(FSELComboBoxTheme, optionGap),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, popupGap), FLOWUI_DEV_FIELD(FSELComboBoxTheme, scrollGap),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, fontSize),
	FLOWUI_DEV_FIELD(FSELComboBoxTheme, cursorPriority))

FLOWUI_DEV_SCHEMA(
	FSELTheme, FLOWUI_DEV_FIELD(FSELTheme, boxTheme),
	FLOWUI_DEV_FIELD(FSELTheme, splitterHandleTheme), FLOWUI_DEV_FIELD(FSELTheme, buttonTheme),
	FLOWUI_DEV_FIELD(FSELTheme, checkboxTheme), FLOWUI_DEV_FIELD(FSELTheme, switchTheme),
	FLOWUI_DEV_FIELD(FSELTheme, sliderTheme), FLOWUI_DEV_FIELD(FSELTheme, progressBarTheme),
	FLOWUI_DEV_FIELD(FSELTheme, labelTheme), FLOWUI_DEV_FIELD(FSELTheme, textInputTheme),
	FLOWUI_DEV_FIELD(FSELTheme, numberInputTheme), FLOWUI_DEV_FIELD(FSELTheme, dragValueTheme),
	FLOWUI_DEV_FIELD(FSELTheme, textAreaTheme), FLOWUI_DEV_FIELD(FSELTheme, selectableSurfaceTheme),
	FLOWUI_DEV_FIELD(FSELTheme, popupSurfaceTheme), FLOWUI_DEV_FIELD(FSELTheme, comboBoxTheme))
#endif

} // namespace FlowUi::FSEL
