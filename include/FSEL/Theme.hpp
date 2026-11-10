#pragma once

#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"
#include "clay.h"
#include "FlowUi/Public/App.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/schema/DevSchemaDescriptor.hpp"
#endif
#include "FlowUi/Public/managerStructs/InputFieldTypes.hpp"

namespace FlowUi::FSEL {

/**
 * Default box appearance and geometry. Dimensions use layout pixels and colors use Clay RGBA
 * channels.
 */
struct FSELBoxTheme {
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#00000000");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#00000000");
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth = {0, 0, 0, 0, 0};
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = {0, 0, 0, 0};
};

/**
 * Default popup surface appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELPopupSurfaceTheme {
	/** Axis and order used to arrange children. */
	Clay_LayoutDirection layoutDirection = CLAY_TOP_TO_BOTTOM;
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#252526ff");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#454545ff");
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	/** Inset between the root border and its children, in layout pixels. */
	Clay_Padding padding = CLAY_PADDING_ALL(10);
	/** Space between consecutive children, in layout pixels. */
	uint16_t childGap = 6;
	/** Alignment of children inside the available content area. */
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_TOP,
	};
};

/**
 * Default splitter handle appearance and geometry. Dimensions use layout pixels and colors use
 * Clay RGBA channels.
 */
struct FSELSplitterHandleTheme {
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#00000000");
	/** Hovered handle background color in Clay RGBA channels. */
	Clay_Color hoverColor = Flow_Color("#ffffff18");
	/** Dragged handle background color in Clay RGBA channels. */
	Clay_Color draggingColor = Flow_Color("#ffffff30");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#00000000");
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth = {0, 0, 0, 0, 0};
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = {0, 0, 0, 0};
	/** Cursor used while resizing the horizontal extent. */
	CursorType horizontalResizeCursor = CursorType::ResizeHorizontal;
	/** Cursor used while resizing the vertical extent. */
	CursorType verticalResizeCursor = CursorType::ResizeVertical;
	/** Priority used to arbitrate this control's cursor request. */
	uint8_t cursorPriority = 100;
};

/**
 * Default button state appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELButtonStateTheme {
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#00000000");
	/** Button label color in Clay RGBA channels. */
	Clay_Color labelColor = Flow_Color("#ffffffff");
	/** Icon tint color in Clay RGBA channels. */
	Clay_Color iconColor = Flow_Color("#ffffffff");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#00000000");
};

/**
 * Default button appearance and geometry. Dimensions use layout pixels and colors use Clay RGBA
 * channels.
 */
struct FSELButtonTheme {
	/** Axis and order used to arrange children. */
	Clay_LayoutDirection layoutDirection = CLAY_LEFT_TO_RIGHT;

	/** Registered font family used for the button label. */
	FontFamilyId labelFontFamily = 0;
	/** Wrapping rule for the button label. */
	Clay_TextElementConfigWrapMode labelWrapMode = CLAY_TEXT_WRAP_NONE;
	/** Horizontal alignment of the trigger or button label. */
	Clay_TextAlignment labelAlignment = CLAY_TEXT_ALIGN_CENTER;
	/** Appearance when enabled and idle. */
	FSELButtonStateTheme idle = {
		.backgroundColor = Flow_Color("#252526ff"),
		.labelColor = Flow_Color("#ffffffff"),
		.iconColor = Flow_Color("#ffffffff"),
		.borderColor = Flow_Color("#454545ff"),
	};
	/** Appearance while hovered. */
	FSELButtonStateTheme hovered = {
		.backgroundColor = Flow_Color("#3e3e42ff"),
		.labelColor = Flow_Color("#ffffffff"),
		.iconColor = Flow_Color("#ffffffff"),
		.borderColor = Flow_Color("#5a5a5eff"),
	};
	/** Appearance during an armed pointer press. */
	FSELButtonStateTheme pressed = {
		.backgroundColor = Flow_Color("#094771ff"),
		.labelColor = Flow_Color("#ffffffff"),
		.iconColor = Flow_Color("#ffffffff"),
		.borderColor = Flow_Color("#007accff"),
	};
	/** Appearance when interaction is disabled. */
	FSELButtonStateTheme disabled = {
		.backgroundColor = Flow_Color("#252526b8"),
		.labelColor = Flow_Color("#6c6c6cb8"),
		.iconColor = Flow_Color("#6c6c6cb8"),
		.borderColor = Flow_Color("#3e3e42b8"),
	};
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	/** Font weight requested for the button label, such as 400 for normal. */
	uint32_t labelFontWeight = 400;
	/** Font style requested for the button label. */
	FontStyle labelFontStyle = FontStyle::Normal;
	/** Rendered icon side length in layout pixels. */
	float iconSize = 18.0f;

	/** Cursor requested while this enabled control is hovered. */
	CursorType cursor = CursorType::PointingHand;
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};

	/** Inset between the root border and its children, in layout pixels. */
	Clay_Padding padding = CLAY_PADDING_ALL(10);
	/** Space between adjacent content items, in layout pixels. */
	uint16_t contentGap = 8;
	/** Button label font size in layout pixels. */
	uint16_t labelFontSize = 16;
	/** Alignment of children inside the available content area. */
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	/** Priority used to arbitrate this control's cursor request. */
	uint8_t cursorPriority = 10;
};

/**
 * Default checkbox state appearance and geometry. Dimensions use layout pixels and colors use
 * Clay RGBA channels.
 */
struct FSELCheckboxStateTheme {
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#252526ff");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#5a5a5eff");
	/** Icon tint color in Clay RGBA channels. */
	Clay_Color iconColor = Flow_Color("#ffffffff");
};

/**
 * Default checkbox value appearance and geometry. Dimensions use layout pixels and colors use
 * Clay RGBA channels.
 */
struct FSELCheckboxValueTheme {
	/** Appearance when enabled and idle. */
	FSELCheckboxStateTheme idle{};
	/** Appearance while hovered. */
	FSELCheckboxStateTheme hovered = {
		.backgroundColor = Flow_Color("#2d2d30ff"),
		.borderColor = Flow_Color("#6a6a6eff"),
	};
	/** Appearance during an armed pointer press. */
	FSELCheckboxStateTheme pressed = {
		.backgroundColor = Flow_Color("#3e3e42ff"),
		.borderColor = Flow_Color("#7a7a7eff"),
	};
	/** Appearance when interaction is disabled. */
	FSELCheckboxStateTheme disabled = {
		.backgroundColor = Flow_Color("#252526b8"),
		.borderColor = Flow_Color("#3e3e42b8"),
		.iconColor = Flow_Color("#6c6c6cb8"),
	};
};

/**
 * Default checkbox appearance and geometry. Dimensions use layout pixels and colors use Clay RGBA
 * channels.
 */
struct FSELCheckboxTheme {
	/** Appearance for the unchecked value. */
	FSELCheckboxValueTheme unchecked{};
	/** Appearance for the checked value. */
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
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);

	/** Checkbox side length in layout pixels. */
	float size = 20.0f;
	/** Rendered icon side length in layout pixels. */
	float iconSize = 14.0f;
	/** Cursor requested while this enabled control is hovered. */
	CursorType cursor = CursorType::PointingHand;
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	/** Priority used to arbitrate this control's cursor request. */
	uint8_t cursorPriority = 10;
};

/**
 * Default switch state appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELSwitchStateTheme {
	/** Track background color in Clay RGBA channels. */
	Clay_Color trackColor = Flow_Color("#3e3e42ff");
	/** Track border color in Clay RGBA channels. */
	Clay_Color trackBorderColor = Flow_Color("#5a5a5eff");
	/** Switch knob color in Clay RGBA channels. */
	Clay_Color knobColor = Flow_Color("#c8c8c8ff");
	/** Switch knob border color in Clay RGBA channels. */
	Clay_Color knobBorderColor = Flow_Color("#00000000");
};

/**
 * Default switch value appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELSwitchValueTheme {
	/** Appearance when enabled and idle. */
	FSELSwitchStateTheme idle{};
	/** Appearance while hovered. */
	FSELSwitchStateTheme hovered = {
		.trackColor = Flow_Color("#4a4a4eff"),
		.trackBorderColor = Flow_Color("#6a6a6eff"),
		.knobColor = Flow_Color("#ffffffff"),
	};
	/** Appearance during an armed pointer press. */
	FSELSwitchStateTheme pressed = {
		.trackColor = Flow_Color("#2d2d30ff"),
		.trackBorderColor = Flow_Color("#5a5a5eff"),
		.knobColor = Flow_Color("#ffffffff"),
	};
	/** Appearance when interaction is disabled. */
	FSELSwitchStateTheme disabled = {
		.trackColor = Flow_Color("#2d2d30b8"),
		.trackBorderColor = Flow_Color("#3e3e42b8"),
		.knobColor = Flow_Color("#6c6c6cb8"),
	};
};

/**
 * Default switch appearance and geometry. Dimensions use layout pixels and colors use Clay RGBA
 * channels.
 */
struct FSELSwitchTheme {
	/** Appearance for the off value. */
	FSELSwitchValueTheme off{};
	/** Appearance for the on value. */
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

	/** Switch track width in layout pixels. */
	float trackWidth = 40.0f;
	/** Switch track height in layout pixels. */
	float trackHeight = 22.0f;
	/** Switch roundness factor; 0 is square and 1 is fully rounded. */
	float roundness = 1.0f;
	/** Cursor requested while this enabled control is hovered. */
	CursorType cursor = CursorType::PointingHand;
	/** Per-edge track border widths in layout pixels. */
	Clay_BorderWidth trackBorderWidth = {1, 1, 1, 1, 0};
	/** Per-edge knob border widths in layout pixels. */
	Clay_BorderWidth knobBorderWidth = {0, 0, 0, 0, 0};
	/** Space between the track edge and knob, in layout pixels. */
	uint16_t knobInset = 2;
	/** Priority used to arbitrate this control's cursor request. */
	uint8_t cursorPriority = 10;
};

/**
 * Default slider state appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELSliderStateTheme {
	/** Track background color in Clay RGBA channels. */
	Clay_Color trackColor = Flow_Color("#3e3e42ff");
	/** Track border color in Clay RGBA channels. */
	Clay_Color trackBorderColor = Flow_Color("#5a5a5eff");
	/** Filled portion color in Clay RGBA channels. */
	Clay_Color fillColor = Flow_Color("#007accff");
	/** Filled portion border color in Clay RGBA channels. */
	Clay_Color fillBorderColor = Flow_Color("#007accff");
	/** Slider thumb color in Clay RGBA channels. */
	Clay_Color thumbColor = Flow_Color("#c8c8c8ff");
	/** Slider thumb border color in Clay RGBA channels. */
	Clay_Color thumbBorderColor = Flow_Color("#00000000");
};

/**
 * Default slider appearance and geometry. Dimensions use layout pixels and colors use Clay RGBA
 * channels.
 */
struct FSELSliderTheme {
	/** Appearance when enabled and idle. */
	FSELSliderStateTheme idle{};
	/** Appearance while hovered. */
	FSELSliderStateTheme hovered = {
		.trackColor = Flow_Color("#4a4a4eff"),
		.trackBorderColor = Flow_Color("#6a6a6eff"),
		.fillColor = Flow_Color("#1c97eaff"),
		.fillBorderColor = Flow_Color("#1c97eaff"),
		.thumbColor = Flow_Color("#ffffffff"),
	};
	/** Appearance while dragging. */
	FSELSliderStateTheme dragging = {
		.trackColor = Flow_Color("#2d2d30ff"),
		.trackBorderColor = Flow_Color("#5a5a5eff"),
		.fillColor = Flow_Color("#005fb8ff"),
		.fillBorderColor = Flow_Color("#005fb8ff"),
		.thumbColor = Flow_Color("#ffffffff"),
	};
	/** Appearance when interaction is disabled. */
	FSELSliderStateTheme disabled = {
		.trackColor = Flow_Color("#2d2d30b8"),
		.trackBorderColor = Flow_Color("#3e3e42b8"),
		.fillColor = Flow_Color("#094771b8"),
		.fillBorderColor = Flow_Color("#094771b8"),
		.thumbColor = Flow_Color("#6c6c6cb8"),
	};

	/** Control length along its main axis, in layout pixels. */
	float length = 140.0f;
	/** Interactive thickness perpendicular to the main axis, in layout pixels. */
	float hitThickness = 20.0f;
	/** Visible track thickness in layout pixels. */
	float trackThickness = 6.0f;
	/** Thumb length along the value axis, in layout pixels. */
	float thumbLength = 12.0f;
	/** Thumb size perpendicular to the value axis, in layout pixels. */
	float thumbThickness = 18.0f;
	/** Track roundness factor; 0 is square and 1 is fully rounded. */
	float trackRoundness = 1.0f;
	/** Thumb roundness factor; 0 is square and 1 is fully rounded. */
	float thumbRoundness = 1.0f;
	/** Per-edge track border widths in layout pixels. */
	Clay_BorderWidth trackBorderWidth = {0, 0, 0, 0, 0};
	/** Per-edge fill border widths in layout pixels. */
	Clay_BorderWidth fillBorderWidth = {0, 0, 0, 0, 0};
	/** Per-edge thumb border widths in layout pixels. */
	Clay_BorderWidth thumbBorderWidth = {0, 0, 0, 0, 0};
	/** Cursor requested while this enabled control is hovered. */
	CursorType cursor = CursorType::PointingHand;
	/** Cursor requested while dragging the slider. */
	CursorType draggingCursor = CursorType::Grabbing;
	/** Priority used to arbitrate this control's cursor request. */
	uint8_t cursorPriority = 10;
};

/**
 * Default progress bar appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELProgressBarTheme {
	/** Unfilled progress background color in Clay RGBA channels. */
	Clay_Color baseColor = Flow_Color("#2d2d30ff");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#454545ff");
	/** Filled portion color in Clay RGBA channels. */
	Clay_Color fillColor = Flow_Color("#007accff");
	/** Control length along its main axis, in layout pixels. */
	float length = 160.0f;
	/** Visible bar thickness in layout pixels. */
	float thickness = 12.0f;
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
};

/**
 * Default label appearance and geometry. Dimensions use layout pixels and colors use Clay RGBA
 * channels.
 */
struct FSELLabelTheme {
	/** Text color in Clay RGBA channels. */
	Clay_Color textColor = Flow_Color("#ffffffff");
	/** Registered font family used for field or label text. */
	FontFamilyId fontFamily = 0;
	/** Font weight requested for text, such as 400 for normal. */
	uint32_t fontWeight = 400;
	/** Font style requested for text. */
	FontStyle fontStyle = FontStyle::Normal;
	/** Text font size in layout pixels. */
	uint16_t fontSize = 14;
	/** Additional spacing between glyphs in layout pixels. */
	uint16_t letterSpacing = 0;
	/** Wrapping rule for label text. */
	Clay_TextElementConfigWrapMode wrapMode = CLAY_TEXT_WRAP_NONE;
	/** Horizontal alignment of text within its line. */
	Clay_TextAlignment textAlignment = CLAY_TEXT_ALIGN_LEFT;
};

/**
 * Default text field state appearance and geometry. Dimensions use layout pixels and colors use
 * Clay RGBA channels.
 */
struct FSELTextFieldStateTheme {
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#1e1e1eff");
	/** Text color in Clay RGBA channels. */
	Clay_Color textColor = Flow_Color("#f0f0f0ff");
	/** Empty-field placeholder color in Clay RGBA channels. */
	Clay_Color placeholderColor = Flow_Color("#858585ff");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#454545ff");
};

/**
 * Default text field appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELTextFieldTheme {

	/** Default caret and text selection rendering style. */
	InputFieldOverlayStyle overlayStyle{};

	/** Registered font family used for field or label text. */
	FontFamilyId fontFamily = 0;
	/** Appearance when enabled and idle. */
	FSELTextFieldStateTheme idle{};
	/** Appearance while hovered. */
	FSELTextFieldStateTheme hovered = {
		.backgroundColor = Flow_Color("#1e1e1eff"),
		.textColor = Flow_Color("#ffffffff"),
		.placeholderColor = Flow_Color("#909090ff"),
		.borderColor = Flow_Color("#666666ff"),
	};
	/** Appearance while the editor is focused. */
	FSELTextFieldStateTheme focused = {
		.backgroundColor = Flow_Color("#1e1e1eff"),
		.textColor = Flow_Color("#ffffffff"),
		.placeholderColor = Flow_Color("#858585ff"),
		.borderColor = Flow_Color("#007accff"),
	};
	/** Appearance while the editor is read-only. */
	FSELTextFieldStateTheme readOnly = {
		.backgroundColor = Flow_Color("#252526ff"),
		.textColor = Flow_Color("#c8c8c8ff"),
		.placeholderColor = Flow_Color("#777777ff"),
		.borderColor = Flow_Color("#454545ff"),
	};
	/** Appearance for an invalid value. */
	FSELTextFieldStateTheme invalid = {
		.backgroundColor = Flow_Color("#1e1e1eff"),
		.textColor = Flow_Color("#ffffffff"),
		.placeholderColor = Flow_Color("#858585ff"),
		.borderColor = Flow_Color("#f14c4cff"),
	};
	/** Appearance when interaction is disabled. */
	FSELTextFieldStateTheme disabled = {
		.backgroundColor = Flow_Color("#252526b8"),
		.textColor = Flow_Color("#6c6c6cb8"),
		.placeholderColor = Flow_Color("#5f5f5fb8"),
		.borderColor = Flow_Color("#3e3e42b8"),
	};
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);

	/** Default control width in layout pixels. */
	float width = 240.0f;
	/** Default control height in layout pixels. */
	float height = 34.0f;
	/** Font weight requested for text, such as 400 for normal. */
	uint32_t fontWeight = 400;
	/** Font style requested for text. */
	FontStyle fontStyle = FontStyle::Normal;
	/** Cursor requested while this enabled control is hovered. */
	CursorType cursor = CursorType::IBeam;
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	/** Inset between the root border and its children, in layout pixels. */
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	/** Text font size in layout pixels. */
	uint16_t fontSize = 14;
	/** Additional spacing between glyphs in layout pixels. */
	uint16_t letterSpacing = 0;
	/** Width of a tab in space columns. */
	uint8_t tabWidth = 4;
	/** Priority used to arbitrate this control's cursor request. */
	uint8_t cursorPriority = 20;
};

/**
 * Default number input step button state appearance and geometry. Dimensions use layout pixels
 * and colors use Clay RGBA channels.
 */
struct FSELNumberInputStepButtonStateTheme {
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#00000000");
	/** Step button symbol color in Clay RGBA channels. */
	Clay_Color foregroundColor = Flow_Color("#c8c8c8ff");
};

/**
 * Default number input appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELNumberInputTheme {
	/** Base text field appearance and geometry for this numeric control. */
	FSELTextFieldTheme field{};
	/** Appearance of an idle step button. */
	FSELNumberInputStepButtonStateTheme stepIdle{};
	/** Appearance of a hovered step button. */
	FSELNumberInputStepButtonStateTheme stepHovered = {
		.backgroundColor = Flow_Color("#ffffff12"),
		.foregroundColor = Flow_Color("#ffffffff"),
	};
	/** Appearance of a pressed step button. */
	FSELNumberInputStepButtonStateTheme stepPressed = {
		.backgroundColor = Flow_Color("#007accff"),
		.foregroundColor = Flow_Color("#ffffffff"),
	};
	/** Appearance of a disabled step button. */
	FSELNumberInputStepButtonStateTheme stepDisabled = {
		.backgroundColor = Flow_Color("#00000000"),
		.foregroundColor = Flow_Color("#6c6c6cb8"),
	};
	/** Step button separator color in Clay RGBA channels. */
	Clay_Color stepSeparatorColor = Flow_Color("#454545ff");
	/** Width of the trailing step button region in layout pixels. */
	float stepButtonWidth = 22.0f;
	/** Step button artwork side length in layout pixels. */
	float stepIconSize = 10.0f;
	/** Fallback step symbol font size in layout pixels. */
	uint16_t stepTextSize = 12;
	/** Cursor requested while hovering an enabled step button. */
	CursorType stepCursor = CursorType::PointingHand;
	/** Priority used for a step button cursor request. */
	uint8_t stepCursorPriority = 30;
};

/**
 * Default drag value appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELDragValueTheme {
	/** Base text field appearance and geometry for this numeric control. */
	FSELTextFieldTheme field = [] {
		FSELTextFieldTheme theme{};
		theme.width = 160.0f;
		return theme;
	}();
	/** Appearance while dragging. */
	FSELTextFieldStateTheme dragging = {
		.backgroundColor = Flow_Color("#094771ff"),
		.textColor = Flow_Color("#ffffffff"),
		.placeholderColor = Flow_Color("#858585ff"),
		.borderColor = Flow_Color("#007accff"),
	};
	/** Cursor requested while scrubbing the numeric value. */
	CursorType dragCursor = CursorType::ResizeHorizontal;
	/** Cursor requested when text entry is available. */
	CursorType editCursor = CursorType::IBeam;
	/** Priority used to arbitrate this control's cursor request. */
	uint8_t cursorPriority = 30;
};

/**
 * Default selectable surface state appearance and geometry. Dimensions use layout pixels and
 * colors use Clay RGBA channels.
 */
struct FSELSelectableSurfaceStateTheme {
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#00000000");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#00000000");
};

/**
 * Default selectable surface appearance and geometry. Dimensions use layout pixels and colors use
 * Clay RGBA channels.
 */
struct FSELSelectableSurfaceTheme {
	/** Appearance when enabled and idle. */
	FSELSelectableSurfaceStateTheme idle{};
	/** Appearance while hovered. */
	FSELSelectableSurfaceStateTheme hovered = {
		.backgroundColor = Flow_Color("#ffffff12"),
	};
	/** Appearance during an armed pointer press. */
	FSELSelectableSurfaceStateTheme pressed = {
		.backgroundColor = Flow_Color("#ffffff20"),
	};
	/** Appearance when selected. */
	FSELSelectableSurfaceStateTheme selected = {
		.backgroundColor = Flow_Color("#094771ff"),
		.borderColor = Flow_Color("#007accff"),
	};
	/** Appearance when interaction is disabled. */
	FSELSelectableSurfaceStateTheme disabled{};
	/** Appearance when selected and disabled. */
	FSELSelectableSurfaceStateTheme selectedDisabled = {
		.backgroundColor = Flow_Color("#09477168"),
		.borderColor = Flow_Color("#007acc68"),
	};

	/** Inset between the root border and its children, in layout pixels. */
	Clay_Padding padding = CLAY_PADDING_ALL(8);
	/** Alignment of children inside the available content area. */
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	/** Axis and order used to arrange children. */
	Clay_LayoutDirection layoutDirection = CLAY_LEFT_TO_RIGHT;
	/** Space between adjacent content items, in layout pixels. */
	uint16_t contentGap = 8;
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(4);
};

/**
 * Default combo box state appearance and geometry. Dimensions use layout pixels and colors use
 * Clay RGBA channels.
 */
struct FSELComboBoxStateTheme {
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#252526ff");
	/** Text color in Clay RGBA channels. */
	Clay_Color textColor = Flow_Color("#ffffffff");
	/** Icon tint color in Clay RGBA channels. */
	Clay_Color iconColor = Flow_Color("#ffffffff");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#454545ff");
};

/**
 * Default combo box option state appearance and geometry. Dimensions use layout pixels and colors
 * use Clay RGBA channels.
 */
struct FSELComboBoxOptionStateTheme {
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#00000000");
	/** Text color in Clay RGBA channels. */
	Clay_Color textColor = Flow_Color("#ffffffff");
	/** Icon tint color in Clay RGBA channels. */
	Clay_Color iconColor = Flow_Color("#ffffffff");
};

/**
 * Default combo box appearance and geometry. Dimensions use layout pixels and colors use Clay
 * RGBA channels.
 */
struct FSELComboBoxTheme {
	/** Registered font family used for field or label text. */
	FontFamilyId fontFamily = 0;
	/** Appearance when enabled and idle. */
	FSELComboBoxStateTheme idle{};
	/** Appearance while hovered. */
	FSELComboBoxStateTheme hovered = {
		.backgroundColor = Flow_Color("#3e3e42ff"),
		.borderColor = Flow_Color("#5a5a5eff"),
	};
	/** Appearance while the popup is open. */
	FSELComboBoxStateTheme open = {
		.backgroundColor = Flow_Color("#094771ff"),
		.borderColor = Flow_Color("#007accff"),
	};
	/** Appearance when interaction is disabled. */
	FSELComboBoxStateTheme disabled = {
		.backgroundColor = Flow_Color("#252526b8"),
		.textColor = Flow_Color("#6c6c6cb8"),
		.iconColor = Flow_Color("#6c6c6cb8"),
		.borderColor = Flow_Color("#3e3e42b8"),
	};
	/** Appearance of an idle option. */
	FSELComboBoxOptionStateTheme optionIdle{};
	/** Appearance of a hovered option. */
	FSELComboBoxOptionStateTheme optionHovered = {
		.backgroundColor = Flow_Color("#3e3e42ff"),
	};
	/** Appearance of a pressed option. */
	FSELComboBoxOptionStateTheme optionPressed = {
		.backgroundColor = Flow_Color("#094771ff"),
	};
	/** Appearance of the selected option. */
	FSELComboBoxOptionStateTheme optionSelected = {
		.backgroundColor = Flow_Color("#094771a0"),
	};
	/** Appearance of a disabled option. */
	FSELComboBoxOptionStateTheme optionDisabled = {
		.textColor = Flow_Color("#6c6c6cb8"),
		.iconColor = Flow_Color("#6c6c6cb8"),
	};
	/** Empty-field placeholder color in Clay RGBA channels. */
	Clay_Color placeholderColor = Flow_Color("#858585ff");
	/** Popup scroll track color in Clay RGBA channels. */
	Clay_Color scrollTrackColor = Flow_Color("#ffffff12");
	/** Popup scroll thumb color in Clay RGBA channels. */
	Clay_Color scrollThumbColor = Flow_Color("#858585ff");
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	/** Default control width in layout pixels. */
	float width = 240.0f;
	/** Default control height in layout pixels. */
	float height = 34.0f;
	/** Maximum popup viewport height in layout pixels. */
	float popupMaxHeight = 240.0f;
	/** Option row height in layout pixels. */
	float optionHeight = 32.0f;
	/** Rendered icon side length in layout pixels. */
	float iconSize = 16.0f;
	/** Popup scroll track width in layout pixels. */
	float scrollTrackWidth = 6.0f;
	/** Minimum popup scroll thumb length in layout pixels. */
	float scrollThumbMinimum = 22.0f;
	/** Font weight requested for text, such as 400 for normal. */
	uint32_t fontWeight = 400;
	/** Font style requested for text. */
	FontStyle fontStyle = FontStyle::Normal;
	/** Cursor requested while this enabled control is hovered. */
	CursorType cursor = CursorType::PointingHand;
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth = {1, 1, 1, 1, 0};
	/** Insets around trigger content in layout pixels. */
	Clay_Padding triggerPadding = {10, 8, 6, 6};
	/** Insets around option content in layout pixels. */
	Clay_Padding optionPadding = {8, 8, 4, 4};
	/** Space between adjacent content items, in layout pixels. */
	uint16_t contentGap = 8;
	/** Space between option rows in layout pixels. */
	uint16_t optionGap = 2;
	/** Space between popup content sections in layout pixels. */
	uint16_t popupGap = 4;
	/** Space reserved between option content and scroll track, in layout pixels. */
	uint16_t scrollGap = 6;
	/** Text font size in layout pixels. */
	uint16_t fontSize = 16;
	/** Priority used to arbitrate this control's cursor request. */
	uint8_t cursorPriority = 10;
};

/**
 * Theme bundle for the standard element library. Register this type with ThemeManager before
 * drawing themed FSEL elements; per-instance overrides take precedence.
 */
struct FSELTheme {
	/** Default appearance for box elements. */
	FSELBoxTheme boxTheme{};
	/** Default appearance for splitter handle elements. */
	FSELSplitterHandleTheme splitterHandleTheme{};
	/** Default appearance for button elements. */
	FSELButtonTheme buttonTheme{};
	/** Default appearance for checkbox elements. */
	FSELCheckboxTheme checkboxTheme{};
	/** Default appearance for switch elements. */
	FSELSwitchTheme switchTheme{};
	/** Default appearance for slider elements. */
	FSELSliderTheme sliderTheme{};
	/** Default appearance for progress bar elements. */
	FSELProgressBarTheme progressBarTheme{};
	/** Default appearance for label elements. */
	FSELLabelTheme labelTheme{};
	/** Default appearance for text input elements. */
	FSELTextFieldTheme textInputTheme{};
	/** Default appearance for number input elements. */
	FSELNumberInputTheme numberInputTheme{};
	/** Default appearance for drag value elements. */
	FSELDragValueTheme dragValueTheme{};
	/** Default appearance for text area elements. */
	FSELTextFieldTheme textAreaTheme = [] {
		FSELTextFieldTheme theme{};
		theme.width = 480.0f;
		theme.height = 220.0f;
		theme.padding = CLAY_PADDING_ALL(10);
		return theme;
	}();
	/** Default appearance for selectable surface elements. */
	FSELSelectableSurfaceTheme selectableSurfaceTheme{};
	/** Default appearance for popup surface elements. */
	FSELPopupSurfaceTheme popupSurfaceTheme{};
	/** Default appearance for combo box elements. */
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
					 FlowUi::devSystems::tooling::DevFieldOptions{}
						 .withEditor(FlowUi::devSystems::tooling::DevEditorKind::FontChoice)
						 .withChoiceDomain(FlowUi::devSystems::tooling::DevChoiceDomain::FontFamily)),
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
					 FlowUi::devSystems::tooling::DevFieldOptions{}
						 .withEditor(FlowUi::devSystems::tooling::DevEditorKind::FontChoice)
						 .withChoiceDomain(FlowUi::devSystems::tooling::DevChoiceDomain::FontFamily)),
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
					 FlowUi::devSystems::tooling::DevFieldOptions{}
						 .withEditor(FlowUi::devSystems::tooling::DevEditorKind::FontChoice)
						 .withChoiceDomain(FlowUi::devSystems::tooling::DevChoiceDomain::FontFamily)),
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
					 FlowUi::devSystems::tooling::DevFieldOptions{}
						 .withEditor(FlowUi::devSystems::tooling::DevEditorKind::FontChoice)
						 .withChoiceDomain(FlowUi::devSystems::tooling::DevChoiceDomain::FontFamily)),
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
