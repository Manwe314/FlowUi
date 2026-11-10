#pragma once

#include <string_view>

#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"

namespace FlowUi {

/** Parameters for FlowUi's reusable action-driven Button element. */
struct ButtonParameters {
	/** Action invoked for an enabled activation gesture. */
	ActionCall onActivate{};
	/** Borrowed UTF-8 label; keep its storage valid through the frame. */
	std::string_view label{};
	/** Clay width and height sizing rules for the root element. */
	Clay_Sizing sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	/** Surface background color in Clay RGBA channels. */
	Clay_Color backgroundColor = Flow_Color("#cfcfcfff");
	/** Surface fill while activation is disabled. */
	Clay_Color disabledBackgroundColor = Flow_Color("#aaaaaaff");
	/** Text color in Clay RGBA channels. */
	Clay_Color textColor = Flow_Color("#000000ff");
	/** Label color while activation is disabled. */
	Clay_Color disabledTextColor = Flow_Color("#666666ff");
	/** Border color in Clay RGBA channels. */
	Clay_Color borderColor = Flow_Color("#8f8d8dff");
	/** Corner radii in layout pixels. */
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	/** Per-edge border widths in layout pixels. */
	Clay_BorderWidth borderWidth{1, 1, 1, 1, 0};
	/** Inset between the root border and its children, in layout pixels. */
	Clay_Padding padding = CLAY_PADDING_ALL(10);
	/** Resolved font face identifier used by Clay text rendering. */
	uint16_t fontId = 0;
	/** Text font size in layout pixels. */
	uint16_t fontSize = 16;
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
};

/** Stateless standard Button. All activation behavior lives in onActivate. */
struct Button {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = ButtonParameters;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<Button>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<Button>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.standard.button");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "Button";

	/** Return whether the element and its action are available. */
	[[nodiscard]] static bool isEnabled(const InteractionContext& context) {
		return context.params.enabled &&
			context.actionAvailability(context.params.onActivate).enabled;
	}

	/**
	 * ElementBuilder hover hook; requests the enabled control cursor. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onHovered(InteractionContext& context) {
		if (isEnabled(context)) {
			context.uiManager.requestCursor(CursorType::PointingHand);
		}
	}

	/**
	 * ElementBuilder press hook; begins the enabled control interaction. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onPressed(InteractionContext& context) {
		if (isEnabled(context)) {
			(void)context.invoke(context.params.onActivate);
		}
	}

	/**
	 * ElementBuilder draw hook; emits this control and its children for the active frame. @param
	 * context Borrowed context valid for this callback only.
	 */
	static void buildElement(BuildContext& context) {
		const bool enabled = context.params.enabled &&
			context.uiManager.actions().availability(context.params.onActivate).enabled;
		Clay_ElementDeclaration root{};
		root.layout.sizing = context.params.sizing;
		root.layout.padding = context.params.padding;
		root.layout.childAlignment = {
			.x = CLAY_ALIGN_X_CENTER,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		root.backgroundColor = enabled
			? context.params.backgroundColor
			: context.params.disabledBackgroundColor;
		root.cornerRadius = context.params.cornerRadius;
		root.border = {
			.color = context.params.borderColor,
			.width = context.params.borderWidth,
		};

		Clay_TextElementConfig text{};
		text.textColor = enabled
			? context.params.textColor
			: context.params.disabledTextColor;
		text.fontId = context.params.fontId;
		text.fontSize = context.params.fontSize;
		text.wrapMode = CLAY_TEXT_WRAP_NONE;
		text.textAlignment = CLAY_TEXT_ALIGN_CENTER;

		CLAY(context.clayID(), root) {
			CLAY_TEXT(
				context.uiManager.toClayString(context.params.label),
				CLAY_TEXT_CONFIG(text));
		}
	}
};

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr Button kButton{};
static_assert(FlowElement<Button>);

} // namespace FlowUi
