#pragma once

#include <string_view>

#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::FSEL {

struct SpacerParameters {
	Clay_Sizing sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0)
	};
};

struct Spacer {
	using Parameters = SpacerParameters;
	using BuildContext = ElementBuildContext<Spacer>;

	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.spacer");
	static constexpr std::string_view debugName = "FSEL Spacer";

	static void buildElement(BuildContext& context) {
		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing;
		CLAY(context.clayID(), root);
	}
};

inline constexpr Spacer kSpacer{};
static_assert(FlowElement<Spacer>);

} // namespace FlowUi::FSEL
