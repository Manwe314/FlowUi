#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "FSEL/internal/TextFieldCommon.hpp"

namespace FlowUi::FSEL {

struct TextInputParameters {
	std::string* value = nullptr;
	TextFieldSyncPolicy syncPolicy = TextFieldSyncPolicy::Live;
	TextFieldActions actions{};
	bool enabled = true;
	bool readOnly = false;
	bool valid = true;
	bool clearFocusOnSubmit = true;
	size_t maxBytes = std::numeric_limits<size_t>::max();
	TransactionReportDetail transactionDetail = TransactionReportDetail::Summary;
	std::string_view placeholder{};

	std::optional<Clay_Sizing> sizing = std::nullopt;
	std::optional<float> viewportWidth = std::nullopt;
	std::optional<float> viewportHeight = std::nullopt;
	std::optional<Clay_Padding> padding = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;

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
	/** Layer for the field's floating text/placeholder nodes. Set this when the
	 * input is nested in another floating surface such as a popup. */
	std::optional<int16_t> floatingZIndex = std::nullopt;
	TextFieldCaretOverrides caret{};
	std::optional<CursorType> cursor = std::nullopt;
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

using TextInputState = detail::text_field::State;

/** Single-line, application-bound editor backed entirely by InputFieldManager. */
struct TextInput {
	using Parameters = TextInputParameters;
	using State = TextInputState;
	using BuildContext = ElementBuildContext<TextInput>;
	using InteractionContext = ElementInteractionContext<TextInput>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.text-input");
	static constexpr std::string_view debugName = "FSEL TextInput";

	struct Parts {
		static constexpr FlowElementPart content = Part("content");
		static constexpr FlowElementPart text = Part("text");
		static constexpr FlowElementPart placeholder = Part("placeholder");
	};

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
					CLAY(textId, line.declaration) {
						CLAY_TEXT(
							context.uiManager.toClayString(line.text),
							CLAY_TEXT_CONFIG(textConfig));
					}
				}
				if (field.text.empty() && !context.params.placeholder.empty()) {
					CLAY(
						placeholderId,
						detail::text_field::makePlaceholderDeclaration(
							context.params.floatingZIndex.value_or(0))) {
						CLAY_TEXT(
							context.uiManager.toClayString(context.params.placeholder),
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

inline constexpr TextInput kTextInput{};
static_assert(FlowElement<TextInput>);
static_assert(DrawableFlowElement<TextInput>);
static_assert(!ConstructibleFlowElement<TextInput>);

} // namespace FlowUi::FSEL
