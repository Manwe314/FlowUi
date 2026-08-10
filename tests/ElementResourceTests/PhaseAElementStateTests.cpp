#include "ElementResourceContractTestDriver.hpp"
#include "TestHarness.hpp"

#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// Prospective Phase A contract tests. Do not add this file to tests/CMakeLists.txt
// until ElementStorageController and its thin test driver have been implemented.

namespace {

using namespace FlowUi::test::element_resource;

constexpr FlowUi::WindowId kFirstWindow = 101;
constexpr FlowUi::WindowId kSecondWindow = 202;
constexpr FlowUi::FlowElementId kSharedFlowId = FLOW_ID("tests/shared/root");

template <typename Function>
void checkThrowsContaining(Function&& function, std::string_view expectedText) {
	try {
		std::forward<Function>(function)();
	} catch (const std::exception& error) {
		FLOWUI_CHECK(std::string_view(error.what()).find(expectedText) != std::string_view::npos);
		return;
	}
	throw FlowUi::test::CheckFailure("expected an exception containing: " + std::string(expectedText));
}

void testSameFlowIdIsIndependentAcrossWindows() {
	PrimaryState::resetCounts();
	auto driver = makeElementControllerContractDriver();
	driver->registerWindow(kFirstWindow);
	driver->registerWindow(kSecondWindow);

	driver->beginFrame(kFirstWindow, 1);
	PrimaryState& first = driver->resolvePrimary(kFirstWindow, kSharedFlowId);
	first.value = 11;
	driver->commitFrame(kFirstWindow);

	driver->beginFrame(kSecondWindow, 1);
	PrimaryState& second = driver->resolvePrimary(kSecondWindow, kSharedFlowId);
	second.value = 22;
	driver->commitFrame(kSecondWindow);

	FLOWUI_CHECK(&first != &second);
	FLOWUI_CHECK(first.value == 11);
	FLOWUI_CHECK(second.value == 22);
	FLOWUI_CHECK(PrimaryState::constructions == 2);
	FLOWUI_CHECK(driver->liveStateCount(kFirstWindow) == 1);
	FLOWUI_CHECK(driver->liveStateCount(kSecondWindow) == 1);

	driver->destroyWindow(kFirstWindow);
	FLOWUI_CHECK(PrimaryState::destructions == 1);
	FLOWUI_CHECK(driver->findPrimary(kSecondWindow, kSharedFlowId) == &second);
	FLOWUI_CHECK(second.value == 22);
	driver->destroyWindow(kSecondWindow);
	FLOWUI_CHECK(PrimaryState::destructions == 2);
}

void testStateAddressIsOnlyWindowAndFlowId() {
	PrimaryState::resetCounts();
	AlternateState::resetCounts();
	auto driver = makeElementControllerContractDriver();
	driver->registerWindow(kFirstWindow);

	driver->beginFrame(kFirstWindow, 1);
	PrimaryState& primary = driver->resolvePrimary(kFirstWindow, kSharedFlowId);
	primary.value = 77;
	driver->commitFrame(kFirstWindow);

	driver->beginFrame(kFirstWindow, 2);
	checkThrowsContaining(
		[&] { static_cast<void>(driver->resolveAlternate(kFirstWindow, kSharedFlowId)); },
		"PrimaryElement");
	driver->cancelFrame(kFirstWindow);

	// If definition/type were part of key equality, the second request could
	// create a parallel record. The correct behavior is one record plus a type
	// diagnostic before AlternateState is constructed or cast.
	FLOWUI_CHECK(driver->liveStateCount(kFirstWindow) == 1);
	FLOWUI_CHECK(AlternateState::constructions == 0);
	FLOWUI_CHECK(driver->findPrimary(kFirstWindow, kSharedFlowId) == &primary);
	FLOWUI_CHECK(primary.value == 77);
}

void testStoredMetadataIsValidatedBeforeTypedAccess() {
	PrimaryState::resetCounts();
	AlternateState::resetCounts();
	auto driver = makeElementControllerContractDriver();
	driver->registerWindow(kFirstWindow);

	driver->beginFrame(kFirstWindow, 1);
	PrimaryState& primary = driver->resolvePrimary(kFirstWindow, kSharedFlowId);
	primary.value = 42;
	driver->commitFrame(kFirstWindow);

	const StateRecordMetadataView metadata = driver->metadata(kFirstWindow, kSharedFlowId);
	FLOWUI_CHECK(metadata.flowId == kSharedFlowId);
	FLOWUI_CHECK(metadata.definitionId == PrimaryElement::definitionId);
	FLOWUI_CHECK(metadata.stateTypeHash != 0);

	checkThrowsContaining(
		[&] { static_cast<void>(driver->findAlternate(kFirstWindow, kSharedFlowId)); },
		"AlternateElement");
	FLOWUI_CHECK(AlternateState::constructions == 0);
	FLOWUI_CHECK(primary.value == 42);
}

void testDefinitionChangeAtSameFlowIdReportsFocusedDiagnostic() {
	auto driver = makeElementControllerContractDriver();
	driver->registerWindow(kFirstWindow);

	driver->beginFrame(kFirstWindow, 1);
	static_cast<void>(driver->resolvePrimary(kFirstWindow, kSharedFlowId));
	driver->commitFrame(kFirstWindow);

	driver->beginFrame(kFirstWindow, 2);
	try {
		static_cast<void>(driver->resolveAlternate(kFirstWindow, kSharedFlowId));
	} catch (const std::exception& error) {
		const std::string_view message = error.what();
		const std::string flowIdText = std::to_string(kSharedFlowId);
		FLOWUI_CHECK(message.find("PrimaryElement") != std::string_view::npos);
		FLOWUI_CHECK(message.find("AlternateElement") != std::string_view::npos);
		FLOWUI_CHECK(message.find(flowIdText) != std::string_view::npos);
		driver->cancelFrame(kFirstWindow);
		return;
	}
	driver->cancelFrame(kFirstWindow);
	throw FlowUi::test::CheckFailure("definition replacement did not report a diagnostic");
}

void testWindowDestructionDestroysEveryRetentionClass() {
	PrimaryState::resetCounts();
	auto driver = makeElementControllerContractDriver();
	driver->registerWindow(kFirstWindow);

	driver->beginFrame(kFirstWindow, 1);
	static_cast<void>(driver->resolvePrimary(
		kFirstWindow,
		FLOW_ID("tests/transient"),
		TestStatePolicy::transient(100)));
	static_cast<void>(driver->resolvePrimary(
		kFirstWindow,
		FLOW_ID("tests/window-lifetime"),
		TestStatePolicy::windowLifetime()));
	driver->commitFrame(kFirstWindow);

	FLOWUI_CHECK(PrimaryState::constructions == 2);
	FLOWUI_CHECK(PrimaryState::destructions == 0);
	driver->destroyWindow(kFirstWindow);
	FLOWUI_CHECK(PrimaryState::destructions == 2);
}

void testStateAddressRemainsStableAcrossRegistryMutationAndRecursion() {
	PrimaryState::resetCounts();
	auto driver = makeElementControllerContractDriver();
	driver->registerWindow(kFirstWindow);

	driver->beginFrame(kFirstWindow, 1);
	PrimaryState& root = driver->resolvePrimary(kFirstWindow, FLOW_ID("tests/recursive/root"));
	PrimaryState* const rootAddress = &root;
	root.value = 5;

	for (uint64_t index = 0; index < 2048; ++index) {
		PrimaryState& nested = driver->resolvePrimary(
			kFirstWindow,
			FlowUi::createIndexedFlowId(FLOW_ID("tests/recursive/child"), index));
		nested.value = static_cast<int>(index);
	}

	for (uint64_t index = 0; index < 1024; ++index) {
		FLOWUI_CHECK(driver->erasePrimary(
			kFirstWindow,
			FlowUi::createIndexedFlowId(FLOW_ID("tests/recursive/child"), index)));
	}

	FLOWUI_CHECK(driver->findPrimary(kFirstWindow, FLOW_ID("tests/recursive/root")) == rootAddress);
	FLOWUI_CHECK(rootAddress->value == 5);
	driver->commitFrame(kFirstWindow);
}

void testCanceledFramePreservesPreexistingState() {
	auto driver = makeElementControllerContractDriver();
	driver->registerWindow(kFirstWindow);

	const FlowUi::FlowElementId existingId = FLOW_ID("tests/cancel/existing");
	driver->beginFrame(kFirstWindow, 1);
	PrimaryState& existing = driver->resolvePrimary(
		kFirstWindow,
		existingId,
		TestStatePolicy::transient(0));
	existing.value = 91;
	driver->commitFrame(kFirstWindow);

	driver->beginFrame(kFirstWindow, 2);
	// The pre-existing state is intentionally not touched in this failed frame.
	driver->cancelFrame(kFirstWindow);
	driver->collectAllEligible(kFirstWindow);

	PrimaryState* const found = driver->findPrimary(kFirstWindow, existingId);
	FLOWUI_CHECK(found == &existing);
	FLOWUI_CHECK(found->value == 91);
}

void testCanceledFrameRollsBackNewState() {
	PrimaryState::resetCounts();
	auto driver = makeElementControllerContractDriver();
	driver->registerWindow(kFirstWindow);

	const FlowUi::FlowElementId canceledId = FLOW_ID("tests/cancel/new");
	driver->beginFrame(kFirstWindow, 1);
	static_cast<void>(driver->resolvePrimary(kFirstWindow, canceledId));
	FLOWUI_CHECK(PrimaryState::constructions == 1);
	driver->cancelFrame(kFirstWindow);

	FLOWUI_CHECK(driver->findPrimary(kFirstWindow, canceledId) == nullptr);
	FLOWUI_CHECK(driver->liveStateCount(kFirstWindow) == 0);
	FLOWUI_CHECK(PrimaryState::destructions == 1);
}

void testResourcesConstructOnceForAppAndNotPerWindow() {
	SharedResources::resetCounts();
	{
		auto driver = makeElementControllerContractDriver();
		driver->registerWindow(kFirstWindow);
		driver->registerWindow(kSecondWindow);

		const SharedResources& fromFirst = driver->resolveSharedResources(kFirstWindow);
		const SharedResources& fromFirstAgain = driver->resolveSharedResources(kFirstWindow);
		const SharedResources& fromSecond = driver->resolveSharedResources(kSecondWindow);

		FLOWUI_CHECK(&fromFirst == &fromFirstAgain);
		FLOWUI_CHECK(&fromFirst == &fromSecond);
		FLOWUI_CHECK(SharedResources::constructions == 1);

		driver->destroyWindow(kFirstWindow);
		driver->destroyWindow(kSecondWindow);
		FLOWUI_CHECK(SharedResources::destructions == 0);
	}
	FLOWUI_CHECK(SharedResources::destructions == 1);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("same Flow ID is independent across windows", testSameFlowIdIsIndependentAcrossWindows);
	runner.run("state address is window plus Flow ID only", testStateAddressIsOnlyWindowAndFlowId);
	runner.run("stored metadata is validated before typed access", testStoredMetadataIsValidatedBeforeTypedAccess);
	runner.run("definition change reports a focused diagnostic", testDefinitionChangeAtSameFlowIdReportsFocusedDiagnostic);
	runner.run("window destruction clears every retention class", testWindowDestructionDestroysEveryRetentionClass);
	runner.run("state addresses survive registry mutation and recursion", testStateAddressRemainsStableAcrossRegistryMutationAndRecursion);
	runner.run("canceled frame preserves pre-existing state", testCanceledFramePreservesPreexistingState);
	runner.run("canceled frame rolls back newly-created state", testCanceledFrameRollsBackNewState);
	runner.run("resources construct once for the app", testResourcesConstructOnceForAppAndNotPerWindow);
	return runner.finish();
}
