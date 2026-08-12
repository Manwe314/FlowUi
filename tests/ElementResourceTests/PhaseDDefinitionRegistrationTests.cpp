#include "TestHarness.hpp"

#include <cstddef>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>

#include "internal/ManagerStorage/ElementStorageController.hpp"
#include "managers/structs/ElementManagerStructs.hpp"

namespace {

struct CurrentParameters {
	int value = 0;
};

struct CurrentState {
	static inline int constructions = 0;
	static inline int destructions = 0;

	CurrentState() { ++constructions; }
	CurrentState(const CurrentState&) = delete;
	CurrentState& operator=(const CurrentState&) = delete;
	CurrentState(CurrentState&&) = delete;
	CurrentState& operator=(CurrentState&&) = delete;
	~CurrentState() noexcept { ++destructions; }
};

struct AlternateState {
	float value = 0.0f;
};

struct AppResources {
	static inline int constructions = 0;
	static inline int destructions = 0;

	explicit AppResources(FlowUi::App&) { ++constructions; }
	AppResources(const AppResources&) = delete;
	AppResources& operator=(const AppResources&) = delete;
	AppResources(AppResources&&) = delete;
	AppResources& operator=(AppResources&&) = delete;
	~AppResources() noexcept { ++destructions; }
};

inline constexpr FlowUi::FlowDefinitionID kSharedDefinitionId =
	FlowUi::DefinitionID("flowui/tests/phase-d/shared-definition");

struct CurrentDefinition {
	using Parameters = CurrentParameters;
	using State = CurrentState;
	using Resources = AppResources;
	using BuildContext = FlowUi::ElementBuildContext<CurrentDefinition>;
	static constexpr FlowUi::FlowDefinitionID definitionId = kSharedDefinitionId;
	static void buildElement(BuildContext&) {}
};

struct CollidingDefinition {
	using Parameters = CurrentParameters;
	using State = AlternateState;
	using Resources = AppResources;
	using BuildContext = FlowUi::ElementBuildContext<CollidingDefinition>;
	static constexpr FlowUi::FlowDefinitionID definitionId = kSharedDefinitionId;
	static void buildElement(BuildContext&) {}
};

struct FutureDefinition {
	using Parameters = CurrentParameters;
	using State = CurrentState;
	using Resources = AppResources;
	using BuildContext = FlowUi::ElementBuildContext<FutureDefinition>;

	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("flowui/tests/phase-d/future-definition");
	static constexpr std::string_view debugName = "FutureDefinition";

	static void buildElement(BuildContext&) {}
};

static_assert(FlowUi::FlowElement<CurrentDefinition>);
static_assert(FlowUi::FlowElement<FutureDefinition>);
static_assert(std::same_as<FlowUi::ParametersOf<CurrentDefinition>, CurrentParameters>);
static_assert(std::same_as<FlowUi::StateOf<CurrentDefinition>, CurrentState>);
static_assert(std::same_as<FlowUi::ResourcesOf<CurrentDefinition>, AppResources>);
static_assert(std::same_as<FlowUi::ParametersOf<FutureDefinition>, CurrentParameters>);
static_assert(std::same_as<FlowUi::StateOf<FutureDefinition>, CurrentState>);
static_assert(std::same_as<FlowUi::ResourcesOf<FutureDefinition>, AppResources>);
static_assert(FlowUi::HasState<CurrentDefinition>);
static_assert(FlowUi::HasResources<CurrentDefinition>);
static_assert(FlowUi::detail::element::elementDescriptor<CurrentDefinition>.stateSize == sizeof(CurrentState));
static_assert(FlowUi::detail::element::elementDescriptor<FutureDefinition>.resourcesAlignment == alignof(AppResources));

void testRegistrationIsIdempotent() {
	FlowUi::detail::manager_storage::ElementDefinitionRegistry registry;
	const auto& first = registry.ensureDefinition(FlowUi::detail::element::elementDescriptor<CurrentDefinition>);
	const auto& second = registry.ensureDefinition(FlowUi::detail::element::elementDescriptor<CurrentDefinition>);

	FLOWUI_CHECK(&first == &second);
	FLOWUI_CHECK(registry.size() == 1);
	FLOWUI_CHECK(first.descriptor.definitionId == kSharedDefinitionId);
}

void testIncompatibleDefinitionIdCollisionIsRejected() {
	FlowUi::detail::manager_storage::ElementDefinitionRegistry registry;
	(void)registry.ensureDefinition(FlowUi::detail::element::elementDescriptor<CurrentDefinition>);

	try {
		(void)registry.ensureDefinition(FlowUi::detail::element::elementDescriptor<CollidingDefinition>);
	} catch (const std::logic_error& error) {
		const std::string_view message = error.what();
		FLOWUI_CHECK(message.find("already registered") != std::string_view::npos);
		FLOWUI_CHECK(message.find(std::to_string(kSharedDefinitionId.value)) != std::string_view::npos);
		FLOWUI_CHECK(registry.size() == 1);
		return;
	}
	throw FlowUi::test::CheckFailure("incompatible definition metadata was accepted");
}

void testImmutableTypeOperationsSupportNonMovablePayloads() {
	CurrentState::constructions = 0;
	CurrentState::destructions = 0;
	AppResources::constructions = 0;
	AppResources::destructions = 0;

	alignas(CurrentState) std::byte stateMemory[sizeof(CurrentState)];
	const auto& stateOperations =
		FlowUi::detail::element::elementDescriptor<CurrentDefinition>.stateOperations;
	stateOperations.defaultConstruct(stateMemory);
	FLOWUI_CHECK(CurrentState::constructions == 1);
	stateOperations.destroy(stateMemory);
	FLOWUI_CHECK(CurrentState::destructions == 1);

	FlowUi::App app;
	alignas(AppResources) std::byte resourceMemory[sizeof(AppResources)];
	const auto& resourceOperations =
		FlowUi::detail::element::elementDescriptor<FutureDefinition>.resourceOperations;
	resourceOperations.constructWithApp(resourceMemory, app);
	FLOWUI_CHECK(AppResources::constructions == 1);
	resourceOperations.destroy(resourceMemory);
	FLOWUI_CHECK(AppResources::destructions == 1);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("definition registration is idempotent", testRegistrationIsIdempotent);
	runner.run("incompatible definition id collision is rejected", testIncompatibleDefinitionIdCollisionIsRejected);
	runner.run("immutable type operations support non-movable payloads", testImmutableTypeOperationsSupportNonMovablePayloads);
	return runner.finish();
}
