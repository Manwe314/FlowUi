#include "TestHarness.hpp"

#include <concepts>
#include <type_traits>
#include <utility>

#include "managers/ElementManager.hpp"
#include "managers/structs/FlowUiElementStructs.hpp"

namespace {

struct Parameters {
	int input = 0;
};

struct State {
	int value = 0;
};

struct StatefulDefinition {
	using Parameters = ::Parameters;
	using State = ::State;
	using BuildContext = FlowUi::ElementBuildContext<StatefulDefinition>;
	using InteractionContext = FlowUi::ElementInteractionContext<StatefulDefinition>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("flowui/tests/phase-f/stateful");
	static void buildElement(BuildContext&) {}
};

struct StatelessDefinition {
	using Parameters = ::Parameters;
	using BuildContext = FlowUi::ElementBuildContext<StatelessDefinition>;
	using InteractionContext = FlowUi::ElementInteractionContext<StatelessDefinition>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("flowui/tests/phase-f/stateless");
	static void buildElement(BuildContext&) {}
};

inline constexpr StatefulDefinition kStatefulElement{};
inline constexpr StatelessDefinition kStatelessElement{};

template <typename Context>
concept HasMutableState = requires(Context& context) {
	{ context.state() } -> std::same_as<State&>;
};

template <typename Context>
concept HasConstState = requires(const Context& context) {
	{ context.state() } -> std::same_as<const State&>;
};

template <typename Context>
concept HasAnyState = requires(Context& context) {
	context.state();
};

struct StateMutator {
	void operator()(State&) const noexcept {}
};

template <typename Manager>
concept HasMutableStatePointer = requires(
	Manager& manager,
	FlowUi::WindowId window,
	FlowUi::FlowElementId flowId) {
	{ manager.getStatePointer(kStatefulElement, window, flowId) } -> std::same_as<State*>;
};

template <typename Manager>
concept HasMutableStatePointerOnConstManager = requires(
	const Manager& manager,
	FlowUi::WindowId window,
	FlowUi::FlowElementId flowId) {
	manager.getStatePointer(kStatefulElement, window, flowId);
};

template <typename Manager>
concept HasLegacyCallableStateApi = requires(
	Manager& manager,
	FlowUi::WindowId window,
	FlowUi::FlowElementId flowId) {
	manager.withState(kStatefulElement, window, flowId, StateMutator{});
};

template <typename Manager>
concept HasLegacyReadState = requires(
	const Manager& manager,
	FlowUi::WindowId window,
	FlowUi::FlowElementId flowId) {
	manager.readState(kStatefulElement, window, flowId);
};

template <typename Manager>
concept HasLegacyModifyState = requires(
	Manager& manager,
	FlowUi::WindowId window,
	FlowUi::FlowElementId flowId) {
	manager.modifyState(kStatefulElement, window, flowId);
};

static_assert(std::same_as<
	typename StatefulDefinition::BuildContext::ElementType,
	StatefulDefinition>);
static_assert(std::same_as<
	typename StatefulDefinition::InteractionContext::ElementType,
	StatefulDefinition>);
static_assert(HasMutableState<StatefulDefinition::BuildContext>);
static_assert(HasConstState<StatefulDefinition::BuildContext>);
static_assert(HasMutableState<StatefulDefinition::InteractionContext>);
static_assert(HasConstState<StatefulDefinition::InteractionContext>);
static_assert(!HasAnyState<StatelessDefinition::BuildContext>);
static_assert(!HasAnyState<StatelessDefinition::InteractionContext>);

static_assert(std::same_as<
	decltype(std::declval<const FlowUi::ElementManager&>().getStatePointerConst(
		kStatefulElement,
		FlowUi::MainWindowId,
		FLOW_ID("flowui/tests/phase-f/instance"))),
	const State*>);
static_assert(HasMutableStatePointer<FlowUi::ElementManager>);
static_assert(!HasMutableStatePointerOnConstManager<FlowUi::ElementManager>);
static_assert(std::same_as<
	decltype(std::declval<FlowUi::ElementManager&>().eraseState(
		kStatefulElement,
		FlowUi::MainWindowId,
		FLOW_ID("flowui/tests/phase-f/instance"))),
	bool>);
static_assert(!HasLegacyCallableStateApi<FlowUi::ElementManager>);
static_assert(!HasLegacyReadState<FlowUi::ElementManager>);
static_assert(!HasLegacyModifyState<FlowUi::ElementManager>);

void compileTypedManagerCalls(
	FlowUi::ElementManager& manager,
	const FlowUi::ElementManager& constManager) {
	const FlowUi::FlowElementId flowId = FLOW_ID("flowui/tests/phase-f/instance");
	[[maybe_unused]] const State* read =
		constManager.getStatePointerConst(kStatefulElement, FlowUi::MainWindowId, flowId);
	[[maybe_unused]] State* mutableState =
		manager.getStatePointer(kStatefulElement, FlowUi::MainWindowId, flowId);
	[[maybe_unused]] const bool erased =
		manager.eraseState(kStatefulElement, FlowUi::MainWindowId, flowId);
}

void testPhaseFPublicSurfaceCompiles() {
	[[maybe_unused]] auto* compileOnly = &compileTypedManagerCalls;
	FLOWUI_CHECK(true);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("Phase F typed state surface compiles", testPhaseFPublicSurfaceCompiles);
	return runner.finish();
}
