#pragma once

#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "FSEL/internal/TextFieldCommon.hpp"

namespace FlowUi::FSEL {

/**
 * Per-invocation TextArea configuration. Borrowed models and text must remain valid through the
 * frame; empty visual overrides inherit the active FSELTheme.
 */
struct TextAreaParameters {
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
	/** Font style requested for text. Empty inherits the active theme. */
	std::optional<FontStyle> fontStyle = std::nullopt;
	/** Width of a tab in space columns. Empty inherits the active theme. */
	std::optional<uint8_t> tabWidth = std::nullopt;
	/** Cursor requested while this enabled control is hovered. Empty inherits the active theme. */
	std::optional<CursorType> cursor = std::nullopt;
	/** Priority used to arbitrate this control's cursor request. Empty inherits the active theme. */
	std::optional<uint8_t> cursorPriority = std::nullopt;
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
	/** Allow display and text selection while preventing value edits. */
	bool readOnly = false;
	/** Application validation result used for invalid styling. */
	bool valid = true;
	/** Wrap long lines to the text area viewport width when true. */
	bool softWrap = true;
};

/** Retained interaction state used by this standard element. */
using TextAreaState = detail::text_field::State;

/** Multiline, virtualized editor backed entirely by InputFieldManager. */
struct TextArea {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = TextAreaParameters;
	/** Per-instance state owned and retained by ElementManager. */
	using State = TextAreaState;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<TextArea>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<TextArea>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.text-area");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL TextArea";

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
			context.uiManager.theme<FSELTheme>().textAreaTheme;
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
			context.uiManager.theme<FSELTheme>().textAreaTheme;
		const Clay_Padding padding = context.params.padding.value_or(theme.padding);
		const Clay_BorderWidth borderWidth =
			context.params.borderWidth.value_or(theme.borderWidth);
		const Clay_CornerRadius cornerRadius =
			context.params.cornerRadius.value_or(theme.cornerRadius);
		const Clay_ElementId contentId = context.uiManager.toClayEID(
			context.part(Parts::content));
		const Clay_ElementId textLayerId = context.uiManager.toClayEID(
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
			TextFieldMode::MultiLine,
			textLayerId,
			contentId,
			TextLayoutDescriptor{
				.fontId = textConfig.fontId,
				.fontSize = textConfig.fontSize,
				.letterSpacing = textConfig.letterSpacing,
				.viewportWidth = viewport.width,
				.viewportHeight = viewport.height,
				.tabWidth = context.params.tabWidth.value_or(theme.tabWidth),
			},
			detail::text_field::resolveOverlayStyle(theme, context.params.caret),
			context.params.softWrap,
			false);
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
		Clay_ElementDeclaration textLayer =
			detail::text_field::makeContentDeclaration();

		CLAY(context.clayID(), root) {
			CLAY(contentId, content) {
				CLAY(textLayerId, textLayer) {
					for (const VisibleTextLine& line : field.visibleLines) {
						const std::string lineName =
							"line-" + std::to_string(line.visualLineIndex);
						const Clay_ElementId lineId = context.clayID(
							RuntimeName(lineName));
						CLAY(lineId, line.declaration) {
							CLAY_TEXT(
								context.uiManager.toClayString(line.text),
								CLAY_TEXT_CONFIG(textConfig));
						}
						(void)context.uiManager.inputFields().submitTextSpan(
							field.field,
							FieldTextSpanSubmission{
								.logicalRange = line.logicalRange,
								.textElementId = lineId,
								.visualLineIndex = line.visualLineIndex,
							});
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
inline constexpr TextArea kTextArea{};
static_assert(FlowElement<TextArea>);
static_assert(DrawableFlowElement<TextArea>);
static_assert(!ConstructibleFlowElement<TextArea>);

} // namespace FlowUi::FSEL
