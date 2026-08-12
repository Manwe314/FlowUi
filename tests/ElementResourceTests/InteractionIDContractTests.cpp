#include "TestHarness.hpp"

#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "managers/UiManager.hpp"
#include "managers/structs/FlowUiElementStructs.hpp"

namespace {

constexpr FlowUi::FlowElementID kLocal{
	.value = 0x123456789abcdef0ull,
#if FLOW_UI_DEV_MODE
	.debugName = "tests/interaction/local",
#endif
};

constexpr FlowUi::GlobalFlowID kGlobal{
	.value = 0xfedcba9876543210ull,
#if FLOW_UI_DEV_MODE
	.debugName = "tests/interaction/global",
#endif
};

constexpr FlowUi::FlowElementPartID kPart{
	.value = 0x0f1e2d3c4b5a6978ull,
#if FLOW_UI_DEV_MODE
	.debugName = "tests/interaction/part",
#endif
};

template <typename Manager>
concept HasStringClayElementId = requires(Manager& ui, std::string_view name) {
	ui.toClayEID(name);
};

template <typename Manager, typename ID>
concept HasTypedClayElementId = requires(Manager& ui, ID id) {
	{ ui.toClayEID(id) } -> std::same_as<Clay_ElementId>;
};

static_assert(!HasStringClayElementId<FlowUi::UiManager>);
static_assert(HasTypedClayElementId<FlowUi::UiManager, FlowUi::FlowElementID>);
static_assert(HasTypedClayElementId<FlowUi::UiManager, FlowUi::GlobalFlowID>);
static_assert(HasTypedClayElementId<FlowUi::UiManager, FlowUi::FlowElementPartID>);
static_assert(std::same_as<
	typename decltype(FlowUi::InteractionSnapshot::hoveredElementIds)::value_type,
	uint32_t>);

void testTypedInteractionQueries() {
	FlowUi::InteractionSnapshot snapshot{};
	snapshot.hoveredElementIds.push_back(FlowUi::FlowIDToClayID(kLocal));
	snapshot.pressedElementIds.push_back(FlowUi::FlowIDToClayID(kGlobal));
	snapshot.heldElementIds.push_back(FlowUi::FlowIDToClayID(kPart));
	snapshot.releasedElementIds.push_back(FlowUi::FlowIDToClayID(kLocal));

	FLOWUI_CHECK(snapshot.isHovered(kLocal));
	FLOWUI_CHECK(snapshot.isPressed(kGlobal));
	FLOWUI_CHECK(snapshot.isHeld(kPart));
	FLOWUI_CHECK(snapshot.isReleased(kLocal));
	FLOWUI_CHECK(!snapshot.isPressed(kLocal));
	FLOWUI_CHECK(snapshot.isHovered(Clay_ElementId{.id = FlowUi::FlowIDToClayID(kLocal)}));
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("interaction snapshots query strong numeric IDs", testTypedInteractionQueries);
	return runner.finish();
}
