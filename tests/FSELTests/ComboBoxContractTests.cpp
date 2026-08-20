#include "TestHarness.hpp"

#include <array>
#include <type_traits>

#include <FSEL/ComboBox.hpp>

using FlowUi::FSEL::ComboBox;
using FlowUi::FSEL::ComboBoxOption;
using FlowUi::FSEL::ComboBoxParameters;

static_assert(FlowUi::FlowElement<ComboBox>);
static_assert(FlowUi::DrawableFlowElement<ComboBox>);
static_assert(!FlowUi::ConstructibleFlowElement<ComboBox>);
static_assert(FlowUi::HasState<ComboBox>);
static_assert(FlowUi::HasResources<ComboBox>);
static_assert(std::is_empty_v<ComboBox>);

int main() {
	FlowUi::test::Runner runner;

	runner.run("combo box defaults preserve the lean data contract", [] {
		const ComboBoxParameters parameters{};
		FLOWUI_CHECK(parameters.selectedValue == nullptr);
		FLOWUI_CHECK(parameters.open == nullptr);
		FLOWUI_CHECK(parameters.options.empty());
		FLOWUI_CHECK(parameters.outsidePress ==
			FlowUi::PopupOutsidePressPolicy::DismissAndBlockAnchor);
		FLOWUI_CHECK(parameters.showScrollIndicator);
	});

	runner.run("options borrow text and retain native selection values", [] {
		constexpr std::array options{
			ComboBoxOption{.value = 10, .text = "Ten"},
			ComboBoxOption{.value = 20, .text = "Twenty", .enabled = false},
		};
		uint64_t selected = 10;
		const ComboBoxParameters parameters{
			.options = options,
			.selectedValue = &selected,
		};
		FLOWUI_CHECK(parameters.options.size() == 2);
		FLOWUI_CHECK(parameters.options[1].value == 20);
		FLOWUI_CHECK(!parameters.options[1].enabled);
	});

	runner.run("scroll indicator geometry clamps both ends", [] {
		using FlowUi::FSEL::detail::scroll_indicator::calculate;
		const auto start = calculate(100.0f, 400.0f, 0.0f, 80.0f, 16.0f);
		FLOWUI_CHECK(start.visible);
		FLOWUI_CHECK(start.maximumScroll == 300.0f);
		FLOWUI_CHECK(start.extent == 20.0f);
		FLOWUI_CHECK(start.offset == 0.0f);

		const auto end = calculate(100.0f, 400.0f, -999.0f, 80.0f, 16.0f);
		FLOWUI_CHECK(end.offset == 60.0f);
		FLOWUI_CHECK(!calculate(100.0f, 100.0f, 0.0f, 80.0f, 16.0f).visible);
	});

	return runner.finish();
}
