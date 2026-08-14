#include "TestHarness.hpp"

#include <type_traits>

#include <FSEL/RadioChoice.hpp>
#include <FSEL/SelectableSurface.hpp>

static_assert(FlowUi::FlowElement<FlowUi::FSEL::RadioChoice>);
static_assert(FlowUi::ConstructibleFlowElement<FlowUi::FSEL::RadioChoice>);
static_assert(!FlowUi::DrawableFlowElement<FlowUi::FSEL::RadioChoice>);
static_assert(FlowUi::HasState<FlowUi::FSEL::RadioChoice>);
static_assert(std::is_empty_v<FlowUi::FSEL::RadioChoice>);
static_assert(std::is_same_v<
	FlowUi::FSEL::RadioChoiceState,
	FlowUi::FSEL::SelectableSurfaceState>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("radio choice defaults are safely unbound", [] {
		const FlowUi::FSEL::RadioChoiceParameters parameters{};
		FLOWUI_CHECK(parameters.choiceValue == 0);
		FLOWUI_CHECK(parameters.selectedValue == nullptr);
		FLOWUI_CHECK(parameters.enabled);
		FLOWUI_CHECK(!parameters.onSelected);
	});

	runner.run("radio choices can share one application selection", [] {
		uint64_t selectedValue = 2;
		const FlowUi::FSEL::RadioChoiceParameters first{
			.choiceValue = 1,
			.selectedValue = &selectedValue,
		};
		const FlowUi::FSEL::RadioChoiceParameters second{
			.choiceValue = 2,
			.selectedValue = &selectedValue,
		};
		FLOWUI_CHECK(first.selectedValue == second.selectedValue);
		FLOWUI_CHECK(*first.selectedValue != first.choiceValue);
		FLOWUI_CHECK(*second.selectedValue == second.choiceValue);
	});

	runner.run("radio choice and selectable surface share style", [] {
		FlowUi::FSEL::RadioChoiceParameters radio{};
		FlowUi::FSEL::SelectableSurfaceParameters surface{};
		radio.style.selectedOverrides.backgroundColor =
			FlowUi::Flow_Color("#112233ff");
		FLOWUI_CHECK(radio.style.selectedOverrides.backgroundColor.has_value());
		FLOWUI_CHECK(!surface.style.selectedOverrides.backgroundColor.has_value());
	});

	return runner.finish();
}
