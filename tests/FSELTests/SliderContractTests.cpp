#include "TestHarness.hpp"

#include <cmath>
#include <limits>
#include <type_traits>

#include <FSEL/Slider.hpp>

static_assert(FlowUi::FlowElement<FlowUi::FSEL::Slider>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::Slider>);
static_assert(!FlowUi::ConstructibleFlowElement<FlowUi::FSEL::Slider>);
static_assert(FlowUi::HasState<FlowUi::FSEL::Slider>);
static_assert(std::is_empty_v<FlowUi::FSEL::Slider>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("slider defaults to a disabled unbound horizontal control", [] {
		const FlowUi::FSEL::SliderParameters parameters{};
		FLOWUI_CHECK(parameters.value == nullptr);
		FLOWUI_CHECK(parameters.axis == FlowUi::FSEL::SliderAxis::Horizontal);
		FLOWUI_CHECK(
			parameters.pressBehavior ==
			FlowUi::FSEL::SliderPressBehavior::JumpToPointer);
		FLOWUI_CHECK(parameters.enabled);
		FLOWUI_CHECK(!parameters.roundingStep.has_value());
		FLOWUI_CHECK(
			parameters.visualParts == FlowUi::FSEL::SliderVisualParts::All);
	});

	runner.run("slider visual parts form a usable mask", [] {
		using FlowUi::FSEL::SliderVisualParts;
		const auto parts = SliderVisualParts::Track | SliderVisualParts::Thumb;
		FLOWUI_CHECK(FlowUi::FSEL::hasSliderVisualPart(
			parts,
			SliderVisualParts::Track));
		FLOWUI_CHECK(!FlowUi::FSEL::hasSliderVisualPart(
			parts,
			SliderVisualParts::Fill));
		FLOWUI_CHECK(FlowUi::FSEL::hasSliderVisualPart(
			parts,
			SliderVisualParts::Thumb));
		FLOWUI_CHECK(!FlowUi::FSEL::hasSliderVisualPart(
			SliderVisualParts::None,
			SliderVisualParts::Track));
	});

	runner.run("slider bounds and ratios normalize safely", [] {
		using namespace FlowUi::FSEL::detail::slider;
		const Bounds inverted = normalizeBounds(10.0, -10.0);
		FLOWUI_CHECK(inverted.lower == -10.0);
		FLOWUI_CHECK(inverted.upper == 10.0);
		FLOWUI_CHECK(normalizedRatio(0.0, inverted) == 0.5);
		FLOWUI_CHECK(valueFromRatio(2.0, inverted) == 10.0);
		FLOWUI_CHECK(valueFromRatio(-1.0, inverted) == -10.0);
		const Bounds invalid = normalizeBounds(
			std::numeric_limits<double>::quiet_NaN(),
			2.0);
		FLOWUI_CHECK(invalid.lower == 0.0);
		FLOWUI_CHECK(invalid.upper == 1.0);
	});

	runner.run("slider rounding step snaps relative to its lower bound", [] {
		using namespace FlowUi::FSEL::detail::slider;
		const Bounds bounds{.lower = -1.0, .upper = 1.0};
		FLOWUI_CHECK(
			std::abs(snapValue(0.56872948391238, bounds, 0.01) - 0.57) <
			1.0e-12);
		FLOWUI_CHECK(snapValue(-0.8, bounds, 0.25) == -0.75);
		FLOWUI_CHECK(snapValue(0.876, bounds, std::nullopt) == 0.876);
		FLOWUI_CHECK(snapValue(5.0, bounds, 0.25) == 1.0);
		FLOWUI_CHECK(snapValue(-5.0, bounds, 0.25) == -1.0);
	});

	runner.run("slider state owns only drag mechanics", [] {
		const FlowUi::FSEL::SliderState state{};
		FLOWUI_CHECK(!state.isDragging);
		FLOWUI_CHECK(state.pointerAtPress == 0.0f);
		FLOWUI_CHECK(state.valueAtPress == 0.0);
	});

	return runner.finish();
}
