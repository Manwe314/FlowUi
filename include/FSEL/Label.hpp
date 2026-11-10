#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::FSEL {

/**
 * Per-invocation Label configuration. Borrowed models and text must remain valid through the
 * frame; empty visual overrides inherit the active FSELTheme.
 */
struct LabelParameters {
	/** Borrowed UTF-8 display text; keep its storage valid through the frame. */
	std::string_view text{};

	/** Clay width and height sizing rules for the root element. */
	Clay_Sizing sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};

	// Optional theme overrides
	/** Text color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> textColor = std::nullopt;
	/** Registered font family used for field or label text. Empty inherits the active theme. */
	std::optional<FontFamilyId> fontFamily = std::nullopt;
	/** Font weight requested for text, such as 400 for normal. Empty inherits the active theme. */
	std::optional<uint32_t> fontWeight = std::nullopt;
	/** Font style requested for text. Empty inherits the active theme. */
	std::optional<FontStyle> fontStyle = std::nullopt;
	/** Text font size in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> fontSize = std::nullopt;
	/** Additional spacing between glyphs in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> letterSpacing = std::nullopt;
	/** Wrapping rule for label text. Empty inherits the active theme. */
	std::optional<Clay_TextElementConfigWrapMode> wrapMode = std::nullopt;
	/** Horizontal alignment of text within its line. Empty inherits the active theme. */
	std::optional<Clay_TextAlignment> textAlignment = std::nullopt;
};

/** Stateless, build-only passive text with FSEL typography defaults. */
struct Label {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = LabelParameters;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<Label>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.label");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL Label";

	/**
	 * ElementBuilder draw hook; emits this control and its children for the active frame. @param
	 * context Borrowed context valid for this callback only.
	 */
	static void buildElement(BuildContext& context) {
		const FSELLabelTheme& theme =
			context.uiManager.theme<FSELTheme>().labelTheme;

		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing;

		Clay_TextElementConfig textConfig{};
		textConfig.textColor = context.params.textColor.value_or(theme.textColor);
		textConfig.fontId = context.uiManager.resolveFont(
			context.params.fontFamily.value_or(theme.fontFamily),
			context.params.fontWeight.value_or(theme.fontWeight),
			context.params.fontStyle.value_or(theme.fontStyle));
		textConfig.fontSize = std::max<uint16_t>(
			context.params.fontSize.value_or(theme.fontSize),
			1u);
		textConfig.letterSpacing = context.params.letterSpacing.value_or(
			theme.letterSpacing);
		textConfig.wrapMode = context.params.wrapMode.value_or(theme.wrapMode);
		textConfig.textAlignment = context.params.textAlignment.value_or(
			theme.textAlignment);

		CLAY(context.clayID(), root) {
			CLAY_TEXT(
				context.uiManager.toClayString(context.params.text),
				CLAY_TEXT_CONFIG(textConfig));
		}
	}
};

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr Label kLabel{};
static_assert(FlowElement<Label>);
static_assert(DrawableFlowElement<Label>);

} // namespace FlowUi::FSEL
