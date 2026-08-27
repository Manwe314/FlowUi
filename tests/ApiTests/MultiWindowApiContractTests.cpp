#include "TestHarness.hpp"

#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "FlowUi/Flow.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Elements/DevRoot.hpp"
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
		FlowUi::devSystems::interface_elements::DevRoot>);
#endif
	(void)&instantiateDeclarativeOverloads;
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("Phase 1 multi-window convenience API contract", testPublicSurface);
	return runner.finish();
}
