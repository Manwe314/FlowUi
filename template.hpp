#pragma once

#include <FlowUi/Flow.hpp>
#include <devMode/devApi.hpp>

struct templateParams {
	int importantInt = 42;
};

struct templateState {
	bool enabled = true;
};

struct templateResources {
	templateResources() = default;
	explicit templateResources(FlowUi::App& app) {
		(void)app;
	}
};

using TemplateDefinition = FlowUi::ElementDefinition<
	templateParams,
	templateState,
	templateResources,
	FLOW_DEF_ID("template")>;

inline const TemplateDefinition kTemplate = {
	// onHovered: optional callback when this element was hovered in the previous frame
	+[](TemplateDefinition::InteractionContext& context) {
		(void)context;
	},

	// onPressed: optional callback when this element was pressed in the previous frame
	+[](TemplateDefinition::InteractionContext& context) {
		(void)context;
	},

	// onHeld: optional callback when this element was held in the previous frame
	+[](TemplateDefinition::InteractionContext& context) {
		(void)context;
	},

	// onReleased: optional callback when this element was released in the previous frame
	+[](TemplateDefinition::InteractionContext& context) {
		(void)context;
	},

	// runLogic: optional per-frame logic callback before build/construct callback
	+[](TemplateDefinition::InteractionContext& context) {
		(void)context;
		// Example state access:
		// auto& state = context.state();
	},

	// constructElement: optional callback used by .construct() flows
	+[](TemplateDefinition::BuildContext& context) -> Clay_ElementDeclaration {
		(void)context;
		// Example resources access:
		// const auto& resources = context.resources();
		return Clay_ElementDeclaration{};
	},

	// buildElement: callback used by .draw()
	+[](TemplateDefinition::BuildContext& context) {
		(void)context;
	},
};

// Dev-mode registration examples:
FLOWUI_DEV_REGISTER_STRUCT(
	templateParams,
	FLOWUI_DEV_REFLECT_FIELD(templateParams, importantInt));

FLOWUI_DEV_REGISTER_STRUCT(
	templateState,
	FLOWUI_DEV_REFLECT_FIELD(templateState, enabled));

// Empty registration is valid when a struct has no editable fields yet.
FLOWUI_DEV_REGISTER_STRUCT(templateResources);

FLOWUI_DEV_REGISTER_ELEMENT(TemplateDefinition, "Template");
