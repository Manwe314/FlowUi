#pragma once

#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::FSEL {

struct BoxParameters {
	Clay_Sizing sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0)
	};
	Clay_Padding padding = {0, 0, 0, 0};
	uint16_t childGap = 0;
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER
	};
	Clay_LayoutDirection layoutDirection = CLAY_LEFT_TO_RIGHT;
	Clay_ClipElementConfig clipConfig{};

	// Optional theme overrides
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	std::optional<Clay_Color> borderColor = std::nullopt;
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
};

struct Box {
	using Parameters = BoxParameters;
	using BuildContext = ElementBuildContext<Box>;

	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.box");
	static constexpr std::string_view debugName = "FSEL Box";

	static Clay_ElementDeclaration constructElement(BuildContext& context) {
		Clay_ElementDeclaration declaration{};
		const auto& theme = context.uiManager.theme<FSELTheme>();
		Clay_LayoutConfig layout = {
			.sizing = context.params.sizing,
			.padding = context.params.padding,
			.childGap = context.params.childGap,
			.childAlignment = context.params.childAlignment,
			.layoutDirection = context.params.layoutDirection
		};
		declaration.layout = layout;
		declaration.backgroundColor = context.params.backgroundColor.value_or(
			theme.boxTheme.backgroundColor);
		declaration.cornerRadius = context.params.cornerRadius.value_or(
			theme.boxTheme.cornerRadius);
		declaration.clip = context.params.clipConfig;
		declaration.border = {
			.color = context.params.borderColor.value_or(
				theme.boxTheme.borderColor),
			.width = context.params.borderWidth.value_or(
				theme.boxTheme.borderWidth)
		};
		return declaration;
	}
};

inline constexpr Box kBox{};
static_assert(FlowElement<Box>);

} // namespace FlowUi::FSEL
