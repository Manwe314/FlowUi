#include "TestHarness.hpp"

#include <type_traits>

#include <FSEL/Button.hpp>

static_assert(FlowUi::FlowElement<FlowUi::FSEL::Button>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::Button>);
static_assert(FlowUi::ConstructibleFlowElement<FlowUi::FSEL::Button>);
static_assert(FlowUi::HasState<FlowUi::FSEL::Button>);
static_assert(std::is_empty_v<FlowUi::FSEL::Button>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("button defaults are safe and content-free", [] {
		const FlowUi::FSEL::ButtonParameters parameters{};
		FLOWUI_CHECK(parameters.enabled);
		FLOWUI_CHECK(!parameters.onActivate);
		FLOWUI_CHECK(
			parameters.contentMode == FlowUi::FSEL::ButtonContentMode::None);
		FLOWUI_CHECK(parameters.text.empty());
		FLOWUI_CHECK(!parameters.icon.handle);
	});

	runner.run("button state starts disarmed", [] {
		const FlowUi::FSEL::ButtonState state{};
		FLOWUI_CHECK(!state.isArmed);
		FLOWUI_CHECK(!state.mouseUpObserved);
	});

	runner.run("button state overrides are independently optional", [] {
		FlowUi::FSEL::ButtonParameters parameters{};
		parameters.hoveredOverrides.labelColor =
			FlowUi::Flow_Color("#112233ff");
		parameters.hoveredOverrides.iconColor =
			FlowUi::Flow_Color("#445566ff");
		FLOWUI_CHECK(parameters.hoveredOverrides.labelColor.has_value());
		FLOWUI_CHECK(parameters.hoveredOverrides.iconColor.has_value());
		FLOWUI_CHECK(!parameters.hoveredOverrides.backgroundColor.has_value());
	});

	return runner.finish();
}
