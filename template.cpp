#include "managers/FlowUiElementSystem.hpp"
#include "managers/UiManager.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// FlowUi Element Template
// ---------------------------------------------------------------------------
// Copy this file (or sections of it) into your project and rename:
// - element type names ("TemplateButton", "TemplateTextField")
// - parameter keys ("label", "placeholder", ...)
// - binding keys ("text", "focusedFieldPath", ...)
// ---------------------------------------------------------------------------


// Register one custom element type into your ElementRegistry.
// Call this once during your app setup/initialization.
void registerTemplatePanel(FlowUi::ElementRegistry& elementRegistry)
{
	FlowUi::ElementDefinition definition{};
	definition.elementTypeName = "TemplatePanel";

	definition.initializeDefaultParameters = [](FlowUi::ElementParameters& defaults) {
		defaults.setValue("padding", 12);
		defaults.setValue("childGap", 10);
		defaults.setValue("layoutDirection", CLAY_TOP_TO_BOTTOM);
		defaults.setValue("backgroundColor", Clay_Color{24.0f, 29.0f, 39.0f, 255.0f});
		defaults.setValue("cornerRadius", 12.0f);
	};

	// New composable path: construct root only, children are authored at call site
	// between .construct() and ui.drawConstructed().
	definition.constructElment = [](FlowUi::ElementBuildContext& context) -> Clay_ElementDeclaration {
		Clay_ElementDeclaration root{};
		root.id = context.elementId;
		root.backgroundColor = context.parameters.getValue<Clay_Color>("backgroundColor");
		root.cornerRadius = CLAY_CORNER_RADIUS(context.parameters.getValue<float>("cornerRadius"));
		root.layout.sizing.width = CLAY_SIZING_FIT(220, 640);
		root.layout.sizing.height = CLAY_SIZING_FIT(0, 0);
		root.layout.padding = CLAY_PADDING_ALL(context.parameters.getValue<int>("padding"));
		root.layout.childGap = context.parameters.getValue<int>("childGap");
		root.layout.layoutDirection = context.parameters.getValue<Clay_LayoutDirection>("layoutDirection");
		return root;
	};

	elementRegistry.registerElement(std::move(definition));
}

// Register one self-contained custom element type into your ElementRegistry.
// Call this once during your app setup/initialization.
void registerTemplateButton(FlowUi::ElementRegistry& elementRegistry)
{
	FlowUi::ElementDefinition definition{};

	// 1) Unique element type name (used by UiManager::createElement).
	definition.elementTypeName = "TemplateButton";

	// 2) Default parameters (users can override with .set(...)).
	definition.initializeDefaultParameters = [](FlowUi::ElementParameters& defaults) {
		defaults.setValue("label", std::string("Template Button"));
		defaults.setValue("width", 220.0f);
		defaults.setValue("height", 44.0f);
		defaults.setValue("cornerRadius", 10.0f);
		defaults.setValue("fontSize", 18);
		defaults.setValue("backgroundColor", Clay_Color{52.0f, 94.0f, 239.0f, 255.0f});
		defaults.setValue("textColor", Clay_Color{255.0f, 255.0f, 255.0f, 255.0f});
	};

	// 3) Optional input event callbacks.
	//    Bind external state with ElementBuilder::bind("key", reference).
	definition.onPressed = [](FlowUi::ElementInteractionContext& context) {
		if (int* clickCount = context.bindings.getPointer<int>("clickCount")) {
			++(*clickCount);
		}
		if (bool* pressedThisFrame = context.bindings.getPointer<bool>("pressedThisFrame")) {
			*pressedThisFrame = true;
		}

		// Optional per-instance callback parameter.
		if (FlowUi::ElementCustomCallback customPressedCallback =
				context.parameters.getValue<FlowUi::ElementCustomCallback>("onPressedCallback");
			customPressedCallback) {
			customPressedCallback(context);
		}
	};

	// 4) Optional per-frame logic callback.
	definition.runLogic = [](FlowUi::ElementInteractionContext& context) {
		// Example: if bound, clear "pressedThisFrame" before event handling.
		// Keep/remove this based on your game/app state model.
		if (bool* pressedThisFrame = context.bindings.getPointer<bool>("pressedThisFrame")) {
			*pressedThisFrame = false;
		}
	};

	// 5) Mandatory build callback (actual Clay UI construction).
	definition.buildElement = [](FlowUi::ElementBuildContext& context) {
		const std::string_view label = context.parameters.getString("label");
		const float width = context.parameters.getValue<float>("width");
		const float height = context.parameters.getValue<float>("height");
		const float cornerRadius = context.parameters.getValue<float>("cornerRadius");
		const int fontSize = context.parameters.getValue<int>("fontSize");
		const Clay_Color backgroundColor = context.parameters.getValue<Clay_Color>("backgroundColor");
		const Clay_Color textColor = context.parameters.getValue<Clay_Color>("textColor");

		Clay_ElementDeclaration root{};
		root.id = context.elementId;
		root.backgroundColor = backgroundColor;
		root.cornerRadius = CLAY_CORNER_RADIUS(cornerRadius);
		root.layout.sizing.width = CLAY_SIZING_FIXED(width);
		root.layout.sizing.height = CLAY_SIZING_FIXED(height);
		root.layout.padding = CLAY_PADDING_ALL(10);
		root.layout.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;

		CLAY(root) {
			CLAY_TEXT(
				context.userInterface.toClayString(label),
				CLAY_TEXT_CONFIG({
					.textColor = textColor,
					.fontId = 0,
					.fontSize = static_cast<uint16_t>(fontSize),
					.wrapMode = CLAY_TEXT_WRAP_NONE,
					.textAlignment = CLAY_TEXT_ALIGN_CENTER}));
		}
	};

	elementRegistry.registerElement(std::move(definition));
}

// Convenience helper: register both template elements.
void registerTemplateElements(FlowUi::ElementRegistry& elementRegistry)
{
	registerTemplatePanel(elementRegistry);
	registerTemplateButton(elementRegistry);
}

// Example usage each frame (inside your UI build code).
void drawTemplateButton(FlowUi::UiManager& uiContext, int& clickCount, bool& pressedThisFrame)
{
	uiContext.createElement("TemplateButton", "main_menu/play_button")
		// Optional per-instance overrides:
		.set("label", "Play")
		.set("width", 260.0f)
		.set("height", 52.0f)
		.set("onPressedCallback", [](FlowUi::ElementInteractionContext& callbackContext) {
			if (int* count = callbackContext.bindings.getPointer<int>("clickCount")) {
				*count += 10;
			}
		})
		// Bind references for callbacks/logic:
		.bind("clickCount", clickCount)
		.bind("pressedThisFrame", pressedThisFrame)
		.draw();
}

// Example composed usage:
// Parent uses construct(), children are declared in the same frame scope.
void drawTemplateButtonColumn(FlowUi::UiManager& uiContext, int& clickCount, bool& pressedThisFrame)
{
	uiContext.createElement("TemplatePanel", "main_menu/button_column")
		.set("padding", 14)
		.set("childGap", 12)
		.construct();

	uiContext.createElement("TemplateButton", "main_menu/button_column/play_button")
		.set("label", "Play")
		.bind("clickCount", clickCount)
		.bind("pressedThisFrame", pressedThisFrame)
		.draw();

	uiContext.createElement("TemplateButton", "main_menu/button_column/settings_button")
		.set("label", "Settings")
		.bind("clickCount", clickCount)
		.bind("pressedThisFrame", pressedThisFrame)
		.draw();

	uiContext.drawConstructed();
}
