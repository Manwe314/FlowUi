#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include "FSEL/Numeric.hpp"
#include "FSEL/internal/DragValueMath.hpp"
#include "FSEL/internal/NumericControl.hpp"
#include "FSEL/internal/NumericText.hpp"
#include "FSEL/internal/TextFieldCommon.hpp"

namespace FlowUi::FSEL {

enum class DragValueInteractionMode : uint8_t {
	Idle = 0,
	Pending,
	Dragging,
	Editing,
};

template<NumericValueType T>
struct DragValueParameters {
	/** Borrowed authoritative native value. Null disables interaction. */
	T* value = nullptr;
	std::optional<T> minimum = std::nullopt;
	std::optional<T> maximum = std::nullopt;
	T step = T{1};
	float pixelsPerStep = 4.0f;
	float dragThreshold = 4.0f;
	bool allowTextEntry = true;
	NumericTextSyncPolicy syncPolicy = NumericTextSyncPolicy::Live;
	NumericTextBoundsPolicy boundsPolicy = NumericTextBoundsPolicy::SoftClamp;
	NumericFormatOptions format{};
	NumericEditActions edit{};
	ActionCall onSubmit{};
	ActionCall onFocus{};
	ActionCall onBlur{};
	ActionCall onUndoRequested{};
	ActionCall onRedoRequested{};

	bool enabled = true;
	bool readOnly = false;
	bool valid = true;
	bool clearFocusOnSubmit = true;
	size_t maxBytes = 128;
	TransactionReportDetail transactionDetail = TransactionReportDetail::Summary;

	std::optional<Clay_Sizing> sizing = std::nullopt;
	std::optional<float> viewportWidth = std::nullopt;
	std::optional<float> viewportHeight = std::nullopt;
	std::optional<Clay_Padding> padding = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;

	TextFieldStateOverrides idleOverrides{};
	TextFieldStateOverrides hoveredOverrides{};
	TextFieldStateOverrides draggingOverrides{};
	TextFieldStateOverrides focusedOverrides{};
	TextFieldStateOverrides readOnlyOverrides{};
	TextFieldStateOverrides invalidOverrides{};
	TextFieldStateOverrides disabledOverrides{};

	std::optional<FontFamilyId> fontFamily = std::nullopt;
	std::optional<uint32_t> fontWeight = std::nullopt;
	std::optional<FontStyle> fontStyle = std::nullopt;
	std::optional<uint16_t> fontSize = std::nullopt;
	std::optional<uint16_t> letterSpacing = std::nullopt;
	TextFieldCaretOverrides caret{};
	std::optional<CursorType> dragCursor = std::nullopt;
	std::optional<CursorType> editCursor = std::nullopt;
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

template<NumericValueType T>
struct DragValueState {
	DragValueInteractionMode mode = DragValueInteractionMode::Idle;
	NumericEditSession<T> editSession{};
	T valueAtPress{};
	float pointerXAtPress = 0.0f;
	NumericTextStatus textStatus = NumericTextStatus::Complete;
	bool wasFocused = false;
	bool forceTextSynchronization = false;
};

template<NumericValueType T>
struct DefaultDragValueIdentity;

template<>
struct DefaultDragValueIdentity<int> {
	static constexpr FlowDefinitionID value =
		DefinitionID("FSEL.drag-value.int");
};

template<>
struct DefaultDragValueIdentity<unsigned int> {
	static constexpr FlowDefinitionID value =
		DefinitionID("FSEL.drag-value.uint");
};

template<>
struct DefaultDragValueIdentity<float> {
	static constexpr FlowDefinitionID value =
		DefinitionID("FSEL.drag-value.float");
};

/** Build-only native numeric scrub field with click-to-edit text entry. */
template<
	NumericValueType T,
	FlowDefinitionID Definition = DefaultDragValueIdentity<T>::value>
struct DragValue {
	using Parameters = DragValueParameters<T>;
	using State = DragValueState<T>;
	using BuildContext = ElementBuildContext<DragValue>;
	using InteractionContext = ElementInteractionContext<DragValue>;

	static constexpr FlowDefinitionID definitionId = Definition;
	static constexpr std::string_view debugName = "FSEL DragValue";

	struct Parts {
		static constexpr FlowElementPart content = Part("content");
		static constexpr FlowElementPart text = Part("text");
	};

	static void onHovered(InteractionContext& context) {
		if (!canStartInteraction(context.params)) {
			return;
		}
		const FSELDragValueTheme& theme =
			context.uiManager.template theme<FSELTheme>().dragValueTheme;
		const bool editCursor =
			context.state().mode == DragValueInteractionMode::Editing ||
			!canDrag(context.params);
		context.uiManager.requestCursor(
			editCursor
				? context.params.editCursor.value_or(theme.editCursor)
				: context.params.dragCursor.value_or(theme.dragCursor),
			context.params.cursorPriority.value_or(theme.cursorPriority));
	}

	static void onPressed(InteractionContext& context) {
		auto& state = context.state();
		if (state.mode == DragValueInteractionMode::Editing) {
			return;
		}
		if (!canStartInteraction(context.params)) {
			state.mode = DragValueInteractionMode::Idle;
			return;
		}

		const FrameInput& input = context.uiManager.getCurrentFrameInput();
		state.mode = DragValueInteractionMode::Pending;
		state.pointerXAtPress = input.mouseX;
		state.valueAtPress = *context.params.value;
	}

	static void onReleased(InteractionContext& context) {
		auto& state = context.state();
		if (state.mode != DragValueInteractionMode::Pending) {
			return;
		}
		// A drag transitions out of Pending while the button is held. If the
		// gesture is still pending at release, classify it as a click so small
		// pointer jitter cannot prevent click-to-edit.
		if (canEditText(context.params)) {
			state.mode = DragValueInteractionMode::Editing;
			state.forceTextSynchronization = true;
			context.uiManager.inputFields().requestCaret(
				context.id,
				CaretRequestKind::SetPrimary);
		} else {
			state.mode = DragValueInteractionMode::Idle;
		}
	}

	static void runLogic(InteractionContext& context) {
		auto& state = context.state();
		if (state.mode == DragValueInteractionMode::Idle ||
			state.mode == DragValueInteractionMode::Editing) {
			return;
		}
		if (!hasUsableBinding(context.params)) {
			cancelGesture(context);
			return;
		}

		const FrameInput& input = context.uiManager.getCurrentFrameInput();
		const float horizontalDelta = input.mouseX - state.pointerXAtPress;
		if (state.mode == DragValueInteractionMode::Pending) {
			if (!input.mouseDown[0]) {
				// Keep the press pending until onReleased classifies it as a
				// click-to-edit gesture. Clearing it here races the release
				// callback and makes every short click behave like a cancelled drag.
				return;
			}
			if (!canDrag(context.params) ||
				!detail::drag_value::crossedThreshold(
					horizontalDelta,
					context.params.dragThreshold)) {
				return;
			}
			state.mode = DragValueInteractionMode::Dragging;
		}

		if (escapeIsDown(input)) {
			cancelGesture(context);
			return;
		}
		if (!canDrag(context.params)) {
			cancelGesture(context);
			return;
		}

		if (input.mouseDown[0]) {
			requestDragCursor(context);
		}
		const detail::numeric::Bounds<T> bounds = resolveBounds(context.params);
		const int64_t stepCount = detail::drag_value::stepCountForDelta(
			horizontalDelta,
			context.params.pixelsPerStep);
		const T next = detail::numeric::offsetBySteps(
			state.valueAtPress,
			context.params.step,
			stepCount,
			bounds);
		if (next != *context.params.value) {
			detail::numeric_control::beginEdit<T>(context);
			if (detail::numeric_control::applyValue<T>(context, next)) {
				state.editSession.hasLastValidValue = true;
				state.editSession.lastValidValue = next;
			}
		}
		if (!input.mouseDown[0]) {
			detail::numeric_control::commitEdit(context);
			state.mode = DragValueInteractionMode::Idle;
			state.forceTextSynchronization = true;
		}
	}

	static void buildElement(BuildContext& context) {
		const FSELDragValueTheme& dragTheme =
			context.uiManager.template theme<FSELTheme>().dragValueTheme;
		const FSELTextFieldTheme& theme = dragTheme.field;
		auto& state = context.state();
		auto& manager = context.uiManager.inputFields();
		const detail::numeric::Bounds<T> bounds = resolveBounds(context.params);
		const bool fieldEnabled = isBoundAndEnabled(context.params) && bounds.valid;
		const bool textEditable = fieldEnabled && canEditText(context.params);

		if (state.mode == DragValueInteractionMode::Editing && !textEditable) {
			const bool hadFocus = state.wasFocused;
			detail::numeric_control::cancelEdit<T>(context);
			manager.requestCaret(context.id, CaretRequestKind::ClearAll);
			state.mode = DragValueInteractionMode::Idle;
			state.wasFocused = false;
			if (hadFocus) {
				detail::numeric_control::invoke(context, context.params.onBlur);
			}
		}
		const bool editing = state.mode == DragValueInteractionMode::Editing;

		const Clay_ElementId contentId = context.uiManager.toClayEID(
			context.part(Parts::content));
		const Clay_ElementId textId = context.uiManager.toClayEID(
			context.part(Parts::text));
		const Clay_Padding padding = context.params.padding.value_or(theme.padding);
		const Clay_Dimensions viewport = detail::text_field::resolveViewport(
			contentId,
			theme,
			padding,
			context.params.viewportWidth,
			context.params.viewportHeight);

		Clay_TextElementConfig textConfig{};
		textConfig.fontId = context.uiManager.resolveFont(
			context.params.fontFamily.value_or(theme.fontFamily),
			context.params.fontWeight.value_or(theme.fontWeight),
			context.params.fontStyle.value_or(theme.fontStyle));
		textConfig.fontSize = context.params.fontSize.value_or(theme.fontSize);
		textConfig.letterSpacing = context.params.letterSpacing.value_or(
			theme.letterSpacing);
		textConfig.wrapMode = CLAY_TEXT_WRAP_NONE;
		textConfig.textAlignment = CLAY_TEXT_ALIGN_LEFT;

		const T currentValue = context.params.value ? *context.params.value : T{};
		const detail::numeric_text::FormattedValue formattedValue =
			detail::numeric_text::format(currentValue, context.params.format);
		FieldRequest request{
			.initialText = formattedValue.view(),
			.config = FieldConfig{
				.mode = TextFieldMode::SingleLine,
				.readOnly = !textEditable,
				.allowNewline = false,
				.softWrap = false,
				.allowArrowNavigation = true,
				.maxBytes = context.params.maxBytes,
				.transactionDetail = context.params.transactionDetail,
			},
			.layout = TextLayoutDescriptor{
				.fontId = textConfig.fontId,
				.fontSize = textConfig.fontSize,
				.letterSpacing = textConfig.letterSpacing,
				.viewportWidth = viewport.width,
				.viewportHeight = viewport.height,
				.tabWidth = theme.tabWidth,
			},
			.overlayStyle = detail::text_field::resolveOverlayStyle(
				theme,
				context.params.caret),
			.textElementId = editing && fieldEnabled ? textId : Clay_ElementId{},
			.contentElementId = editing && fieldEnabled
				? contentId
				: Clay_ElementId{},
		};
		FieldQueryResult field = manager.requestField(context.id, request);

		const bool userTextChanged = editing && std::ranges::any_of(
			field.transactions,
			[](const FieldEditTransaction& transaction) {
				return transaction.origin != EditOrigin::Programmatic;
			});
		if (textEditable && userTextChanged) {
			const std::string fieldText = field.text.copy();
			const std::string sanitized = detail::numeric_text::sanitize<T>(
				fieldText,
				context.params.format.allowScientificInput);
			if (sanitized != fieldText) {
				(void)manager.replaceText(context.id, sanitized, true);
				field = manager.requestField(context.id, request);
			}
		}

		bool focused = editing && fieldEnabled && field.hasPrimaryCaret;
		if (state.forceTextSynchronization ||
			(!editing && !userTextChanged && !state.editSession.active &&
			 !detail::text_field::textEquals(
				 field.text,
				 formattedValue.view()))) {
			(void)manager.replaceText(
				context.id,
				formattedValue.view(),
				focused);
			field = manager.requestField(context.id, request);
			focused = editing && fieldEnabled && field.hasPrimaryCaret;
			state.forceTextSynchronization = false;
		}

		const detail::numeric_text::ClassifiedValue<T> classified =
			detail::numeric_text::classify(
				detail::numeric_text::parse<T>(field.text.copy()),
				bounds);
		const std::optional<T> candidate = classified.candidate;
		state.textStatus = classified.status;
		if (!bounds.valid || !detail::numeric::isFinite(currentValue)) {
			state.textStatus = NumericTextStatus::ConfigurationError;
		}

		if (userTextChanged && textEditable) {
			detail::numeric_control::beginEdit<T>(context);
			if (candidate.has_value()) {
				state.editSession.hasLastValidValue = true;
				state.editSession.lastValidValue = *candidate;
				if (context.params.syncPolicy == NumericTextSyncPolicy::Live) {
					(void)detail::numeric_control::applyValue<T>(
						context,
						*candidate);
				}
			}
		}

		const bool cancelled = editing && detail::text_field::hasCommand(
			field,
			FieldCommandRequest::Cancel);
		const bool submitted = editing && detail::text_field::hasCommand(
			field,
			FieldCommandRequest::Submit);
		const bool blurred = editing && state.wasFocused && !focused;
		if (editing && !state.wasFocused && focused) {
			detail::numeric_control::invoke(context, context.params.onFocus);
		}

		if (cancelled) {
			detail::numeric_control::cancelEdit<T>(context);
			const detail::numeric_text::FormattedValue restored =
				detail::numeric_text::format(
					context.params.value ? *context.params.value : T{},
					context.params.format);
			(void)manager.replaceText(context.id, restored.view(), true);
			manager.requestCaret(context.id, CaretRequestKind::ClearAll);
			field = manager.requestField(context.id, request);
			focused = false;
			state.mode = DragValueInteractionMode::Idle;
		} else if (submitted || blurred) {
			detail::numeric_control::commitTextEdit<T>(
				context,
				candidate,
				context.params.syncPolicy);
			const detail::numeric_text::FormattedValue committed =
				detail::numeric_text::format(
					context.params.value ? *context.params.value : T{},
					context.params.format);
			(void)manager.replaceText(context.id, committed.view(), focused);
			field = manager.requestField(context.id, request);
			if (submitted) {
				detail::numeric_control::invoke(context, context.params.onSubmit);
			}
			if (blurred || context.params.clearFocusOnSubmit) {
				manager.requestCaret(context.id, CaretRequestKind::ClearAll);
				focused = false;
				state.mode = DragValueInteractionMode::Idle;
			}
		}

		if ((submitted && context.params.clearFocusOnSubmit) ||
			blurred || cancelled) {
			detail::numeric_control::invoke(context, context.params.onBlur);
		}
		if (editing && detail::text_field::hasCommand(
			field,
			FieldCommandRequest::Undo)) {
			detail::numeric_control::invoke(
				context,
				context.params.onUndoRequested);
		}
		if (editing && detail::text_field::hasCommand(
			field,
			FieldCommandRequest::Redo)) {
			detail::numeric_control::invoke(
				context,
				context.params.onRedoRequested);
		}

		if (state.mode == DragValueInteractionMode::Editing &&
			context.params.boundsPolicy ==
				NumericTextBoundsPolicy::ClampTextImmediately &&
			candidate.has_value() &&
			state.textStatus != NumericTextStatus::Complete) {
			const detail::numeric_text::FormattedValue clamped =
				detail::numeric_text::format(
					*candidate,
					context.params.format);
			(void)manager.replaceText(context.id, clamped.view(), focused);
			field = manager.requestField(context.id, request);
			state.textStatus = NumericTextStatus::Complete;
		}

		const detail::numeric_text::FormattedValue displayValue =
			detail::numeric_text::format(
				context.params.value ? *context.params.value : T{},
				context.params.format);
		if (state.mode != DragValueInteractionMode::Editing) {
			state.textStatus = detail::numeric_text::classify(
				detail::numeric_text::parse<T>(displayValue.view()),
				bounds).status;
			if (!bounds.valid || (context.params.value &&
				!detail::numeric::isFinite(*context.params.value))) {
				state.textStatus = NumericTextStatus::ConfigurationError;
			}
		}

		state.wasFocused = focused;
		drawField(
			context,
			field,
			displayValue.view(),
			textConfig,
			padding,
			contentId,
			textId,
			isBoundAndEnabled(context.params),
			focused,
			dragTheme);
	}

private:
	static detail::numeric::Bounds<T> resolveBounds(const Parameters& params) {
		return detail::numeric::resolveBounds(params.minimum, params.maximum);
	}

	static bool isBoundAndEnabled(const Parameters& params) {
		return params.enabled && params.value != nullptr;
	}

	static bool hasUsableBinding(const Parameters& params) {
		return isBoundAndEnabled(params) && !params.readOnly &&
			resolveBounds(params).valid;
	}

	static bool canEditText(const Parameters& params) {
		return hasUsableBinding(params) && params.allowTextEntry;
	}

	static bool canDrag(const Parameters& params) {
		return hasUsableBinding(params) &&
			detail::numeric::validStep(params.step) &&
			detail::drag_value::validPixelsPerStep(params.pixelsPerStep) &&
			std::isfinite(params.dragThreshold) &&
			detail::numeric::isFinite(*params.value);
	}

	static bool canStartInteraction(const Parameters& params) {
		return canDrag(params) || canEditText(params);
	}

	static bool escapeIsDown(const FrameInput& input) {
		// FrameInput uses GLFW-compatible key slots with the default backend.
		constexpr size_t kEscapeKey = 256u;
		return kEscapeKey < input.keyDown.size() && input.keyDown[kEscapeKey];
	}

	static void cancelGesture(InteractionContext& context) {
		if (context.params.value != nullptr) {
			detail::numeric_control::cancelEdit<T>(context);
		} else {
			detail::numeric::resetSession(context.state().editSession);
		}
		context.state().mode = DragValueInteractionMode::Idle;
		context.state().forceTextSynchronization = true;
	}

	static void requestDragCursor(InteractionContext& context) {
		const FSELDragValueTheme& theme =
			context.uiManager.template theme<FSELTheme>().dragValueTheme;
		context.uiManager.requestCursor(
			context.params.dragCursor.value_or(theme.dragCursor),
			context.params.cursorPriority.value_or(theme.cursorPriority));
	}

	static FSELTextFieldStateTheme resolveAppearance(
		BuildContext& context,
		const FSELDragValueTheme& theme,
		bool enabled,
		bool focused) {
		if (!enabled) {
			return detail::text_field::applyOverrides(
				theme.field.disabled,
				context.params.disabledOverrides);
		}
		const bool dragConfigurationValid =
			detail::numeric::validStep(context.params.step) &&
			detail::drag_value::validPixelsPerStep(
				context.params.pixelsPerStep) &&
			std::isfinite(context.params.dragThreshold);
		if (!context.params.valid || !resolveBounds(context.params).valid ||
			!dragConfigurationValid ||
			context.state().textStatus != NumericTextStatus::Complete) {
			return detail::text_field::applyOverrides(
				theme.field.invalid,
				context.params.invalidOverrides);
		}
		if (context.state().mode == DragValueInteractionMode::Dragging) {
			return detail::text_field::applyOverrides(
				theme.dragging,
				context.params.draggingOverrides);
		}
		if (focused) {
			return detail::text_field::applyOverrides(
				theme.field.focused,
				context.params.focusedOverrides);
		}
		if (context.params.readOnly) {
			return detail::text_field::applyOverrides(
				theme.field.readOnly,
				context.params.readOnlyOverrides);
		}
		if (context.uiManager.getPreviousFramesInteraction().isHovered(
			context.clayID())) {
			return detail::text_field::applyOverrides(
				theme.field.hovered,
				context.params.hoveredOverrides);
		}
		return detail::text_field::applyOverrides(
			theme.field.idle,
			context.params.idleOverrides);
	}

	static void drawField(
		BuildContext& context,
		const FieldQueryResult& field,
		std::string_view displayText,
		Clay_TextElementConfig textConfig,
		Clay_Padding padding,
		Clay_ElementId contentId,
		Clay_ElementId textId,
		bool enabled,
		bool focused,
		const FSELDragValueTheme& dragTheme) {
		const FSELTextFieldTheme& theme = dragTheme.field;
		const FSELTextFieldStateTheme appearance = resolveAppearance(
			context,
			dragTheme,
			enabled,
			focused);
		textConfig.textColor = appearance.textColor;

		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing.value_or(Clay_Sizing{
			.width = CLAY_SIZING_FIXED(std::max(theme.width, 1.0f)),
			.height = CLAY_SIZING_FIXED(std::max(theme.height, 1.0f)),
		});
		// Keep the content box itself inset. Editing lines are floating children
		// of that box, so padding the content element would only affect the
		// normal-flow display text and make it jump when editing begins.
		root.layout.padding = padding;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_TOP,
		};
		root.backgroundColor = appearance.backgroundColor;
		root.cornerRadius = context.params.cornerRadius.value_or(
			theme.cornerRadius);
		root.clip = {.horizontal = true, .vertical = true};
		root.border = {
			.color = appearance.borderColor,
			.width = context.params.borderWidth.value_or(theme.borderWidth),
		};

		const Clay_ElementDeclaration content =
			detail::text_field::makeContentDeclaration();
		const bool editing =
			context.state().mode == DragValueInteractionMode::Editing;
		CLAY(context.clayID(), root) {
			CLAY(contentId, content) {
				if (editing && !field.visibleLines.empty()) {
					const VisibleTextLine& line = field.visibleLines.front();
					CLAY(textId, line.declaration) {
						CLAY_TEXT(
							context.uiManager.toClayString(line.text),
							CLAY_TEXT_CONFIG(textConfig));
					}
				} else if (!editing) {
					CLAY(textId, {}) {
						CLAY_TEXT(
							context.uiManager.toClayString(displayText),
							CLAY_TEXT_CONFIG(textConfig));
					}
				}
			}
		}

		if (editing && !field.visibleLines.empty()) {
			const VisibleTextLine& line = field.visibleLines.front();
			(void)context.uiManager.inputFields().submitTextSpan(
				field.field,
				FieldTextSpanSubmission{
					.logicalRange = line.logicalRange,
					.textElementId = textId,
					.visualLineIndex = line.visualLineIndex,
				});
		}
	}
};

inline constexpr DragValue<int> kDragValueInt{};
inline constexpr DragValue<unsigned int> kDragValueUInt{};
inline constexpr DragValue<float> kDragValueFloat{};

static_assert(FlowElement<DragValue<int>>);
static_assert(DrawableFlowElement<DragValue<int>>);
static_assert(!ConstructibleFlowElement<DragValue<int>>);
static_assert(FlowElement<DragValue<unsigned int>>);
static_assert(FlowElement<DragValue<float>>);

} // namespace FlowUi::FSEL
