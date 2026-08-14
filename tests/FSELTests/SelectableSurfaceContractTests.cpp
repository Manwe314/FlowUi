#include "TestHarness.hpp"

#include <type_traits>

#include <FSEL/SelectableSurface.hpp>

static_assert(FlowUi::FlowElement<FlowUi::FSEL::SelectableSurface>);
static_assert(FlowUi::ConstructibleFlowElement<FlowUi::FSEL::SelectableSurface>);
static_assert(!FlowUi::DrawableFlowElement<FlowUi::FSEL::SelectableSurface>);
static_assert(FlowUi::HasState<FlowUi::FSEL::SelectableSurface>);
static_assert(std::is_empty_v<FlowUi::FSEL::SelectableSurface>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("selectable surface defaults are controlled and unselected", [] {
		const FlowUi::FSEL::SelectableSurfaceParameters parameters{};
		FLOWUI_CHECK(!parameters.selected);
		FLOWUI_CHECK(parameters.enabled);
		FLOWUI_CHECK(!parameters.onSelected);
	});

	runner.run("selectable surface state starts disarmed", [] {
		const FlowUi::FSEL::SelectableSurfaceState state{};
		FLOWUI_CHECK(!state.isArmed);
		FLOWUI_CHECK(!state.mouseUpObserved);
	});

	runner.run("selection visual overrides remain independent", [] {
		FlowUi::FSEL::SelectableSurfaceParameters parameters{};
		parameters.style.selectedOverrides.backgroundColor =
			FlowUi::Flow_Color("#112233ff");
		parameters.style.selectedDisabledOverrides.borderColor =
			FlowUi::Flow_Color("#445566ff");
		FLOWUI_CHECK(
			parameters.style.selectedOverrides.backgroundColor.has_value());
		FLOWUI_CHECK(
			!parameters.style.selectedOverrides.borderColor.has_value());
		FLOWUI_CHECK(
			parameters.style.selectedDisabledOverrides.borderColor.has_value());
	});

	return runner.finish();
}
