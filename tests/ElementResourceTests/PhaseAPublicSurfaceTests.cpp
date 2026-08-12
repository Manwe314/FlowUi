#include "ElementResourceContractTestDriver.hpp"
#include "TestHarness.hpp"

#include "managers/ElementManager.hpp"
#include "managers/structs/ElementManagerStructs.hpp"

// Prospective Phase A compile-time API tests. The included headers are created
// in later phases, so this file remains outside tests/CMakeLists.txt for now.

namespace {

using namespace FlowUi::test::element_resource;

template <typename Policy>
concept HasAppLifetimePolicyFactory = requires {
	Policy::appLifetime(FlowUi::FlowElementID{.value = 1});
};

template <typename Manager>
concept HasWithAppState = requires(Manager& manager) {
	manager.withAppState(
		PrimaryElement{},
		FlowUi::FlowElementID{.value = 1},
		[](PrimaryState&) {});
};

static_assert(
	!HasAppLifetimePolicyFactory<FlowUi::ElementStatePolicy>,
	"Element state retention must not expose app-lifetime state.");
static_assert(
	!HasWithAppState<FlowUi::ElementManager>,
	"ElementManager must not expose an app-wide/shared element-state partition.");

void testStateRetentionSurfaceContainsOnlyWindowOwnedPolicies() {
	constexpr FlowUi::ElementStatePolicy transient =
		FlowUi::ElementStatePolicy::transient(3);
	constexpr FlowUi::ElementStatePolicy retained =
		FlowUi::ElementStatePolicy::windowLifetime();

	FLOWUI_CHECK(transient.retention == FlowUi::ElementStateRetention::Transient);
	FLOWUI_CHECK(transient.graceFrames == 3);
	FLOWUI_CHECK(retained.retention == FlowUi::ElementStateRetention::WindowLifetime);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run(
		"state retention exposes only window-owned policies",
		testStateRetentionSurfaceContainsOnlyWindowOwnedPolicies);
	return runner.finish();
}
