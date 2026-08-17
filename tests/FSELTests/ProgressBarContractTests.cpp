#include "TestHarness.hpp"

#include <cmath>
#include <limits>
#include <type_traits>

#include <FSEL/ProgressBar.hpp>

static_assert(FlowUi::FlowElement<FlowUi::FSEL::ProgressBar>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::ProgressBar>);
static_assert(!FlowUi::ConstructibleFlowElement<FlowUi::FSEL::ProgressBar>);
static_assert(!FlowUi::HasState<FlowUi::FSEL::ProgressBar>);
static_assert(std::is_empty_v<FlowUi::FSEL::ProgressBar>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("progress bar defaults to normalized horizontal progress", [] {
		const FlowUi::FSEL::ProgressBarParameters parameters{};
		FLOWUI_CHECK(
			parameters.axis == FlowUi::FSEL::ProgressBarAxis::Horizontal);
		FLOWUI_CHECK(!parameters.inverted);
		FLOWUI_CHECK(parameters.value == 0.0);
		FLOWUI_CHECK(parameters.minimum == 0.0);
		FLOWUI_CHECK(parameters.maximum == 1.0);
		FLOWUI_CHECK(!parameters.sizing.has_value());
	});

	runner.run("progress bar visual overrides are independently optional", [] {
		FlowUi::FSEL::ProgressBarParameters parameters{};
		parameters.baseColor = FlowUi::Flow_Color("#112233ff");
		parameters.fillColor = FlowUi::Flow_Color("#445566ff");
		FLOWUI_CHECK(parameters.baseColor.has_value());
		FLOWUI_CHECK(parameters.fillColor.has_value());
		FLOWUI_CHECK(!parameters.borderColor.has_value());
		FLOWUI_CHECK(!parameters.borderWidth.has_value());
		FLOWUI_CHECK(!parameters.cornerRadius.has_value());
	});

	runner.run("shared bounded value math normalizes progress safely", [] {
		using namespace FlowUi::FSEL::detail::bounded_value;
		const Bounds inverted = normalizeBounds(200.0, 100.0);
		FLOWUI_CHECK(inverted.lower == 100.0);
		FLOWUI_CHECK(inverted.upper == 200.0);
		FLOWUI_CHECK(normalizedRatio(150.0, inverted) == 0.5);
		FLOWUI_CHECK(normalizedRatio(250.0, inverted) == 1.0);
		FLOWUI_CHECK(normalizedRatio(50.0, inverted) == 0.0);

		const Bounds zeroRange = normalizeBounds(4.0, 4.0);
		FLOWUI_CHECK(normalizedRatio(4.0, zeroRange) == 0.0);

		const Bounds invalid = normalizeBounds(
			std::numeric_limits<double>::quiet_NaN(),
			2.0);
		FLOWUI_CHECK(invalid.lower == 0.0);
		FLOWUI_CHECK(invalid.upper == 1.0);
		FLOWUI_CHECK(normalizedRatio(
			std::numeric_limits<double>::infinity(),
			invalid) == 0.0);
	});

	runner.run("shared math remains suitable for slider snapping", [] {
		using namespace FlowUi::FSEL::detail::bounded_value;
		const Bounds bounds{.lower = -1.0, .upper = 1.0};
		FLOWUI_CHECK(
			std::abs(snapValue(0.56872948391238, bounds, 0.01) - 0.57) <
			1.0e-12);
		FLOWUI_CHECK(valueFromRatio(0.25, bounds) == -0.5);
	});

	return runner.finish();
}
