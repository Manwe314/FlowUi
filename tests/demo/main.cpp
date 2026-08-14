#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include <FlowUi/Flow.hpp>

namespace {

using namespace FlowUi;
using namespace FlowUi::FSEL;

struct DemoState {
	uint64_t buttonActivations = 0;
	bool surfaceSelected = false;
	uint64_t selectedTool = 1;
	bool checkboxChecked = false;
	bool switchOn = true;
	double volume = 0.64;
	double temperature = 42.0;
	double invisibleValue = 0.25;
	uint64_t sliderBegins = 0;
	uint64_t sliderChanges = 0;
	uint64_t sliderCommits = 0;
	float leftPaneWidth = 465.0f;
};

constexpr auto kIncrement = UiAction(
	"fsel.demo.increment",
	[](uint64_t& value) { ++value; });
constexpr auto kToggle = UiAction(
	"fsel.demo.toggle",
	[](bool& value) { value = !value; });
constexpr auto kSelect = UiAction(
	"fsel.demo.select",
	[](bool& value) { value = true; });
constexpr auto kClear = UiAction(
	"fsel.demo.clear",
	[](bool& value) { value = false; });
constexpr auto kReset = UiAction(
	"fsel.demo.reset",
	[](DemoState& state) { state = DemoState{}; });

struct DemoActions {
	ActionCall activateButton{};
	ActionCall selectSurface{};
	ActionCall clearSurface{};
	ActionCall toggleCheckbox{};
	ActionCall toggleSwitch{};
	ActionCall sliderBegin{};
	ActionCall sliderChange{};
	ActionCall sliderCommit{};
	ActionCall reset{};
};

DemoActions makeActions(App& app, DemoState& state) {
	auto& actions = app.actions().uiActions();
	return {
		.activateButton = ActionCall{actions.make(kIncrement, state.buttonActivations)},
		.selectSurface = ActionCall{actions.make(kSelect, state.surfaceSelected)},
		.clearSurface = ActionCall{actions.make(kClear, state.surfaceSelected)},
		.toggleCheckbox = ActionCall{actions.make(kToggle, state.checkboxChecked)},
		.toggleSwitch = ActionCall{actions.make(kToggle, state.switchOn)},
		.sliderBegin = ActionCall{actions.make(kIncrement, state.sliderBegins)},
		.sliderChange = ActionCall{actions.make(kIncrement, state.sliderChanges)},
		.sliderCommit = ActionCall{actions.make(kIncrement, state.sliderCommits)},
		.reset = ActionCall{actions.make(kReset, state)},
	};
}

Clay_TextElementConfig textStyle(
	UiManager& ui,
	uint16_t size,
	Clay_Color color,
	uint32_t weight = 400,
	Clay_TextElementConfigWrapMode wrap = CLAY_TEXT_WRAP_WORDS) {
	Clay_TextElementConfig style{};
	style.textColor = color;
	style.fontId = ui.resolveFont(0, weight, FontStyle::Normal);
	style.fontSize = size;
	style.wrapMode = wrap;
	style.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	return style;
}

const Clay_Color kText = Flow_Color("#eef4ffff");
const Clay_Color kMuted = Flow_Color("#91a0b8ff");
const Clay_Color kAccent = Flow_Color("#70d6ffff");
const Clay_Color kWarm = Flow_Color("#ffb86cff");
const Clay_Color kCard = Flow_Color("#151d2aff");
const Clay_Color kCardBorder = Flow_Color("#2d3a50ff");

void drawText(
	UiManager& ui,
	std::string_view text,
	const Clay_TextElementConfig& style) {
	CLAY_TEXT(ui.toClayString(text), CLAY_TEXT_CONFIG(style));
}

std::string decimal(double value, int precision = 2) {
	std::ostringstream stream;
	stream << std::fixed << std::setprecision(precision) << value;
	return stream.str();
}

BoxParameters cardParameters() {
	return {
		.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIT(0),
		},
		.padding = CLAY_PADDING_ALL(16),
		.childGap = 10,
		.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_TOP,
		},
		.layoutDirection = CLAY_TOP_TO_BOTTOM,
		.backgroundColor = kCard,
		.borderColor = kCardBorder,
		.cornerRadius = CLAY_CORNER_RADIUS(10),
		.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0},
	};
}

Clay_ElementDeclaration row(float gap = 10.0f) {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	declaration.layout.childGap = static_cast<uint16_t>(gap);
	declaration.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	declaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	return declaration;
}

Clay_ElementDeclaration badge(Clay_Color color) {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIT(0),
	};
	declaration.layout.padding = {
		.left = 8,
		.right = 8,
		.top = 4,
		.bottom = 4,
	};
	declaration.backgroundColor = color;
	declaration.cornerRadius = CLAY_CORNER_RADIUS(999);
	return declaration;
}

void drawCardHeading(UiManager& ui, std::string_view title, std::string_view detail) {
	drawText(ui, title, textStyle(ui, 17, kText, 650));
	drawText(ui, detail, textStyle(ui, 12, kMuted));
}

void drawButtonsCard(UiManager& ui, const DemoState& state, const DemoActions& actions) {
	ui.createElement(kBox, "buttons-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"Button",
		"One control supports a built label or caller-authored constructed children.");

	CLAY(ui.toClaySID("demo/buttons/row"), row()) {
		ui.createElement(FlowUi::FSEL::kButton, "built-button")
			.setParameters(FlowUi::FSEL::ButtonParameters{
				.onActivate = actions.activateButton,
				.contentMode = ButtonContentMode::TextOnly,
				.text = "Built button",
			})
			.draw();

		ui.createElement(FlowUi::FSEL::kButton, "constructed-button")
			.setParameters(FlowUi::FSEL::ButtonParameters{
				.onActivate = actions.activateButton,
				.contentMode = ButtonContentMode::None,
				.contentGap = 8,
			})
			.construct();
		drawText(ui, "Constructed", textStyle(ui, 14, kText, 550, CLAY_TEXT_WRAP_NONE));
		CLAY(ui.toClaySID("demo/buttons/custom-badge"), badge(Flow_Color("#163c4aff"))) {
			drawText(ui, "USER CHILD", textStyle(ui, 10, kAccent, 700, CLAY_TEXT_WRAP_NONE));
		}
		ui.drawConstructed();
	}

	CLAY(ui.toClaySID("demo/buttons/status"), row()) {
		drawText(
			ui,
			"Activations: " + std::to_string(state.buttonActivations),
			textStyle(ui, 13, kWarm, 600, CLAY_TEXT_WRAP_NONE));
		ui.createElement(FlowUi::FSEL::kButton, "disabled-button")
			.setParameters(FlowUi::FSEL::ButtonParameters{
				.onActivate = actions.activateButton,
				.enabled = false,
				.contentMode = ButtonContentMode::TextOnly,
				.text = "Disabled",
				.padding = Clay_Padding{8, 8, 5, 5},
			})
			.draw();
	}
	ui.drawConstructed();
}

void drawSelectableCard(
	UiManager& ui,
	const DemoState& state,
	const DemoActions& actions) {
	ui.createElement(kBox, "selectable-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"SelectableSurface",
		"A controlled, construct-only surface. It reports selection once and then becomes selected.");

	ui.createElement(kSelectableSurface, "sample-surface")
		.setParameters(SelectableSurfaceParameters{
			.selected = state.surfaceSelected,
			.onSelected = actions.selectSurface,
			.style = SelectableSurfaceStyle{
				.sizing = {
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_FIT(0),
				},
			},
		})
		.construct();
	drawText(
		ui,
		state.surfaceSelected ? "Selected surface" : "Select this surface",
		textStyle(ui, 14, kText, 550));
	ui.createElement(kSpacer, "surface-spacer")
		.setParameters(SpacerParameters{
			.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIXED(1),
			},
		})
		.draw();
	CLAY(ui.toClaySID("demo/selectable/state"), badge(
		state.surfaceSelected
			? Flow_Color("#16452fff")
			: Flow_Color("#3a3047ff"))) {
		drawText(
			ui,
			state.surfaceSelected ? "SELECTED" : "READY",
			textStyle(ui, 10, state.surfaceSelected ? kAccent : kWarm, 700));
	}
	ui.drawConstructed();

	ui.createElement(FlowUi::FSEL::kButton, "clear-surface")
		.setParameters(FlowUi::FSEL::ButtonParameters{
			.onActivate = actions.clearSurface,
			.enabled = state.surfaceSelected,
			.contentMode = ButtonContentMode::TextOnly,
			.text = "Clear external selection",
			.padding = Clay_Padding{8, 8, 5, 5},
		})
		.draw();
	ui.drawConstructed();
}

void drawRadioOption(
	UiManager& ui,
	DemoState& state,
	uint64_t value,
	std::string_view label) {
	ui.createElement(kRadioChoice, Indexed("tool-option", value))
		.setParameters(RadioChoiceParameters{
			.choiceValue = value,
			.selectedValue = &state.selectedTool,
			.style = SelectableSurfaceStyle{
				.padding = Clay_Padding{12, 12, 8, 8},
			},
		})
		.construct();
	drawText(ui, label, textStyle(ui, 13, kText, 550, CLAY_TEXT_WRAP_NONE));
	ui.drawConstructed();
}

void drawRadioCard(UiManager& ui, DemoState& state) {
	ui.createElement(kBox, "radio-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"RadioChoice",
		"Each option is one constructed element. All three borrow the same selected value.");
	CLAY(ui.toClaySID("demo/radio/options"), row(8)) {
		drawRadioOption(ui, state, 1, "Move");
		drawRadioOption(ui, state, 2, "Paint");
		drawRadioOption(ui, state, 3, "Crop");
	}
	const std::string_view selected = state.selectedTool == 1
		? "Move"
		: state.selectedTool == 2 ? "Paint" : "Crop";
	drawText(
		ui,
		std::string("Shared selected value: ") + std::string(selected),
		textStyle(ui, 12, kAccent, 600));
	ui.drawConstructed();
}

void drawBooleanCard(
	UiManager& ui,
	const DemoState& state,
	const DemoActions& actions) {
	ui.createElement(kBox, "boolean-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"Checkbox + Switch",
		"Both are controlled booleans: parameters show the value; actions mutate application state.");

	CLAY(ui.toClaySID("demo/booleans/checkbox"), row(12)) {
		ui.createElement(kCheckbox, "checkbox")
			.setParameters(CheckboxParameters{
				.isChecked = state.checkboxChecked,
				.onToggle = actions.toggleCheckbox,
			})
			.draw();
		drawText(
			ui,
			state.checkboxChecked ? "Checkbox is checked" : "Checkbox is unchecked",
			textStyle(ui, 14, kText, 500));
	}

	CLAY(ui.toClaySID("demo/booleans/switch"), row(12)) {
		ui.createElement(kSwitch, "switch")
			.setParameters(SwitchParameters{
				.isOn = state.switchOn,
				.onToggle = actions.toggleSwitch,
				.roundness = 1.0f,
			})
			.draw();
		drawText(
			ui,
			state.switchOn ? "Switch is on" : "Switch is off",
			textStyle(ui, 14, kText, 500));
	}
	ui.drawConstructed();
}

void drawSlidersCard(
	UiManager& ui,
	DemoState& state,
	const DemoActions& actions) {
	ui.createElement(kBox, "sliders-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"Slider",
		"The full root is draggable. Values stay application-owned and can snap to an interval.");

	CLAY(ui.toClaySID("demo/sliders/volume-row"), row(14)) {
		drawText(ui, "Volume", textStyle(ui, 13, kText, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kSlider, "volume-slider")
			.setParameters(SliderParameters{
				.value = &state.volume,
				.minimum = 0.0,
				.maximum = 1.0,
				.roundingStep = 0.01,
				.onBegin = actions.sliderBegin,
				.onChanged = actions.sliderChange,
				.onCommit = actions.sliderCommit,
				.length = 260.0f,
			})
			.draw();
		drawText(
			ui,
			decimal(state.volume),
			textStyle(ui, 13, kAccent, 700, CLAY_TEXT_WRAP_NONE));
	}

	CLAY(ui.toClaySID("demo/sliders/variants"), row(18)) {
		Clay_ElementDeclaration verticalGroup{};
		verticalGroup.layout.sizing = {
			.width = CLAY_SIZING_FIXED(105),
			.height = CLAY_SIZING_FIT(0),
		};
		verticalGroup.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		verticalGroup.layout.childGap = 6;
		verticalGroup.layout.childAlignment = {
			.x = CLAY_ALIGN_X_CENTER,
			.y = CLAY_ALIGN_Y_TOP,
		};
		CLAY(ui.toClaySID("demo/sliders/vertical"), verticalGroup) {
			drawText(ui, "100", textStyle(ui, 11, kMuted, 500));
			ui.createElement(kSlider, "temperature-slider")
				.setParameters(SliderParameters{
					.axis = SliderAxis::Vertical,
					.value = &state.temperature,
					.minimum = 0.0,
					.maximum = 100.0,
					.roundingStep = 1.0,
					.length = 112.0f,
					.idleOverrides = SliderStateOverrides{
						.fillColor = kWarm,
					},
				})
				.draw();
			drawText(ui, "0", textStyle(ui, 11, kMuted, 500));
		}

		Clay_ElementDeclaration invisibleGroup{};
		invisibleGroup.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIT(0),
		};
		invisibleGroup.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		invisibleGroup.layout.childGap = 8;
		CLAY(ui.toClaySID("demo/sliders/invisible-group"), invisibleGroup) {
			drawText(
				ui,
				"Invisible mode",
				textStyle(ui, 13, kText, 600));
			drawText(
				ui,
				"Drag anywhere inside the outlined lane. The Slider emits no track, fill, or thumb.",
				textStyle(ui, 11, kMuted));

			Clay_ElementDeclaration invisibleLane{};
			invisibleLane.layout.sizing = {
				.width = CLAY_SIZING_FIT(0),
				.height = CLAY_SIZING_FIT(0),
			};
			invisibleLane.layout.padding = CLAY_PADDING_ALL(4);
			invisibleLane.backgroundColor = Flow_Color("#101722ff");
			invisibleLane.cornerRadius = CLAY_CORNER_RADIUS(6);
			invisibleLane.border = {
				.color = Flow_Color("#3a4b66ff"),
				.width = {1, 1, 1, 1, 0},
			};
			CLAY(ui.toClaySID("demo/sliders/invisible-lane"), invisibleLane) {
				ui.createElement(kSlider, "invisible-slider")
					.setParameters(SliderParameters{
						.pressBehavior = SliderPressBehavior::DragFromCurrent,
						.value = &state.invisibleValue,
						.minimum = -1.0,
						.maximum = 1.0,
						.roundingStep = 0.05,
						.visualParts = SliderVisualParts::None,
						.length = 245.0f,
						.hitThickness = 28.0f,
					})
					.draw();
			}
			drawText(
				ui,
				"Invisible value: " + decimal(state.invisibleValue),
				textStyle(ui, 12, kAccent, 650));
		}
	}

	drawText(
		ui,
		"Drag lifecycle — begin " + std::to_string(state.sliderBegins) +
			"  change " + std::to_string(state.sliderChanges) +
			"  commit " + std::to_string(state.sliderCommits),
		textStyle(ui, 11, kMuted, 500));
	ui.drawConstructed();
}

void drawLayoutCard(UiManager& ui, const DemoState& state) {
	ui.createElement(kBox, "layout-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"Box + Spacer + SplitterHandle",
		"Every card is an FSEL Box. The header Spacer pushes its badge right, and the center handle authors this pane width.");
	drawText(
		ui,
		"Left pane extent: " + std::to_string(static_cast<int>(state.leftPaneWidth)) + " px",
		textStyle(ui, 14, kWarm, 650));
	ui.drawConstructed();
}

void drawHeader(UiManager& ui, const DemoActions& actions) {
	Clay_ElementDeclaration header = row(14);
	header.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	CLAY(ui.toClaySID("demo/header"), header) {
		Clay_ElementDeclaration titleGroup{};
		titleGroup.layout.sizing = {
			.width = CLAY_SIZING_FIT(0),
			.height = CLAY_SIZING_FIT(0),
		};
		titleGroup.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		titleGroup.layout.childGap = 3;
		CLAY(ui.toClaySID("demo/header/title"), titleGroup) {
			drawText(ui, "FSEL · Immediate Gallery", textStyle(ui, 27, kText, 700));
			drawText(
				ui,
				"A live tour of the first Flow Standard Element Library primitives",
				textStyle(ui, 12, kMuted));
		}

		ui.createElement(kSpacer, "header-spacer")
			.setParameters(SpacerParameters{
				.sizing = {
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_FIXED(1),
				},
			})
			.draw();

		CLAY(ui.toClaySID("demo/header/severity"), badge(Flow_Color("#183848ff"))) {
			drawText(ui, "IMMEDIATE", textStyle(ui, 11, kAccent, 750, CLAY_TEXT_WRAP_NONE));
		}
		ui.createElement(FlowUi::FSEL::kButton, "reset-demo")
			.setParameters(FlowUi::FSEL::ButtonParameters{
				.onActivate = actions.reset,
				.contentMode = ButtonContentMode::TextOnly,
				.text = "Reset demo",
				.padding = Clay_Padding{10, 10, 6, 6},
			})
			.draw();
	}
}

void drawGallery(UiManager& ui, DemoState& state, const DemoActions& actions) {
	ui.createElement(kBox, "gallery-page")
		.setParameters(BoxParameters{
			.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			},
			.padding = CLAY_PADDING_ALL(22),
			.childGap = 18,
			.childAlignment = {
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_TOP,
			},
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.backgroundColor = Flow_Color("#0c111bff"),
		})
		.construct();

	drawHeader(ui, actions);

	Clay_ElementDeclaration panes{};
	panes.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	panes.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_TOP,
	};
	panes.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	CLAY(ui.toClaySID("demo/panes"), panes) {
		ui.createElement(kBox, "left-pane")
			.setParameters(BoxParameters{
				.sizing = {
					.width = CLAY_SIZING_FIXED(state.leftPaneWidth),
					.height = CLAY_SIZING_GROW(0),
				},
				.padding = Clay_Padding{0, 10, 0, 0},
				.childGap = 12,
				.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT,
					.y = CLAY_ALIGN_Y_TOP,
				},
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
			})
			.construct();
		drawButtonsCard(ui, state, actions);
		drawSelectableCard(ui, state, actions);
		drawRadioCard(ui, state);
		ui.drawConstructed();

		ui.createElement(kSplitterHandle, "gallery-splitter")
			.setParameters(SplitterHandleParameters{
				.axis = SplitterAxis::Horizontal,
				.position = SplitterPosition::Trailing,
				.targetExtent = &state.leftPaneWidth,
				.minExtent = 390.0f,
				.maxExtent = 610.0f,
				.thickness = 2.0f,
				.hitThickness = 12.0f,
				.backgroundColor = Flow_Color("#36506fff"),
				.hoverColor = Flow_Color("#70d6ff22"),
				.draggingColor = Flow_Color("#70d6ff38"),
				.cornerRadius = CLAY_CORNER_RADIUS(4),
			})
			.draw();

		ui.createElement(kBox, "right-pane")
			.setParameters(BoxParameters{
				.sizing = {
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_GROW(0),
				},
				.padding = Clay_Padding{10, 0, 0, 0},
				.childGap = 12,
				.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT,
					.y = CLAY_ALIGN_Y_TOP,
				},
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
			})
			.construct();
		drawBooleanCard(ui, state, actions);
		drawSlidersCard(ui, state, actions);
		drawLayoutCard(ui, state);
		ui.drawConstructed();
	}

	ui.drawConstructed();
}

} // namespace

int main() {
	try {
		AppConfig config{};
		config.window.title = "FlowUi — FSEL Immediate Gallery";
		config.window.width = 1280;
		config.window.height = 840;
		config.ui.fontAtlasSize = 1024;

		App app = makeApplication(config);
		DemoState state{};
		const DemoActions actions = makeActions(app, state);

		while (!app.shouldClose()) {
			app.beginFrame();
			drawGallery(app.ui(), state, actions);
			app.endFrame();
			app.drawFrame();
		}
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "FSEL demo failed: " << error.what() << '\n';
		return 1;
	}
}
