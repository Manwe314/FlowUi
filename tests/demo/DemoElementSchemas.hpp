#pragma once

// Demo-local reflection declarations for every FSEL element instantiated by
// the gallery. Production FSEL remains usable without developer tooling.
#if FLOW_UI_DEV_MODE

namespace FlowUi::FSEL {

FLOWUI_DEV_SCHEMA(
	BoxParameters,
	FLOWUI_DEV_FIELD(BoxParameters, sizing),
	FLOWUI_DEV_FIELD(BoxParameters, padding),
	FLOWUI_DEV_FIELD(BoxParameters, childGap),
	FLOWUI_DEV_FIELD(BoxParameters, childAlignment),
	FLOWUI_DEV_FIELD(BoxParameters, layoutDirection),
	FLOWUI_DEV_FIELD(BoxParameters, clipConfig),
	FLOWUI_DEV_FIELD(BoxParameters, backgroundColor),
	FLOWUI_DEV_FIELD(BoxParameters, borderColor),
	FLOWUI_DEV_FIELD(BoxParameters, cornerRadius),
	FLOWUI_DEV_FIELD(BoxParameters, borderWidth))

FLOWUI_DEV_SCHEMA(
	ButtonParameters,
	FLOWUI_DEV_FIELD(ButtonParameters, onActivate),
	FLOWUI_DEV_FIELD(ButtonParameters, enabled),
	FLOWUI_DEV_FIELD(ButtonParameters, contentMode),
	FLOWUI_DEV_FIELD(ButtonParameters, text),
	FLOWUI_DEV_FIELD(ButtonParameters, icon),
	FLOWUI_DEV_FIELD(ButtonParameters, tintIcon),
	FLOWUI_DEV_FIELD(ButtonParameters, sizing),
	FLOWUI_DEV_FIELD(ButtonParameters, padding),
	FLOWUI_DEV_FIELD(ButtonParameters, childAlignment),
	FLOWUI_DEV_FIELD(ButtonParameters, layoutDirection),
	FLOWUI_DEV_FIELD(ButtonParameters, contentGap),
	FLOWUI_DEV_FIELD(ButtonParameters, borderWidth),
	FLOWUI_DEV_FIELD(ButtonParameters, cornerRadius),
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
	FLOWUI_DEV_FIELD(CheckboxParameters, borderWidth),
	FLOWUI_DEV_FIELD(CheckboxParameters, cornerRadius),
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
	FLOWUI_DEV_FIELD(SwitchParameters, trackBorderWidth),
	FLOWUI_DEV_FIELD(SwitchParameters, knobBorderWidth),
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
	FLOWUI_DEV_FIELD(SliderParameters, trackBorderWidth),
	FLOWUI_DEV_FIELD(SliderParameters, fillBorderWidth),
	FLOWUI_DEV_FIELD(SliderParameters, thumbBorderWidth),
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
	FLOWUI_DEV_FIELD(ProgressBarParameters, baseColor),
	FLOWUI_DEV_FIELD(ProgressBarParameters, borderColor),
	FLOWUI_DEV_FIELD(ProgressBarParameters, fillColor),
	FLOWUI_DEV_FIELD(ProgressBarParameters, borderWidth),
	FLOWUI_DEV_FIELD(ProgressBarParameters, cornerRadius))

FLOWUI_DEV_SCHEMA(
	SplitterHandleParameters,
	FLOWUI_DEV_FIELD(SplitterHandleParameters, axis),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, position),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, targetExtent),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, minExtent),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, maxExtent),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, thickness),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, hitThickness),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, backgroundColor),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, hoverColor),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, draggingColor),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, borderColor),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, borderWidth),
	FLOWUI_DEV_FIELD(SplitterHandleParameters, cornerRadius),
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
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, padding),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, childGap),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, childAlignment),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, layoutDirection),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, backgroundColor),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, borderColor),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, borderWidth),
	FLOWUI_DEV_FIELD(PopupSurfaceParameters, cornerRadius))

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
	FLOWUI_DEV_FIELD(TextInputParameters, padding),
	FLOWUI_DEV_FIELD(TextInputParameters, borderWidth),
	FLOWUI_DEV_FIELD(TextInputParameters, cornerRadius),
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
	FLOWUI_DEV_FIELD(TextAreaParameters, padding),
	FLOWUI_DEV_FIELD(TextAreaParameters, borderWidth),
	FLOWUI_DEV_FIELD(TextAreaParameters, cornerRadius),
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
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, padding), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, borderWidth), \
		FLOWUI_DEV_FIELD(NumberInputParameters<TYPE>, cornerRadius), \
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
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, padding), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, borderWidth), \
		FLOWUI_DEV_FIELD(DragValueParameters<TYPE>, cornerRadius), \
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
