#include "TestHarness.hpp"

#include <limits>
#include <type_traits>

#include <FSEL/Checkbox.hpp>
#include <FSEL/Switch.hpp>

static_assert(FlowUi::FlowElement<FlowUi::FSEL::Checkbox>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::Checkbox>);
static_assert(!FlowUi::ConstructibleFlowElement<FlowUi::FSEL::Checkbox>);
static_assert(FlowUi::HasState<FlowUi::FSEL::Checkbox>);
static_assert(std::is_empty_v<FlowUi::FSEL::Checkbox>);

static_assert(FlowUi::FlowElement<FlowUi::FSEL::Switch>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::Switch>);
static_assert(!FlowUi::ConstructibleFlowElement<FlowUi::FSEL::Switch>);
static_assert(FlowUi::HasState<FlowUi::FSEL::Switch>);
static_assert(std::is_empty_v<FlowUi::FSEL::Switch>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("boolean controls default to external false values", [] {
		const FlowUi::FSEL::CheckboxParameters checkbox{};
		const FlowUi::FSEL::SwitchParameters switchParameters{};
		FLOWUI_CHECK(!checkbox.isChecked);
		FLOWUI_CHECK(checkbox.enabled);
		FLOWUI_CHECK(!checkbox.onToggle);
		FLOWUI_CHECK(!checkbox.uncheckedIcon.handle);
		FLOWUI_CHECK(!checkbox.checkedIcon.handle);
		FLOWUI_CHECK(!switchParameters.isOn);
		FLOWUI_CHECK(switchParameters.enabled);
		FLOWUI_CHECK(!switchParameters.onToggle);
	});

	runner.run("shared boolean interaction state starts disarmed", [] {
		const FlowUi::FSEL::CheckboxState checkbox{};
		const FlowUi::FSEL::SwitchState switchState{};
		FLOWUI_CHECK(!checkbox.isArmed);
		FLOWUI_CHECK(!checkbox.mouseUpObserved);
		FLOWUI_CHECK(!switchState.isArmed);
		FLOWUI_CHECK(!switchState.mouseUpObserved);
	});

	runner.run("switch roundness is normalized", [] {
		using FlowUi::FSEL::detail::boolean_control::clampRoundness;
		FLOWUI_CHECK(clampRoundness(-1.0f) == 0.0f);
		FLOWUI_CHECK(clampRoundness(0.25f) == 0.25f);
		FLOWUI_CHECK(clampRoundness(2.0f) == 1.0f);
		FLOWUI_CHECK(
			clampRoundness(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
	});

	runner.run("value-specific colors can be overridden independently", [] {
		FlowUi::FSEL::CheckboxParameters checkbox{};
		checkbox.checkedOverrides.hovered.iconColor =
			FlowUi::Flow_Color("#112233ff");
		FLOWUI_CHECK(checkbox.checkedOverrides.hovered.iconColor.has_value());
		FLOWUI_CHECK(!checkbox.uncheckedOverrides.hovered.iconColor.has_value());

		FlowUi::FSEL::SwitchParameters switchParameters{};
		switchParameters.onOverrides.disabled.knobBorderColor =
			FlowUi::Flow_Color("#445566ff");
		FLOWUI_CHECK(
			switchParameters.onOverrides.disabled.knobBorderColor.has_value());
		FLOWUI_CHECK(
			!switchParameters.offOverrides.disabled.knobBorderColor.has_value());
	});

	return runner.finish();
}
