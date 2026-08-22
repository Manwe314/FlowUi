#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"

#include <iostream>
#include <stdexcept>

#include <FlowUi/Flow.hpp>
#include "internal/ActionManagerAccess.hpp"
#include "internal/StorageSystem/FlowStorageSystem.hpp"

namespace {

constexpr auto kIncrement = FlowUi::AppAction("tests.actions.increment");
constexpr auto kCompute = FlowUi::AppAction("tests.actions.compute");
constexpr auto kReentrant = FlowUi::AppAction("tests.actions.reentrant_unbind");
constexpr auto kThrow = FlowUi::AppAction("tests.actions.throw");
constexpr auto kFailure = FlowUi::AppAction("tests.actions.failure_injection");

class ActionStore {
public:
	explicit ActionStore(FlowUi::test::HeadlessVulkanFixture& vulkan)
		: storage_(vulkan.context()) {
		FlowUi::detail::storage::StorageConfig config{};
		config.initialPersistentCpuBytes = 4096;
		config.expectedPersistentRecords = 32;
		storage_.initialize(config);
		FlowUi::detail::action::ActionManagerAccess::init(manager_, app_, storage_);
	}

	~ActionStore() {
		FlowUi::detail::action::ActionManagerAccess::destroy(manager_);
		storage_.shutdown();
	}

	FlowUi::ActionManager& manager() noexcept { return manager_; }
	FlowUi::detail::storage::FlowStorageSystem& storage() noexcept { return storage_; }

private:
	FlowUi::App app_;
	FlowUi::detail::storage::FlowStorageSystem storage_;
	FlowUi::ActionManager manager_;
};

void increment(int* value) { ++*value; }
int add(int* left, int* right) { return *left + *right; }
void unbindSelf(FlowUi::AppActions* actions, FlowUi::AppActionID* id, int* count) {
	++*count;
	(void)actions->unbind(*id);
}
void throwAction() { throw std::runtime_error("action failure"); }

void testBindSelectInvokeAndAvailability(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	ActionStore store(vulkan);
	auto& actions = store.manager().appActions();
	int value = 0;
	const auto boundResult = actions.bind(kIncrement, &increment, &value);
	FLOWUI_CHECK(boundResult);
	const auto bound = boundResult.value();
	FLOWUI_CHECK(actions.isBound(kIncrement));
	FLOWUI_CHECK(actions.select(kIncrement).id == bound.id);
	FLOWUI_CHECK(store.manager().invoke(FlowUi::ActionCall{bound}) ==
		FlowUi::ActionInvocationStatus::Invoked);
	FLOWUI_CHECK(value == 1);

	FLOWUI_CHECK(actions.setAvailability(kIncrement, {.enabled = false}));
	FLOWUI_CHECK(!store.manager().availability(FlowUi::ActionCall{bound}).enabled);
	FLOWUI_CHECK(actions.invoke(bound) == FlowUi::ActionInvocationStatus::Disabled);
	FLOWUI_CHECK(value == 1);
	FLOWUI_CHECK(actions.setAvailability(kIncrement, {.enabled = true}));
	FLOWUI_CHECK(actions.invoke(bound) == FlowUi::ActionInvocationStatus::Invoked);
	FLOWUI_CHECK(value == 2);
	FLOWUI_CHECK(actions.unbind(kIncrement));
	FLOWUI_CHECK(actions.invoke(bound) == FlowUi::ActionInvocationStatus::Unbound);
	FLOWUI_CHECK(!actions.unbind(kIncrement));
}

void testResultsReplacementAndFailures(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	ActionStore store(vulkan);
	auto& actions = store.manager().appActions();
	int left = 20;
	int right = 22;
	const auto selectedResult = actions.bind(
		{.id = kCompute, .debugName = "Compute answer"}, &add, &left, &right);
	FLOWUI_CHECK(selectedResult);
	const auto selected = selectedResult.value();
	const auto result = actions.invokeFor<int>(selected);
	FLOWUI_CHECK(result);
	FLOWUI_CHECK(result.value() == 42);
	const auto mismatch = actions.invokeFor<float>(selected);
	FLOWUI_CHECK(!mismatch);
	FLOWUI_CHECK(mismatch.error() == FlowUi::ActionInvokeError::ResultTypeMismatch);
	const auto duplicate = actions.bind(kCompute, &add, &left, &right);
	FLOWUI_CHECK(!duplicate);
	FLOWUI_CHECK(duplicate.error().code == FlowUi::ErrorCode::ActionAlreadyBound);

	left = 1;
	right = 2;
	const auto reboundResult = actions.rebind(
		{.id = kCompute, .debugName = "Compute replacement"}, &add, &left, &right);
	FLOWUI_CHECK(reboundResult);
	const auto rebound = reboundResult.value();
	FLOWUI_CHECK(rebound.id == selected.id);
	FLOWUI_CHECK(actions.invokeFor<int>(selected).value() == 3);
	FLOWUI_CHECK(actions.invoke(selected) == FlowUi::ActionInvocationStatus::Invoked);
}

void testReentrantUnbindAndExceptions(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	ActionStore store(vulkan);
	auto& actions = store.manager().appActions();
	int count = 0;
	auto* actionSurface = &actions;
	FlowUi::AppActionID id = kReentrant;
	const auto selfResult = actions.bind(kReentrant, &unbindSelf, actionSurface, &id, &count);
	FLOWUI_CHECK(selfResult);
	const auto self = selfResult.value();
	FLOWUI_CHECK(actions.invoke(self) == FlowUi::ActionInvocationStatus::Invoked);
	FLOWUI_CHECK(count == 1);
	FLOWUI_CHECK(!actions.isBound(self));
	FLOWUI_CHECK(actions.invoke(self) == FlowUi::ActionInvocationStatus::Unbound);

	const auto throwingResult = actions.bind(kThrow, &throwAction);
	FLOWUI_CHECK(throwingResult);
	const auto throwing = throwingResult.value();
	FLOWUI_CHECK_THROWS(actions.invoke(throwing));
	FLOWUI_CHECK(actions.isBound(throwing));
}

void testStorageFailureRollsBack(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	ActionStore store(vulkan);
	auto& actions = store.manager().appActions();
	int value = 0;
	for (uint32_t checkpoint = 1; checkpoint <= 4; ++checkpoint) {
		store.storage().setRecordFailureCountdown(checkpoint);
		const auto failed = actions.bind(kFailure, &increment, &value);
		FLOWUI_CHECK(!failed);
		FLOWUI_CHECK(!actions.isBound(kFailure));
		FLOWUI_CHECK(store.storage().resourceStats(
			FlowUi::detail::storage::ResourceKind::AppActionBinding).live == 0);
	}
	store.storage().setRecordFailureCountdown(0);
	const auto callResult = actions.bind(kFailure, &increment, &value);
	FLOWUI_CHECK(callResult);
	const auto call = callResult.value();
	FLOWUI_CHECK(actions.invoke(call) == FlowUi::ActionInvocationStatus::Invoked);
	FLOWUI_CHECK(value == 1);
}

} // namespace

int main() {
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		FlowUi::test::Runner runner;
		runner.run("app actions bind, select, invoke, disable, and unbind", [&] {
			testBindSelectInvokeAndAvailability(vulkan);
		});
		runner.run("typed results, explicit replacement, and mismatch reporting", [&] {
			testResultsReplacementAndFailures(vulkan);
		});
		runner.run("active bindings survive reentrant unbind and exceptions", [&] {
			testReentrantUnbindAndExceptions(vulkan);
		});
		runner.run("StorageSystem publication failures roll back action records", [&] {
			testStorageFailureRollsBack(vulkan);
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
