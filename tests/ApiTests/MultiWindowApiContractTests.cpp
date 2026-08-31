#include "TestHarness.hpp"

#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "FlowUi/Flow.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "devSystems/devInterface/Elements/DevContent.hpp"
#include "devSystems/devInterface/Elements/DevContentAreas.hpp"
#include "devSystems/devInterface/Elements/DevCatalogueContent.hpp"
#include "devSystems/devInterface/Elements/DevChangesContent.hpp"
#include "devSystems/devInterface/Elements/DevDiagnosticsContent.hpp"
#include "devSystems/devInterface/Elements/DevInspectContent.hpp"
#include "devSystems/devInterface/Elements/DevMemoryContent.hpp"
#include "devSystems/devInterface/Elements/DevPerformanceContent.hpp"
#include "devSystems/devInterface/Elements/DevContentHeader.hpp"
#include "devSystems/devInterface/Elements/DevInterface.hpp"
#include "devSystems/devInterface/Elements/DevInterfaceFooter.hpp"
#include "devSystems/devInterface/Elements/DevInterfaceHeader.hpp"
#endif

namespace {

struct WindowElementParams {
	std::string label;
};

struct WindowElement {
	using Parameters = WindowElementParams;
	using BuildContext = FlowUi::ElementBuildContext<WindowElement>;
	using InteractionContext = FlowUi::ElementInteractionContext<WindowElement>;
	static constexpr FlowUi::FlowDefinitionID definitionId =
		FlowUi::DefinitionID("tests.multi_window.element");

	static void buildElement(BuildContext&) {}
};

inline constexpr WindowElement kWindowElement{};

void instantiateDeclarativeOverloads(FlowUi::App& app) {
	(void)app.createWindow(
		FlowUi::WindowConfigOverrides{}, kWindowElement,
		WindowElementParams{.label = "value"});
	(void)app.createWindow(
		FlowUi::WindowConfigOverrides{}, kWindowElement,
		std::function<void(FlowUi::ElementBuilder<WindowElement>&, FlowUi::WindowId)>{
			[](FlowUi::ElementBuilder<WindowElement>& builder, FlowUi::WindowId) {
				builder.setParameters(WindowElementParams{.label = "configured"});
			}});
	(void)app.createWindowWithState(
		FlowUi::WindowConfigOverrides{}, kWindowElement,
		std::make_shared<WindowElementParams>());
}

void testPublicSurface() {
	using FlowUi::App;
	using FlowUi::Result;
	using FlowUi::Status;
	using FlowUi::WindowConfigOverrides;
	using FlowUi::WindowId;

	static_assert(std::is_same_v<
		decltype(static_cast<Result<WindowId> (App::*)()>(&App::createWindowLikeMain)),
		Result<WindowId> (App::*)()>);
	static_assert(std::is_same_v<
		decltype(static_cast<Result<WindowId> (App::*)(const WindowConfigOverrides&)>(&App::createWindow)),
		Result<WindowId> (App::*)(const WindowConfigOverrides&)>);
	static_assert(std::is_same_v<
		decltype(static_cast<Result<WindowId> (App::*)(std::string_view, int, int)>(&App::createWindow)),
		Result<WindowId> (App::*)(std::string_view, int, int)>);
	static_assert(std::is_same_v<
		decltype(static_cast<Status (App::*)(WindowId, FlowUi::UiBuildCallback, FlowUi::ManagedWindowFlags)>(
			&App::setWindowUiCallback)),
		Status (App::*)(WindowId, FlowUi::UiBuildCallback, FlowUi::ManagedWindowFlags)>);
	static_assert(std::is_same_v<decltype(&App::dispatchManagedWindows), Status (App::*)()>);

	WindowConfigOverrides overrides{};
	FLOWUI_CHECK(!overrides.width.has_value());
	FLOWUI_CHECK(!overrides.title.has_value());
	FLOWUI_CHECK(FlowUi::describeError(FlowUi::ErrorCode::UiBuildCallbackFailed).impact ==
		FlowUi::ErrorImpact::FrameCanceled);
#if FLOW_UI_DEV_MODE
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevInterface>);
	static_assert(FlowUi::HasState<
		FlowUi::devSystems::interface_elements::DevInterface>);
	static_assert(std::same_as<
		FlowUi::StateOf<FlowUi::devSystems::interface_elements::DevInterface>,
		FlowUi::devSystems::DevInterfaceState>);
	static_assert(std::same_as<
		FlowUi::ParametersOf<FlowUi::devSystems::interface_elements::DevInterface>,
		FlowUi::devSystems::interface_elements::DevInterfaceParameters>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevInterfaceHeader>);
	static_assert(FlowUi::ConstructibleFlowElement<
		FlowUi::devSystems::interface_elements::DevContent>);
	static_assert(!FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevContent>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevSelectorArea>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevWorkbenchArea>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevInspectorArea>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevInspectSelector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevInspectWorkbench>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevInspectInspector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevPerformanceSelector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevPerformanceWorkbench>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevPerformanceInspector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevMemorySelector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevMemoryWorkbench>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevMemoryInspector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevDiagnosticsSelector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevDiagnosticsWorkbench>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevDiagnosticsInspector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevChangesSelector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevChangesWorkbench>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevChangesInspector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevCatalogueSelector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevCatalogueWorkbench>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevCatalogueInspector>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevContentHeader>);
	static_assert(std::same_as<
		FlowUi::ParametersOf<FlowUi::devSystems::interface_elements::DevContentHeader>,
		FlowUi::devSystems::interface_elements::DevContentHeaderParameters>);
	static_assert(std::same_as<
		FlowUi::ParametersOf<FlowUi::devSystems::interface_elements::DevInterfaceHeader>,
		FlowUi::devSystems::interface_elements::DevInterfaceHeaderParameters>);
	static_assert(FlowUi::DrawableFlowElement<
		FlowUi::devSystems::interface_elements::DevInterfaceFooter>);
	static_assert(FlowUi::HasResources<
		FlowUi::devSystems::interface_elements::DevInterfaceFooter>);
	static_assert(std::same_as<
		FlowUi::ParametersOf<FlowUi::devSystems::interface_elements::DevInterfaceFooter>,
		FlowUi::devSystems::interface_elements::DevInterfaceFooterParameters>);
	static_assert(std::same_as<
		decltype(std::declval<const App&>().devWindowSnapshot()),
		std::vector<FlowUi::DevWindowInfo>>);
	static_assert(std::same_as<
		decltype(std::declval<FlowUi::AppConfig&>().dev.monitoring.timing),
		FlowUi::devSystems::DevTimingConfig>);
	static_assert(std::same_as<
		decltype(std::declval<FlowUi::AppConfig&>().dev.monitoring.memory),
		FlowUi::devSystems::DevMemoryConfig>);
	static_assert(std::same_as<
		decltype(std::declval<FlowUi::AppConfig&>().dev.monitoring.errors),
		FlowUi::devSystems::DevErrorConfig>);
	static_assert(std::same_as<
		decltype(std::declval<FlowUi::AppConfig&>().dev.tooling.treeCapture),
		FlowUi::devSystems::tooling::DevTreeCaptureConfig>);
#endif
	(void)&instantiateDeclarativeOverloads;
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("Phase 1 multi-window convenience API contract", testPublicSurface);
	return runner.finish();
}
