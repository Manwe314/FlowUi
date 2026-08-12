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
	struct Parts {
		static constexpr auto editor = FlowUi::Part("editor");
	};
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("flowui/tests/phase-f/stateful");
	static void buildElement(BuildContext&) {}
};

struct StatelessDefinition {
	using Parameters = ::Parameters;
	using BuildContext = FlowUi::ElementBuildContext<StatelessDefinition>;
	using InteractionContext = FlowUi::ElementInteractionContext<StatelessDefinition>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("flowui/tests/phase-f/stateless");
	static void buildElement(BuildContext&) {}
};

inline constexpr StatefulDefinition kStatefulElement{};
inline constexpr StatelessDefinition kStatelessElement{};
inline constexpr FlowUi::FlowElementID kStatefulInstance =
	FlowUi::detail::element_id::resolveLocal(
		FlowUi::RootFlowScopeID,
		StatefulDefinition::definitionId,
		FlowUi::LocalElementName("instance").token
#if FLOW_UI_DEV_MODE
		, "instance"
#endif
	);
inline constexpr FlowUi::GlobalFlowID kGlobalStatefulInstance =
	FlowUi::Global<kStatefulElement>("global-instance");
inline constexpr FlowUi::FlowElementPartID kStatefulPartInstance =
	FlowUi::PartID(
		kStatefulElement,
		kStatefulInstance,
		StatefulDefinition::Parts::editor);

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
	FlowUi::FlowElementID elementId) {
	{ manager.getStatePointer(kStatefulElement, window, elementId) } -> std::same_as<State*>;
};

template <typename Manager>
concept HasMutableStatePointerOnConstManager = requires(
	const Manager& manager,
	FlowUi::WindowId window,
	FlowUi::FlowElementID elementId) {
	manager.getStatePointer(kStatefulElement, window, elementId);
};

template <typename Manager>
concept AcceptsRawNumericStateId = requires(
	Manager& manager,
	FlowUi::WindowId window,
	uint64_t flatId) {
	manager.getStatePointer(kStatefulElement, window, flatId);
};

template <typename Manager>
concept HasRawNumericCallableStateApi = requires(
	Manager& manager,
	FlowUi::WindowId window,
	uint64_t flowId) {
	manager.withState(kStatefulElement, window, flowId, StateMutator{});
};

template <typename Manager>
concept HasRawNumericReadState = requires(
	const Manager& manager,
	FlowUi::WindowId window,
	uint64_t flowId) {
	manager.readState(kStatefulElement, window, flowId);
};

template <typename Manager>
concept HasRawNumericModifyState = requires(
	Manager& manager,
	FlowUi::WindowId window,
	uint64_t flowId) {
	manager.modifyState(kStatefulElement, window, flowId);
};

static_assert(std::same_as<
	typename StatefulDefinition::BuildContext::ElementType,
	StatefulDefinition>);
static_assert(std::same_as<
	typename StatefulDefinition::InteractionContext::ElementType,
	StatefulDefinition>);
static_assert(std::same_as<
	decltype(std::declval<StatefulDefinition::BuildContext&>().id),
	FlowUi::FlowElementID>);
static_assert(std::same_as<
	decltype(std::declval<StatefulDefinition::InteractionContext&>().id),
	FlowUi::FlowElementID>);
static_assert(std::same_as<
	decltype(std::declval<StatefulDefinition::BuildContext&>().clayID()),
	Clay_ElementId>);
static_assert(std::same_as<
	decltype(std::declval<StatefulDefinition::BuildContext&>().childID(
		std::declval<FlowUi::LocalElementName>())),
	FlowUi::FlowElementID>);
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
		kStatefulInstance)),
	const State*>);
static_assert(HasMutableStatePointer<FlowUi::ElementManager>);
static_assert(!HasMutableStatePointerOnConstManager<FlowUi::ElementManager>);
static_assert(!AcceptsRawNumericStateId<FlowUi::ElementManager>);
static_assert(std::same_as<
	decltype(std::declval<FlowUi::ElementManager&>().eraseState(
		kStatefulElement,
		FlowUi::MainWindowId,
		kStatefulInstance)),
	bool>);
static_assert(std::same_as<
	decltype(std::declval<const FlowUi::ElementManager&>().getStatePointerConst(
		kStatefulElement,
		FlowUi::MainWindowId,
		kStatefulPartInstance)),
	const State*>);
static_assert(std::same_as<
	decltype(std::declval<FlowUi::ElementManager&>().getStatePointer(
		kStatefulElement,
		FlowUi::MainWindowId,
		kStatefulPartInstance)),
	State*>);
static_assert(std::same_as<
	decltype(std::declval<FlowUi::ElementManager&>().eraseState(
		kStatefulElement,
		FlowUi::MainWindowId,
		kStatefulPartInstance)),
	bool>);
static_assert(std::same_as<
	decltype(std::declval<const FlowUi::ElementManager&>().getStatePointerConst(
		kStatefulElement,
		FlowUi::MainWindowId,
		kGlobalStatefulInstance)),
	const State*>);
static_assert(std::same_as<
	decltype(std::declval<FlowUi::ElementManager&>().getStatePointer(
		kStatefulElement,
		FlowUi::MainWindowId,
		kGlobalStatefulInstance)),
	State*>);
static_assert(std::same_as<
	decltype(std::declval<FlowUi::ElementManager&>().eraseState(
		kStatefulElement,
		FlowUi::MainWindowId,
		kGlobalStatefulInstance)),
	bool>);
static_assert(!HasRawNumericCallableStateApi<FlowUi::ElementManager>);
static_assert(!HasRawNumericReadState<FlowUi::ElementManager>);
static_assert(!HasRawNumericModifyState<FlowUi::ElementManager>);

void compileTypedManagerCalls(
	FlowUi::ElementManager& manager,
	const FlowUi::ElementManager& constManager) {
	const FlowUi::FlowElementID elementId = kStatefulInstance;
	[[maybe_unused]] const State* read =
		constManager.getStatePointerConst(kStatefulElement, FlowUi::MainWindowId, elementId);
	[[maybe_unused]] State* mutableState =
		manager.getStatePointer(kStatefulElement, FlowUi::MainWindowId, elementId);
	[[maybe_unused]] const bool erased =
		manager.eraseState(kStatefulElement, FlowUi::MainWindowId, elementId);
	[[maybe_unused]] const State* globalRead =
		constManager.getStatePointerConst(
			kStatefulElement, FlowUi::MainWindowId, kGlobalStatefulInstance);
	[[maybe_unused]] State* globalMutable =
		manager.getStatePointer(
			kStatefulElement, FlowUi::MainWindowId, kGlobalStatefulInstance);
	[[maybe_unused]] const bool globalErased =
		manager.eraseState(
			kStatefulElement, FlowUi::MainWindowId, kGlobalStatefulInstance);
	[[maybe_unused]] const State* partRead =
		constManager.getStatePointerConst(
			kStatefulElement, FlowUi::MainWindowId, kStatefulPartInstance);
	[[maybe_unused]] State* partMutable =
		manager.getStatePointer(
			kStatefulElement, FlowUi::MainWindowId, kStatefulPartInstance);
	[[maybe_unused]] const bool partErased =
		manager.eraseState(
			kStatefulElement, FlowUi::MainWindowId, kStatefulPartInstance);
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
