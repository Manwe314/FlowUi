#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/ComboBox.hpp"
#include "FSEL/NumberInput.hpp"
#include "FSEL/internal/SelectableSurfaceBehavior.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "managers/UiManager.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <string>

namespace FlowUi::devSystems::interface_elements::diagnostics_ui {

/** Shared Diagnostics column with the same depth and spacing vocabulary as Memory. */
[[nodiscard]] inline Clay_ElementDeclaration column(uint16_t gap = 6) noexcept {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	declaration.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	declaration.layout.childGap = gap;
	return declaration;
}

/** A compact control row; callers can switch to a column at their local width. */
[[nodiscard]] inline Clay_ElementDeclaration row(float height = 28) noexcept {
	auto declaration = column();
	declaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	declaration.layout.sizing.height = CLAY_SIZING_FIXED(height);
	declaration.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	return declaration;
}

/** Elevated evidence/advice surface, with a restrained structural border. */
[[nodiscard]] inline Clay_ElementDeclaration card(uint16_t gap = 7) noexcept {
	auto declaration = column(gap);
	declaration.layout.padding = {11, 11, 10, 10};
	declaration.backgroundColor = interface_theme::kDepth3Elevated;
	declaration.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
	declaration.cornerRadius = CLAY_CORNER_RADIUS(4);
	return declaration;
}

/** Wrapped prose; exact numerical/source values remain available in the Inspector. */
template <class Context>
void text(Context& context, std::string_view label,
		  Clay_Color color = interface_theme::kTextSecondary, uint16_t size = 13) {
	Clay_TextElementConfig configuration{};
	configuration.textColor = color;
	configuration.fontSize = size;
	configuration.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(configuration));
}

/** Pointer help keeps its bottom-left corner above/right of the cursor, flipping below near the
 * top. */
template <class Context, class Identity>
void pointer_tooltip(Context& context, const Identity& identity, std::string_view description) {
	const auto& input = context.uiManager.getCurrentFrameInput();
	if (input.mouseX < 0 || input.mouseY < 0)
		return;
	const auto dimensions = Clay_GetLayoutDimensions();
	const auto previous = Clay_GetElementData(context.clayID(identity));
	const float width = std::min(300.0f, std::max(80.0f, dimensions.width - 16));
	const float height = previous.found ? previous.boundingBox.height : 160;
	constexpr float gap = 12;
	const bool above = input.mouseY >= height + gap + 8;
	auto tooltip = card();
	tooltip.layout.sizing.width = CLAY_SIZING_FIXED(width);
	tooltip.floating.attachTo = CLAY_ATTACH_TO_ROOT;
	tooltip.floating.attachPoints = {above ? CLAY_ATTACH_POINT_LEFT_BOTTOM
										   : CLAY_ATTACH_POINT_LEFT_TOP,
									 CLAY_ATTACH_POINT_LEFT_TOP};
	tooltip.floating.offset = {
		std::clamp(input.mouseX + gap, 8.0f, std::max(8.0f, dimensions.width - width - 8)),
		above ? input.mouseY - gap : input.mouseY + gap};
	tooltip.floating.zIndex = 140;
	tooltip.floating.clipTo = CLAY_CLIP_TO_NONE;
	tooltip.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
	CLAY(context.clayID(identity), tooltip)
	text(context, description, interface_theme::kTextCanvas, 11);
}

/** Render registered SVG artwork with the surface's semantic tint. */
template <class Context, class Identity>
void icon_with_identity(Context& context, const Identity& identity,
						diagnostics_icons::Icon semantic,
						Clay_Color color = interface_theme::kTextSecondary, float size = 14) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	auto texture = context.resources().texture(semantic);
	if (!texture.handle)
		return;
	texture.tintEnabled = true;
	Clay_ElementDeclaration image{};
	image.layout.sizing = {.width = CLAY_SIZING_FIXED(size), .height = CLAY_SIZING_FIXED(size)};
	image.backgroundColor = color;
	image.image.imageData = context.uiManager.imageData(texture);
	CLAY(context.clayID(identity), image) {}
#else
	text(context, diagnostics_icons::fallback(semantic), color, static_cast<uint16_t>(size));
#endif
}

template <class Context>
void icon(Context& context, LocalElementName identity, diagnostics_icons::Icon semantic,
		  Clay_Color color = interface_theme::kTextSecondary, float size = 14) {
	icon_with_identity(context, identity, semantic, color, size);
}
template <class Context>
void icon(Context& context, IndexedElementName identity, diagnostics_icons::Icon semantic,
		  Clay_Color color = interface_theme::kTextSecondary, float size = 14) {
	icon_with_identity(context, identity, semantic, color, size);
}

/** Retained release-inside control; callbacks only update UI state or queue commands. */
struct Control {
	struct Parameters {
		std::string label{}, secondary{}, badge{}, tooltip{}, clipboard_text{};
		diagnostics_icons::Icon icon = diagnostics_icons::Icon::None;
		void (*activate)(void*, uint64_t, uint64_t) = nullptr;
		void* target = nullptr;
		uint64_t action = 0, value = 0;
		Clay_Color marker = interface_theme::kTextMuted;
		bool enabled = true, selected = false, focused = false, issue_row = false, flexible = false,
			 histogram_bin = false, keyboard_activation = true, domain_row = false,
			 section_header = false;
	};
	using State = FSEL::detail::selectable_surface::State;
	using Resources = diagnostics_icons::Resources;
	using BuildContext = ElementBuildContext<Control>;
	using InteractionContext = ElementInteractionContext<Control>;
	static constexpr FlowDefinitionID definitionId = DefinitionID("flowui.dev.diagnostics.control");
	static constexpr std::string_view debugName = "Diagnostics Control";
	static constexpr bool isDevInternal = true;

	static void onPressed(InteractionContext& context) {
		FSEL::detail::selectable_surface::onPressed(context, context.params.enabled);
	}
	static void activate(InteractionContext& context) {
		if (!context.params.enabled)
			return;
		context.uiManager.shortcuts().setFocusedElement(context.clayID());
		if (!context.params.clipboard_text.empty() && context.uiManager.hasClipboardAccess())
			context.uiManager.setClipboardText(context.params.clipboard_text);
		if (context.params.activate) {
			context.params.activate(context.params.target, context.params.action,
									context.params.value);
		}
	}
	static void onReleased(InteractionContext& context) {
		if (FSEL::detail::selectable_surface::onReleased(context, context.params.enabled))
			activate(context);
	}
	static void runLogic(InteractionContext& context) {
		FSEL::detail::selectable_surface::runLogic(context, context.params.enabled);
		const auto& current = context.uiManager.getCurrentFrameInput();
		const auto& previous = context.uiManager.getPreviousFrameInput();
		if (context.params.keyboard_activation &&
			context.uiManager.shortcuts().focusedElement().id == context.clayID().id &&
			!context.uiManager.inputFields().hasPrimaryFieldFocus() &&
			((current.keyDown[GLFW_KEY_ENTER] && !previous.keyDown[GLFW_KEY_ENTER]) ||
			 (current.keyDown[GLFW_KEY_SPACE] && !previous.keyDown[GLFW_KEY_SPACE])))
			activate(context);
	}
	static void onHovered(InteractionContext& context) {
		if (context.params.enabled)
			context.uiManager.requestCursor(CursorType::PointingHand, 4);
	}
	/** Surface suitable for caller-owned content such as a histogram bin. */
	[[nodiscard]] static Clay_ElementDeclaration constructElement(BuildContext& context) {
		const auto& parameters = context.params;
		const bool hovered =
			context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID());
		if (parameters.section_header) {
			auto surface = row();
			surface.layout.sizing.height = CLAY_SIZING_FIT(28);
			surface.layout.padding = {12, 12, 8, 8};
			surface.backgroundColor =
				hovered ? interface_theme::kHoverSurface : interface_theme::kDepth2Ink;
			const bool focused =
				parameters.focused ||
				context.uiManager.shortcuts().focusedElement().id == context.clayID().id;
			surface.border = {.color = focused ? interface_theme::kAccentCurrent
											   : interface_theme::kBorderPrimary,
							  .width = {0, 0, 0, 1, 0}};
			return surface;
		}
		if (parameters.domain_row) {
			auto surface = row(30);
			surface.layout.sizing.width = CLAY_SIZING_GROW(0);
			surface.layout.padding = {12, 10, 0, 0};
			surface.backgroundColor =
				hovered ? interface_theme::kHoverSurface : interface_theme::kDepth1Panel;
			surface.border = {.color = parameters.focused ? interface_theme::kAccentCurrent
														  : interface_theme::kBorderPrimary,
							  .width = {0, 0, 0, 1, 0}};
			return surface;
		}
		if (parameters.histogram_bin) {
			auto surface = column(0);
			surface.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			surface.layout.childAlignment.y = CLAY_ALIGN_Y_BOTTOM;
			surface.backgroundColor =
				hovered ? interface_theme::kHoverSurface : interface_theme::kDepth0Keel;
			surface.border = {.color = parameters.selected ? interface_theme::kAccentSeaGlass
														   : interface_theme::kBorderSubtle,
							  .width = {0, 0, 0, 1, 0}};
			return surface;
		}
		auto surface = parameters.issue_row ? column(4) : row();
		if (!parameters.issue_row)
			surface.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
		surface.layout.padding =
			parameters.issue_row ? Clay_Padding{12, 9, 7, 7} : Clay_Padding{8, 8, 4, 4};
		surface.layout.sizing.height =
			parameters.issue_row
				? (parameters.flexible ? CLAY_SIZING_FIT(48) : CLAY_SIZING_FIXED(48))
				: CLAY_SIZING_FIT(28);
		surface.layout.sizing.width =
			parameters.issue_row ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIT(0);
		surface.backgroundColor = parameters.selected			  ? interface_theme::kSelectedRow
								  : hovered && parameters.enabled ? interface_theme::kHoverSurface
								  : parameters.issue_row ? interface_theme::kDepth0Keel
														 : interface_theme::kDepth3Elevated;
		const bool focused =
			parameters.focused ||
			context.uiManager.shortcuts().focusedElement().id == context.clayID().id;
		surface.border = {.color = focused				  ? interface_theme::kAccentCurrent
								   : parameters.issue_row ? interface_theme::kBorderSubtle
														  : interface_theme::kBorderVisible,
						  .width = parameters.issue_row ? Clay_BorderWidth{3, 0, 0, 1, 0}
														: Clay_BorderWidth{1, 1, 1, 1, 0}};
		surface.cornerRadius = CLAY_CORNER_RADIUS(parameters.issue_row ? 0.0f : 3.0f);
		return surface;
	}
	static void buildElement(BuildContext& context) {
		const auto& parameters = context.params;
		const auto surface = constructElement(context);
		CLAY(context.clayID(), surface) {
			if (parameters.issue_row) {
				auto headline = row(16);
				if (parameters.flexible)
					headline.layout.sizing.height = CLAY_SIZING_FIT(16);
				CLAY(context.clayID("headline"), headline) {
					icon(context, "marker",
						 parameters.icon == diagnostics_icons::Icon::None
							 ? diagnostics_icons::Icon::Issue
							 : parameters.icon,
						 parameters.marker, 14);
					auto label = column(0);
					label.layout.sizing.height =
						parameters.flexible ? CLAY_SIZING_FIT(0) : CLAY_SIZING_GROW(0);
					label.clip.horizontal = true;
					label.clip.scrollInputDisabled = true;
					CLAY(context.clayID("label"), label) {
						Clay_TextElementConfig configuration{};
						configuration.fontSize = 12;
						configuration.textColor = interface_theme::kTextCanvas;
						configuration.wrapMode =
							parameters.flexible ? CLAY_TEXT_WRAP_WORDS : CLAY_TEXT_WRAP_NONE;
						CLAY_TEXT(context.uiManager.toClayString(parameters.label),
								  CLAY_TEXT_CONFIG(configuration));
					}
					text(context, parameters.badge, interface_theme::kTextSecondary, 11);
				}
				if (parameters.flexible) {
					text(context, parameters.secondary, interface_theme::kTextSecondary, 12);
				} else {
					auto summary = column(0);
					summary.layout.sizing.height = CLAY_SIZING_FIXED(14);
					summary.clip.horizontal = true;
					summary.clip.scrollInputDisabled = true;
					CLAY(context.clayID("summary"), summary) {
						Clay_TextElementConfig configuration{};
						configuration.fontSize = 11;
						configuration.textColor = interface_theme::kTextSecondary;
						configuration.wrapMode = CLAY_TEXT_WRAP_NONE;
						CLAY_TEXT(context.uiManager.toClayString(parameters.secondary),
								  CLAY_TEXT_CONFIG(configuration));
					}
				}
			} else {
				const auto semantic = !parameters.clipboard_text.empty() &&
											  parameters.icon == diagnostics_icons::Icon::None
										  ? diagnostics_icons::Icon::Copy
										  : parameters.icon;
				icon(context, "action-icon", semantic,
					 parameters.enabled ? interface_theme::kTextSecondary
										: interface_theme::kTextMuted,
					 parameters.section_header ? 12 : 14);
				text(context, parameters.label,
					 parameters.enabled ? interface_theme::kTextCanvas
										: interface_theme::kTextMuted,
					 12);
			}
			if (!parameters.tooltip.empty() &&
				context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID())) {
				pointer_tooltip(context, LocalElementName{"tooltip"}, parameters.tooltip);
			}
		}
	}
};

inline constexpr Control kControl{};
inline constexpr FSEL::NumberInput<uint64_t, DefinitionID("flowui.dev.diagnostics.number")>
	kNumber{};

/** Draw a stable, keyboard-focusable action with owned labels. */
template <class Context, class Identity>
void button_with_identity(Context& context, const Identity& identity, std::string label,
						  void* target, void (*activate)(void*, uint64_t, uint64_t),
						  uint64_t action, uint64_t value = 0, bool enabled = true,
						  std::string tooltip = {},
						  diagnostics_icons::Icon icon = diagnostics_icons::Icon::None) {
	context.uiManager.createElement(kControl, identity)
		.setParameters({.label = std::move(label),
						.tooltip = std::move(tooltip),
						.icon = icon,
						.activate = activate,
						.target = target,
						.action = action,
						.value = value,
						.enabled = enabled})
		.setDevInternalCapture(true)
		.draw();
}

template <class Context>
void button(Context& context, LocalElementName identity, std::string label, void* target,
			void (*activate)(void*, uint64_t, uint64_t), uint64_t action, uint64_t value = 0,
			bool enabled = true, std::string tooltip = {},
			diagnostics_icons::Icon icon = diagnostics_icons::Icon::None) {
	button_with_identity(context, identity, std::move(label), target, activate, action, value,
						 enabled, std::move(tooltip), icon);
}
template <class Context>
void button(Context& context, IndexedElementName identity, std::string label, void* target,
			void (*activate)(void*, uint64_t, uint64_t), uint64_t action, uint64_t value = 0,
			bool enabled = true, std::string tooltip = {},
			diagnostics_icons::Icon icon = diagnostics_icons::Icon::None) {
	button_with_identity(context, identity, std::move(label), target, activate, action, value,
						 enabled, std::move(tooltip), icon);
}

/** Inset selector consistent with existing shared Performance and Memory controls. */
template <class Context>
void choice(Context& context, LocalElementName identity, std::string_view label, uint64_t& selected,
			std::span<const FSEL::ComboBoxOption> options) {
	text(context, label, interface_theme::kTextMuted, 11);
	FSEL::ComboBoxParameters parameters{};
	parameters.options = options;
	parameters.selectedValue = &selected;
	parameters.fontSize = 12;
	parameters.labelAlignment = CLAY_TEXT_ALIGN_CENTER;
	parameters.sizing = Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(30)};
	context.uiManager.createElement(FSEL::kComboBox, identity)
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
}

/** Exact unsigned identity or application-tick bound, without floating-point loss. */
template <class Context>
void number(Context& context, LocalElementName identity, std::string_view label, uint64_t& value,
			uint64_t minimum = 0, uint64_t maximum = UINT64_MAX,
			std::optional<int16_t> floating_z_index = std::nullopt) {
	text(context, label, interface_theme::kTextMuted, 11);
	FSEL::NumberInputParameters<uint64_t> parameters{};
	parameters.value = &value;
	parameters.minimum = minimum;
	parameters.maximum = maximum;
	parameters.fontSize = 12;
	parameters.floatingZIndex = floating_z_index;
	parameters.centerText = true;
	parameters.padding = Clay_Padding{8, 8, 4, 4};
	parameters.sizing = Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(30)};
	context.uiManager.createElement(kNumber, identity)
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
}

} // namespace FlowUi::devSystems::interface_elements::diagnostics_ui

#endif
