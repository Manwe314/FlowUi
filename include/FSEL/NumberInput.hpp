#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

#include "FSEL/Numeric.hpp"
#include "FSEL/StandardIcons.hpp"
#include "FSEL/internal/NumericControl.hpp"
#include "FSEL/internal/NumericText.hpp"
#include "FSEL/internal/TextFieldCommon.hpp"

namespace FlowUi::FSEL {

using NumberInputSyncPolicy = NumericTextSyncPolicy;
using NumberInputBoundsPolicy = NumericTextBoundsPolicy;

enum class NumberInputStepButtons : uint8_t {
	None = 0,
	TrailingVertical,
	TrailingHorizontal,
};

using NumberInputTextStatus = NumericTextStatus;

template<NumericValueType T>
struct NumberInputParameters {
	/** Borrowed authoritative native value. Null disables interaction. */
	T* value = nullptr;
	std::optional<T> minimum = std::nullopt;
	std::optional<T> maximum = std::nullopt;
	T step = T{1};
	NumberInputSyncPolicy syncPolicy = NumberInputSyncPolicy::Live;
	NumberInputBoundsPolicy boundsPolicy = NumberInputBoundsPolicy::SoftClamp;
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
	NumberInputStepButtons stepButtons =
		NumberInputStepButtons::TrailingVertical;
	size_t maxBytes = 128;
	TransactionReportDetail transactionDetail = TransactionReportDetail::Summary;
	std::string_view placeholder{};

	std::optional<Clay_Sizing> sizing = std::nullopt;
	std::optional<float> viewportWidth = std::nullopt;
	std::optional<float> viewportHeight = std::nullopt;
	std::optional<Clay_Padding> padding = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
	std::optional<float> stepButtonWidth = std::nullopt;
	std::optional<float> stepIconSize = std::nullopt;

	TextFieldStateOverrides idleOverrides{};
	TextFieldStateOverrides hoveredOverrides{};
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
	std::optional<TextureRef> incrementIcon = std::nullopt;
	std::optional<TextureRef> decrementIcon = std::nullopt;
	std::optional<CursorType> cursor = std::nullopt;
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

template<NumericValueType T>
struct NumberInputState {
	NumericEditSession<T> editSession{};
	NumberInputTextStatus textStatus = NumberInputTextStatus::Complete;
	bool wasFocused = false;
	bool forceTextSynchronization = false;
};

struct NumberInputResources {
	TextureRef incrementIcon{};
	TextureRef decrementIcon{};

	NumberInputResources() = default;
	explicit NumberInputResources(App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
		IconManager& icons = app.icons();
		if (icons.contains(standard_icons::kIncrementKey)) {
			incrementIcon = icons.textureRef(standard_icons::kIncrementKey);
		}
		if (icons.contains(standard_icons::kDecrementKey)) {
			decrementIcon = icons.textureRef(standard_icons::kDecrementKey);
		}
#else
		(void)app;
#endif
	}
};

template<NumericValueType T>
struct DefaultNumberInputIdentity;

template<>
struct DefaultNumberInputIdentity<int> {
	static constexpr FlowDefinitionID value =
		DefinitionID("FSEL.number-input.int");
};

template<>
struct DefaultNumberInputIdentity<unsigned int> {
	static constexpr FlowDefinitionID value =
		DefinitionID("FSEL.number-input.uint");
};

template<>
struct DefaultNumberInputIdentity<float> {
	static constexpr FlowDefinitionID value =
		DefinitionID("FSEL.number-input.float");
};

/** Native typed numeric text editor backed by InputFieldManager. */
template<
	NumericValueType T,
	FlowDefinitionID Definition = DefaultNumberInputIdentity<T>::value>
struct NumberInput {
	using Parameters = NumberInputParameters<T>;
	using State = NumberInputState<T>;
	using Resources = NumberInputResources;
	using BuildContext = ElementBuildContext<NumberInput>;
	using InteractionContext = ElementInteractionContext<NumberInput>;

	static constexpr FlowDefinitionID definitionId = Definition;
	static constexpr std::string_view debugName = "FSEL NumberInput";

	struct Parts {
		static constexpr FlowElementPart content = Part("content");
		static constexpr FlowElementPart text = Part("text");
		static constexpr FlowElementPart placeholder = Part("placeholder");
		static constexpr FlowElementPart decrement = Part("decrement");
		static constexpr FlowElementPart increment = Part("increment");
	};

	static void onHovered(InteractionContext& context) {
		if (!isBoundAndEnabled(context.params)) {
			return;
		}
		const FSELNumberInputTheme& theme =
			context.uiManager.template theme<FSELTheme>().numberInputTheme;
		const bool overStepButton = hasStepButtons(context.params) &&
			(context.previousInteraction.isHovered(context.part(Parts::increment)) ||
			 context.previousInteraction.isHovered(context.part(Parts::decrement)));
		const detail::numeric::Bounds<T> bounds = detail::numeric::resolveBounds(
			context.params.minimum,
			context.params.maximum);
		const bool stepInteractive = overStepButton && !context.params.readOnly &&
			bounds.valid && detail::numeric::validStep(context.params.step) &&
			detail::numeric::isFinite(*context.params.value);
		context.uiManager.requestCursor(
			stepInteractive
				? theme.stepCursor
				: context.params.cursor.value_or(theme.field.cursor),
			stepInteractive
				? theme.stepCursorPriority
				: context.params.cursorPriority.value_or(
					theme.field.cursorPriority));
	}

	static void runLogic(InteractionContext& context) {
		if (!hasStepButtons(context.params)) {
			return;
		}
		const bool increment = context.previousInteraction.isPressed(
			context.part(Parts::increment));
		const bool decrement = context.previousInteraction.isPressed(
			context.part(Parts::decrement));
		if (increment == decrement) {
			return;
		}
		applyStep(context, increment ? 1 : -1);
	}

	static void buildElement(BuildContext& context) {
		const FSELNumberInputTheme& numberTheme =
			context.uiManager.template theme<FSELTheme>().numberInputTheme;
		const FSELTextFieldTheme& theme = numberTheme.field;
		auto& state = context.state();
		auto& manager = context.uiManager.inputFields();
		const detail::numeric::Bounds<T> bounds = detail::numeric::resolveBounds(
			context.params.minimum,
			context.params.maximum);
		const bool enabled = isBoundAndEnabled(context.params) && bounds.valid;
		const bool editable = enabled && !context.params.readOnly;
		const bool buttonsVisible = hasStepButtons(context.params);
		if (!enabled && state.wasFocused) {
			manager.requestCaret(context.id, CaretRequestKind::ClearAll);
		}

		const Clay_ElementId contentId = context.uiManager.toClayEID(
			context.part(Parts::content));
		const Clay_ElementId textId = context.uiManager.toClayEID(
			context.part(Parts::text));
		const Clay_ElementId placeholderId = context.uiManager.toClayEID(
			context.part(Parts::placeholder));
		const Clay_ElementId decrementId = context.uiManager.toClayEID(
			context.part(Parts::decrement));
		const Clay_ElementId incrementId = context.uiManager.toClayEID(
			context.part(Parts::increment));
		const std::array<Clay_ElementId, 2> focusRetentionIds{
			decrementId,
			incrementId,
		};

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
		const detail::numeric_text::FormattedValue initialText =
			detail::numeric_text::format(currentValue, context.params.format);
		FieldRequest request{
			.initialText = initialText.view(),
			.config = FieldConfig{
				.mode = TextFieldMode::SingleLine,
				.readOnly = !editable,
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
			.textElementId = enabled ? textId : Clay_ElementId{},
			.contentElementId = enabled ? contentId : Clay_ElementId{},
			.focusRetentionElementIds = buttonsVisible && enabled
				? std::span<const Clay_ElementId>(focusRetentionIds)
				: std::span<const Clay_ElementId>{},
		};
		FieldQueryResult field = manager.requestField(context.id, request);

		const bool userTextChanged = std::ranges::any_of(
			field.transactions,
			[](const FieldEditTransaction& transaction) {
				return transaction.origin != EditOrigin::Programmatic;
			});
		if (editable && userTextChanged) {
			const std::string fieldText = field.text.copy();
			const std::string sanitized = detail::numeric_text::sanitize<T>(
				fieldText,
				context.params.format.allowScientificInput);
			if (sanitized != fieldText) {
				(void)manager.replaceText(context.id, sanitized, true);
				field = manager.requestField(context.id, request);
			}
		}

		bool focused = enabled && field.hasPrimaryCaret;
		if (state.forceTextSynchronization ||
			(!focused && !userTextChanged && !state.editSession.active &&
			 !detail::text_field::textEquals(field.text, initialText.view()))) {
			(void)manager.replaceText(context.id, initialText.view(), focused);
			field = manager.requestField(context.id, request);
			focused = enabled && field.hasPrimaryCaret;
			state.forceTextSynchronization = false;
		}

		const detail::numeric_text::ClassifiedValue<T> classified =
			detail::numeric_text::classify(
				detail::numeric_text::parse<T>(field.text.copy()),
				bounds);
		const std::optional<T> candidate = classified.candidate;
		state.textStatus = classified.status;

		if (!bounds.valid || !detail::numeric::isFinite(currentValue)) {
			state.textStatus = NumberInputTextStatus::ConfigurationError;
		}

		if (userTextChanged && editable) {
			detail::numeric_control::beginEdit<T>(context);
			if (candidate.has_value()) {
				state.editSession.hasLastValidValue = true;
				state.editSession.lastValidValue = *candidate;
				if (context.params.syncPolicy == NumberInputSyncPolicy::Live) {
					(void)detail::numeric_control::applyValue<T>(
						context,
						*candidate);
				}
			}
		}

		const bool cancelled = detail::text_field::hasCommand(
			field,
			FieldCommandRequest::Cancel);
		const bool submitted = detail::text_field::hasCommand(
			field,
			FieldCommandRequest::Submit);
		const bool blurred = state.wasFocused && !focused;
		if (!state.wasFocused && focused) {
			invoke(context, context.params.onFocus);
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
				invoke(context, context.params.onSubmit);
				if (context.params.clearFocusOnSubmit) {
					manager.requestCaret(context.id, CaretRequestKind::ClearAll);
					focused = false;
				}
			}
		}

		if (submitted && context.params.clearFocusOnSubmit) {
			invoke(context, context.params.onBlur);
		} else if (blurred || cancelled) {
			invoke(context, context.params.onBlur);
		}
		if (detail::text_field::hasCommand(field, FieldCommandRequest::Undo)) {
			invoke(context, context.params.onUndoRequested);
		}
		if (detail::text_field::hasCommand(field, FieldCommandRequest::Redo)) {
			invoke(context, context.params.onRedoRequested);
		}

		if (context.params.boundsPolicy ==
				NumberInputBoundsPolicy::ClampTextImmediately &&
			candidate.has_value() &&
			state.textStatus != NumberInputTextStatus::Complete) {
			const detail::numeric_text::FormattedValue clamped =
				detail::numeric_text::format(*candidate, context.params.format);
			(void)manager.replaceText(context.id, clamped.view(), focused);
			field = manager.requestField(context.id, request);
			state.textStatus = NumberInputTextStatus::Complete;
		}
		if (submitted || blurred || cancelled) {
			const detail::numeric_text::FormattedValue canonical =
				detail::numeric_text::format(
					context.params.value ? *context.params.value : T{},
					context.params.format);
			state.textStatus = detail::numeric_text::classify(
				detail::numeric_text::parse<T>(canonical.view()),
				bounds).status;
			if (!bounds.valid || (context.params.value &&
				!detail::numeric::isFinite(*context.params.value))) {
				state.textStatus = NumberInputTextStatus::ConfigurationError;
			}
		}

		state.wasFocused = focused;
		drawField(
			context,
			field,
			textConfig,
			padding,
			contentId,
			textId,
			placeholderId,
			decrementId,
			incrementId,
			enabled,
			focused,
			numberTheme);
	}

private:
	static bool isBoundAndEnabled(const Parameters& params) {
		return params.enabled && params.value != nullptr;
	}

	static bool hasStepButtons(const Parameters& params) {
		return params.stepButtons != NumberInputStepButtons::None;
	}

	static void invoke(BuildContext& context, ActionCall action) {
		detail::numeric_control::invoke(context, action);
	}

	static void invoke(InteractionContext& context, ActionCall action) {
		detail::numeric_control::invoke(context, action);
	}

	static void applyStep(InteractionContext& context, int direction) {
		const detail::numeric::Bounds<T> bounds = detail::numeric::resolveBounds(
			context.params.minimum,
			context.params.maximum);
		if (!isBoundAndEnabled(context.params) || context.params.readOnly ||
			!bounds.valid || !detail::numeric::validStep(context.params.step) ||
			!detail::numeric::isFinite(*context.params.value)) {
			return;
		}
		const T next = detail::numeric::stepValue(
			*context.params.value,
			context.params.step,
			direction,
			bounds);
		if (next == *context.params.value) {
			return;
		}
		detail::numeric_control::beginEdit<T>(context);
		(void)detail::numeric_control::applyValue<T>(context, next);
		context.state().editSession.hasLastValidValue = true;
		context.state().editSession.lastValidValue = next;
		context.state().forceTextSynchronization = true;
		if (!context.state().wasFocused) {
			detail::numeric_control::commitEdit(context);
		}
	}

	static FSELTextFieldStateTheme resolveAppearance(
		BuildContext& context,
		const FSELTextFieldTheme& theme,
		bool enabled,
		bool focused) {
		if (!enabled) {
			return detail::text_field::applyOverrides(
				theme.disabled,
				context.params.disabledOverrides);
		}
		if (!context.params.valid ||
			context.state().textStatus != NumberInputTextStatus::Complete) {
			return detail::text_field::applyOverrides(
				theme.invalid,
				context.params.invalidOverrides);
		}
		if (focused) {
			return detail::text_field::applyOverrides(
				theme.focused,
				context.params.focusedOverrides);
		}
		if (context.params.readOnly) {
			return detail::text_field::applyOverrides(
				theme.readOnly,
				context.params.readOnlyOverrides);
		}
		if (context.uiManager.getPreviousFramesInteraction().isHovered(
			context.clayID())) {
			return detail::text_field::applyOverrides(
				theme.hovered,
				context.params.hoveredOverrides);
		}
		return detail::text_field::applyOverrides(
			theme.idle,
			context.params.idleOverrides);
	}

	static FSELNumberInputStepButtonStateTheme resolveStepAppearance(
		BuildContext& context,
		Clay_ElementId id,
		bool enabled,
		const FSELNumberInputTheme& theme) {
		if (!enabled) {
			return theme.stepDisabled;
		}
		const InteractionSnapshot& interaction =
			context.uiManager.getPreviousFramesInteraction();
		if (interaction.isPressed(id) || interaction.isHeld(id)) {
			return theme.stepPressed;
		}
		if (interaction.isHovered(id)) {
			return theme.stepHovered;
		}
		return theme.stepIdle;
	}

	static void drawField(
		BuildContext& context,
		const FieldQueryResult& field,
		Clay_TextElementConfig textConfig,
		Clay_Padding padding,
		Clay_ElementId contentId,
		Clay_ElementId textId,
		Clay_ElementId placeholderId,
		Clay_ElementId decrementId,
		Clay_ElementId incrementId,
		bool enabled,
		bool focused,
		const FSELNumberInputTheme& numberTheme) {
		const FSELTextFieldTheme& theme = numberTheme.field;
		const FSELTextFieldStateTheme appearance = resolveAppearance(
			context,
			theme,
			enabled,
			focused);
		textConfig.textColor = appearance.textColor;
		Clay_TextElementConfig placeholderConfig = textConfig;
		placeholderConfig.textColor = appearance.placeholderColor;

		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing.value_or(Clay_Sizing{
			.width = CLAY_SIZING_FIXED(std::max(theme.width, 1.0f)),
			.height = CLAY_SIZING_FIXED(std::max(theme.height, 1.0f)),
		});
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.backgroundColor = appearance.backgroundColor;
		root.cornerRadius = context.params.cornerRadius.value_or(theme.cornerRadius);
		root.clip = {.horizontal = true, .vertical = true};
		root.border = {
			.color = appearance.borderColor,
			.width = context.params.borderWidth.value_or(theme.borderWidth),
		};

		Clay_ElementDeclaration content = detail::text_field::makeContentDeclaration();
		content.layout.padding = padding;
		const bool buttonsVisible = hasStepButtons(context.params);
		const float buttonWidth = std::max(
			context.params.stepButtonWidth.value_or(numberTheme.stepButtonWidth),
			1.0f);
		Clay_ElementDeclaration stepContainer{};
		stepContainer.layout.sizing = {
			.width = CLAY_SIZING_FIXED(
				context.params.stepButtons == NumberInputStepButtons::TrailingHorizontal
					? buttonWidth * 2.0f
					: buttonWidth),
			.height = CLAY_SIZING_GROW(0),
		};
		stepContainer.layout.layoutDirection =
			context.params.stepButtons == NumberInputStepButtons::TrailingHorizontal
				? CLAY_LEFT_TO_RIGHT
				: CLAY_TOP_TO_BOTTOM;
		stepContainer.border = {
			.color = numberTheme.stepSeparatorColor,
			.width = {.left = 1},
		};

		CLAY(context.clayID(), root) {
			CLAY(contentId, content) {
				if (!field.visibleLines.empty()) {
					const VisibleTextLine& line = field.visibleLines.front();
					CLAY(textId, line.declaration) {
						CLAY_TEXT(
							context.uiManager.toClayString(line.text),
							CLAY_TEXT_CONFIG(textConfig));
					}
				}
				if (field.text.empty() && !context.params.placeholder.empty()) {
					CLAY(
						placeholderId,
						detail::text_field::makePlaceholderDeclaration()) {
						CLAY_TEXT(
							context.uiManager.toClayString(context.params.placeholder),
							CLAY_TEXT_CONFIG(placeholderConfig));
					}
				}
			}
			if (buttonsVisible) {
				CLAY(context.clayID("step-buttons"), stepContainer) {
					drawStepButton(
						context,
						incrementId,
						true,
						enabled,
						buttonWidth,
						numberTheme);
					drawStepButton(
						context,
						decrementId,
						false,
						enabled,
						buttonWidth,
						numberTheme);
				}
			}
		}

		if (!field.visibleLines.empty()) {
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

	static void drawStepButton(
		BuildContext& context,
		Clay_ElementId id,
		bool increment,
		bool fieldEnabled,
		float buttonWidth,
		const FSELNumberInputTheme& theme) {
		const bool enabled = fieldEnabled && !context.params.readOnly &&
			detail::numeric::validStep(context.params.step) &&
			context.params.value != nullptr &&
			detail::numeric::isFinite(*context.params.value);
		const FSELNumberInputStepButtonStateTheme appearance =
			resolveStepAppearance(context, id, enabled, theme);
		Clay_ElementDeclaration button{};
		button.layout.sizing = {
			.width = context.params.stepButtons ==
					NumberInputStepButtons::TrailingHorizontal
				? CLAY_SIZING_FIXED(buttonWidth)
				: CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		button.layout.childAlignment = {
			.x = CLAY_ALIGN_X_CENTER,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		button.backgroundColor = appearance.backgroundColor;

		TextureRef icon = increment
			? context.params.incrementIcon.value_or(
				context.resources().incrementIcon)
			: context.params.decrementIcon.value_or(
				context.resources().decrementIcon);
		CLAY(id, button) {
			if (icon.handle) {
				icon.tintEnabled = true;
				const float iconSize = std::max(
					context.params.stepIconSize.value_or(theme.stepIconSize),
					1.0f);
				Clay_ElementDeclaration iconDeclaration{};
				iconDeclaration.layout.sizing = {
					.width = CLAY_SIZING_FIXED(iconSize),
					.height = CLAY_SIZING_FIXED(iconSize),
				};
				iconDeclaration.backgroundColor = appearance.foregroundColor;
				iconDeclaration.image = {
					.imageData = context.uiManager.imageData(icon),
				};
				CLAY(
					increment
						? context.clayID("increment-icon")
						: context.clayID("decrement-icon"),
					iconDeclaration);
			} else {
				Clay_TextElementConfig fallbackText{};
				fallbackText.textColor = appearance.foregroundColor;
				fallbackText.fontId = context.uiManager.resolveFont(
					context.params.fontFamily.value_or(theme.field.fontFamily),
					context.params.fontWeight.value_or(theme.field.fontWeight),
					context.params.fontStyle.value_or(theme.field.fontStyle));
				fallbackText.fontSize = theme.stepTextSize;
				fallbackText.wrapMode = CLAY_TEXT_WRAP_NONE;
				fallbackText.textAlignment = CLAY_TEXT_ALIGN_CENTER;
				CLAY_TEXT(
					context.uiManager.toClayString(increment ? "+" : "-"),
					CLAY_TEXT_CONFIG(fallbackText));
			}
		}
	}
};

inline constexpr NumberInput<int> kNumberInputInt{};
inline constexpr NumberInput<unsigned int> kNumberInputUInt{};
inline constexpr NumberInput<float> kNumberInputFloat{};

static_assert(FlowElement<NumberInput<int>>);
static_assert(DrawableFlowElement<NumberInput<int>>);
static_assert(!ConstructibleFlowElement<NumberInput<int>>);
static_assert(FlowElement<NumberInput<unsigned int>>);
static_assert(FlowElement<NumberInput<float>>);

} // namespace FlowUi::FSEL
