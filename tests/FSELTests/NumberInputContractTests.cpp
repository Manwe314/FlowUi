#include "TestHarness.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include <FSEL/NumberInput.hpp>

using UInt64NumberInput = FlowUi::FSEL::NumberInput<
	std::uint64_t,
	FlowUi::DefinitionID("tests.number-input.uint64")>;

inline constexpr UInt64NumberInput kUInt64NumberInput{};

static_assert(FlowUi::FlowElement<FlowUi::FSEL::NumberInput<int>>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::NumberInput<int>>);
static_assert(!FlowUi::ConstructibleFlowElement<FlowUi::FSEL::NumberInput<int>>);
static_assert(FlowUi::HasState<FlowUi::FSEL::NumberInput<int>>);
static_assert(FlowUi::HasResources<FlowUi::FSEL::NumberInput<int>>);
static_assert(FlowUi::FlowElement<UInt64NumberInput>);
static_assert(std::is_empty_v<UInt64NumberInput>);

[[maybe_unused]] constexpr auto kIntNumberInputBuild =
	&FlowUi::FSEL::NumberInput<int>::buildElement;
[[maybe_unused]] constexpr auto kUIntNumberInputLogic =
	&FlowUi::FSEL::NumberInput<unsigned int>::runLogic;
[[maybe_unused]] constexpr auto kFloatNumberInputHover =
	&FlowUi::FSEL::NumberInput<float>::onHovered;

int main() {
	FlowUi::test::Runner runner;

	runner.run("number input defaults retain native binding types", [] {
		int value = 4;
		FlowUi::FSEL::NumberInputParameters<int> parameters{};
		parameters.value = &value;
		FLOWUI_CHECK(parameters.value == &value);
		FLOWUI_CHECK(parameters.step == 1);
		FLOWUI_CHECK(parameters.minimum == std::nullopt);
		FLOWUI_CHECK(parameters.maximum == std::nullopt);
		FLOWUI_CHECK(
			parameters.stepButtons ==
			FlowUi::FSEL::NumberInputStepButtons::TrailingVertical);
	});

	runner.run("numeric grammar removes disallowed characters before parsing", [] {
		using FlowUi::FSEL::detail::numeric_text::sanitize;
		FLOWUI_CHECK(sanitize<unsigned int>("-12px", false) == "12");
		FLOWUI_CHECK(sanitize<int>("1-2", false) == "12");
		FLOWUI_CHECK(sanitize<float>("-1.2.3f", false) == "-1.23");
		FLOWUI_CHECK(sanitize<float>("1e-3", true) == "1e-3");
		FLOWUI_CHECK(sanitize<float>("1e-3", false) == "13");
	});

	runner.run("intermediate floating buffers are preserved but not parsed", [] {
		using namespace FlowUi::FSEL::detail::numeric_text;
		FLOWUI_CHECK(parse<float>("-").status == ParseStatus::Incomplete);
		FLOWUI_CHECK(parse<float>(".").status == ParseStatus::Incomplete);
		FLOWUI_CHECK(parse<float>("1e-").status == ParseStatus::Incomplete);
		const auto complete = parse<float>("-12.5");
		FLOWUI_CHECK(complete.status == ParseStatus::Complete);
		FLOWUI_CHECK(complete.value == -12.5f);
	});

	runner.run("uint64 parsing and formatting never pass through double", [] {
		using namespace FlowUi::FSEL::detail::numeric_text;
		constexpr std::string_view maximum = "18446744073709551615";
		const auto parsed = parse<std::uint64_t>(maximum);
		FLOWUI_CHECK(parsed.status == ParseStatus::Complete);
		FLOWUI_CHECK(parsed.value == std::numeric_limits<std::uint64_t>::max());
		FLOWUI_CHECK(format(parsed.value).view() == maximum);
	});

	runner.run("checked native steps saturate at authored bounds", [] {
		using namespace FlowUi::FSEL::detail::numeric;
		const Bounds<int> signedBounds{.lower = -10, .upper = 10};
		FLOWUI_CHECK(stepValue(9, 4, 1, signedBounds) == 10);
		FLOWUI_CHECK(stepValue(-9, 4, -1, signedBounds) == -10);
		const Bounds<unsigned int> unsignedBounds{.lower = 0, .upper = 8};
		FLOWUI_CHECK(stepValue(1u, 4u, -1, unsignedBounds) == 0u);
		FLOWUI_CHECK(stepValue(7u, 4u, 1, unsignedBounds) == 8u);
		FLOWUI_CHECK(!validStep(0));
		const auto inverted = resolveBounds<int>(10, -10);
		FLOWUI_CHECK(!inverted.valid);
		FLOWUI_CHECK(stepValue(4, 1, 1, inverted) == 4);
	});

	runner.run("element specializations have stable distinct definitions", [] {
		FLOWUI_CHECK(
			FlowUi::FSEL::NumberInput<int>::definitionId !=
			FlowUi::FSEL::NumberInput<unsigned int>::definitionId);
		FLOWUI_CHECK(
			UInt64NumberInput::definitionId ==
			FlowUi::DefinitionID("tests.number-input.uint64"));
		(void)kUInt64NumberInput;
	});

	return runner.finish();
}
