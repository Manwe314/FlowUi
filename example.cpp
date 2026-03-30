#include <FlowUi/Flow.hpp>

struct CheckBoxParams {
	float width = 70.0f;
	float height = 70.0f;
	float cornerRadius = 10.0f;
	float borderWidth = 2.0f;
	Clay_Color middleColor{0.0f, 255.0f, 0.0f, 255.0f};
	Clay_Color borderColor{255.0f, 255.0f, 255.0f, 255.0f};
	int id = -1;
};

struct CheckBoxState {
	bool isActive = false;
};

using CheckBoxDefinition = FlowUi::ElementDefinition<
	CheckBoxParams,
	CheckBoxState,
	void,
	FLOW_DEF_ID("checkbox")>;

inline const CheckBoxDefinition kCheckBox = {
	nullptr,
	+[](CheckBoxDefinition::InteractionContext& context) {
		const uint64_t clickedFlowId = FlowUi::toFlowId(context.elementID);
		for (auto& entry : CheckBoxDefinition::statePool) {
			entry.second.isActive = false;
		}
		CheckBoxDefinition::getOrCreateState(clickedFlowId).isActive = true;
	},
	nullptr,
	nullptr,

	nullptr,

	nullptr,
	+[](CheckBoxDefinition::BuildContext& context) {
		CheckBoxState& state = CheckBoxDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
		root.backgroundColor = state.isActive ? context.params.middleColor : Clay_Color{0.0f, 0.0f, 0.0f, 0.0f};
		root.cornerRadius = CLAY_CORNER_RADIUS(context.params.cornerRadius);
		root.layout.sizing.width = CLAY_SIZING_FIXED(context.params.width);
		root.layout.sizing.height = CLAY_SIZING_FIXED(context.params.height);
		root.layout.padding = CLAY_PADDING_ALL(10);
		root.border = Clay_BorderElementConfig{
			.color = context.params.borderColor,
			.width = Clay_BorderWidth{
				.left = static_cast<uint16_t>(context.params.borderWidth),
				.right = static_cast<uint16_t>(context.params.borderWidth),
				.top = static_cast<uint16_t>(context.params.borderWidth),
				.bottom = static_cast<uint16_t>(context.params.borderWidth),
				.betweenChildren = 0,
			}
		};

		CLAY(root) {}
	},
};

static Clay_Color setBackGroundColorFromState()
{
	const CheckBoxState* box1State = CheckBoxDefinition::tryGetStateConst(FLOW_ID("box1"));
	if (box1State && box1State->isActive) {
		return Clay_Color{255.0f, 0.0f, 0.0f, 255.0f};
	}
	const CheckBoxState* box2State = CheckBoxDefinition::tryGetStateConst(FLOW_ID("box2"));
	if (box2State && box2State->isActive) {
		return Clay_Color{0.0f, 255.0f, 0.0f, 255.0f};
	}
	const CheckBoxState* box3State = CheckBoxDefinition::tryGetStateConst(FLOW_ID("box3"));
	if (box3State && box3State->isActive) {
		return Clay_Color{0.0f, 0.0f, 255.0f, 255.0f};
	}
	return Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
}

int main()
{
	FlowUi::AppConfig config{};
	FlowUi::App application = FlowUi::makeApplication(config);
	FlowUi::UiManager& ui = application.ui();

	while (!application.shouldClose()) {
		application.beginFrame();

		CLAY({
			.id = CLAY_ID("BackGround"),
			.layout = {
				.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
				.childGap = 16,
				.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
				.layoutDirection = CLAY_TOP_TO_BOTTOM
			},
			.backgroundColor = setBackGroundColorFromState()
		}) {
			ui.createElement(kCheckBox, "box1")
				.setParameters(CheckBoxParams{
					.borderColor = Clay_Color{255.0f, 0.0f, 0.0f, 255.0f},
					.id = 0,
				})
				.draw();

			ui.createElement(kCheckBox, "box2")
				.setParameters(CheckBoxParams{
					.borderColor = Clay_Color{0.0f, 255.0f, 0.0f, 255.0f},
					.id = 1,
				})
				.draw();

			ui.createElement(kCheckBox, "box3")
				.setParameters(CheckBoxParams{
					.borderColor = Clay_Color{0.0f, 0.0f, 255.0f, 255.0f},
					.id = 2,
				})
				.draw();
		}

		application.endFrame();
		application.drawFrame();
	}
	return 0;
}
