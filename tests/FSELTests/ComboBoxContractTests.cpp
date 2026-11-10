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

	runner.run("compact triggers reserve the disclosure and clamp oversized decoration", [] {
		using FlowUi::FSEL::detail::combo_box::fit_content;
		for (const float height : {20.0f, 24.0f, 28.0f}) {
			const auto layout = fit_content(180, height, {10, 8, 6, 6}, 80, 8, true, true);
			FLOWUI_CHECK(layout.icon_size == height - 12);
			FLOWUI_CHECK(layout.show_leading_icon);
			FLOWUI_CHECK(2 * layout.icon_size + 2 * layout.gap <= 162);
		}
		const auto narrow = fit_content(24, 20, {10, 8, 6, 6}, 80, 8, true, true);
		FLOWUI_CHECK(!narrow.show_leading_icon);
		FLOWUI_CHECK(narrow.icon_size + narrow.gap <= 6);
		const auto tiny = fit_content(4, 3, {10, 8, 6, 6}, 80, 8, true, true);
		FLOWUI_CHECK(tiny.padding.left + tiny.padding.right <= 4);
		FLOWUI_CHECK(tiny.padding.top + tiny.padding.bottom <= 3);
		FLOWUI_CHECK(tiny.icon_size == 0);
		const auto option = fit_content(120, 20, {8, 8, 4, 4}, 80, 8, true, false);
		FLOWUI_CHECK(option.show_leading_icon && option.icon_size == 12);
		const auto readable = fit_content(180, 20, {10, 8, 6, 6}, 80, 8, true, true, 12);
		FLOWUI_CHECK(readable.content_height == 12);
		FLOWUI_CHECK(readable.padding.top + readable.padding.bottom == 8);
		FLOWUI_CHECK(readable.icon_size == 12);
		FLOWUI_CHECK(readable.label_width + 2 * (readable.icon_size + readable.gap) == 162);
	});

	return runner.finish();
}
