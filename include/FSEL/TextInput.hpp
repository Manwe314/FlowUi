#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "FSEL/internal/TextFieldCommon.hpp"

namespace FlowUi::FSEL {

/**
 * Per-invocation TextInput configuration. Borrowed models and text must remain valid through the
 * frame; empty visual overrides inherit the active FSELTheme.
 */
struct TextInputParameters {
	/** Optional edit, commit, submit and focus notifications. */
	TextFieldActions actions{};
	/** Per-instance caret and text selection overrides. */
	TextFieldCaretOverrides caret{};
	/** Borrowed UTF-8 text shown when no value is displayed. */
	std::string_view placeholder{};
	/**
	 * Borrowed authoritative application value; null disables interaction. Keep the model alive
	 * through the frame.
	 */
	std::string* value = nullptr;
	/** Maximum UTF-8 byte length accepted by the editor buffer. */
	size_t maxBytes = std::numeric_limits<size_t>::max();

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
	/** Explicit content viewport width in layout pixels; empty uses measured layout. */
	std::optional<float> viewportWidth = std::nullopt;
	/** Explicit content viewport height in layout pixels; empty uses measured layout. */
	std::optional<float> viewportHeight = std::nullopt;

	/** Registered font family used for field or label text. Empty inherits the active theme. */
	std::optional<FontFamilyId> fontFamily = std::nullopt;
	/** Font weight requested for text, such as 400 for normal. Empty inherits the active theme. */
	std::optional<uint32_t> fontWeight = std::nullopt;
	/** When editor text is published to the borrowed application value. */
	TextFieldSyncPolicy syncPolicy = TextFieldSyncPolicy::Live;
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
	/** Layer for the field's floating text/placeholder nodes. Set this when the
	 * input is nested in another floating surface such as a popup. */
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

/** Retained interaction state used by this standard element. */
using TextInputState = detail::text_field::State;

/** Single-line, application-bound editor backed entirely by InputFieldManager. */
struct TextInput {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = TextInputParameters;
	/** Per-instance state owned and retained by ElementManager. */
	using State = TextInputState;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<TextInput>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<TextInput>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.text-input");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL TextInput";

	/** Definition-time semantic parts used to address child surfaces and interaction targets. */
	struct Parts {
		/** Semantic identity of the content part relative to its owner instance. */
		static constexpr FlowElementPart content = Part("content");
		/** Semantic identity of the text part relative to its owner instance. */
		static constexpr FlowElementPart text = Part("text");
		/** Semantic identity of the placeholder part relative to its owner instance. */
		static constexpr FlowElementPart placeholder = Part("placeholder");
	};

	/**
	 * ElementBuilder hover hook; requests the enabled control cursor. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onHovered(InteractionContext& context) {
		if (!context.params.enabled || context.params.value == nullptr) {
			return;
		}
		const FSELTextFieldTheme& theme =
			context.uiManager.theme<FSELTheme>().textInputTheme;
		context.uiManager.requestCursor(
			context.params.cursor.value_or(theme.cursor),
			context.params.cursorPriority.value_or(theme.cursorPriority));
	}

	/**
	 * ElementBuilder draw hook; emits this control and its children for the active frame. @param
	 * context Borrowed context valid for this callback only.
	 */
	static void buildElement(BuildContext& context) {
		const FSELTextFieldTheme& theme =
			context.uiManager.theme<FSELTheme>().textInputTheme;
		const Clay_Padding padding = context.params.padding.value_or(theme.padding);
		const Clay_BorderWidth borderWidth =
			context.params.borderWidth.value_or(theme.borderWidth);
		const Clay_CornerRadius cornerRadius =
			context.params.cornerRadius.value_or(theme.cornerRadius);
		const Clay_ElementId contentId = context.uiManager.toClayEID(
			context.part(Parts::content));
		const Clay_ElementId textId = context.uiManager.toClayEID(
			context.part(Parts::text));
		const Clay_ElementId placeholderId = context.uiManager.toClayEID(
			context.part(Parts::placeholder));
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

		const detail::text_field::BuildResult buildResult =
			detail::text_field::requestAndSynchronize(
			context,
			context.params,
			TextFieldMode::SingleLine,
			textId,
			contentId,
			TextLayoutDescriptor{
				.fontId = textConfig.fontId,
				.fontSize = textConfig.fontSize,
				.letterSpacing = textConfig.letterSpacing,
				.viewportWidth = viewport.width,
				.viewportHeight = viewport.height,
				.tabWidth = theme.tabWidth,
				.floatingZIndex = context.params.floatingZIndex.value_or(0),
			},
			detail::text_field::resolveOverlayStyle(theme, context.params.caret),
			false,
			context.params.clearFocusOnSubmit);
		const FieldQueryResult& field = buildResult.field;

		const bool enabled = context.params.enabled && context.params.value != nullptr;
		const bool hovered = context.uiManager.getPreviousFramesInteraction()
			.isHovered(context.clayID());
		const FSELTextFieldStateTheme appearance =
			detail::text_field::resolveAppearance(
				theme,
				context.params,
				enabled,
				field.hasPrimaryCaret,
				hovered);
		textConfig.textColor = appearance.textColor;
		Clay_TextElementConfig placeholderConfig = textConfig;
		placeholderConfig.textColor = appearance.placeholderColor;

		const Clay_ElementDeclaration root =
			detail::text_field::makeRootDeclaration(
				detail::text_field::resolveSizing(context.params, theme),
				padding,
				borderWidth,
				cornerRadius,
				appearance);
		const Clay_ElementDeclaration content =
			detail::text_field::makeContentDeclaration();

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
					auto placeholder_declaration = detail::text_field::makePlaceholderDeclaration(
						context.params.floatingZIndex.value_or(0));
					if (context.params.centerText) {
						placeholder_declaration.floating.attachPoints = {
							CLAY_ATTACH_POINT_CENTER_CENTER, CLAY_ATTACH_POINT_CENTER_CENTER};
						placeholder_declaration.floating.offset = {};
					}
					CLAY(placeholderId, placeholder_declaration) {
						CLAY_TEXT(context.uiManager.toClayString(context.params.placeholder),
								  CLAY_TEXT_CONFIG(placeholderConfig));
					}
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
		detail::text_field::invokeActionsAfterBuild(
			context,
			context.params,
			buildResult.notifications);
	}
};

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr TextInput kTextInput{};
static_assert(FlowElement<TextInput>);
static_assert(DrawableFlowElement<TextInput>);
static_assert(!ConstructibleFlowElement<TextInput>);

} // namespace FlowUi::FSEL
