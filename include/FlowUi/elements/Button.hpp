#pragma once

#include <string_view>

#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi {

/** Parameters for FlowUi's reusable action-driven Button element. */
struct ButtonParameters {
	std::string_view label{};
	ActionCall onActivate{};
	bool enabled = true;
	Clay_Padding padding = CLAY_PADDING_ALL(10);
	Clay_Sizing sizing{
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	Clay_Color backgroundColor = Flow_Color("#cfcfcfff");
	Clay_Color disabledBackgroundColor = Flow_Color("#aaaaaaff");
	Clay_Color textColor = Flow_Color("#000000ff");
	Clay_Color disabledTextColor = Flow_Color("#666666ff");
	Clay_Color borderColor = Flow_Color("#8f8d8dff");
	Clay_CornerRadius cornerRadius = CLAY_CORNER_RADIUS(6);
	Clay_BorderWidth borderWidth{1, 1, 1, 1, 0};
	uint16_t fontId = 0;
	uint16_t fontSize = 16;
};

/** Stateless standard Button. All activation behavior lives in onActivate. */
struct Button {
	using Parameters = ButtonParameters;
	using BuildContext = ElementBuildContext<Button>;
	using InteractionContext = ElementInteractionContext<Button>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.standard.button");
	static constexpr std::string_view debugName = "Button";

	static bool isEnabled(const InteractionContext& context) {
		return context.params.enabled &&
			context.actionAvailability(context.params.onActivate).enabled;
	}

	static void onHovered(InteractionContext& context) {
		if (isEnabled(context)) {
			context.uiManager.requestCursor(CursorType::PointingHand);
		}
	}

	static void onPressed(InteractionContext& context) {
		if (isEnabled(context)) {
			(void)context.invoke(context.params.onActivate);
		}
	}

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

inline constexpr Button kButton{};
static_assert(FlowElement<Button>);

} // namespace FlowUi
