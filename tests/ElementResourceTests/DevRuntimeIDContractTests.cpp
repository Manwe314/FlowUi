#include "TestHarness.hpp"

#include <cstdint>
#include <string_view>
#include <type_traits>

#include "devMode/devRuntime.hpp"
#include "devMode/registry.hpp"
#include "managers/UiManager.hpp"
#include "internal/ManagerStorage/UiManagerState.hpp"

namespace {

static_assert(std::is_trivially_copyable_v<FlowUi::devMode::DefinitionFieldKey>);
static_assert(std::is_trivially_copyable_v<FlowUi::devMode::InstanceScopeKey>);
static_assert(std::is_trivially_copyable_v<FlowUi::devMode::InstanceFieldKey>);
static_assert(sizeof(FlowUi::devMode::InstanceScopeKey) == sizeof(uint64_t) * 2u);
static_assert(sizeof(FlowUi::devMode::InstanceFieldKey) == sizeof(uint64_t) * 3u);

struct RegistryElement {
	using BuildContext = FlowUi::ElementBuildContext<RegistryElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests/dev-runtime/registry-element");
	static void buildElement(BuildContext&) {}
};

constexpr FlowUi::FlowDefinitionID kDefinition =
	FlowUi::DefinitionID("tests/dev-runtime/definition");

FlowUi::FlowElementID makeId(uint64_t value, std::string_view debugPath) {
	return FlowUi::FlowElementID{
		.value = value,
		.debugName = debugPath,
	};
}

void testInstanceKeysIgnoreDebugPaths() {
	FlowUi::devMode::DevRuntime runtime;
	const FlowUi::FlowElementID first = makeId(0x1234u, "panel/first-name");
	const FlowUi::FlowElementID renamed = makeId(0x1234u, "panel/renamed");

	runtime.setInstanceParamOverride(kDefinition, first, 17u, int64_t{42});
	const FlowUi::devMode::DevValue* found =
		runtime.findInstanceParamOverride(kDefinition, renamed, 17u);
	FLOWUI_CHECK(found != nullptr);
	if (!found) return;
	FLOWUI_CHECK(std::get<int64_t>(*found) == 42);
	FLOWUI_CHECK(runtime.instanceParamOverrides().size() == 1u);

	runtime.captureLastSeenParamField(kDefinition, first, 23u, int64_t{7});
	FLOWUI_CHECK(runtime.findLastSeenParams(kDefinition, renamed) != nullptr);
	FLOWUI_CHECK(runtime.lastSeenParamsByInstance().size() == 1u);
}

void testCaptureCopiesDebugPathForDisplayOnly() {
	FlowUi::devMode::DevRuntime runtime;
	runtime.beginElementTreeCapture();
	std::string debugPath = "settings/color/slider";
	const FlowUi::FlowElementID id = makeId(0xabcdu, debugPath);
	const std::size_t index = runtime.beginCapturedFlowElement(
		kDefinition, 99u, id, "Color slider", "RegistryElement");
	FLOWUI_CHECK(index != FlowUi::devMode::DevRuntime::kInvalidCaptureIndex);
	if (index == FlowUi::devMode::DevRuntime::kInvalidCaptureIndex) return;
	debugPath.assign("overwritten");

	const auto& node = runtime.elementTreePlaceholder().flatNodes[index];
	FLOWUI_CHECK(node.definitionId == kDefinition);
	FLOWUI_CHECK(node.instanceId.value == id.value);
	FLOWUI_CHECK(node.debugPath == "settings/color/slider");
}

void testRegistryUsesStrongDefinitionIds() {
	auto& registry = FlowUi::devMode::DevRegistry::instance();
	registry.registerElement<RegistryElement>("Registry element");
	const auto* descriptor = registry.findElementByDefinitionId(
		RegistryElement::definitionId);
	FLOWUI_CHECK(descriptor != nullptr);
	if (!descriptor) return;
	FLOWUI_CHECK(descriptor->definitionId == RegistryElement::definitionId);
}

void testDiagnosticTrackersUseNumericIdentity() {
	using namespace FlowUi::detail;
	manager_storage::FlowRootIdTrackerForDev roots;
	roots.beginFrame();
	const element::ElementInstanceKey first{.value = 0x111u};
	const element::ElementInstanceKey second{.value = 0x222u};
	const manager_storage::FlowRootClaimSourceForDev source{
		.definitionId = kDefinition,
		.debugPath = "first/path",
	};
	FLOWUI_CHECK(roots.claim(first, source) == nullptr);
	const auto* duplicate = roots.claim(first, manager_storage::FlowRootClaimSourceForDev{
		.definitionId = kDefinition,
		.debugPath = "renamed/path",
	});
	FLOWUI_CHECK(duplicate != nullptr);
	if (!duplicate) return;
	FLOWUI_CHECK(duplicate->instanceId == first);

	manager_storage::ClayBridgeIdTrackerForDev clay;
	clay.beginFrame();
	FLOWUI_CHECK(clay.claim(first, 77u, source) == nullptr);
	const auto* collision = clay.claim(second, 77u, source);
	FLOWUI_CHECK(collision != nullptr);
	if (!collision) return;
	FLOWUI_CHECK(collision->first.instanceId == first);
	FLOWUI_CHECK(collision->duplicate.instanceId == second);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("dev runtime numeric instance keys", testInstanceKeysIgnoreDebugPaths);
	runner.run("dev capture display path copy", testCaptureCopiesDebugPathForDisplayOnly);
	runner.run("dev registry strong definition IDs", testRegistryUsesStrongDefinitionIds);
	runner.run("dev numeric diagnostic trackers", testDiagnosticTrackersUseNumericIdentity);
	return runner.finish();
}
