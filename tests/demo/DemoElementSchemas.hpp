#pragma once

// Demo-local reflection declarations for every FSEL element instantiated by
// the gallery. Production FSEL remains usable without developer tooling.
#if FLOW_UI_DEV_MODE

namespace FlowUi {

FLOWUI_DEV_ENUM_SCHEMA(
	CursorType,
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::Default),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::Arrow),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::IBeam),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::Crosshair),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::PointingHand),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::ResizeHorizontal),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::ResizeVertical),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::ResizeDiagonalTL),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::ResizeDiagonalTR),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::ResizeAll),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::NotAllowed),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::Wait),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::Progress),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::Grab),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::Grabbing),
	FLOWUI_DEV_ENUM_VALUE(CursorType, CursorType::Custom))

FLOWUI_DEV_ENUM_SCHEMA(
	FontStyle,
	FLOWUI_DEV_ENUM_VALUE(FontStyle, FontStyle::Normal),
	FLOWUI_DEV_ENUM_VALUE(FontStyle, FontStyle::Italic))

FLOWUI_DEV_ENUM_SCHEMA(
	InputCaretShape,
	FLOWUI_DEV_ENUM_VALUE(InputCaretShape, InputCaretShape::Bar),
	FLOWUI_DEV_ENUM_VALUE(InputCaretShape, InputCaretShape::Block),
	FLOWUI_DEV_ENUM_VALUE(InputCaretShape, InputCaretShape::Underline))

FLOWUI_DEV_ENUM_SCHEMA(
	TransactionReportDetail,
	FLOWUI_DEV_ENUM_VALUE(TransactionReportDetail, TransactionReportDetail::Summary),
	FLOWUI_DEV_ENUM_VALUE(TransactionReportDetail, TransactionReportDetail::Reversible))

FLOWUI_DEV_ENUM_SCHEMA(
	PopupAttachmentPoint,
	FLOWUI_DEV_ENUM_VALUE(PopupAttachmentPoint, PopupAttachmentPoint::TopLeft),
	FLOWUI_DEV_ENUM_VALUE(PopupAttachmentPoint, PopupAttachmentPoint::TopMiddle),
	FLOWUI_DEV_ENUM_VALUE(PopupAttachmentPoint, PopupAttachmentPoint::TopRight),
	FLOWUI_DEV_ENUM_VALUE(PopupAttachmentPoint, PopupAttachmentPoint::MiddleLeft),
	FLOWUI_DEV_ENUM_VALUE(PopupAttachmentPoint, PopupAttachmentPoint::Center),
	FLOWUI_DEV_ENUM_VALUE(PopupAttachmentPoint, PopupAttachmentPoint::MiddleRight),
	FLOWUI_DEV_ENUM_VALUE(PopupAttachmentPoint, PopupAttachmentPoint::BottomLeft),
	FLOWUI_DEV_ENUM_VALUE(PopupAttachmentPoint, PopupAttachmentPoint::BottomMiddle),
	FLOWUI_DEV_ENUM_VALUE(PopupAttachmentPoint, PopupAttachmentPoint::BottomRight))

FLOWUI_DEV_ENUM_SCHEMA(
	PopupAnchorKind,
	FLOWUI_DEV_ENUM_VALUE(PopupAnchorKind, PopupAnchorKind::Parent),
	FLOWUI_DEV_ENUM_VALUE(PopupAnchorKind, PopupAnchorKind::Element),
	FLOWUI_DEV_ENUM_VALUE(PopupAnchorKind, PopupAnchorKind::PointerSnapshot),
	FLOWUI_DEV_ENUM_VALUE(PopupAnchorKind, PopupAnchorKind::PointerFollow),
	FLOWUI_DEV_ENUM_VALUE(PopupAnchorKind, PopupAnchorKind::Position),
	FLOWUI_DEV_ENUM_VALUE(PopupAnchorKind, PopupAnchorKind::Viewport))

FLOWUI_DEV_ENUM_SCHEMA(
	PopupLayer,
	FLOWUI_DEV_ENUM_VALUE(PopupLayer, PopupLayer::CasualPopup),
	FLOWUI_DEV_ENUM_VALUE(PopupLayer, PopupLayer::WarningPopup),
	FLOWUI_DEV_ENUM_VALUE(PopupLayer, PopupLayer::CriticalPopup))

FLOWUI_DEV_ENUM_SCHEMA(
	PopupFirstFramePolicy,
	FLOWUI_DEV_ENUM_VALUE(PopupFirstFramePolicy, PopupFirstFramePolicy::DeferUntilMeasured),
	FLOWUI_DEV_ENUM_VALUE(PopupFirstFramePolicy, PopupFirstFramePolicy::PlaceImmediately))

FLOWUI_DEV_ENUM_SCHEMA(
	PopupOutsidePressPolicy,
	FLOWUI_DEV_ENUM_VALUE(PopupOutsidePressPolicy, PopupOutsidePressPolicy::DismissAndConsume),
	FLOWUI_DEV_ENUM_VALUE(PopupOutsidePressPolicy, PopupOutsidePressPolicy::DismissAndBlockAnchor),
	FLOWUI_DEV_ENUM_VALUE(PopupOutsidePressPolicy, PopupOutsidePressPolicy::Ignore))

FLOWUI_DEV_SCHEMA(
	PopupPlacement,
	FLOWUI_DEV_FIELD(PopupPlacement, anchorPoint),
	FLOWUI_DEV_FIELD(PopupPlacement, popupPoint),
	FLOWUI_DEV_FIELD(PopupPlacement, offset))

FLOWUI_DEV_SCHEMA(
	PopupAnchor,
	FLOWUI_DEV_FIELD(PopupAnchor, kind),
	FLOWUI_DEV_FIELD(PopupAnchor, elementValue),
	FLOWUI_DEV_FIELD(PopupAnchor, coordinate))

FLOWUI_DEV_SCHEMA(
	PopupOverflowPolicy,
	FLOWUI_DEV_FIELD(PopupOverflowPolicy, flipX),
	FLOWUI_DEV_FIELD(PopupOverflowPolicy, flipY),
	FLOWUI_DEV_FIELD(PopupOverflowPolicy, shiftToFit))

FLOWUI_DEV_SCHEMA(
	PopupRequest,
	FLOWUI_DEV_FIELD(PopupRequest, anchor),
	FLOWUI_DEV_FIELD(PopupRequest, placement),
	FLOWUI_DEV_FIELD(PopupRequest, overflow),
	FLOWUI_DEV_FIELD(PopupRequest, firstFrame),
	FLOWUI_DEV_FIELD(PopupRequest, expectedSize),
	FLOWUI_DEV_FIELD(PopupRequest, layer),
	FLOWUI_DEV_FIELD(PopupRequest, pointerCaptureMode),
	FLOWUI_DEV_FIELD(PopupRequest, clipTo),
	FLOWUI_DEV_FIELD(PopupRequest, outsidePress),
	FLOWUI_DEV_FIELD(PopupRequest, dismissOnEscape))

} // namespace FlowUi

namespace FlowUi::FSEL {

#define FLOWUI_DEMO_COLOR_FIELD(TYPE, MEMBER) \
	FLOWUI_DEV_FIELD( \
		TYPE, MEMBER, \
		::FlowUi::devMode::DevFieldOptions{}.withEditor( \
			::FlowUi::devMode::DevEditorKind::Color))

#define FLOWUI_DEMO_PADDING_FIELD(TYPE, MEMBER) \
	FLOWUI_DEV_FIELD( \
		TYPE, MEMBER, \
		::FlowUi::devMode::DevFieldOptions{}.withEditor( \
			::FlowUi::devMode::DevEditorKind::Spacing))

#define FLOWUI_DEMO_BORDER_FIELD(TYPE, MEMBER) \
	FLOWUI_DEV_FIELD( \
		TYPE, MEMBER, \
		::FlowUi::devMode::DevFieldOptions{}.withEditor( \
			::FlowUi::devMode::DevEditorKind::Spacing))

#define FLOWUI_DEMO_CORNER_FIELD(TYPE, MEMBER) \
	FLOWUI_DEV_FIELD( \
		TYPE, MEMBER, \
		::FlowUi::devMode::DevFieldOptions{}.withEditor( \
			::FlowUi::devMode::DevEditorKind::CornerRadius))

FLOWUI_DEV_ENUM_SCHEMA(
	ButtonContentMode,
	FLOWUI_DEV_ENUM_VALUE(ButtonContentMode, ButtonContentMode::None),
	FLOWUI_DEV_ENUM_VALUE(ButtonContentMode, ButtonContentMode::TextOnly),
	FLOWUI_DEV_ENUM_VALUE(ButtonContentMode, ButtonContentMode::IconOnly),
	FLOWUI_DEV_ENUM_VALUE(ButtonContentMode, ButtonContentMode::IconThenText),
	FLOWUI_DEV_ENUM_VALUE(ButtonContentMode, ButtonContentMode::TextThenIcon))

FLOWUI_DEV_ENUM_SCHEMA(
	ComboBoxPopupWidthPolicy,
	FLOWUI_DEV_ENUM_VALUE(ComboBoxPopupWidthPolicy, ComboBoxPopupWidthPolicy::MatchTrigger),
	FLOWUI_DEV_ENUM_VALUE(ComboBoxPopupWidthPolicy, ComboBoxPopupWidthPolicy::ContentAtLeastTrigger),
	FLOWUI_DEV_ENUM_VALUE(ComboBoxPopupWidthPolicy, ComboBoxPopupWidthPolicy::Fixed))

FLOWUI_DEV_ENUM_SCHEMA(
	SliderAxis,
	FLOWUI_DEV_ENUM_VALUE(SliderAxis, SliderAxis::Horizontal),
	FLOWUI_DEV_ENUM_VALUE(SliderAxis, SliderAxis::Vertical))

FLOWUI_DEV_ENUM_SCHEMA(
	SliderPressBehavior,
	FLOWUI_DEV_ENUM_VALUE(SliderPressBehavior, SliderPressBehavior::JumpToPointer),
	FLOWUI_DEV_ENUM_VALUE(SliderPressBehavior, SliderPressBehavior::DragFromCurrent))

FLOWUI_DEV_ENUM_SCHEMA(
	SliderVisualParts,
	FLOWUI_DEV_ENUM_VALUE(SliderVisualParts, SliderVisualParts::None),
	FLOWUI_DEV_ENUM_VALUE(SliderVisualParts, SliderVisualParts::Track),
	FLOWUI_DEV_ENUM_VALUE(SliderVisualParts, SliderVisualParts::Fill),
	FLOWUI_DEV_ENUM_VALUE(SliderVisualParts, SliderVisualParts::Thumb),
	FLOWUI_DEV_ENUM_VALUE(SliderVisualParts, SliderVisualParts::All))

FLOWUI_DEV_ENUM_SCHEMA(
	ProgressBarAxis,
	FLOWUI_DEV_ENUM_VALUE(ProgressBarAxis, ProgressBarAxis::Horizontal),
	FLOWUI_DEV_ENUM_VALUE(ProgressBarAxis, ProgressBarAxis::Vertical))

FLOWUI_DEV_ENUM_SCHEMA(
	SplitterAxis,
	FLOWUI_DEV_ENUM_VALUE(SplitterAxis, SplitterAxis::Horizontal),
	FLOWUI_DEV_ENUM_VALUE(SplitterAxis, SplitterAxis::Vertical))

FLOWUI_DEV_ENUM_SCHEMA(
	SplitterPosition,
	FLOWUI_DEV_ENUM_VALUE(SplitterPosition, SplitterPosition::Leading),
	FLOWUI_DEV_ENUM_VALUE(SplitterPosition, SplitterPosition::Trailing))

FLOWUI_DEV_ENUM_SCHEMA(
	TextFieldSyncPolicy,
	FLOWUI_DEV_ENUM_VALUE(TextFieldSyncPolicy, TextFieldSyncPolicy::Live),
	FLOWUI_DEV_ENUM_VALUE(TextFieldSyncPolicy, TextFieldSyncPolicy::OnCommit),
	FLOWUI_DEV_ENUM_VALUE(TextFieldSyncPolicy, TextFieldSyncPolicy::ReadOnly))

FLOWUI_DEV_ENUM_SCHEMA(
	NumericTextSyncPolicy,
	FLOWUI_DEV_ENUM_VALUE(NumericTextSyncPolicy, NumericTextSyncPolicy::Live),
	FLOWUI_DEV_ENUM_VALUE(NumericTextSyncPolicy, NumericTextSyncPolicy::OnCommit))

FLOWUI_DEV_ENUM_SCHEMA(
	NumericTextBoundsPolicy,
	FLOWUI_DEV_ENUM_VALUE(NumericTextBoundsPolicy, NumericTextBoundsPolicy::SoftClamp),
	FLOWUI_DEV_ENUM_VALUE(NumericTextBoundsPolicy, NumericTextBoundsPolicy::ClampTextImmediately))

FLOWUI_DEV_ENUM_SCHEMA(
	NumericTextStatus,
	FLOWUI_DEV_ENUM_VALUE(NumericTextStatus, NumericTextStatus::Complete),
	FLOWUI_DEV_ENUM_VALUE(NumericTextStatus, NumericTextStatus::Incomplete),
	FLOWUI_DEV_ENUM_VALUE(NumericTextStatus, NumericTextStatus::BelowMinimum),
	FLOWUI_DEV_ENUM_VALUE(NumericTextStatus, NumericTextStatus::AboveMaximum),
	FLOWUI_DEV_ENUM_VALUE(NumericTextStatus, NumericTextStatus::Overflow),
	FLOWUI_DEV_ENUM_VALUE(NumericTextStatus, NumericTextStatus::ConfigurationError))

FLOWUI_DEV_ENUM_SCHEMA(
	NumericFloatNotation,
	FLOWUI_DEV_ENUM_VALUE(NumericFloatNotation, NumericFloatNotation::General),
	FLOWUI_DEV_ENUM_VALUE(NumericFloatNotation, NumericFloatNotation::Fixed),
	FLOWUI_DEV_ENUM_VALUE(NumericFloatNotation, NumericFloatNotation::Scientific))

FLOWUI_DEV_ENUM_SCHEMA(
	NumberInputStepButtons,
	FLOWUI_DEV_ENUM_VALUE(NumberInputStepButtons, NumberInputStepButtons::None),
	FLOWUI_DEV_ENUM_VALUE(NumberInputStepButtons, NumberInputStepButtons::TrailingVertical),
	FLOWUI_DEV_ENUM_VALUE(NumberInputStepButtons, NumberInputStepButtons::TrailingHorizontal))

FLOWUI_DEV_ENUM_SCHEMA(
	DragValueInteractionMode,
	FLOWUI_DEV_ENUM_VALUE(DragValueInteractionMode, DragValueInteractionMode::Idle),
	FLOWUI_DEV_ENUM_VALUE(DragValueInteractionMode, DragValueInteractionMode::Pending),
	FLOWUI_DEV_ENUM_VALUE(DragValueInteractionMode, DragValueInteractionMode::Dragging),
	FLOWUI_DEV_ENUM_VALUE(DragValueInteractionMode, DragValueInteractionMode::Editing))

FLOWUI_DEV_SCHEMA(
	ButtonStateOverrides,
	FLOWUI_DEMO_COLOR_FIELD(ButtonStateOverrides, backgroundColor),
	FLOWUI_DEMO_COLOR_FIELD(ButtonStateOverrides, labelColor),
	FLOWUI_DEMO_COLOR_FIELD(ButtonStateOverrides, iconColor),
	FLOWUI_DEMO_COLOR_FIELD(ButtonStateOverrides, borderColor))

FLOWUI_DEV_SCHEMA(
	SelectableSurfaceStateOverrides,
	FLOWUI_DEMO_COLOR_FIELD(SelectableSurfaceStateOverrides, backgroundColor),
	FLOWUI_DEMO_COLOR_FIELD(SelectableSurfaceStateOverrides, borderColor))

FLOWUI_DEV_SCHEMA(
	SelectableSurfaceStyle,
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, sizing),
	FLOWUI_DEMO_PADDING_FIELD(SelectableSurfaceStyle, padding),
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, childAlignment),
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, layoutDirection),
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, contentGap),
	FLOWUI_DEMO_BORDER_FIELD(SelectableSurfaceStyle, borderWidth),
	FLOWUI_DEMO_CORNER_FIELD(SelectableSurfaceStyle, cornerRadius),
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, idleOverrides),
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, hoveredOverrides),
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, pressedOverrides),
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, selectedOverrides),
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, disabledOverrides),
	FLOWUI_DEV_FIELD(SelectableSurfaceStyle, selectedDisabledOverrides))

FLOWUI_DEV_SCHEMA(
	CheckboxStateOverrides,
	FLOWUI_DEMO_COLOR_FIELD(CheckboxStateOverrides, backgroundColor),
	FLOWUI_DEMO_COLOR_FIELD(CheckboxStateOverrides, borderColor),
	FLOWUI_DEMO_COLOR_FIELD(CheckboxStateOverrides, iconColor))

FLOWUI_DEV_SCHEMA(
	CheckboxValueOverrides,
	FLOWUI_DEV_FIELD(CheckboxValueOverrides, idle),
	FLOWUI_DEV_FIELD(CheckboxValueOverrides, hovered),
	FLOWUI_DEV_FIELD(CheckboxValueOverrides, pressed),
	FLOWUI_DEV_FIELD(CheckboxValueOverrides, disabled))

FLOWUI_DEV_SCHEMA(
	SwitchStateOverrides,
	FLOWUI_DEMO_COLOR_FIELD(SwitchStateOverrides, trackColor),
	FLOWUI_DEMO_COLOR_FIELD(SwitchStateOverrides, trackBorderColor),
	FLOWUI_DEMO_COLOR_FIELD(SwitchStateOverrides, knobColor),
	FLOWUI_DEMO_COLOR_FIELD(SwitchStateOverrides, knobBorderColor))

FLOWUI_DEV_SCHEMA(
	SwitchValueOverrides,
	FLOWUI_DEV_FIELD(SwitchValueOverrides, idle),
	FLOWUI_DEV_FIELD(SwitchValueOverrides, hovered),
	FLOWUI_DEV_FIELD(SwitchValueOverrides, pressed),
	FLOWUI_DEV_FIELD(SwitchValueOverrides, disabled))

FLOWUI_DEV_SCHEMA(
	SliderStateOverrides,
	FLOWUI_DEMO_COLOR_FIELD(SliderStateOverrides, trackColor),
	FLOWUI_DEMO_COLOR_FIELD(SliderStateOverrides, trackBorderColor),
	FLOWUI_DEMO_COLOR_FIELD(SliderStateOverrides, fillColor),
	FLOWUI_DEMO_COLOR_FIELD(SliderStateOverrides, fillBorderColor),
	FLOWUI_DEMO_COLOR_FIELD(SliderStateOverrides, thumbColor),
	FLOWUI_DEMO_COLOR_FIELD(SliderStateOverrides, thumbBorderColor))

FLOWUI_DEV_SCHEMA(
	TextFieldActions,
	FLOWUI_DEV_FIELD(TextFieldActions, onChanged),
	FLOWUI_DEV_FIELD(TextFieldActions, onCommit),
	FLOWUI_DEV_FIELD(TextFieldActions, onSubmit),
	FLOWUI_DEV_FIELD(TextFieldActions, onFocus),
	FLOWUI_DEV_FIELD(TextFieldActions, onBlur),
	FLOWUI_DEV_FIELD(TextFieldActions, onUndoRequested),
	FLOWUI_DEV_FIELD(TextFieldActions, onRedoRequested))

FLOWUI_DEV_SCHEMA(
	TextFieldStateOverrides,
	FLOWUI_DEMO_COLOR_FIELD(TextFieldStateOverrides, backgroundColor),
	FLOWUI_DEMO_COLOR_FIELD(TextFieldStateOverrides, textColor),
	FLOWUI_DEMO_COLOR_FIELD(TextFieldStateOverrides, placeholderColor),
	FLOWUI_DEMO_COLOR_FIELD(TextFieldStateOverrides, borderColor))

FLOWUI_DEV_SCHEMA(
	TextFieldCaretOverrides,
	FLOWUI_DEV_FIELD(TextFieldCaretOverrides, shape),
	FLOWUI_DEV_FIELD(TextFieldCaretOverrides, thicknessPx),
	FLOWUI_DEV_FIELD(TextFieldCaretOverrides, blockWidthPx),
	FLOWUI_DEV_FIELD(TextFieldCaretOverrides, heightOverflowTopPx),
	FLOWUI_DEV_FIELD(TextFieldCaretOverrides, heightOverflowBottomPx),
	FLOWUI_DEMO_COLOR_FIELD(TextFieldCaretOverrides, color),
	FLOWUI_DEMO_COLOR_FIELD(TextFieldCaretOverrides, selectionBoxColor),
	FLOWUI_DEMO_COLOR_FIELD(TextFieldCaretOverrides, selectedTextColor),
	FLOWUI_DEV_FIELD(TextFieldCaretOverrides, blinkPeriodSeconds),
	FLOWUI_DEV_FIELD(TextFieldCaretOverrides, blinkVisibleSeconds))

FLOWUI_DEV_SCHEMA(
	NumericEditActions,
	FLOWUI_DEV_FIELD(NumericEditActions, onBegin),
	FLOWUI_DEV_FIELD(NumericEditActions, onChanged),
	FLOWUI_DEV_FIELD(NumericEditActions, onCommit),
	FLOWUI_DEV_FIELD(NumericEditActions, onCancel))

FLOWUI_DEV_SCHEMA(
	NumericFormatOptions,
	FLOWUI_DEV_FIELD(NumericFormatOptions, notation),
	FLOWUI_DEV_FIELD(NumericFormatOptions, precision),
	FLOWUI_DEV_FIELD(NumericFormatOptions, trimTrailingZeros),
	FLOWUI_DEV_FIELD(NumericFormatOptions, canonicalizeNegativeZero),
	FLOWUI_DEV_FIELD(NumericFormatOptions, allowScientificInput))

#define FLOWUI_DEMO_NUMERIC_EDIT_SESSION(TYPE) \
	FLOWUI_DEV_SCHEMA( \
		NumericEditSession<TYPE>, \
		FLOWUI_DEV_FIELD(NumericEditSession<TYPE>, active), \
		FLOWUI_DEV_FIELD(NumericEditSession<TYPE>, valueChanged), \
		FLOWUI_DEV_FIELD(NumericEditSession<TYPE>, hasLastValidValue), \
		FLOWUI_DEV_FIELD(NumericEditSession<TYPE>, beginValue), \
		FLOWUI_DEV_FIELD(NumericEditSession<TYPE>, lastValidValue))

FLOWUI_DEMO_NUMERIC_EDIT_SESSION(int)
FLOWUI_DEMO_NUMERIC_EDIT_SESSION(unsigned int)
FLOWUI_DEMO_NUMERIC_EDIT_SESSION(float)

#undef FLOWUI_DEMO_NUMERIC_EDIT_SESSION

FLOWUI_DEV_SCHEMA(
	ComboBoxOption,
	FLOWUI_DEV_FIELD(ComboBoxOption, value),
	FLOWUI_DEV_FIELD(ComboBoxOption, text),
	FLOWUI_DEV_FIELD(ComboBoxOption, icon),
	FLOWUI_DEV_FIELD(ComboBoxOption, enabled))

FLOWUI_DEV_SCHEMA(
	BoxParameters,
	FLOWUI_DEV_FIELD(BoxParameters, sizing),
	FLOWUI_DEV_FIELD(BoxParameters, padding),
	FLOWUI_DEV_FIELD(BoxParameters, childGap),
	FLOWUI_DEV_FIELD(BoxParameters, childAlignment),
	FLOWUI_DEV_FIELD(BoxParameters, layoutDirection),
	FLOWUI_DEV_FIELD(BoxParameters, clipConfig),
	FLOWUI_DEMO_COLOR_FIELD(BoxParameters, backgroundColor),
	FLOWUI_DEMO_COLOR_FIELD(BoxParameters, borderColor),
	FLOWUI_DEMO_CORNER_FIELD(BoxParameters, cornerRadius),
	FLOWUI_DEMO_BORDER_FIELD(BoxParameters, borderWidth))

FLOWUI_DEV_SCHEMA(
	ButtonParameters,
	FLOWUI_DEV_FIELD(ButtonParameters, onActivate),
	FLOWUI_DEV_FIELD(ButtonParameters, enabled),
	FLOWUI_DEV_FIELD(ButtonParameters, contentMode),
	FLOWUI_DEV_FIELD(ButtonParameters, text),
	FLOWUI_DEV_FIELD(ButtonParameters, icon),
	FLOWUI_DEV_FIELD(ButtonParameters, tintIcon),
	FLOWUI_DEV_FIELD(ButtonParameters, sizing),
	FLOWUI_DEMO_PADDING_FIELD(ButtonParameters, padding),
	FLOWUI_DEV_FIELD(ButtonParameters, childAlignment),
	FLOWUI_DEV_FIELD(ButtonParameters, layoutDirection),
	FLOWUI_DEV_FIELD(ButtonParameters, contentGap),
	FLOWUI_DEMO_BORDER_FIELD(ButtonParameters, borderWidth),
	FLOWUI_DEMO_CORNER_FIELD(ButtonParameters, cornerRadius),
	FLOWUI_DEV_FIELD(ButtonParameters, idleOverrides),
	FLOWUI_DEV_FIELD(ButtonParameters, hoveredOverrides),
	FLOWUI_DEV_FIELD(ButtonParameters, pressedOverrides),
	FLOWUI_DEV_FIELD(ButtonParameters, disabledOverrides),
	FLOWUI_DEV_FIELD(ButtonParameters, labelFontFamily),
	FLOWUI_DEV_FIELD(ButtonParameters, labelFontWeight),
	FLOWUI_DEV_FIELD(ButtonParameters, labelFontStyle),
	FLOWUI_DEV_FIELD(ButtonParameters, labelFontSize),
	FLOWUI_DEV_FIELD(ButtonParameters, labelWrapMode),
	FLOWUI_DEV_FIELD(ButtonParameters, labelAlignment),
	FLOWUI_DEV_FIELD(ButtonParameters, iconSize),
	FLOWUI_DEV_FIELD(ButtonParameters, cursor),
	FLOWUI_DEV_FIELD(ButtonParameters, cursorPriority))

FLOWUI_DEV_SCHEMA(
	ButtonState,
	FLOWUI_DEV_FIELD(ButtonState, isArmed),
	FLOWUI_DEV_FIELD(ButtonState, mouseUpObserved))

FLOWUI_DEV_SCHEMA(
	SelectableSurfaceParameters,
	FLOWUI_DEV_FIELD(SelectableSurfaceParameters, selected),
	FLOWUI_DEV_FIELD(SelectableSurfaceParameters, enabled),
	FLOWUI_DEV_FIELD(SelectableSurfaceParameters, onSelected),
	FLOWUI_DEV_FIELD(SelectableSurfaceParameters, style))

FLOWUI_DEV_SCHEMA(
	RadioChoiceParameters,
	FLOWUI_DEV_FIELD(RadioChoiceParameters, choiceValue),
	FLOWUI_DEV_FIELD(RadioChoiceParameters, selectedValue),
	FLOWUI_DEV_FIELD(RadioChoiceParameters, enabled),
	FLOWUI_DEV_FIELD(RadioChoiceParameters, onSelected),
	FLOWUI_DEV_FIELD(RadioChoiceParameters, style))

FLOWUI_DEV_SCHEMA(
	CheckboxParameters,
	FLOWUI_DEV_FIELD(CheckboxParameters, isChecked),
	FLOWUI_DEV_FIELD(CheckboxParameters, enabled),
	FLOWUI_DEV_FIELD(CheckboxParameters, onToggle),
	FLOWUI_DEV_FIELD(CheckboxParameters, uncheckedIcon),
	FLOWUI_DEV_FIELD(CheckboxParameters, checkedIcon),
	FLOWUI_DEV_FIELD(CheckboxParameters, tintIcon),
	FLOWUI_DEV_FIELD(CheckboxParameters, size),
	FLOWUI_DEV_FIELD(CheckboxParameters, iconSize),
	FLOWUI_DEMO_BORDER_FIELD(CheckboxParameters, borderWidth),
	FLOWUI_DEMO_CORNER_FIELD(CheckboxParameters, cornerRadius),
	FLOWUI_DEV_FIELD(CheckboxParameters, uncheckedOverrides),
	FLOWUI_DEV_FIELD(CheckboxParameters, checkedOverrides),
	FLOWUI_DEV_FIELD(CheckboxParameters, cursor),
	FLOWUI_DEV_FIELD(CheckboxParameters, cursorPriority))

FLOWUI_DEV_SCHEMA(
	SwitchParameters,
	FLOWUI_DEV_FIELD(SwitchParameters, isOn),
	FLOWUI_DEV_FIELD(SwitchParameters, enabled),
	FLOWUI_DEV_FIELD(SwitchParameters, onToggle),
	FLOWUI_DEV_FIELD(SwitchParameters, trackWidth),
	FLOWUI_DEV_FIELD(SwitchParameters, trackHeight),
	FLOWUI_DEV_FIELD(SwitchParameters, knobInset),
	FLOWUI_DEV_FIELD(SwitchParameters, roundness),
	FLOWUI_DEMO_BORDER_FIELD(SwitchParameters, trackBorderWidth),
	FLOWUI_DEMO_BORDER_FIELD(SwitchParameters, knobBorderWidth),
	FLOWUI_DEV_FIELD(SwitchParameters, offOverrides),
	FLOWUI_DEV_FIELD(SwitchParameters, onOverrides),
	FLOWUI_DEV_FIELD(SwitchParameters, cursor),
	FLOWUI_DEV_FIELD(SwitchParameters, cursorPriority))

FLOWUI_DEV_SCHEMA(
	SliderParameters,
	FLOWUI_DEV_FIELD(SliderParameters, axis),
	FLOWUI_DEV_FIELD(SliderParameters, pressBehavior),
	FLOWUI_DEV_FIELD(SliderParameters, inverted),
	FLOWUI_DEV_FIELD(SliderParameters, value),
	FLOWUI_DEV_FIELD(SliderParameters, minimum),
	FLOWUI_DEV_FIELD(SliderParameters, maximum),
	FLOWUI_DEV_FIELD(SliderParameters, roundingStep),
	FLOWUI_DEV_FIELD(SliderParameters, enabled),
	FLOWUI_DEV_FIELD(SliderParameters, onBegin),
	FLOWUI_DEV_FIELD(SliderParameters, onChanged),
	FLOWUI_DEV_FIELD(SliderParameters, onCommit),
	FLOWUI_DEV_FIELD(SliderParameters, visualParts),
	FLOWUI_DEV_FIELD(SliderParameters, length),
	FLOWUI_DEV_FIELD(SliderParameters, hitThickness),
	FLOWUI_DEV_FIELD(SliderParameters, trackThickness),
	FLOWUI_DEV_FIELD(SliderParameters, thumbLength),
	FLOWUI_DEV_FIELD(SliderParameters, thumbThickness),
	FLOWUI_DEV_FIELD(SliderParameters, trackRoundness),
	FLOWUI_DEV_FIELD(SliderParameters, thumbRoundness),
	FLOWUI_DEMO_BORDER_FIELD(SliderParameters, trackBorderWidth),
	FLOWUI_DEMO_BORDER_FIELD(SliderParameters, fillBorderWidth),
	FLOWUI_DEMO_BORDER_FIELD(SliderParameters, thumbBorderWidth),
	FLOWUI_DEV_FIELD(SliderParameters, idleOverrides),
	FLOWUI_DEV_FIELD(SliderParameters, hoveredOverrides),
	FLOWUI_DEV_FIELD(SliderParameters, draggingOverrides),
	FLOWUI_DEV_FIELD(SliderParameters, disabledOverrides),
	FLOWUI_DEV_FIELD(SliderParameters, cursor),
	FLOWUI_DEV_FIELD(SliderParameters, draggingCursor),
	FLOWUI_DEV_FIELD(SliderParameters, cursorPriority))

FLOWUI_DEV_SCHEMA(
	SliderState,
	FLOWUI_DEV_FIELD(SliderState, isDragging),
	FLOWUI_DEV_FIELD(SliderState, pointerAtPress),
	FLOWUI_DEV_FIELD(SliderState, valueAtPress))

FLOWUI_DEV_SCHEMA(
	ProgressBarParameters,
	FLOWUI_DEV_FIELD(ProgressBarParameters, axis),
	FLOWUI_DEV_FIELD(ProgressBarParameters, inverted),
	FLOWUI_DEV_FIELD(ProgressBarParameters, value),
	FLOWUI_DEV_FIELD(ProgressBarParameters, minimum),
	FLOWUI_DEV_FIELD(ProgressBarParameters, maximum),
	FLOWUI_DEV_FIELD(ProgressBarParameters, sizing),
	FLOWUI_DEMO_COLOR_FIELD(ProgressBarParameters, baseColor),
	FLOWUI_DEMO_COLOR_FIELD(ProgressBarParameters, borderColor),
	FLOWUI_DEMO_COLOR_FIELD(ProgressBarParameters, fillColor),
	FLOWUI_DEMO_BORDER_FIELD(ProgressBarParameters, borderWidth),
	FLOWUI_DEMO_CORNER_FIELD(ProgressBarParameters, cornerRadius))

FLOWUI_DEV_SCHEMA(
	SplitterHandleParameters,
	FLOWUI_DEV_FIELD(SplitterHandleParameters, axis),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, position),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, targetExtent),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, minExtent),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, maxExtent),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, thickness),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, hitThickness),
	FLOWUI_DEMO_COLOR_FIELD(SplitterHandleParameters, backgroundColor),
	FLOWUI_DEMO_COLOR_FIELD(SplitterHandleParameters, hoverColor),
	FLOWUI_DEMO_COLOR_FIELD(SplitterHandleParameters, draggingColor),
	FLOWUI_DEMO_COLOR_FIELD(SplitterHandleParameters, borderColor),
	FLOWUI_DEMO_BORDER_FIELD(SplitterHandleParameters, borderWidth),
	FLOWUI_DEMO_CORNER_FIELD(SplitterHandleParameters, cornerRadius),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, horizontalCursor),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, verticalCursor),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, cursorPriority))

FLOWUI_DEV_SCHEMA(
	SplitterHandleState,
	FLOWUI_DEV_FIELD(SplitterHandleState, isDragging),
	FLOWUI_DEV_FIELD(SplitterHandleState, pointerAtPress),
	FLOWUI_DEV_FIELD(SplitterHandleState, extentAtPress))

FLOWUI_DEV_SCHEMA(
	PopupSurfaceParameters,
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, popupRequest),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, onDismissed),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, sizing),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, clipConfig),
	FLOWUI_DEMO_PADDING_FIELD(PopupSurfaceParameters, padding),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, childGap),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, childAlignment),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, layoutDirection),
	FLOWUI_DEMO_COLOR_FIELD(PopupSurfaceParameters, backgroundColor),
	FLOWUI_DEMO_COLOR_FIELD(PopupSurfaceParameters, borderColor),
	FLOWUI_DEMO_BORDER_FIELD(PopupSurfaceParameters, borderWidth),
	FLOWUI_DEMO_CORNER_FIELD(PopupSurfaceParameters, cornerRadius))

FLOWUI_DEV_SCHEMA(
	ComboBoxParameters,
	FLOWUI_DEV_FIELD(ComboBoxParameters, options),
	FLOWUI_DEV_FIELD(ComboBoxParameters, selectedValue),
	FLOWUI_DEV_FIELD(ComboBoxParameters, open),
	FLOWUI_DEV_FIELD(ComboBoxParameters, placeholder),
	FLOWUI_DEV_FIELD(ComboBoxParameters, enabled),
	FLOWUI_DEV_FIELD(ComboBoxParameters, onChanged),
	FLOWUI_DEV_FIELD(ComboBoxParameters, onOpened),
	FLOWUI_DEV_FIELD(ComboBoxParameters, onClosed),
	FLOWUI_DEV_FIELD(ComboBoxParameters, popupSearchValue),
	FLOWUI_DEV_FIELD(ComboBoxParameters, popupSearchPlaceholder),
	FLOWUI_DEV_FIELD(ComboBoxParameters, popupSearchHeight),
	FLOWUI_DEV_FIELD(ComboBoxParameters, sizing),
	FLOWUI_DEV_FIELD(ComboBoxParameters, popupWidthPolicy),
	FLOWUI_DEV_FIELD(ComboBoxParameters, popupWidth),
	FLOWUI_DEV_FIELD(ComboBoxParameters, popupMaxHeight),
	FLOWUI_DEV_FIELD(ComboBoxParameters, optionHeight),
	FLOWUI_DEV_FIELD(ComboBoxParameters, showScrollIndicator),
	FLOWUI_DEV_FIELD(ComboBoxParameters, placement),
	FLOWUI_DEV_FIELD(ComboBoxParameters, overflow),
	FLOWUI_DEV_FIELD(ComboBoxParameters, layer),
	FLOWUI_DEV_FIELD(ComboBoxParameters, outsidePress),
	FLOWUI_DEV_FIELD(ComboBoxParameters, pointerCaptureMode),
	FLOWUI_DEV_FIELD(ComboBoxParameters, closedIcon),
	FLOWUI_DEV_FIELD(ComboBoxParameters, openIcon),
	FLOWUI_DEV_FIELD(ComboBoxParameters, iconSize),
	FLOWUI_DEV_FIELD(ComboBoxParameters, fontFamily),
	FLOWUI_DEV_FIELD(ComboBoxParameters, fontWeight),
	FLOWUI_DEV_FIELD(ComboBoxParameters, fontStyle),
	FLOWUI_DEV_FIELD(ComboBoxParameters, fontSize),
	FLOWUI_DEV_FIELD(ComboBoxParameters, cursor),
	FLOWUI_DEV_FIELD(ComboBoxParameters, cursorPriority))

FLOWUI_DEV_SCHEMA(
	ComboBoxState,
	FLOWUI_DEV_FIELD(ComboBoxState, open),
	FLOWUI_DEV_FIELD(ComboBoxState, triggerArmed),
	FLOWUI_DEV_FIELD(ComboBoxState, triggerMouseUpObserved),
	FLOWUI_DEV_FIELD(ComboBoxState, optionArmed),
	FLOWUI_DEV_FIELD(ComboBoxState, optionMouseUpObserved),
	FLOWUI_DEV_FIELD(ComboBoxState, armedOptionValue))

FLOWUI_DEV_SCHEMA(
	ComboBoxResources,
	FLOWUI_DEV_FIELD(ComboBoxResources, openIcon),
	FLOWUI_DEV_FIELD(ComboBoxResources, closedIcon))

FLOWUI_DEV_SCHEMA(SpacerParameters, FLOWUI_DEV_FIELD(SpacerParameters, sizing))

FLOWUI_DEV_SCHEMA(
	TextInputParameters,
	FLOWUI_DEV_FIELD(TextInputParameters, value),
	FLOWUI_DEV_FIELD(TextInputParameters, syncPolicy),
	FLOWUI_DEV_FIELD(TextInputParameters, actions),
	FLOWUI_DEV_FIELD(TextInputParameters, enabled),
	FLOWUI_DEV_FIELD(TextInputParameters, readOnly),
	FLOWUI_DEV_FIELD(TextInputParameters, valid),
	FLOWUI_DEV_FIELD(TextInputParameters, clearFocusOnSubmit),
	FLOWUI_DEV_FIELD(TextInputParameters, maxBytes),
	FLOWUI_DEV_FIELD(TextInputParameters, transactionDetail),
	FLOWUI_DEV_FIELD(TextInputParameters, placeholder),
	FLOWUI_DEV_FIELD(TextInputParameters, sizing),
	FLOWUI_DEV_FIELD(TextInputParameters, viewportWidth),
	FLOWUI_DEV_FIELD(TextInputParameters, viewportHeight),
	FLOWUI_DEMO_PADDING_FIELD(TextInputParameters, padding),
	FLOWUI_DEMO_BORDER_FIELD(TextInputParameters, borderWidth),
	FLOWUI_DEMO_CORNER_FIELD(TextInputParameters, cornerRadius),
	FLOWUI_DEV_FIELD(TextInputParameters, idleOverrides),
	FLOWUI_DEV_FIELD(TextInputParameters, hoveredOverrides),
	FLOWUI_DEV_FIELD(TextInputParameters, focusedOverrides),
	FLOWUI_DEV_FIELD(TextInputParameters, readOnlyOverrides),
	FLOWUI_DEV_FIELD(TextInputParameters, invalidOverrides),
	FLOWUI_DEV_FIELD(TextInputParameters, disabledOverrides),
	FLOWUI_DEV_FIELD(TextInputParameters, fontFamily),
	FLOWUI_DEV_FIELD(TextInputParameters, fontWeight),
	FLOWUI_DEV_FIELD(TextInputParameters, fontStyle),
	FLOWUI_DEV_FIELD(TextInputParameters, fontSize),
	FLOWUI_DEV_FIELD(TextInputParameters, letterSpacing),
	FLOWUI_DEV_FIELD(TextInputParameters, caret),
	FLOWUI_DEV_FIELD(TextInputParameters, cursor),
	FLOWUI_DEV_FIELD(TextInputParameters, cursorPriority))

FLOWUI_DEV_SCHEMA(
	TextAreaParameters,
	FLOWUI_DEV_FIELD(TextAreaParameters, value),
	FLOWUI_DEV_FIELD(TextAreaParameters, syncPolicy),
	FLOWUI_DEV_FIELD(TextAreaParameters, actions),
	FLOWUI_DEV_FIELD(TextAreaParameters, enabled),
	FLOWUI_DEV_FIELD(TextAreaParameters, readOnly),
	FLOWUI_DEV_FIELD(TextAreaParameters, valid),
	FLOWUI_DEV_FIELD(TextAreaParameters, softWrap),
	FLOWUI_DEV_FIELD(TextAreaParameters, maxBytes),
	FLOWUI_DEV_FIELD(TextAreaParameters, transactionDetail),
	FLOWUI_DEV_FIELD(TextAreaParameters, placeholder),
	FLOWUI_DEV_FIELD(TextAreaParameters, sizing),
	FLOWUI_DEV_FIELD(TextAreaParameters, viewportWidth),
	FLOWUI_DEV_FIELD(TextAreaParameters, viewportHeight),
	FLOWUI_DEMO_PADDING_FIELD(TextAreaParameters, padding),
	FLOWUI_DEMO_BORDER_FIELD(TextAreaParameters, borderWidth),
	FLOWUI_DEMO_CORNER_FIELD(TextAreaParameters, cornerRadius),
	FLOWUI_DEV_FIELD(TextAreaParameters, idleOverrides),
	FLOWUI_DEV_FIELD(TextAreaParameters, hoveredOverrides),
	FLOWUI_DEV_FIELD(TextAreaParameters, focusedOverrides),
	FLOWUI_DEV_FIELD(TextAreaParameters, readOnlyOverrides),
	FLOWUI_DEV_FIELD(TextAreaParameters, invalidOverrides),
	FLOWUI_DEV_FIELD(TextAreaParameters, disabledOverrides),
	FLOWUI_DEV_FIELD(TextAreaParameters, fontFamily),
	FLOWUI_DEV_FIELD(TextAreaParameters, fontWeight),
	FLOWUI_DEV_FIELD(TextAreaParameters, fontStyle),
	FLOWUI_DEV_FIELD(TextAreaParameters, fontSize),
	FLOWUI_DEV_FIELD(TextAreaParameters, letterSpacing),
	FLOWUI_DEV_FIELD(TextAreaParameters, tabWidth),
	FLOWUI_DEV_FIELD(TextAreaParameters, caret),
	FLOWUI_DEV_FIELD(TextAreaParameters, cursor),
	FLOWUI_DEV_FIELD(TextAreaParameters, cursorPriority))

#define FLOWUI_DEMO_NUMBER_PARAMETERS(TYPE) \
	FLOWUI_DEV_SCHEMA( \
		NumberInputParameters<TYPE>, \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, value), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, minimum), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, maximum), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, step), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, syncPolicy), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, boundsPolicy), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, format), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, edit), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, onSubmit), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, onFocus), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, onBlur), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, onUndoRequested), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, onRedoRequested), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, enabled), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, readOnly), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, valid), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, clearFocusOnSubmit), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, stepButtons), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, maxBytes), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, transactionDetail), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, placeholder), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, sizing), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, viewportWidth), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, viewportHeight), \
		FLOWUI_DEMO_PADDING_FIELD(NumberInputParameters<TYPE>, padding), \
		FLOWUI_DEMO_BORDER_FIELD(NumberInputParameters<TYPE>, borderWidth), \
		FLOWUI_DEMO_CORNER_FIELD(NumberInputParameters<TYPE>, cornerRadius), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, stepButtonWidth), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, stepIconSize), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, idleOverrides), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, hoveredOverrides), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, focusedOverrides), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, readOnlyOverrides), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, invalidOverrides), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, disabledOverrides), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, fontFamily), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, fontWeight), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, fontStyle), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, fontSize), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, letterSpacing), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, caret), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, incrementIcon), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, decrementIcon), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, cursor), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, cursorPriority))

#define FLOWUI_DEMO_NUMBER_STATE(TYPE) \
	FLOWUI_DEV_SCHEMA( \
		NumberInputState<TYPE>, \
		FLOWUI_DEV_FIELD(NumberInputState<TYPE>, editSession), \
		FLOWUI_DEV_FIELD(NumberInputState<TYPE>, textStatus), \
		FLOWUI_DEV_FIELD(NumberInputState<TYPE>, wasFocused), \
		FLOWUI_DEV_FIELD(NumberInputState<TYPE>, forceTextSynchronization))

FLOWUI_DEMO_NUMBER_PARAMETERS(int)
FLOWUI_DEMO_NUMBER_PARAMETERS(unsigned int)
FLOWUI_DEMO_NUMBER_PARAMETERS(float)
FLOWUI_DEMO_NUMBER_STATE(int)
FLOWUI_DEMO_NUMBER_STATE(unsigned int)
FLOWUI_DEMO_NUMBER_STATE(float)

FLOWUI_DEV_SCHEMA(
	NumberInputResources,
	FLOWUI_DEV_FIELD(NumberInputResources, incrementIcon),
	FLOWUI_DEV_FIELD(NumberInputResources, decrementIcon))

#undef FLOWUI_DEMO_NUMBER_PARAMETERS
#undef FLOWUI_DEMO_NUMBER_STATE

#define FLOWUI_DEMO_DRAG_PARAMETERS(TYPE) \
	FLOWUI_DEV_SCHEMA( \
		DragValueParameters<TYPE>, \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, value), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, minimum), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, maximum), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, step), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, pixelsPerStep), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, dragThreshold), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, allowTextEntry), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, syncPolicy), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, boundsPolicy), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, format), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, edit), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, onSubmit), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, onFocus), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, onBlur), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, onUndoRequested), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, onRedoRequested), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, enabled), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, readOnly), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, valid), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, clearFocusOnSubmit), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, maxBytes), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, transactionDetail), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, sizing), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, viewportWidth), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, viewportHeight), \
		FLOWUI_DEMO_PADDING_FIELD(DragValueParameters<TYPE>, padding), \
		FLOWUI_DEMO_BORDER_FIELD(DragValueParameters<TYPE>, borderWidth), \
		FLOWUI_DEMO_CORNER_FIELD(DragValueParameters<TYPE>, cornerRadius), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, idleOverrides), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, hoveredOverrides), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, draggingOverrides), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, focusedOverrides), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, readOnlyOverrides), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, invalidOverrides), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, disabledOverrides), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, fontFamily), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, fontWeight), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, fontStyle), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, fontSize), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, letterSpacing), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, caret), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, dragCursor), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, editCursor), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, cursorPriority))

#define FLOWUI_DEMO_DRAG_STATE(TYPE) \
	FLOWUI_DEV_SCHEMA( \
		DragValueState<TYPE>, \
		FLOWUI_DEV_FIELD(DragValueState<TYPE>, mode), \
		FLOWUI_DEV_FIELD(DragValueState<TYPE>, editSession), \
		FLOWUI_DEV_FIELD(DragValueState<TYPE>, valueAtPress), \
		FLOWUI_DEV_FIELD(DragValueState<TYPE>, pointerXAtPress), \
		FLOWUI_DEV_FIELD(DragValueState<TYPE>, textStatus), \
		FLOWUI_DEV_FIELD(DragValueState<TYPE>, wasFocused), \
		FLOWUI_DEV_FIELD(DragValueState<TYPE>, forceTextSynchronization))

FLOWUI_DEMO_DRAG_PARAMETERS(int)
FLOWUI_DEMO_DRAG_PARAMETERS(unsigned int)
FLOWUI_DEMO_DRAG_PARAMETERS(float)
FLOWUI_DEMO_DRAG_STATE(int)
FLOWUI_DEMO_DRAG_STATE(unsigned int)
FLOWUI_DEMO_DRAG_STATE(float)

#undef FLOWUI_DEMO_DRAG_PARAMETERS
#undef FLOWUI_DEMO_DRAG_STATE

#undef FLOWUI_DEMO_COLOR_FIELD
#undef FLOWUI_DEMO_PADDING_FIELD
#undef FLOWUI_DEMO_BORDER_FIELD
#undef FLOWUI_DEMO_CORNER_FIELD

} // namespace FlowUi::FSEL

namespace FlowUi::FSEL::detail::boolean_control {
FLOWUI_DEV_SCHEMA(
	State,
	FLOWUI_DEV_FIELD(State, isArmed),
	FLOWUI_DEV_FIELD(State, mouseUpObserved))
} // namespace FlowUi::FSEL::detail::boolean_control

namespace FlowUi::FSEL::detail::selectable_surface {
FLOWUI_DEV_SCHEMA(
	State,
	FLOWUI_DEV_FIELD(State, isArmed),
	FLOWUI_DEV_FIELD(State, mouseUpObserved))
} // namespace FlowUi::FSEL::detail::selectable_surface

namespace FlowUi::FSEL::detail::text_field {
FLOWUI_DEV_SCHEMA(
	State,
	FLOWUI_DEV_FIELD(State, wasFocused),
	FLOWUI_DEV_FIELD(State, editSessionDirty))
} // namespace FlowUi::FSEL::detail::text_field

#endif
