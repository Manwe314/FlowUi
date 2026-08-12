#include "ElementResourceContractTestDriver.hpp"
#include "TestHarness.hpp"

// This test executable must only be compiled with FLOW_UI_DEV_MODE=1.
#include "FlowUi/BuildConfig.hpp"

#if !FLOW_UI_DEV_MODE
#error "PhaseADevFlowIdTests must be compiled only in a FlowUi developer-mode build."
#endif

// Phase E contract tests for the dev-only UiManager Flow-ID tracker.

namespace {

using namespace FlowUi::test::element_resource;

constexpr FlowUi::FlowElementID kDuplicateId{
	.value = 0x7e13b8c24690ad5full,
	.debugName = "tests/dev/duplicate",
};

void testDuplicateSameDefinitionWarnsAndContinues() {
	auto driver = makeDevFlowIdContractDriver();
	driver->beginFrame();

	FLOWUI_CHECK(driver->claim(
		kDuplicateId,
		StatelessElement::definitionId,
		"tests/dev/duplicate") == ClaimDisposition::Continue);
	FLOWUI_CHECK(driver->claim(
		kDuplicateId,
		StatelessElement::definitionId,
		"tests/dev/duplicate") == ClaimDisposition::Continue);

	FLOWUI_CHECK(driver->warningCount() == 1);
	const FlowIdCollisionWarning& warning = driver->warning(0);
	FLOWUI_CHECK(warning.elementId == kDuplicateId);
	FLOWUI_CHECK(warning.firstDefinition == StatelessElement::definitionId);
	FLOWUI_CHECK(warning.duplicateDefinition == StatelessElement::definitionId);
	FLOWUI_CHECK(warning.debugPath == "tests/dev/duplicate");
	FLOWUI_CHECK(warning.firstDebugPath == "tests/dev/duplicate");
	FLOWUI_CHECK(!warning.firstFileName.empty());
	FLOWUI_CHECK(!warning.duplicateFileName.empty());
	FLOWUI_CHECK(warning.firstLine != 0);
	FLOWUI_CHECK(warning.duplicateLine != 0);
}

void testDuplicateDifferentDefinitionWarnsAndContinues() {
	auto driver = makeDevFlowIdContractDriver();
	driver->beginFrame();

	FLOWUI_CHECK(driver->claim(
		kDuplicateId,
		StatelessElement::definitionId,
		"tests/dev/duplicate") == ClaimDisposition::Continue);
	FLOWUI_CHECK(driver->claim(
		kDuplicateId,
		AlternateElement::definitionId,
		"tests/dev/duplicate") == ClaimDisposition::Continue);

	FLOWUI_CHECK(driver->warningCount() == 1);
	const FlowIdCollisionWarning& warning = driver->warning(0);
	FLOWUI_CHECK(warning.firstDefinition == StatelessElement::definitionId);
	FLOWUI_CHECK(warning.duplicateDefinition == AlternateElement::definitionId);
}

void testTrackerIsOwnedPerUiManager() {
	auto firstUiManager = makeDevFlowIdContractDriver();
	auto secondUiManager = makeDevFlowIdContractDriver();
	firstUiManager->beginFrame();
	secondUiManager->beginFrame();

	static_cast<void>(firstUiManager->claim(
		kDuplicateId,
		StatelessElement::definitionId,
		"tests/dev/duplicate"));
	static_cast<void>(secondUiManager->claim(
		kDuplicateId,
		StatelessElement::definitionId,
		"tests/dev/duplicate"));

	FLOWUI_CHECK(firstUiManager->warningCount() == 0);
	FLOWUI_CHECK(secondUiManager->warningCount() == 0);
}

void testClaimsResetAtNextFrame() {
	auto driver = makeDevFlowIdContractDriver();
	driver->beginFrame();
	static_cast<void>(driver->claim(
		kDuplicateId,
		StatelessElement::definitionId,
		"tests/dev/duplicate"));

	driver->beginFrame();
	static_cast<void>(driver->claim(
		kDuplicateId,
		StatelessElement::definitionId,
		"tests/dev/duplicate"));
	FLOWUI_CHECK(driver->warningCount() == 0);
}

void testDistinctIdsDoNotWarn() {
	auto driver = makeDevFlowIdContractDriver();
	driver->beginFrame();

	for (uint64_t index = 0; index < 1024; ++index) {
		FLOWUI_CHECK(driver->claim(
			FlowUi::FlowElementID{
				.value = index + 1u,
				.debugName = "tests/dev/distinct",
			},
			StatelessElement::definitionId,
			"tests/dev/distinct") == ClaimDisposition::Continue);
	}

	FLOWUI_CHECK(driver->warningCount() == 0);
}

void testNestedFlowIdsUseTheSameClaimTable() {
	auto driver = makeDevFlowIdContractDriver();
	driver->beginFrame();

	const FlowUi::FlowElementID parent{
		.value = 0x1234u,
		.debugName = "tests/dev/nested",
	};
	const FlowUi::FlowElementID child{
		.value = 0x5678u,
		.debugName = "tests/dev/nested/child",
	};
	static_cast<void>(driver->claim(parent, StatelessElement::definitionId, "tests/dev/nested"));
	static_cast<void>(driver->claim(child, AlternateElement::definitionId, "tests/dev/nested/child"));
	static_cast<void>(driver->claim(child, StatelessElement::definitionId, "tests/dev/nested/child"));

	FLOWUI_CHECK(driver->warningCount() == 1);
	FLOWUI_CHECK(driver->warning(0).elementId == child);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("duplicate ID from same definition warns without rejecting", testDuplicateSameDefinitionWarnsAndContinues);
	runner.run("duplicate ID from different definition warns without rejecting", testDuplicateDifferentDefinitionWarnsAndContinues);
	runner.run("Flow-ID claims are scoped to one UiManager", testTrackerIsOwnedPerUiManager);
	runner.run("Flow-ID claims reset every frame", testClaimsResetAtNextFrame);
	runner.run("distinct Flow IDs do not warn", testDistinctIdsDoNotWarn);
	runner.run("nested Flow IDs use the same claim table", testNestedFlowIdsUseTheSameClaimTable);
	return runner.finish();
}
