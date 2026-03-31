#pragma once

#include <FlowUi/Flow.hpp>

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

using TemplateDefiition = FlowUi::ElementDefinition<
	templateParams,
	templateState,
	templateResources,
	FLOW_DEF_ID("template")>;

inline const TemplateDefiition kTemplate = {
	// onHovered: optional callback when this element was hovered in the previous frame
	+[](TemplateDefiition::InteractionContext& context) {
		(void)context;
	},

	// onPressed: optional callback when this element was pressed in the previous frame
	+[](TemplateDefiition::InteractionContext& context) {
		(void)context;
	},

	// onHeld: optional callback when this element was held in the previous frame
	+[](TemplateDefiition::InteractionContext& context) {
		(void)context;
	},

	// onReleased: optional callback when this element was released in the previous frame
	+[](TemplateDefiition::InteractionContext& context) {
		(void)context;
	},

	// runLogic: optional per-frame logic callback before build/construct callback
	+[](TemplateDefiition::InteractionContext& context) {
		(void)context;
		// Example state access:
		// auto& state = TemplateDefiition::getOrCreateState(FlowUi::toFlowId(context.elementID));
	},

	// constructElment: optional callback used by .construct() flows
	+[](TemplateDefiition::BuildContext& context) -> Clay_ElementDeclaration {
		(void)context;
		// Example resources access:
		// auto& resources = TemplateDefiition::resources.value();
		return Clay_ElementDeclaration{};
	},

	// buildElement: callback used by .draw()
	+[](TemplateDefiition::BuildContext& context) {
		(void)context;
	},
};
