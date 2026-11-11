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

/** Numeric text synchronization policy used by NumberInput. */
using NumberInputSyncPolicy = NumericTextSyncPolicy;
/** Numeric text bounds policy used by NumberInput. */
using NumberInputBoundsPolicy = NumericTextBoundsPolicy;

/** Visibility and trailing arrangement of numeric step buttons. */
enum class NumberInputStepButtons : uint8_t {
	/** Hide increment and decrement buttons. */
	None = 0,
	/** Stack increment and decrement buttons vertically at the trailing edge. */
	TrailingVertical,
	/** Place decrement and increment buttons horizontally at the trailing edge. */
	TrailingHorizontal,
};

/** Numeric text classification retained by NumberInput state. */
using NumberInputTextStatus = NumericTextStatus;

/**
 * Per-invocation NumberInput configuration. Borrowed models and text must remain valid through
 * the frame; empty visual overrides inherit the active FSELTheme.
 */
template<NumericValueType T>
struct NumberInputParameters {
	/** Optional begin, change, commit and cancel notifications for native numeric editing. */
	NumericEditActions edit{};
	/** Per-instance caret and text selection overrides. */
	TextFieldCaretOverrides caret{};
	/** Optional notification when the editor receives a submit request. */
	ActionCall onSubmit{};
	/** Optional notification when the editor gains focus. */
	ActionCall onFocus{};
	/** Optional notification when the editor loses focus. */
	ActionCall onBlur{};
	/** Optional notification that the editor requested application undo. */
	ActionCall onUndoRequested{};
	/** Optional notification that the editor requested application redo. */
	ActionCall onRedoRequested{};
	/** Increment artwork; omitted uses the cached standard increment icon. */
	std::optional<TextureRef> incrementIcon = std::nullopt;
	/** Decrement artwork; omitted uses the cached standard decrement icon. */
	std::optional<TextureRef> decrementIcon = std::nullopt;
	/** Borrowed UTF-8 text shown when no value is displayed. */
	std::string_view placeholder{};
	/** Borrowed authoritative native value. Null disables interaction. */
	T* value = nullptr;
	/** When editor text is published to the borrowed application value. */
	NumberInputSyncPolicy syncPolicy = NumberInputSyncPolicy::Live;
	/** Whether out-of-range text remains visible or is clamped immediately. */
	NumberInputBoundsPolicy boundsPolicy = NumberInputBoundsPolicy::SoftClamp;
	/** Maximum UTF-8 byte length accepted by the editor buffer. */
	size_t maxBytes = 128;

	/** Appearance when enabled and idle. Empty colors inherit the theme. */
	TextFieldStateOverrides idleOverrides{};
	/** Appearance while hovered. Empty colors inherit the theme. */
	TextFieldStateOverrides hoveredOverrides{};
	/** Appearance while the editor is focused. Empty colors inherit the theme. */
	TextFieldStateOverrides focusedOverrides{};
	/** Appearance while the editor is read-only. Empty colors inherit the theme. */
	TextFieldStateOverrides readOnlyOverrides{};
	/** Appearance for an invalid value. Empty colors inherit the theme. */
	TextFieldStateOverrides invalidOverrides{};
	/** Appearance when interaction is disabled. Empty colors inherit the theme. */
	TextFieldStateOverrides disabledOverrides{};

	/** Clay width and height sizing rules for the root element. Empty inherits the active theme. */
	std::optional<Clay_Sizing> sizing = std::nullopt;
	/** Corner radii in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
	/** Locale-independent native value formatting and parsing policy. */
	NumericFormatOptions format{};
	/** Lower bound of the native value range. Empty uses the native type limit. */
	std::optional<T> minimum = std::nullopt;
	/** Upper bound of the native value range. Empty uses the native type limit. */
	std::optional<T> maximum = std::nullopt;
	/** Explicit content viewport width in layout pixels; empty uses measured layout. */
	std::optional<float> viewportWidth = std::nullopt;
	/** Explicit content viewport height in layout pixels; empty uses measured layout. */
	std::optional<float> viewportHeight = std::nullopt;
	/** Width of the trailing step button region in layout pixels. Empty inherits the active theme. */
	std::optional<float> stepButtonWidth = std::nullopt;
	/** Step button artwork side length in layout pixels. Empty inherits the active theme. */
	std::optional<float> stepIconSize = std::nullopt;

	/** Registered font family used for field or label text. Empty inherits the active theme. */
	std::optional<FontFamilyId> fontFamily = std::nullopt;
	/** Font weight requested for text, such as 400 for normal. Empty inherits the active theme. */
	std::optional<uint32_t> fontWeight = std::nullopt;
	/** Native value delta used by a step button or one drag step. */
	T step = T{1};
	/** Arrangement of optional increment and decrement buttons. */
	NumberInputStepButtons stepButtons =
		NumberInputStepButtons::TrailingVertical;
	/** Amount of edit transaction detail requested from InputFieldManager. */
	TransactionReportDetail transactionDetail = TransactionReportDetail::Summary;
	/** Per-edge border widths in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	/**
	 * Inset between the root border and its children, in layout pixels. Empty inherits the active
	 * theme.
	 */
	std::optional<Clay_Padding> padding = std::nullopt;
	/** Text font size in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> fontSize = std::nullopt;
	/** Additional spacing between glyphs in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> letterSpacing = std::nullopt;
	/** Layer for text, caret and placeholder when nested in a floating surface. */
	std::optional<int16_t> floatingZIndex = std::nullopt;
	/** Font style requested for text. Empty inherits the active theme. */
	std::optional<FontStyle> fontStyle = std::nullopt;
	/** Cursor requested while this enabled control is hovered. Empty inherits the active theme. */
	std::optional<CursorType> cursor = std::nullopt;
	/** Priority used to arbitrate this control's cursor request. Empty inherits the active theme. */
	std::optional<uint8_t> cursorPriority = std::nullopt;

	/** Center single-line text and its caret when it fits the field viewport. */
	bool centerText = false;
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
	/** Allow display and text selection while preventing value edits. */
	bool readOnly = false;
	/** Application validation result used for invalid styling. */
	bool valid = true;
	/** Clear editor focus after submission when true. */
	bool clearFocusOnSubmit = true;
};

/** Per-instance interaction state maintained by the element and retained by ElementManager. */
template<NumericValueType T>
struct NumberInputState {
	/** Transaction bookkeeping retained for this element instance. */
	NumericEditSession<T> editSession{};
	/** Classification of the current numeric editor text. */
	NumberInputTextStatus textStatus = NumberInputTextStatus::Complete;
	/** Focus observed during the previous invocation. */
	bool wasFocused = false;
	/** Request replacement of editor text from the native value on the next build. */
	bool forceTextSynchronization = false;
};

/** App-wide borrowed icon references resolved once for this element definition. */
struct NumberInputResources {
	/** Borrowed cached increment icon texture; empty if the semantic key was unavailable. */
	TextureRef incrementIcon{};
	/** Borrowed cached decrement icon texture; empty if the semantic key was unavailable. */
	TextureRef decrementIcon{};

	/** Construct empty icon references. */
	NumberInputResources() = default;
	/**
	 * Resolve available standard icons from the app; missing icons remain empty. @param app Owning
	 * application whose icon resources outlive these references.
	 */
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

/**
 * Default stable definition identity for the supported native value types; supply an explicit
 * definition ID for other NumericValueType specializations.
 */
template<NumericValueType T>
struct DefaultNumberInputIdentity;

/**
 * Default stable definition identity for the supported native value types; supply an explicit
 * definition ID for other NumericValueType specializations.
 */
template<>
struct DefaultNumberInputIdentity<int> {
	/** Stable definition identity assigned to this native type. */
	static constexpr FlowDefinitionID value =
		DefinitionID("FSEL.number-input.int");
};

/**
 * Default stable definition identity for the supported native value types; supply an explicit
 * definition ID for other NumericValueType specializations.
 */
template<>
struct DefaultNumberInputIdentity<unsigned int> {
	/** Stable definition identity assigned to this native type. */
	static constexpr FlowDefinitionID value =
		DefinitionID("FSEL.number-input.uint");
};

/**
 * Default stable definition identity for the supported native value types; supply an explicit
 * definition ID for other NumericValueType specializations.
 */
template<>
struct DefaultNumberInputIdentity<float> {
	/** Stable definition identity assigned to this native type. */
	static constexpr FlowDefinitionID value =
		DefinitionID("FSEL.number-input.float");
};

/** Native typed numeric text editor backed by InputFieldManager. */
template<
	NumericValueType T,
	FlowDefinitionID Definition = DefaultNumberInputIdentity<T>::value>
struct NumberInput {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = NumberInputParameters<T>;
	/** Per-instance state owned and retained by ElementManager. */
	using State = NumberInputState<T>;
	/** Immutable app-wide resources shared by this element definition. */
	using Resources = NumberInputResources;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<NumberInput>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<NumberInput>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId = Definition;
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL NumberInput";

	/** Definition-time semantic parts used to address child surfaces and interaction targets. */
	struct Parts {
		/** Semantic identity of the content part relative to its owner instance. */
		static constexpr FlowElementPart content = Part("content");
		/** Semantic identity of the text part relative to its owner instance. */
		static constexpr FlowElementPart text = Part("text");
		/** Semantic identity of the placeholder part relative to its owner instance. */
		static constexpr FlowElementPart placeholder = Part("placeholder");
		/** Semantic identity of the decrement part relative to its owner instance. */
		static constexpr FlowElementPart decrement = Part("decrement");
		/** Semantic identity of the increment part relative to its owner instance. */
		static constexpr FlowElementPart increment = Part("increment");
	};

	/**
	 * ElementBuilder hover hook; requests the enabled control cursor. @param context Borrowed
	 * context valid for this callback only.
	 */
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

	/**
	 * ElementBuilder logic hook; reconciles retained interaction state with the current input.
	 * @param context Borrowed context valid for this callback only.
	 */
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

	/**
	 * ElementBuilder draw hook; emits this control and its children for the active frame. @param
	 * context Borrowed context valid for this callback only.
	 */
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
				.maxBytes = context.params.maxBytes,
				.mode = TextFieldMode::SingleLine,
				.transactionDetail = context.params.transactionDetail,
				.readOnly = !editable,
				.softWrap = false,
				.allowArrowNavigation = true,
			},
			.layout = TextLayoutDescriptor{
				.fontId = textConfig.fontId,
				.fontSize = textConfig.fontSize,
				.letterSpacing = textConfig.letterSpacing,
				.viewportWidth = viewport.width,
				.viewportHeight = viewport.height,
				.tabWidth = theme.tabWidth,
				.floatingZIndex = context.params.floatingZIndex.value_or(0),
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
					auto line_declaration = line.declaration;
					if (context.params.centerText) {
						const auto previous_text = Clay_GetElementData(textId);
						const auto previous_content = Clay_GetElementData(contentId);
						const bool fits =
							!previous_text.found || !previous_content.found ||
							previous_text.boundingBox.width <=
								previous_content.boundingBox.width - padding.left - padding.right;
						line_declaration.floating.attachPoints =
							fits ? Clay_FloatingAttachPoints{CLAY_ATTACH_POINT_CENTER_CENTER,
															 CLAY_ATTACH_POINT_CENTER_CENTER}
								 : Clay_FloatingAttachPoints{CLAY_ATTACH_POINT_LEFT_CENTER,
															 CLAY_ATTACH_POINT_LEFT_CENTER};
						if (fits)
							line_declaration.floating.offset.x = 0;
						line_declaration.floating.offset.y = 0;
					}
					CLAY(textId, line_declaration) {
						CLAY_TEXT(context.uiManager.toClayString(line.text),
								  CLAY_TEXT_CONFIG(textConfig));
					}
				}
				if (field.text.empty() && !context.params.placeholder.empty()) {
					CLAY(placeholderId, detail::text_field::makePlaceholderDeclaration(
											context.params.floatingZIndex.value_or(0))) {
						CLAY_TEXT(context.uiManager.toClayString(context.params.placeholder),
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

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr NumberInput<int> kNumberInputInt{};
/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr NumberInput<unsigned int> kNumberInputUInt{};
/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr NumberInput<float> kNumberInputFloat{};

static_assert(validate_flow_element<NumberInput<int>>());
static_assert(DrawableFlowElement<NumberInput<int>>);
static_assert(!ConstructibleFlowElement<NumberInput<int>>);
static_assert(validate_flow_element<NumberInput<unsigned int>>());
static_assert(validate_flow_element<NumberInput<float>>());

} // namespace FlowUi::FSEL
