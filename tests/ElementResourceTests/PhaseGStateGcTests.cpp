#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"

#include <cstdint>
#include <iostream>
#include <limits>

#include "internal/ManagerStorage/ElementStorageController.hpp"
#include "internal/StorageSystem/FlowStorageSystem.hpp"
#include "managers/structs/ElementManagerStructs.hpp"

namespace {

using FlowUi::detail::manager_storage::ElementStorageController;
using FlowUi::detail::storage::FlowStorageSystem;

constexpr FlowUi::WindowId kWindow = 71;

struct Parameters {};

struct State {
	inline static int liveCount = 0;
	inline static int constructionCount = 0;
	inline static int destructionCount = 0;

	int value = 0;

	State() noexcept {
		++liveCount;
		++constructionCount;
	}

	~State() noexcept {
		--liveCount;
		++destructionCount;
	}

	static void resetCounts() noexcept {
		liveCount = 0;
		constructionCount = 0;
		destructionCount = 0;
	}
};

struct StatefulDefinition {
	using Parameters = ::Parameters;
	using State = ::State;
	using BuildContext = FlowUi::ElementBuildContext<StatefulDefinition>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("flowui/tests/phase-g/stateful");
	static void buildElement(BuildContext&) {}
};

struct WindowLifetimeDefinition {
	using Parameters = ::Parameters;
	using State = ::State;
	using BuildContext = FlowUi::ElementBuildContext<WindowLifetimeDefinition>;
	static constexpr FlowUi::FlowDefinitionId definitionId =
		FLOW_DEF_ID("flowui/tests/phase-g/window-lifetime");
	static constexpr FlowUi::ElementStatePolicy statePolicy =
		FlowUi::ElementStatePolicy::windowLifetime();
	static void buildElement(BuildContext&) {}
};

inline constexpr FlowUi::ElementStatePolicy kDefaultTransientPolicy =
	FlowUi::detail::element::statePolicy<StatefulDefinition>();
inline constexpr FlowUi::ElementStatePolicy kWindowLifetimePolicy =
	FlowUi::detail::element::statePolicy<WindowLifetimeDefinition>();

static_assert(
	kDefaultTransientPolicy == FlowUi::ElementStatePolicy::transient(5));
static_assert(
	kDefaultTransientPolicy.graceFrames ==
		FlowUi::DefaultElementStateGraceFrames);
static_assert(
	kWindowLifetimePolicy.retention ==
		FlowUi::ElementStateRetention::WindowLifetime);

class StateStore {
public:
	explicit StateStore(FlowUi::test::HeadlessVulkanFixture& vulkan)
		: storage_(vulkan.context()), controller_(storage_) {
		FlowUi::detail::storage::StorageConfig config{};
		config.initialPersistentCpuBytes = 4096;
		config.expectedPersistentRecords = 32;
		config.expectedWindows = 1;
		storage_.initialize(config);

		FlowUi::detail::storage::WindowStorageDesc window{};
		window.framesInFlight = 1;
		window.workerCount = 1;
		window.initialTextureBindings = 1;
		window.maxTextureBindings = 4;
		storage_.registerWindow(kWindow, window);
		controller_.registerWindow(kWindow);
	}

	~StateStore() {
		controller_.destroyWindow(kWindow);
		controller_.shutdown();
		storage_.unregisterWindow(kWindow, 0);
		storage_.shutdown();
	}

	StateStore(const StateStore&) = delete;
	StateStore& operator=(const StateStore&) = delete;

	State& touch(
		FlowUi::FlowElementId flowId,
		uint64_t epoch,
		FlowUi::ElementStatePolicy policy) {
		controller_.beginWindowFrame(kWindow, epoch);
		auto state = controller_.resolveOrCreateStateForInvocation(
			kWindow,
			flowId,
			FlowUi::detail::element::elementDescriptor<StatefulDefinition>,
			policy);
		controller_.endStateInvocation(kWindow);
		controller_.commitWindowFrame(kWindow, epoch);
		return *static_cast<State*>(state.payload);
	}

	void commitAbsent(uint64_t epoch) {
		controller_.beginWindowFrame(kWindow, epoch);
		controller_.commitWindowFrame(kWindow, epoch);
	}

	void begin(uint64_t epoch) {
		controller_.beginWindowFrame(kWindow, epoch);
	}

	State& resolve(
		FlowUi::FlowElementId flowId,
		FlowUi::ElementStatePolicy policy) {
		auto state = controller_.resolveOrCreateStateForInvocation(
			kWindow,
			flowId,
			FlowUi::detail::element::elementDescriptor<StatefulDefinition>,
			policy);
		controller_.endStateInvocation(kWindow);
		return *static_cast<State*>(state.payload);
	}

	void commit(uint64_t epoch) { controller_.commitWindowFrame(kWindow, epoch); }
	void cancel(uint64_t epoch) { controller_.cancelWindowFrame(kWindow, epoch); }

	[[nodiscard]] const State* read(FlowUi::FlowElementId flowId) {
		return static_cast<const State*>(controller_.readState(
			kWindow, flowId, FlowUi::detail::element::elementDescriptor<StatefulDefinition>));
	}

	bool erase(FlowUi::FlowElementId flowId) {
		return controller_.eraseState(
			kWindow, flowId, FlowUi::detail::element::elementDescriptor<StatefulDefinition>);
	}

	[[nodiscard]] size_t collect() {
		return controller_.collectEligibleStates(
			kWindow, std::numeric_limits<size_t>::max());
	}

private:
	FlowStorageSystem storage_;
	ElementStorageController controller_;
};

void testDefaultTransientExpiry(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	State::resetCounts();
	StateStore store(vulkan);
	const FlowUi::FlowElementId flowId = FLOW_ID("phase-g/default-transient");

	store.touch(flowId, 1, kDefaultTransientPolicy).value = 42;
	FLOWUI_CHECK(State::liveCount == 1);

	for (uint64_t epoch = 2; epoch <= 6; ++epoch) {
		store.commitAbsent(epoch);
		FLOWUI_CHECK(store.read(flowId) != nullptr);
		FLOWUI_CHECK(store.read(flowId)->value == 42);
	}

	store.commitAbsent(7);
	FLOWUI_CHECK(store.read(flowId) == nullptr);
	FLOWUI_CHECK(State::liveCount == 0);
	FLOWUI_CHECK(State::destructionCount == 1);
}

void testCanceledFrameDoesNotAgeOrLeakState(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	State::resetCounts();
	StateStore store(vulkan);
	const FlowUi::FlowElementId existing = FLOW_ID("phase-g/cancel-existing");
	const FlowUi::FlowElementId created = FLOW_ID("phase-g/cancel-created");

	store.touch(existing, 1, FlowUi::ElementStatePolicy::transient(0)).value = 7;
	store.begin(2);
	store.resolve(created, kDefaultTransientPolicy).value = 9;
	store.cancel(2);

	FLOWUI_CHECK(store.read(existing) != nullptr);
	FLOWUI_CHECK(store.read(existing)->value == 7);
	FLOWUI_CHECK(store.read(created) == nullptr);
	FLOWUI_CHECK(State::liveCount == 1);

	store.commitAbsent(3);
	FLOWUI_CHECK(store.read(existing) == nullptr);
}

void testWindowLifetimeAndExplicitErasure(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	State::resetCounts();
	StateStore store(vulkan);
	const FlowUi::FlowElementId flowId = FLOW_ID("phase-g/window-lifetime");

	store.touch(flowId, 1, kWindowLifetimePolicy).value = 83;
	for (uint64_t epoch = 2; epoch <= 12; ++epoch) store.commitAbsent(epoch);

	FLOWUI_CHECK(store.collect() == 0);
	FLOWUI_CHECK(store.read(flowId) != nullptr);
	FLOWUI_CHECK(store.read(flowId)->value == 83);
	FLOWUI_CHECK(store.erase(flowId));
	FLOWUI_CHECK(store.read(flowId) == nullptr);
	FLOWUI_CHECK(State::liveCount == 0);
}

void testPolicyChangesApplyAtCommit(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	State::resetCounts();
	StateStore store(vulkan);
	const FlowUi::FlowElementId flowId = FLOW_ID("phase-g/policy-transition");

	store.touch(flowId, 1, FlowUi::ElementStatePolicy::transient(0));
	store.touch(flowId, 2, FlowUi::ElementStatePolicy::windowLifetime());
	for (uint64_t epoch = 3; epoch <= 8; ++epoch) store.commitAbsent(epoch);
	FLOWUI_CHECK(store.read(flowId) != nullptr);

	store.touch(flowId, 9, FlowUi::ElementStatePolicy::transient(0));
	store.commitAbsent(10);
	FLOWUI_CHECK(store.read(flowId) == nullptr);
	FLOWUI_CHECK(State::liveCount == 0);
}

void testQueuedEraseCommitsAndCancelDiscardsIt(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	State::resetCounts();
	StateStore store(vulkan);
	const FlowUi::FlowElementId flowId = FLOW_ID("phase-g/queued-erase");
	const auto policy = FlowUi::ElementStatePolicy::windowLifetime();

	store.touch(flowId, 1, policy);
	store.begin(2);
	FLOWUI_CHECK(store.erase(flowId));
	store.cancel(2);
	FLOWUI_CHECK(store.read(flowId) != nullptr);

	store.begin(3);
	FLOWUI_CHECK(store.erase(flowId));
	store.commit(3);
	FLOWUI_CHECK(store.read(flowId) == nullptr);
	FLOWUI_CHECK(State::liveCount == 0);
}

void testWindowDestructionReleasesEveryPolicy(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	State::resetCounts();
	{
		StateStore store(vulkan);
		store.touch(
			FLOW_ID("phase-g/destroy-transient"),
			1,
			FlowUi::ElementStatePolicy::transient());
		store.touch(
			FLOW_ID("phase-g/destroy-retained"),
			2,
			FlowUi::ElementStatePolicy::windowLifetime());
		FLOWUI_CHECK(State::liveCount == 2);
	}
	FLOWUI_CHECK(State::liveCount == 0);
	FLOWUI_CHECK(State::destructionCount == 2);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		runner.run("default transient state expires after its grace", [&] {
			testDefaultTransientExpiry(vulkan);
		});
		runner.run("canceled frames do not age state or retain new state", [&] {
			testCanceledFrameDoesNotAgeOrLeakState(vulkan);
		});
		runner.run("window-lifetime state requires erase or window destruction", [&] {
			testWindowLifetimeAndExplicitErasure(vulkan);
		});
		runner.run("retention policy changes apply at successful commit", [&] {
			testPolicyChangesApplyAtCommit(vulkan);
		});
		runner.run("queued erasure follows commit and cancel semantics", [&] {
			testQueuedEraseCommitsAndCancelDiscardsIt(vulkan);
		});
		runner.run("window destruction releases transient and retained state", [&] {
			testWindowDestructionReleasesEveryPolicy(vulkan);
		});
		return runner.finish();
	} catch (const FlowUi::test::VulkanUnavailable& error) {
#ifdef FLOWUI_TEST_REQUIRE_VULKAN_DEVICE
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
#else
		std::cout << "SKIP: " << error.what() << '\n';
		return 77;
#endif
	}
}
