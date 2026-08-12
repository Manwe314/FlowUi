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

struct TemplateElement {
	using Parameters = templateParams;
	using State = templateState;
	using Resources = templateResources;
	using InteractionContext = FlowUi::ElementInteractionContext<TemplateElement>;
	using BuildContext = FlowUi::ElementBuildContext<TemplateElement>;

	struct Parts {
		static constexpr auto content = FlowUi::Part("content");
	};

	static constexpr FlowUi::FlowDefinitionID definitionId = FlowUi::DefinitionID("template");

	// onHovered: optional callback when this element was hovered in the previous frame
	static void onHovered(InteractionContext& context) {
		(void)context;
	}

	// onPressed: optional callback when this element was pressed in the previous frame
	static void onPressed(InteractionContext& context) {
		(void)context;
	}

	// onHeld: optional callback when this element was held in the previous frame
	static void onHeld(InteractionContext& context) {
		(void)context;
	}

	// onReleased: optional callback when this element was released in the previous frame
	static void onReleased(InteractionContext& context) {
		(void)context;
	}

	// runLogic: optional per-frame logic callback before build/construct callback
	static void runLogic(InteractionContext& context) {
		(void)context;
		// Example state access:
		// auto& state = context.state();
	}

	// constructElement: optional callback used by .construct() flows
	static Clay_ElementDeclaration constructElement(BuildContext& context) {
		(void)context;
		// Example resources access:
		// const auto& resources = context.resources();
		return Clay_ElementDeclaration{};
	}

	// buildElement: callback used by .draw()
	static void buildElement(BuildContext& context) {
		CLAY(context.uiManager.toClayEID(context.part(Parts::content)), {}) {}
	}
};

static_assert(FlowUi::FlowElement<TemplateElement>);
inline constexpr TemplateElement kTemplate{};

// Dev-mode registration examples:
FLOWUI_DEV_REGISTER_STRUCT(
	templateParams,
	FLOWUI_DEV_REFLECT_FIELD(templateParams, importantInt));

FLOWUI_DEV_REGISTER_STRUCT(
	templateState,
	FLOWUI_DEV_REFLECT_FIELD(templateState, enabled));

// Empty registration is valid when a struct has no editable fields yet.
FLOWUI_DEV_REGISTER_STRUCT(templateResources);

FLOWUI_DEV_REGISTER_ELEMENT(TemplateElement, "Template");
