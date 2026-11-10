#pragma once

#include <cstdint>
#include <string>

#include <FlowUi/Public/Flow.hpp>
#include <devSystems/devTooling/schema/DevSchemaDescriptor.hpp>

/** Parameters for a reusable, inspectable stateful element. */
struct TemplateParameters {
	std::int32_t value = 42;
};

/** Per-instance state retained by the element manager. */
struct TemplateState {
	bool enabled = true;
};

/** A clickable value card; pressing it toggles its local enabled state. */
struct TemplateElement {
	using Parameters = TemplateParameters;
	using State = TemplateState;
	using InteractionContext = FlowUi::ElementInteractionContext<TemplateElement>;
	using BuildContext = FlowUi::ElementBuildContext<TemplateElement>;

	static constexpr FlowUi::FlowDefinitionID definitionId = FlowUi::DefinitionID("template");
	static constexpr std::string_view debugName = "Template value card";

	static void onHovered(InteractionContext& context) {
		context.uiManager.requestCursor(FlowUi::CursorType::PointingHand);
	}

	static void onPressed(InteractionContext& context) {
		context.state().enabled = !context.state().enabled;
	}

	static void buildElement(BuildContext& context) {
		Clay_ElementDeclaration root{};
		root.layout.sizing = {
			.width = CLAY_SIZING_FIT(0),
			.height = CLAY_SIZING_FIT(0),
		};
		root.layout.padding = CLAY_PADDING_ALL(12);
		root.backgroundColor =
			context.state().enabled ? FlowUi::Flow_Color("#315a80") : FlowUi::Flow_Color("#353a40");
		Clay_TextElementConfig text{};
		text.fontSize = 16;
		text.textColor = FlowUi::Flow_Color("#ffffff");
		const std::string label =
			std::string(context.state().enabled ? "Enabled: " : "Disabled: ") +
			std::to_string(context.params.value);
		CLAY(context.clayID(), root) {
			CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(text));
		}
	}
};

static_assert(FlowUi::DrawableFlowElement<TemplateElement>);
inline constexpr TemplateElement kTemplate{};

// Modern ADL schemas are discovered when the element is registered or built.
FLOWUI_DEV_SCHEMA(TemplateParameters, FLOWUI_DEV_FIELD(TemplateParameters, value))
FLOWUI_DEV_SCHEMA(TemplateState, FLOWUI_DEV_FIELD(TemplateState, enabled))
