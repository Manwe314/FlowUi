#include "TestHarness.hpp"

#include <type_traits>

#include <FlowUi/Flow.hpp>

namespace {

constexpr auto kSetBool = FlowUi::UiAction(
	"tests.actions.set_bool",
	[](bool& target, bool value) { target = value; });
constexpr auto kToggleBool = FlowUi::UiAction(
	"tests.actions.toggle_bool",
	[](bool& target) { target = !target; });

static_assert(FlowUi::AppAction("tests.actions.save").value != 0);
static_assert(FlowUi::AppAction("tests.actions.save").value ==
	FlowUi::AppAction("tests.actions.save").value);
static_assert(FlowUi::AppAction("tests.actions.save").value !=
	FlowUi::AppAction("tests.actions.export").value);
static_assert(std::is_trivially_copyable_v<FlowUi::AppActionCall>);
static_assert(std::is_trivially_copyable_v<FlowUi::UiActionCall>);
static_assert(std::is_trivially_copyable_v<FlowUi::ActionCall>);
static_assert(std::is_empty_v<std::remove_cvref_t<decltype(kSetBool.operation)>>);
static_assert(FlowUi::FlowElement<FlowUi::Button>);
static_assert(!FlowUi::HasState<FlowUi::Button>);

template <typename Call>
concept ShortcutCanRetain = requires(
	FlowUi::ShortcutManager& shortcuts,
	FlowUi::ShortcutChord chord,
	Call call) {
	shortcuts.registerShortcut(
		chord, FlowUi::ShortcutScope::Global, 0, call,
		FlowUi::ShortcutHandling::Consume);
};

static_assert(ShortcutCanRetain<FlowUi::AppActionCall>);
static_assert(!ShortcutCanRetain<FlowUi::UiActionCall>);

void testEmptyContract() {
	const FlowUi::AppActionCall app{};
	const FlowUi::UiActionCall ui{};
	const FlowUi::ActionCall combined{};
	FLOWUI_CHECK(!app);
	FLOWUI_CHECK(!ui);
	FLOWUI_CHECK(!combined);
	FLOWUI_CHECK(combined.kind() == FlowUi::ActionCallKind::None);
}

void testUiRecipesAndTaggedDispatch() {
	FlowUi::ActionManager manager;
	bool first = false;
	bool second = false;

	const auto set = manager.uiActions().make(kSetBool, first, true);
	const auto toggle = manager.uiActions().make(kToggleBool, second);
	FLOWUI_CHECK(set);
	FLOWUI_CHECK(toggle);
	FLOWUI_CHECK(FlowUi::ActionCall{set}.kind() == FlowUi::ActionCallKind::Ui);
	FLOWUI_CHECK(manager.availability(FlowUi::ActionCall{set}).enabled);
	FLOWUI_CHECK(manager.invoke(FlowUi::ActionCall{set}) ==
		FlowUi::ActionInvocationStatus::Invoked);
	FLOWUI_CHECK(first);
	FLOWUI_CHECK(manager.invoke(FlowUi::ActionCall{toggle}) ==
		FlowUi::ActionInvocationStatus::Invoked);
	FLOWUI_CHECK(second);
	FLOWUI_CHECK(manager.invoke({}) == FlowUi::ActionInvocationStatus::Empty);
}

void testOneButtonPropertyCoversBothActionKinds() {
	FlowUi::ActionManager manager;
	bool dropdownOpen = false;
	const auto toggle = manager.uiActions().make(kToggleBool, dropdownOpen);
	const FlowUi::ButtonParameters localButton{
		.label = "Open menu",
		.onActivate = FlowUi::ActionCall{toggle},
	};
	const FlowUi::ButtonParameters appButton{
		.label = "Save",
		.onActivate = FlowUi::ActionCall{
			FlowUi::AppActionCall{FlowUi::AppAction("tests.actions.save")}},
	};
	FLOWUI_CHECK(localButton.onActivate.kind() == FlowUi::ActionCallKind::Ui);
	FLOWUI_CHECK(appButton.onActivate.kind() == FlowUi::ActionCallKind::App);
	FLOWUI_CHECK(manager.invoke(localButton.onActivate) ==
		FlowUi::ActionInvocationStatus::Invoked);
	FLOWUI_CHECK(dropdownOpen);
	FLOWUI_CHECK(manager.invoke(appButton.onActivate) ==
		FlowUi::ActionInvocationStatus::Unbound);
	FLOWUI_CHECK(!FlowUi::ButtonParameters{}.onActivate);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("empty calls are trivial inert values", testEmptyContract);
	runner.run("UI recipes bind resources and tagged dispatch invokes them", testUiRecipesAndTaggedDispatch);
	runner.run("one stateless Button property accepts app and local UI actions",
		testOneButtonPropertyCoversBothActionKinds);
	return runner.finish();
}
