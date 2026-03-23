#pragma once

#include "managers/FlowUiElementSystem.hpp"


inline const FlowUi::ElementDefinition kElement = {
	// elementTypeName: unique element type key (used for registration/lookups).
	"TemplateElement",

	// initializeDefaultParameters: optional defaults merged with per-instance overrides.
	[](FlowUi::ElementParameters& defaults) {
		defaults.setValue("enabled", true);
		defaults.setValue("padding", 8);
	},

	// onHovered: optional callback when this element was hovered in the previous frame.
	[](FlowUi::ElementInteractionContext& context) {
		(void)context;
	},

	// onPressed: optional callback when this element was pressed in the previous frame.
	[](FlowUi::ElementInteractionContext& context) {
		(void)context;
	},

	// onHeld: optional callback when this element was held in the previous frame.
	[](FlowUi::ElementInteractionContext& context) {
		(void)context;
	},

	// onReleased: optional callback when this element was released in the previous frame.
	[](FlowUi::ElementInteractionContext& context) {
		(void)context;
	},

	// runLogic: optional per-frame logic callback before buildElement executes.
	[](FlowUi::ElementInteractionContext& context) {
		(void)context;
	},

	// buildElement: required callback where the element's Clay UI is built.
	[](FlowUi::ElementBuildContext& context) {
		(void)context;
	},
};

