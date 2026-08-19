#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::FSEL {

struct LabelParameters {
	std::string_view text{};

	Clay_Sizing sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};

	// Optional theme overrides
	std::optional<Clay_Color> textColor = std::nullopt;
	std::optional<FontFamilyId> fontFamily = std::nullopt;
	std::optional<uint32_t> fontWeight = std::nullopt;
	std::optional<FontStyle> fontStyle = std::nullopt;
	std::optional<uint16_t> fontSize = std::nullopt;
	std::optional<uint16_t> letterSpacing = std::nullopt;
	std::optional<Clay_TextElementConfigWrapMode> wrapMode = std::nullopt;
	std::optional<Clay_TextAlignment> textAlignment = std::nullopt;
};

/** Stateless, build-only passive text with FSEL typography defaults. */
struct Label {
	using Parameters = LabelParameters;
	using BuildContext = ElementBuildContext<Label>;

	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.label");
	static constexpr std::string_view debugName = "FSEL Label";

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

inline constexpr Label kLabel{};
static_assert(FlowElement<Label>);
static_assert(DrawableFlowElement<Label>);

} // namespace FlowUi::FSEL
