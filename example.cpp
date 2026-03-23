#include <FlowUi.hpp>

void registerTemplateCheckBox(FlowUi::ElementRegistry& elementRegistry)
{
	FlowUi::ElementDefinition definition{};
	definition.elementTypeName = "CheckBox";
	definition.initializeDefaultParameters = [](FlowUi::ElementParameters& defaults)
	{
		defaults.setValue("width", 70.0f);
		defaults.setValue("height", 70.0f);
		defaults.setValue("cornerRadius", 10.0f);
		defaults.setValue("middleColor", Clay_Color{0.0f, 255.0f, 0.0f, 255.0f});
		defaults.setValue("BorderColor", Clay_Color{255.0f, 255.0f, 255.0f, 255.0f});
		defaults.setValue("ID", 0);
		defaults.setValue("IsActive", false);
		defaults.setValue("borderWidth", 2.0f);
	};

	definition.onPressed = [](FlowUi::ElementInteractionContext& context)
	{
		if (int* clickID = context.bindings.getPointer<int>("clickId"))
			*(clickID) = context.parameters.getValueOrDefault<int>("ID", 0);
		context.parameters.setValue("IsActive", true);
	};

	definition.runLogic = [](FlowUi::ElementInteractionContext& context) {
		if (int* clickID = context.bindings.getPointer<int>("clickId"))
		{
			if (*clickID != context.parameters.getValueOrDefault<int>("ID", 0))
				context.parameters.setValue("IsActive", false);
		}
	};

	definition.buildElement = [](FlowUi::ElementBuildContext& context)
	{
		const float width = context.parameters.getValueOrDefault<float>("width", 70.0f);
		const float height = context.parameters.getValueOrDefault<float>("height", 70.0f);
		const float cornerRadius = context.parameters.getValueOrDefault<float>("cornerRadius", 10.0f);
		const Clay_Color middleColor = context.parameters.getValueOrDefault<Clay_Color>(
			"middleColor",
			Clay_Color{0.0f, 255.0f, 0.0f, 255.0f});
		const Clay_Color borderColor = context.parameters.getValueOrDefault<Clay_Color>(
			"BorderColor",
			Clay_Color{255.0f, 255.0f, 255.0f, 255.0f});
		const bool IsActive = context.parameters.getValueOrDefault<bool>("IsActive", false);
		const uint16_t borderWidth = static_cast<uint16_t>(context.parameters.getValueOrDefault<float>("borderWidth", 2.0f));
		

		Clay_ElementDeclaration root{};
		root.id = context.elementId;
		root.backgroundColor = IsActive ? middleColor : Clay_Color{0.0f, 0.0f, 0.0f, 0.0f};
		root.cornerRadius = CLAY_CORNER_RADIUS(cornerRadius);

		root.layout.sizing.width = CLAY_SIZING_FIXED(width);
		root.layout.sizing.height = CLAY_SIZING_FIXED(height);
		root.layout.padding = CLAY_PADDING_ALL(10);
		root.border = Clay_BorderElementConfig {
			.color = borderColor,
			.width = Clay_BorderWidth{
				.left = borderWidth,
				.right = borderWidth,
				.top = borderWidth,
				.bottom = borderWidth,
				.betweenChildren = 0
			}
		};
		CLAY(root) {
		}
	};

	elementRegistry.registerElement(std::move(definition));
}

Clay_Color setBackGroundColor(int& activeId)
{
	Clay_Color color{255.0f, 255.0f, 255.0f, 255.0f};
	switch (activeId)
	{
	case 0:
		color = {255.0f, 0.0f, 0.0f, 255.0f};
		break;
	case 1:
		color = {0.0f, 255.0f, 0.0f, 255.0f};
		break;
	case 2:
		color = {0.0f, 0.0f, 255.0f, 255.0f};
		break;
	default:
		break;
	}
	return color;
}

int main()
{
	FlowUi::AppConfig config{};

	FlowUi::App application = FlowUi::makeApplication(config);
	registerTemplateCheckBox(application.elementRegistry());
	FlowUi::UiContext& Ui = application.ui();
	int activeId = -1;

	while (!application.shouldClose())
	{
		application.beginFrame();

		CLAY({.id = CLAY_ID("BackGround"), .layout = {
				.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
				.childGap = 16,
				.childAlignment = (Clay_ChildAlignment){ CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
				.layoutDirection = CLAY_TOP_TO_BOTTOM
				},
				.backgroundColor = setBackGroundColor(activeId)
			
			}) {
				Ui.createElement("CheckBox", "box1")
					.set("BorderColor", Clay_Color{255.0f, 0.0f, 0.0f, 255.0f})
					.set("ID", 0)
					.bind("clickId", activeId)
					.draw();
				Ui.createElement("CheckBox", "box2")
					.set("BorderColor", Clay_Color{0.0f, 255.0f, 0.0f, 255.0f})
					.set("ID", 1)
					.bind("clickId", activeId)
					.draw();
				Ui.createElement("CheckBox", "box3")
					.set("BorderColor", Clay_Color{0.0f, 0.0f, 255.0f, 255.0f})
					.set("ID", 2)
					.bind("clickId", activeId)
				.draw();
		}

		application.endFrame();
		application.drawFrame();
	}
	return 0;
}
