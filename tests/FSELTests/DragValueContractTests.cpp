#include <FSEL/DragValue.hpp>

#include "TestHarness.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>

using UInt64DragValue = FlowUi::FSEL::DragValue<
	std::uint64_t,
	FlowUi::DefinitionID("tests.drag-value.uint64")>;

inline constexpr UInt64DragValue kUInt64DragValue{};

static_assert(FlowUi::FlowElement<FlowUi::FSEL::DragValue<int>>);
static_assert(FlowUi::DrawableFlowElement<FlowUi::FSEL::DragValue<int>>);
static_assert(!FlowUi::ConstructibleFlowElement<FlowUi::FSEL::DragValue<int>>);
static_assert(FlowUi::HasState<FlowUi::FSEL::DragValue<int>>);
static_assert(!FlowUi::HasResources<FlowUi::FSEL::DragValue<int>>);
static_assert(FlowUi::FlowElement<UInt64DragValue>);
static_assert(std::is_empty_v<UInt64DragValue>);

[[maybe_unused]] constexpr auto kIntDragValueBuild =
	&FlowUi::FSEL::DragValue<int>::buildElement;
[[maybe_unused]] constexpr auto kUIntDragValueLogic =
	&FlowUi::FSEL::DragValue<unsigned int>::runLogic;
[[maybe_unused]] constexpr auto kFloatDragValuePress =
	&FlowUi::FSEL::DragValue<float>::onPressed;

int main() {
	FlowUi::test::Runner runner;

	runner.run("drag value defaults describe a thresholded editable scrub", [] {
		int value = 4;
		FlowUi::FSEL::DragValueParameters<int> parameters{};
		parameters.value = &value;
		FLOWUI_CHECK(parameters.value == &value);
		FLOWUI_CHECK(parameters.step == 1);
		FLOWUI_CHECK(parameters.pixelsPerStep == 4.0f);
		FLOWUI_CHECK(parameters.dragThreshold == 4.0f);
		FLOWUI_CHECK(parameters.allowTextEntry);
		FLOWUI_CHECK(
			parameters.syncPolicy == FlowUi::FSEL::NumericTextSyncPolicy::Live);
	});

	runner.run("drag displacement resolves signed whole steps", [] {
		using namespace FlowUi::FSEL::detail::drag_value;
		FLOWUI_CHECK(!crossedThreshold(3.9f, 4.0f));
		FLOWUI_CHECK(crossedThreshold(-4.0f, 4.0f));
		FLOWUI_CHECK(stepCountForDelta(15.9f, 4.0f) == 3);
		FLOWUI_CHECK(stepCountForDelta(-15.9f, 4.0f) == -3);
		FLOWUI_CHECK(stepCountForDelta(10.0f, 0.0f) == 0);
	});

	runner.run("multi-step native offsets clamp without overflow", [] {
		using namespace FlowUi::FSEL::detail::numeric;
		const Bounds<int> signedBounds{.lower = -10, .upper = 10};
		FLOWUI_CHECK(offsetBySteps(0, 2, 3, signedBounds) == 6);
		FLOWUI_CHECK(offsetBySteps(0, 2, -3, signedBounds) == -6);
		FLOWUI_CHECK(offsetBySteps(9, 4, 100, signedBounds) == 10);
		FLOWUI_CHECK(offsetBySteps(-9, 4, -100, signedBounds) == -10);
		FLOWUI_CHECK(
			offsetBySteps(
				std::numeric_limits<int>::lowest(),
				1,
				std::numeric_limits<int64_t>::max(),
				Bounds<int>{
					.lower = std::numeric_limits<int>::lowest(),
					.upper = std::numeric_limits<int>::max(),
				}) == std::numeric_limits<int>::max());

		using U64 = std::uint64_t;
		const Bounds<U64> unsignedBounds{
			.lower = 0,
			.upper = std::numeric_limits<U64>::max(),
		};
		FLOWUI_CHECK(
			offsetBySteps<U64>(0, std::numeric_limits<U64>::max(), 2,
				unsignedBounds) == std::numeric_limits<U64>::max());
		FLOWUI_CHECK(
			offsetBySteps<U64>(std::numeric_limits<U64>::max(), 7, -2,
				unsignedBounds) == std::numeric_limits<U64>::max() - 14);
	});

	runner.run("drag value specializations have stable distinct definitions", [] {
		FLOWUI_CHECK(
			FlowUi::FSEL::DragValue<int>::definitionId !=
			FlowUi::FSEL::DragValue<unsigned int>::definitionId);
		FLOWUI_CHECK(
			UInt64DragValue::definitionId ==
			FlowUi::DefinitionID("tests.drag-value.uint64"));
		(void)kUInt64DragValue;
	});

	return runner.finish();
}
