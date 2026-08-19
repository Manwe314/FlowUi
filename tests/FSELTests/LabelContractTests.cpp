#include "TestHarness.hpp"

#include <type_traits>

#include <FSEL/Label.hpp>

static_assert(FlowUi::FlowElement<FlowUi::FSEL::Label>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::Label>);
static_assert(!FlowUi::ConstructibleFlowElement<FlowUi::FSEL::Label>);
static_assert(!FlowUi::HasState<FlowUi::FSEL::Label>);
static_assert(std::is_empty_v<FlowUi::FSEL::Label>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("label defaults to passive fit-sized text", [] {
		const FlowUi::FSEL::LabelParameters parameters{};
		FLOWUI_CHECK(parameters.text.empty());
		FLOWUI_CHECK(!parameters.textColor.has_value());
		FLOWUI_CHECK(!parameters.fontFamily.has_value());
		FLOWUI_CHECK(!parameters.fontWeight.has_value());
		FLOWUI_CHECK(!parameters.fontStyle.has_value());
		FLOWUI_CHECK(!parameters.fontSize.has_value());
		FLOWUI_CHECK(!parameters.letterSpacing.has_value());
		FLOWUI_CHECK(!parameters.wrapMode.has_value());
		FLOWUI_CHECK(!parameters.textAlignment.has_value());
	});

	runner.run("label typography overrides are independently optional", [] {
		FlowUi::FSEL::LabelParameters parameters{};
		parameters.text = "Status";
		parameters.fontSize = 18;
		parameters.wrapMode = CLAY_TEXT_WRAP_WORDS;
		parameters.textAlignment = CLAY_TEXT_ALIGN_RIGHT;

		FLOWUI_CHECK(parameters.text == "Status");
		FLOWUI_CHECK(parameters.fontSize == 18);
		FLOWUI_CHECK(parameters.wrapMode == CLAY_TEXT_WRAP_WORDS);
		FLOWUI_CHECK(parameters.textAlignment == CLAY_TEXT_ALIGN_RIGHT);
		FLOWUI_CHECK(!parameters.textColor.has_value());
	});

	return runner.finish();
}
