#include "TestHarness.hpp"

#include <string>
#include <type_traits>

#include <FSEL/TextArea.hpp>
#include <FSEL/TextInput.hpp>

static_assert(FlowUi::FlowElement<FlowUi::FSEL::TextInput>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::TextInput>);
static_assert(!FlowUi::ConstructibleFlowElement<FlowUi::FSEL::TextInput>);
static_assert(FlowUi::HasState<FlowUi::FSEL::TextInput>);

static_assert(FlowUi::FlowElement<FlowUi::FSEL::TextArea>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::TextArea>);
static_assert(!FlowUi::ConstructibleFlowElement<FlowUi::FSEL::TextArea>);
static_assert(FlowUi::HasState<FlowUi::FSEL::TextArea>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("text input is explicitly single-line by surface", [] {
		std::string value = "search";
		FlowUi::FSEL::TextInputParameters parameters{};
		parameters.value = &value;
		parameters.clearFocusOnSubmit = false;
		FLOWUI_CHECK(parameters.value == &value);
		FLOWUI_CHECK(!parameters.clearFocusOnSubmit);
		FLOWUI_CHECK(parameters.syncPolicy == FlowUi::FSEL::TextFieldSyncPolicy::Live);
	});

	runner.run("text area exposes multiline layout policy", [] {
		std::string value = "first\nsecond";
		FlowUi::FSEL::TextAreaParameters parameters{};
		parameters.value = &value;
		parameters.softWrap = false;
		parameters.tabWidth = 8;
		FLOWUI_CHECK(!parameters.softWrap);
		FLOWUI_CHECK(parameters.tabWidth.value() == 8);
	});

	runner.run("caret presentation is field-local and independently optional", [] {
		FlowUi::FSEL::TextInputParameters parameters{};
		parameters.caret.shape = FlowUi::InputCaretShape::Underline;
		parameters.caret.color = FlowUi::Flow_Color("#ff0000ff");
		parameters.caret.blinkPeriodSeconds = 0.8;
		FLOWUI_CHECK(parameters.caret.shape.has_value());
		FLOWUI_CHECK(parameters.caret.color.has_value());
		FLOWUI_CHECK(!parameters.caret.selectionBoxColor.has_value());
		FLOWUI_CHECK(parameters.caret.blinkPeriodSeconds.value() == 0.8);
		FLOWUI_CHECK(!parameters.floatingZIndex.has_value());
		parameters.floatingZIndex = 10000;
		FLOWUI_CHECK(parameters.floatingZIndex.value() == 10000);
	});

	runner.run("shared binding policies remain common to both controls", [] {
		FlowUi::FSEL::TextInputParameters input{};
		FlowUi::FSEL::TextAreaParameters area{};
		input.syncPolicy = FlowUi::FSEL::TextFieldSyncPolicy::OnCommit;
		area.syncPolicy = FlowUi::FSEL::TextFieldSyncPolicy::ReadOnly;
		FLOWUI_CHECK(input.syncPolicy == FlowUi::FSEL::TextFieldSyncPolicy::OnCommit);
		FLOWUI_CHECK(area.syncPolicy == FlowUi::FSEL::TextFieldSyncPolicy::ReadOnly);
	});

	return runner.finish();
}
