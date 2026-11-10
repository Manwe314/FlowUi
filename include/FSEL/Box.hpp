#pragma once

#include <optional>
#include <string_view>

#include "FSEL/Theme.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi::FSEL {

/**
 * Per-invocation Box configuration. Borrowed models and text must remain valid through the frame;
 * empty visual overrides inherit the active FSELTheme.
 */
struct BoxParameters {
	/** Clay width and height sizing rules for the root element. */
	Clay_Sizing sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0)
	};
	/** Inset between the root border and its children, in layout pixels. */
	Clay_Padding padding = {0, 0, 0, 0};
	/** Space between consecutive children, in layout pixels. */
	uint16_t childGap = 0;
	/** Alignment of children inside the available content area. */
	Clay_ChildAlignment childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER
	};
	/** Axis and order used to arrange children. */
	Clay_LayoutDirection layoutDirection = CLAY_LEFT_TO_RIGHT;
	/** Clay clipping and scroll offset configuration for the surface. */
	Clay_ClipElementConfig clipConfig{};

	// Optional theme overrides
	/** Surface background color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> backgroundColor = std::nullopt;
	/** Border color in Clay RGBA channels. Empty inherits the active theme. */
	std::optional<Clay_Color> borderColor = std::nullopt;
	/** Corner radii in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_CornerRadius> cornerRadius = std::nullopt;
	/** Per-edge border widths in layout pixels. Empty inherits the active theme. */
	std::optional<Clay_BorderWidth> borderWidth = std::nullopt;
};

/** Construct-only themed container for caller-authored child elements. */
struct Box {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = BoxParameters;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<Box>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.box");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL Box";

	/**
	 * ElementBuilder construct hook; returns the themed root declaration for caller-authored
	 * children. @param context Borrowed context valid for this callback only. @return Root Clay
	 * declaration; the builder opens it for child composition.
	 */
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

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr Box kBox{};
static_assert(FlowElement<Box>);

} // namespace FlowUi::FSEL
