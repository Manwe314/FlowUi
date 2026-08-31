#include <algorithm>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include <FlowUi/Flow.hpp>

#include "DemoElementSchemas.hpp"

namespace {

using namespace FlowUi;
using namespace FlowUi::FSEL;

void requireStatus(Status status) {
	if (!status) throw FlowUiException(status.error());
}

constexpr GlobalFlowID kAnchoredPopupTriggerId =
	Global<FlowUi::FSEL::kButton>("demo.popup.anchor-trigger");
constexpr GlobalFlowID kAnchoredPopupSurfaceId =
	Global<FlowUi::FSEL::kPopupSurface>("demo.popup.anchored");
constexpr GlobalFlowID kPointerPopupSurfaceId =
	Global<FlowUi::FSEL::kPopupSurface>("demo.popup.pointer");
constexpr GlobalFlowID kHeadsUpPopupSurfaceId =
	Global<FlowUi::FSEL::kPopupSurface>("demo.popup.heads-up");
constexpr GlobalFlowID kHoverPopupAreaId =
	Global<FlowUi::FSEL::kBox>("demo.popup.hover-area");
constexpr GlobalFlowID kHoverPopupSurfaceId =
	Global<FlowUi::FSEL::kPopupSurface>("demo.popup.hover-follow");

enum class DemoPage : uint8_t {
	Gallery = 0,
	WritingStudio,
};

struct DemoState {
	DemoPage page = DemoPage::Gallery;
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
	std::string quickNote = "FSEL text fields";
	uint64_t quickNoteChanges = 0;
	uint64_t quickNoteCommits = 0;
	uint64_t quickNoteSubmits = 0;
	int retryCount = 3;
	unsigned int batchSize = 12;
	float exposure = 0.5f;
	std::string documentTitle = "Field notes · August study";
	std::string document =
		"A quiet place to test TextArea\n"
		"\n"
		"This page is deliberately arranged like a small writing application. "
		"The document remains an ordinary application-owned std::string while "
		"InputFieldManager owns editing mechanics, selection, caret placement, "
		"navigation, and the visible-line window.\n"
		"\n"
		"Things to try\n"
		"  • Click anywhere in a line to place the caret.\n"
		"  • Drag across text, then use the arrow keys with Shift.\n"
		"  • Add enough lines to make the editor scroll.\n"
		"  • Toggle soft wrap from the side panel.\n"
		"\n"
		"A tab follows this label:\tcolumn two\n"
		"The editor asks Clay to draw only the currently materialized visible lines, "
		"so this same element shape can scale beyond this friendly demo document.\n";
	bool editorSoftWrap = true;
	uint64_t documentChanges = 0;
	uint64_t documentCommits = 0;
	uint64_t documentFocuses = 0;
	uint64_t undoRequests = 0;
	uint64_t redoRequests = 0;
	bool anchoredPopupOpen = false;
	bool pointerPopupOpen = false;
	bool headsUpPopupOpen = false;
	uint64_t comboSelection = 2;
	uint64_t comboChanges = 0;
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
constexpr auto kShowGallery = UiAction(
	"fsel.demo.show-gallery",
	[](DemoState& state) { state.page = DemoPage::Gallery; });
constexpr auto kShowWritingStudio = UiAction(
	"fsel.demo.show-writing-studio",
	[](DemoState& state) { state.page = DemoPage::WritingStudio; });

struct DemoActions {
	ActionCall activateButton{};
	ActionCall selectSurface{};
	ActionCall clearSurface{};
	ActionCall toggleCheckbox{};
	ActionCall toggleSwitch{};
	ActionCall sliderBegin{};
	ActionCall sliderChange{};
	ActionCall sliderCommit{};
	ActionCall quickNoteChange{};
	ActionCall quickNoteCommit{};
	ActionCall quickNoteSubmit{};
	ActionCall documentChange{};
	ActionCall documentCommit{};
	ActionCall documentFocus{};
	ActionCall undoRequested{};
	ActionCall redoRequested{};
	ActionCall toggleEditorWrap{};
	ActionCall showGallery{};
	ActionCall showWritingStudio{};
	ActionCall openAnchoredPopup{};
	ActionCall closeAnchoredPopup{};
	ActionCall openPointerPopup{};
	ActionCall closePointerPopup{};
	ActionCall openHeadsUpPopup{};
	ActionCall closeHeadsUpPopup{};
	ActionCall comboChanged{};
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
		.quickNoteChange = ActionCall{actions.make(kIncrement, state.quickNoteChanges)},
		.quickNoteCommit = ActionCall{actions.make(kIncrement, state.quickNoteCommits)},
		.quickNoteSubmit = ActionCall{actions.make(kIncrement, state.quickNoteSubmits)},
		.documentChange = ActionCall{actions.make(kIncrement, state.documentChanges)},
		.documentCommit = ActionCall{actions.make(kIncrement, state.documentCommits)},
		.documentFocus = ActionCall{actions.make(kIncrement, state.documentFocuses)},
		.undoRequested = ActionCall{actions.make(kIncrement, state.undoRequests)},
		.redoRequested = ActionCall{actions.make(kIncrement, state.redoRequests)},
		.toggleEditorWrap = ActionCall{actions.make(kToggle, state.editorSoftWrap)},
		.showGallery = ActionCall{actions.make(kShowGallery, state)},
		.showWritingStudio = ActionCall{actions.make(kShowWritingStudio, state)},
		.openAnchoredPopup = ActionCall{actions.make(kSelect, state.anchoredPopupOpen)},
		.closeAnchoredPopup = ActionCall{actions.make(kClear, state.anchoredPopupOpen)},
		.openPointerPopup = ActionCall{actions.make(kSelect, state.pointerPopupOpen)},
		.closePointerPopup = ActionCall{actions.make(kClear, state.pointerPopupOpen)},
		.openHeadsUpPopup = ActionCall{actions.make(kSelect, state.headsUpPopupOpen)},
		.closeHeadsUpPopup = ActionCall{actions.make(kClear, state.headsUpPopupOpen)},
		.comboChanged = ActionCall{actions.make(kIncrement, state.comboChanges)},
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

size_t lineCount(std::string_view text) {
	return 1u + static_cast<size_t>(std::ranges::count(text, '\n'));
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

void drawTextInputCard(
	UiManager& ui,
	DemoState& state,
	const DemoActions& actions) {
	ui.createElement(kBox, "text-input-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"TextInput",
		"Single-line editing with live binding. Press Enter to submit and release focus.");

	ui.createElement(kTextInput, "quick-note-input")
		.setParameters(TextInputParameters{
			.value = &state.quickNote,
			.syncPolicy = TextFieldSyncPolicy::Live,
			.actions = TextFieldActions{
				.onChanged = actions.quickNoteChange,
				.onCommit = actions.quickNoteCommit,
				.onSubmit = actions.quickNoteSubmit,
			},
			.placeholder = "Type a short note…",
			.sizing = Clay_Sizing{
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIXED(38),
			},
			.focusedOverrides = TextFieldStateOverrides{
				.borderColor = kAccent,
			},
			.caret = TextFieldCaretOverrides{
				.shape = InputCaretShape::Underline,
				.thicknessPx = 2.0f,
				.blockWidthPx = 9.0f,
				.color = kWarm,
				.selectionBoxColor = Flow_Color("#247ba066"),
				.blinkPeriodSeconds = 0.9,
			},
		})
		.draw();

	drawText(
		ui,
		"change " + std::to_string(state.quickNoteChanges) +
			"  ·  commit " + std::to_string(state.quickNoteCommits) +
			"  ·  submit " + std::to_string(state.quickNoteSubmits),
		textStyle(ui, 11, kMuted, 550, CLAY_TEXT_WRAP_NONE));
	ui.drawConstructed();
}

void drawNumberInputCard(UiManager& ui, DemoState& state) {
	ui.createElement(kBox, "number-input-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"NumberInput",
		"Native int, uint, and float bindings with soft bounds and optional step controls.");

	CLAY(ui.toClaySID("demo/number-input/retry"), row(12)) {
		drawText(ui, "Retries", textStyle(ui, 12, kMuted, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kNumberInputInt, "retry-count")
			.setParameters(NumberInputParameters<int>{
				.value = &state.retryCount,
				.minimum = 0,
				.maximum = 20,
				.step = 1,
				.stepButtons = NumberInputStepButtons::TrailingVertical,
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_FIXED(150),
					.height = CLAY_SIZING_FIXED(36),
				},
			})
			.draw();
	}

	CLAY(ui.toClaySID("demo/number-input/batch"), row(12)) {
		drawText(ui, "Batch", textStyle(ui, 12, kMuted, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kNumberInputUInt, "batch-size")
			.setParameters(NumberInputParameters<unsigned int>{
				.value = &state.batchSize,
				.minimum = 1u,
				.maximum = 128u,
				.step = 4u,
				.stepButtons = NumberInputStepButtons::TrailingHorizontal,
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_FIXED(150),
					.height = CLAY_SIZING_FIXED(36),
				},
			})
			.draw();
	}

	CLAY(ui.toClaySID("demo/number-input/exposure"), row(12)) {
		drawText(ui, "Exposure", textStyle(ui, 12, kMuted, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kNumberInputFloat, "exposure")
			.setParameters(NumberInputParameters<float>{
				.value = &state.exposure,
				.minimum = -2.0f,
				.maximum = 2.0f,
				.step = 0.1f,
				.format = NumericFormatOptions{
					.notation = NumericFloatNotation::Fixed,
					.precision = 2,
				},
				.stepButtons = NumberInputStepButtons::None,
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_FIXED(150),
					.height = CLAY_SIZING_FIXED(36),
				},
			})
			.draw();
	}

	ui.drawConstructed();
}

void drawDragValueCard(UiManager& ui, DemoState& state) {
	ui.createElement(kBox, "drag-value-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"DragValue",
		"Drag horizontally to scrub. A short click enters native numeric text editing.");

	CLAY(ui.toClaySID("demo/drag-value/retry"), row(12)) {
		drawText(ui, "Retries", textStyle(ui, 12, kMuted, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kDragValueInt, "retry-count-drag")
			.setParameters(DragValueParameters<int>{
				.value = &state.retryCount,
				.minimum = 0,
				.maximum = 20,
				.step = 1,
				.pixelsPerStep = 6.0f,
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_FIXED(150),
					.height = CLAY_SIZING_FIXED(36),
				},
			})
			.draw();
	}

	CLAY(ui.toClaySID("demo/drag-value/exposure"), row(12)) {
		drawText(ui, "Exposure", textStyle(ui, 12, kMuted, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kDragValueFloat, "exposure-drag")
			.setParameters(DragValueParameters<float>{
				.value = &state.exposure,
				.minimum = -2.0f,
				.maximum = 2.0f,
				.step = 0.05f,
				.pixelsPerStep = 3.0f,
				.format = NumericFormatOptions{
					.notation = NumericFloatNotation::Fixed,
					.precision = 2,
				},
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_FIXED(150),
					.height = CLAY_SIZING_FIXED(36),
				},
			})
			.draw();
	}

	CLAY(ui.toClaySID("demo/drag-value/batch"), row(12)) {
		drawText(ui, "Drag only", textStyle(ui, 12, kMuted, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kDragValueUInt, "batch-size-drag")
			.setParameters(DragValueParameters<unsigned int>{
				.value = &state.batchSize,
				.minimum = 1u,
				.maximum = 128u,
				.step = 4u,
				.allowTextEntry = false,
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_FIXED(150),
					.height = CLAY_SIZING_FIXED(36),
				},
			})
			.draw();
	}

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

void drawProgressBarsCard(UiManager& ui, const DemoState& state) {
	ui.createElement(kBox, "progress-bars-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"ProgressBar",
		"A stateless display of the value snapshot supplied for this frame.");

	CLAY(ui.toClaySID("demo/progress/horizontal-row"), row(14)) {
		drawText(ui, "Volume", textStyle(ui, 13, kText, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kProgressBar, "volume-progress")
			.setParameters(ProgressBarParameters{
				.value = state.volume,
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_FIXED(14),
				},
				.baseColor = Flow_Color("#101722ff"),
				.borderColor = Flow_Color("#3a4b66ff"),
				.fillColor = kAccent,
			})
			.draw();
		drawText(
			ui,
			decimal(state.volume * 100.0, 0) + "%",
			textStyle(ui, 12, kAccent, 650, CLAY_TEXT_WRAP_NONE));
	}

	CLAY(ui.toClaySID("demo/progress/vertical-row"), row(14)) {
		drawText(
			ui,
			"Temperature",
			textStyle(ui, 13, kText, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kProgressBar, "temperature-progress")
			.setParameters(ProgressBarParameters{
				.axis = ProgressBarAxis::Vertical,
				.value = state.temperature,
				.minimum = 0.0,
				.maximum = 100.0,
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_FIXED(14),
					.height = CLAY_SIZING_FIXED(64),
				},
				.fillColor = kWarm,
			})
			.draw();
		drawText(
			ui,
			decimal(state.temperature, 0),
			textStyle(ui, 12, kWarm, 650, CLAY_TEXT_WRAP_NONE));
	}
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

void drawPopupSurfaceCard(
	UiManager& ui,
	const DemoState& state,
	const DemoActions& actions) {
	ui.createElement(kBox, "popup-surface-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"PopupSurface",
		"The same construct-only surface can anchor to an element, snapshot the pointer, or attach to the viewport.");

	CLAY(ui.toClaySID("demo/popup/anchor-row"), row(10)) {
		ui.createElement(FlowUi::FSEL::kButton, "anchored-popup-trigger")
			.withID(kAnchoredPopupTriggerId)
			.setParameters(FlowUi::FSEL::ButtonParameters{
				.onActivate = actions.openAnchoredPopup,
				.enabled = true,
				.contentMode = ButtonContentMode::TextOnly,
				.text = "Anchored dropdown",
			})
			.draw();

		if (state.anchoredPopupOpen) {
			ui.createElement(kPopupSurface, "anchored-popup")
				.withID(kAnchoredPopupSurfaceId)
				.setParameters(PopupSurfaceParameters{
					.popupRequest = PopupRequest{
						.anchor = PopupAnchor::element(kAnchoredPopupTriggerId),
						.placement = PopupPlacement{
							.anchorPoint = PopupAttachmentPoint::BottomLeft,
							.popupPoint = PopupAttachmentPoint::TopLeft,
							.offset = Clay_Vector2{0.0f, 8.0f},
						},
					},
					.onDismissed = actions.closeAnchoredPopup,
					.sizing = Clay_Sizing{
						.width = CLAY_SIZING_FIXED(250),
						.height = CLAY_SIZING_FIXED(72),
					},
				})
				.construct();
			drawText(
				ui,
				"Anchored surfaces work well for dropdowns and compact inspectors.",
				textStyle(ui, 13, kText));
			ui.drawConstructed();
		}
	}

	CLAY(ui.toClaySID("demo/popup/free-row"), row(10)) {
		ui.createElement(FlowUi::FSEL::kButton, "pointer-popup-trigger")
			.setParameters(FlowUi::FSEL::ButtonParameters{
				.onActivate = actions.openPointerPopup,
				.enabled = !state.pointerPopupOpen,
				.contentMode = ButtonContentMode::TextOnly,
				.text = "Pointer popup",
			})
			.draw();

		if (state.pointerPopupOpen) {
			ui.createElement(kPopupSurface, "pointer-popup")
				.withID(kPointerPopupSurfaceId)
				.setParameters(PopupSurfaceParameters{
					.popupRequest = PopupRequest{
						.anchor = PopupAnchor::pointerSnapshot(),
						.placement = PopupPlacement{
							.anchorPoint = PopupAttachmentPoint::TopLeft,
							.popupPoint = PopupAttachmentPoint::TopLeft,
							.offset = Clay_Vector2{12.0f, 12.0f},
						},
						.expectedSize = Clay_Dimensions{230.0f, 64.0f},
					},
					.onDismissed = actions.closePointerPopup,
					.sizing = Clay_Sizing{
						.width = CLAY_SIZING_FIXED(230),
						.height = CLAY_SIZING_FIXED(64),
					},
					.backgroundColor = Flow_Color("#173342ff"),
					.borderColor = Flow_Color("#2d7894ff"),
				})
				.construct();
			drawText(ui, "The opening pointer position is captured once.", textStyle(ui, 13, kAccent));
			ui.drawConstructed();
		}

		ui.createElement(FlowUi::FSEL::kButton, "heads-up-popup-trigger")
			.setParameters(FlowUi::FSEL::ButtonParameters{
				.onActivate = actions.openHeadsUpPopup,
				.enabled = !state.headsUpPopupOpen,
				.contentMode = ButtonContentMode::TextOnly,
				.text = "Heads-up notice",
			})
			.draw();

		if (state.headsUpPopupOpen) {
			ui.createElement(kPopupSurface, "heads-up-popup")
				.withID(kHeadsUpPopupSurfaceId)
				.setParameters(PopupSurfaceParameters{
					.popupRequest = PopupRequest{
						.anchor = PopupAnchor::viewport(),
						.placement = PopupPlacement{
							.anchorPoint = PopupAttachmentPoint::TopMiddle,
							.popupPoint = PopupAttachmentPoint::TopMiddle,
							.offset = Clay_Vector2{0.0f, 24.0f},
						},
						.expectedSize = Clay_Dimensions{340.0f, 66.0f},
						.layer = PopupLayer::WarningPopup,
						.outsidePress = PopupOutsidePressPolicy::DismissAndConsume,
					},
					.onDismissed = actions.closeHeadsUpPopup,
					.sizing = Clay_Sizing{
						.width = CLAY_SIZING_FIXED(340),
						.height = CLAY_SIZING_FIXED(66),
					},
					.backgroundColor = Flow_Color("#46351bff"),
					.borderColor = kWarm,
				})
				.construct();
			drawText(ui, "Viewport-relative warning popup · click outside or press Escape.", textStyle(ui, 13, kWarm, 650));
			ui.drawConstructed();
		}
	}

	ui.createElement(kBox, "hover-popup-area")
		.withID(kHoverPopupAreaId)
		.setParameters(BoxParameters{
			.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIXED(54),
			},
			.padding = CLAY_PADDING_ALL(10),
			.childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
			.backgroundColor = Flow_Color("#101f2dff"),
			.borderColor = Flow_Color("#2d7894ff"),
			.cornerRadius = CLAY_CORNER_RADIUS(7),
			.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0},
		})
		.construct();
	drawText(ui, "Hover here · this popup follows the pointer", textStyle(ui, 12, kAccent, 600));
	ui.drawConstructed();

	if (ui.getPreviousFramesInteraction().isHovered(kHoverPopupAreaId)) {
		ui.createElement(kPopupSurface, "hover-follow-popup")
			.withID(kHoverPopupSurfaceId)
			.setParameters(PopupSurfaceParameters{
				.popupRequest = PopupRequest{
					.anchor = PopupAnchor::pointerFollow(),
					.placement = PopupPlacement{
						.anchorPoint = PopupAttachmentPoint::TopLeft,
						.popupPoint = PopupAttachmentPoint::TopLeft,
						.offset = Clay_Vector2{14.0f, 14.0f},
					},
					.expectedSize = Clay_Dimensions{220.0f, 52.0f},
					.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
					.outsidePress = PopupOutsidePressPolicy::Ignore,
					.dismissOnEscape = false,
				},
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_FIXED(220),
					.height = CLAY_SIZING_FIXED(52),
				},
				.backgroundColor = Flow_Color("#173342ff"),
				.borderColor = kAccent,
			})
			.construct();
		drawText(ui, "PointerFollow updates every frame.", textStyle(ui, 12, kText));
		ui.drawConstructed();
	}
	ui.drawConstructed();
}

void drawComboBoxCard(UiManager& ui, DemoState& state, const DemoActions& actions) {
	static constexpr ComboBoxOption kOptions[] = {
		{.value = 1, .text = "Design"},
		{.value = 2, .text = "Implementation"},
		{.value = 3, .text = "Verification"},
		{.value = 4, .text = "Documentation"},
		{.value = 5, .text = "Release"},
		{.value = 6, .text = "Archived", .enabled = false},
		{.value = 7, .text = "Long option list"},
		{.value = 8, .text = "Scrollable menu"},
		{.value = 9, .text = "Final review"},
	};

	ui.createElement(kBox, "combo-box-card")
		.setParameters(cardParameters())
		.construct();
	drawCardHeading(
		ui,
		"ComboBox",
		"The standard element owns the common text/icon option contract; arbitrary option rows remain a custom composition.");
	ui.createElement(kComboBox, "workflow-combo")
		.setParameters(ComboBoxParameters{
			.options = kOptions,
			.selectedValue = &state.comboSelection,
			.onChanged = actions.comboChanged,
			.popupMaxHeight = 154.0f,
		})
		.draw();
	drawText(
		ui,
		"Selected value " + std::to_string(state.comboSelection) +
			" · changes " + std::to_string(state.comboChanges),
		textStyle(ui, 11, kMuted, 500));
	ui.drawConstructed();
}

void drawHeader(
	UiManager& ui,
	const DemoState& state,
	const DemoActions& actions) {
	const bool isGallery = state.page == DemoPage::Gallery;
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
			drawText(
				ui,
				isGallery ? "FSEL · Immediate Gallery" : "FSEL · Writing Studio",
				textStyle(ui, 27, kText, 700));
			drawText(
				ui,
				isGallery
					? "A live tour of the first Flow Standard Element Library primitives"
					: "A focused TextArea workspace built from the same small element catalogue",
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

		ui.createElement(FlowUi::FSEL::kButton, "page-navigation")
			.setParameters(FlowUi::FSEL::ButtonParameters{
				.onActivate = isGallery
					? actions.showWritingStudio
					: actions.showGallery,
				.contentMode = ButtonContentMode::TextOnly,
				.text = isGallery ? "Open writing studio  →" : "←  Back to gallery",
				.padding = Clay_Padding{11, 11, 6, 6},
				.idleOverrides = ButtonStateOverrides{
					.backgroundColor = Flow_Color("#163c4aff"),
					.labelColor = kAccent,
					.borderColor = Flow_Color("#2d7894ff"),
				},
			})
			.draw();

		CLAY(ui.toClaySID("demo/header/severity"), badge(Flow_Color("#183848ff"))) {
			drawText(
				ui,
				isGallery ? "GALLERY" : "EDITOR",
				textStyle(ui, 11, kAccent, 750, CLAY_TEXT_WRAP_NONE));
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
			.clipConfig = Clay_ClipElementConfig{.vertical = true},
			.backgroundColor = Flow_Color("#0c111bff"),
		})
		.construct();

	drawHeader(ui, state, actions);

	Clay_ElementDeclaration panes{};
	panes.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
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
					.height = CLAY_SIZING_FIT(0),
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
		drawPopupSurfaceCard(ui, state, actions);
		drawComboBoxCard(ui, state, actions);
		drawSelectableCard(ui, state, actions);
		drawRadioCard(ui, state);
		drawTextInputCard(ui, state, actions);
		drawNumberInputCard(ui, state);
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
					.height = CLAY_SIZING_FIT(0),
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
		drawDragValueCard(ui, state);
		drawProgressBarsCard(ui, state);
		drawSlidersCard(ui, state, actions);
		drawLayoutCard(ui, state);
		ui.drawConstructed();
	}

	ui.drawConstructed();
}

void drawEditorSidebar(
	UiManager& ui,
	DemoState& state,
	const DemoActions& actions) {
	ui.createElement(kBox, "editor-sidebar")
		.setParameters(BoxParameters{
			.sizing = {
				.width = CLAY_SIZING_FIXED(255),
				.height = CLAY_SIZING_GROW(0),
			},
			.padding = CLAY_PADDING_ALL(16),
			.childGap = 18,
			.childAlignment = {
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_TOP,
			},
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.backgroundColor = Flow_Color("#111925ff"),
			.borderColor = kCardBorder,
			.cornerRadius = CLAY_CORNER_RADIUS(12),
			.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0},
		})
		.construct();

	drawCardHeading(
		ui,
		"Editor guide",
		"The surrounding chrome is ordinary Box, Button, Switch, Spacer, and text composition.");

	CLAY(ui.toClaySID("demo/editor/wrap-row"), row(10)) {
		ui.createElement(kSwitch, "editor-soft-wrap")
			.setParameters(SwitchParameters{
				.isOn = state.editorSoftWrap,
				.onToggle = actions.toggleEditorWrap,
			})
			.draw();
		drawText(
			ui,
			state.editorSoftWrap ? "Soft wrap on" : "Soft wrap off",
			textStyle(ui, 13, kText, 600, CLAY_TEXT_WRAP_NONE));
	}

	CLAY(ui.toClaySID("demo/editor/model-badge"), badge(Flow_Color("#163c4aff"))) {
		drawText(ui, "LIVE STRING BINDING", textStyle(ui, 10, kAccent, 750));
	}
	drawText(
		ui,
		"The TextArea edits DemoState::document. FSEL synchronizes the application value from manager transactions.",
		textStyle(ui, 12, kMuted));

	drawText(ui, "Try these", textStyle(ui, 13, kText, 650));
	drawText(
		ui,
		"• click to place the caret\n• Shift + arrows to select\n• Ctrl/Cmd + A to select all\n• wheel over the editor to scroll",
		textStyle(ui, 11, kMuted));

	ui.createElement(kSpacer, "editor-sidebar-spacer")
		.setParameters(SpacerParameters{
			.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			},
		})
		.draw();

	drawText(ui, "External history hooks", textStyle(ui, 13, kText, 650));
	drawText(
		ui,
		"Undo and redo remain application transactions. The demo counts requests without installing a document history.",
		textStyle(ui, 11, kMuted));
	CLAY(ui.toClaySID("demo/editor/history-counts"), row(8)) {
		CLAY(ui.toClaySID("demo/editor/undo-count"), badge(Flow_Color("#2b2340ff"))) {
			drawText(
				ui,
				"UNDO " + std::to_string(state.undoRequests),
				textStyle(ui, 10, kWarm, 700, CLAY_TEXT_WRAP_NONE));
		}
		CLAY(ui.toClaySID("demo/editor/redo-count"), badge(Flow_Color("#2b2340ff"))) {
			drawText(
				ui,
				"REDO " + std::to_string(state.redoRequests),
				textStyle(ui, 10, kWarm, 700, CLAY_TEXT_WRAP_NONE));
		}
	}

	ui.drawConstructed();
}

void drawDocumentEditor(
	UiManager& ui,
	DemoState& state,
	const DemoActions& actions) {
	ui.createElement(kBox, "document-editor")
		.setParameters(BoxParameters{
			.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			},
			.padding = CLAY_PADDING_ALL(18),
			.childGap = 12,
			.childAlignment = {
				.x = CLAY_ALIGN_X_LEFT,
				.y = CLAY_ALIGN_Y_TOP,
			},
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.backgroundColor = kCard,
			.borderColor = kCardBorder,
			.cornerRadius = CLAY_CORNER_RADIUS(12),
			.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0},
		})
		.construct();

	Clay_ElementDeclaration titleRow = row(12);
	CLAY(ui.toClaySID("demo/editor/title-row"), titleRow) {
		ui.createElement(kTextInput, "document-title")
			.setParameters(TextInputParameters{
				.value = &state.documentTitle,
				.syncPolicy = TextFieldSyncPolicy::OnCommit,
				.placeholder = "Untitled document",
				.sizing = Clay_Sizing{
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_FIXED(40),
				},
				.idleOverrides = TextFieldStateOverrides{
					.backgroundColor = Flow_Color("#101722ff"),
					.borderColor = Flow_Color("#263750ff"),
				},
				.focusedOverrides = TextFieldStateOverrides{
					.backgroundColor = Flow_Color("#101722ff"),
					.borderColor = kAccent,
				},
				.fontSize = 17,
				.caret = TextFieldCaretOverrides{
					.color = kAccent,
				},
			})
			.draw();
		CLAY(ui.toClaySID("demo/editor/live-badge"), badge(Flow_Color("#173c32ff"))) {
			drawText(ui, "LIVE", textStyle(ui, 10, kAccent, 750, CLAY_TEXT_WRAP_NONE));
		}
	}

	drawText(
		ui,
		"The title uses OnCommit synchronization; the document body below uses Live synchronization.",
		textStyle(ui, 11, kMuted));

	ui.createElement(kTextArea, "main-document")
		.setParameters(TextAreaParameters{
			.value = &state.document,
			.syncPolicy = TextFieldSyncPolicy::Live,
			.actions = TextFieldActions{
				.onChanged = actions.documentChange,
				.onCommit = actions.documentCommit,
				.onFocus = actions.documentFocus,
				.onUndoRequested = actions.undoRequested,
				.onRedoRequested = actions.redoRequested,
			},
			.softWrap = state.editorSoftWrap,
			.transactionDetail = TransactionReportDetail::Reversible,
			.placeholder = "Start writing here…",
			.sizing = Clay_Sizing{
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			},
			.padding = CLAY_PADDING_ALL(18),
			.idleOverrides = TextFieldStateOverrides{
				.backgroundColor = Flow_Color("#0e1520ff"),
				.borderColor = Flow_Color("#263750ff"),
			},
			.focusedOverrides = TextFieldStateOverrides{
				.backgroundColor = Flow_Color("#0e1520ff"),
				.borderColor = kAccent,
			},
			.fontSize = 15,
			.tabWidth = 4,
			.caret = TextFieldCaretOverrides{
				.shape = InputCaretShape::Bar,
				.thicknessPx = 2.0f,
				.color = kAccent,
				.selectionBoxColor = Flow_Color("#247ba077"),
				.selectedTextColor = Flow_Color("#ffffffff"),
				.blinkPeriodSeconds = 1.1,
				.blinkVisibleSeconds = 0.62,
			},
		})
		.draw();

	CLAY(ui.toClaySID("demo/editor/status-row"), row(14)) {
		drawText(
			ui,
			std::to_string(lineCount(state.document)) + " lines  ·  " +
				std::to_string(state.document.size()) + " UTF-8 bytes",
			textStyle(ui, 11, kMuted, 550, CLAY_TEXT_WRAP_NONE));
		ui.createElement(kSpacer, "editor-status-spacer")
			.setParameters(SpacerParameters{
				.sizing = {
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_FIXED(1),
				},
			})
			.draw();
		drawText(
			ui,
			"focus " + std::to_string(state.documentFocuses) +
				"  ·  changes " + std::to_string(state.documentChanges) +
				"  ·  commits " + std::to_string(state.documentCommits),
			textStyle(ui, 11, kMuted, 550, CLAY_TEXT_WRAP_NONE));
	}

	ui.drawConstructed();
}

void drawWritingStudio(
	UiManager& ui,
	DemoState& state,
	const DemoActions& actions) {
	ui.createElement(kBox, "writing-studio-page")
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

	drawHeader(ui, state, actions);

	Clay_ElementDeclaration workspace = row(16);
	workspace.layout.sizing.height = CLAY_SIZING_GROW(0);
	workspace.layout.childAlignment.y = CLAY_ALIGN_Y_TOP;
	CLAY(ui.toClaySID("demo/editor/workspace"), workspace) {
		drawEditorSidebar(ui, state, actions);
		drawDocumentEditor(ui, state, actions);
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
#if FLOW_UI_DEV_MODE
		config.dev.enabled = true;
#endif

		App app = makeApplication(config);
		DemoState state{};
		const DemoActions actions = makeActions(app, state);

		while (!app.shouldClose()) {
			requireStatus(app.beginFrame());
			if (state.page == DemoPage::Gallery) {
				drawGallery(app.ui(), state, actions);
			} else {
				drawWritingStudio(app.ui(), state, actions);
			}
			requireStatus(app.endFrame());
			requireStatus(app.drawFrame());
		}
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "FSEL demo failed: " << error.what() << '\n';
		return 1;
	}
}
