#pragma once

#include <string_view>

#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::FSEL {

/**
 * Per-invocation Spacer configuration. Borrowed models and text must remain valid through the
 * frame; empty visual overrides inherit the active FSELTheme.
 */
struct SpacerParameters {
	/** Clay width and height sizing rules for the root element. */
	Clay_Sizing sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0)
	};
};

/** Build-only empty layout node that occupies the requested size. */
struct Spacer {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = SpacerParameters;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<Spacer>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.spacer");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL Spacer";

	/**
	 * ElementBuilder draw hook; emits this control and its children for the active frame. @param
	 * context Borrowed context valid for this callback only.
	 */
	static void buildElement(BuildContext& context) {
		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing;
		CLAY(context.clayID(), root);
	}
};

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr Spacer kSpacer{};
static_assert(FlowElement<Spacer>);

} // namespace FlowUi::FSEL
