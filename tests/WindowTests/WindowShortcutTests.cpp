#include "FlowUi/Public/Flow.hpp"
#include "TestHarness.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticAction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteractionRuntime.hpp"
#include "devSystems/devAgenticWorkflow/AgenticService.hpp"
#endif

namespace {
using namespace FlowUi;

[[nodiscard]] AppConfig shortcut_configuration() {
	AppConfig configuration{};
	configuration.window = {.width = 720, .height = 460, .title = "FlowUi shortcuts main"};
	configuration.vk.enableValidation = false;
	configuration.ui.shortcuts.window.enabled = true;
	configuration.ui.shortcuts.window.close_window.chord = ShortcutChord{.key = GLFW_KEY_F8};
	configuration.ui.shortcuts.window.minimize.chord = ShortcutChord{.key = GLFW_KEY_F9};
	configuration.ui.shortcuts.window.maximize_restore = {
		.chord = ShortcutChord{.key = GLFW_KEY_F10, .ctrl = true, .shift = true}, .enabled = true};
	configuration.ui.shortcuts.window.toggle_fullscreen.enabled = true;
	configuration.ui.shortcuts.window.toggle_fullscreen.chord = ShortcutChord{.key = GLFW_KEY_F11};
	return configuration;
}

void empty_frame(App& application, WindowId window_id) {
	FLOWUI_CHECK(application.beginFrame(window_id));
	FLOWUI_CHECK(application.endFrame(window_id));
	FLOWUI_CHECK(application.drawFrame(window_id));
}

void inject_key(App& application, WindowId window_id, int key, int action, int modifiers = 0) {
	auto* native_window = static_cast<GLFWwindow*>(application.nativeWindowHandle(window_id));
	const GLFWkeyfun callback = glfwSetKeyCallback(native_window, nullptr);
	FLOWUI_CHECK(callback);
	glfwSetKeyCallback(native_window, callback);
	callback(native_window, key, 0, action, modifiers);
}

#if FLOW_UI_DEV_MODE
[[nodiscard]] std::size_t global_shortcut_count(const App& application, WindowId window_id) {
	std::size_t count = 0;
	FLOWUI_CHECK(application.ui(window_id).shortcuts().visitDevShortcuts(
		&count, [](void* context, const ShortcutManager::DevShortcutView& shortcut) {
			if (shortcut.scope == ShortcutScope::Global)
				++*static_cast<std::size_t*>(context);
			return true;
		}));
	return count;
}
#endif

void test_window_shortcut_dispatch() {
	FlowUiError deferred_failure{};
	auto configuration = shortcut_configuration();
	configuration.errors.observer.mode = ErrorReportingMode::SinkOnly;
	configuration.errors.observer.sink = {
		.userData = &deferred_failure,
		.callback = [](void* context, const ErrorEventView& event) noexcept {
			if (event.error.code == ErrorCode::WindowOperationUnsupported)
				*static_cast<FlowUiError*>(context) = event.error;
		}};
	auto original_application = makeApplication(configuration);
	const auto secondary_result = original_application.createWindow(
		WindowConfigOverrides{.width = 640, .height = 400, .title = "FlowUi shortcuts secondary"});
	FLOWUI_CHECK(secondary_result);
	const WindowId secondary_window = *secondary_result;
	App application = std::move(original_application);
	FLOWUI_CHECK(application.pollEvents());
	empty_frame(application, application.mainWindowId());
	empty_frame(application, secondary_window);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(global_shortcut_count(application, application.mainWindowId()) == 4);
	FLOWUI_CHECK(global_shortcut_count(application, secondary_window) == 4);
#endif
	// Modifier matching is exact, and moving App preserves callback ownership.
	inject_key(application, secondary_window, GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, GLFW_MOD_SHIFT);
	inject_key(application, secondary_window, GLFW_KEY_F8, GLFW_PRESS, GLFW_MOD_SHIFT);
	empty_frame(application, secondary_window);
	FLOWUI_CHECK(!application.shouldClose(secondary_window));
	inject_key(application, secondary_window, GLFW_KEY_F8, GLFW_RELEASE);
	inject_key(application, secondary_window, GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE);
	empty_frame(application, secondary_window);
	inject_key(application, secondary_window, GLFW_KEY_F8, GLFW_PRESS);
	empty_frame(application, secondary_window);
	FLOWUI_CHECK(application.shouldClose(secondary_window));
	FLOWUI_CHECK(!application.shouldClose());
	application.setShouldClose(secondary_window, 0);
	empty_frame(application, secondary_window);
	FLOWUI_CHECK(!application.shouldClose(secondary_window));
	inject_key(application, secondary_window, GLFW_KEY_F8, GLFW_RELEASE);
	empty_frame(application, secondary_window);

	auto* secondary_native =
		static_cast<GLFWwindow*>(application.nativeWindowHandle(secondary_window));
	inject_key(application, secondary_window, GLFW_KEY_F11, GLFW_PRESS);
	empty_frame(application, secondary_window);
	FLOWUI_CHECK(!glfwGetWindowMonitor(secondary_native));
	FLOWUI_CHECK(application.pollEvents());
	FLOWUI_CHECK(glfwGetWindowMonitor(secondary_native));
	FLOWUI_CHECK(application.beginFrame(secondary_window));
	FLOWUI_CHECK(application.toggle_window_maximized(secondary_window));
	FLOWUI_CHECK(application.endFrame(secondary_window));
	FLOWUI_CHECK(application.drawFrame(secondary_window));
	FLOWUI_CHECK(application.pollEvents());
	FLOWUI_CHECK(deferred_failure.code == ErrorCode::WindowOperationUnsupported);
	FLOWUI_CHECK(deferred_failure.subject == secondary_window);
	FLOWUI_CHECK(deferred_failure.subjectKind() == ErrorSubjectKind::Window);
	FLOWUI_CHECK(glfwGetWindowMonitor(secondary_native));
	empty_frame(application, secondary_window);
	FLOWUI_CHECK(application.pollEvents());
	FLOWUI_CHECK(glfwGetWindowMonitor(secondary_native));
	inject_key(application, secondary_window, GLFW_KEY_F11, GLFW_RELEASE);
	empty_frame(application, secondary_window);
	inject_key(application, secondary_window, GLFW_KEY_F11, GLFW_PRESS);
	empty_frame(application, secondary_window);
	FLOWUI_CHECK(application.pollEvents());
	FLOWUI_CHECK(!glfwGetWindowMonitor(secondary_native));
	FLOWUI_CHECK(!glfwGetWindowMonitor(static_cast<GLFWwindow*>(application.nativeWindowHandle())));

	FLOWUI_CHECK(application.beginFrame());
	FLOWUI_CHECK(application.toggle_window_fullscreen(secondary_window));
	FLOWUI_CHECK(application.endFrame());
	FLOWUI_CHECK(application.drawFrame());
	FLOWUI_CHECK(application.destroyWindow(secondary_window));
	FLOWUI_CHECK(application.pollEvents());
	FLOWUI_CHECK(!application.hasWindow(secondary_window));
	FLOWUI_CHECK(!application.shouldClose());
	const auto stale_capabilities = application.window_operation_capabilities(secondary_window);
	FLOWUI_CHECK(!stale_capabilities);
	FLOWUI_CHECK(stale_capabilities.error().code == ErrorCode::InvalidWindowId);
	FLOWUI_CHECK(!application.request_window_close(secondary_window));
	FLOWUI_CHECK(!application.minimize_window(secondary_window));
	FLOWUI_CHECK(!application.toggle_window_maximized(secondary_window));
	FLOWUI_CHECK(!application.toggle_window_fullscreen(secondary_window));
}

void test_window_configuration_validation() {
	{
		auto configuration = shortcut_configuration();
		configuration.ui.shortcuts.window.enabled = false;
		configuration.ui.shortcuts.window.close_window.chord = ShortcutChord{.key = -1};
		auto application = makeApplication(configuration);
#if FLOW_UI_DEV_MODE
		FLOWUI_CHECK(global_shortcut_count(application, application.mainWindowId()) == 0);
#endif
	}
	{
		AppConfig configuration{};
		configuration.vk.enableValidation = false;
		configuration.ui.shortcuts.window.enabled = true;
		configuration.ui.shortcuts.window.maximize_restore.chord = ShortcutChord{.key = -1};
		auto application = makeApplication(configuration);
#if FLOW_UI_DEV_MODE && !defined(__APPLE__)
		FLOWUI_CHECK(global_shortcut_count(application, application.mainWindowId()) == 0);
#endif
	}
	for (int invalid_case = 0; invalid_case < 3; ++invalid_case) {
		auto configuration = shortcut_configuration();
		if (invalid_case == 0)
			configuration.ui.shortcuts.window.minimize.chord = ShortcutChord{.key = GLFW_KEY_F8};
		else if (invalid_case == 1)
			configuration.ui.shortcuts.window.close_window.chord = ShortcutChord{.key = -1};
		else
			configuration.ui.shortcuts.window.close_window.chord =
				ShortcutChord{.key = GLFW_KEY_F8, .trigger = ShortcutTrigger::Down};
		bool rejected = false;
		try {
			auto application = makeApplication(configuration);
		} catch (const FlowUiException& exception) {
			rejected = exception.error().code == ErrorCode::ShortcutInvalid;
		}
		FLOWUI_CHECK(rejected);
	}
}

#if FLOW_UI_AGENTIC_WORKFLOW && COMPILE_FSELI
namespace agentic = FlowUi::devSystems::agentic;

struct WindowFixture {
	std::string editable = "alpha beta gamma";
	std::string read_only = "locked text";
	std::uint64_t history_request_count = 0;
	WindowId window_id = InvalidWindowId;
};

struct Fixture {
	std::array<WindowFixture, 2> windows{};
	std::uint64_t manual_history_count = 0;
	bool reset_requested = false;
};

void draw_fixture_window(App& application, WindowFixture& fixture) {
	FLOWUI_CHECK(application.beginFrame(fixture.window_id));
	auto& ui = application.ui(fixture.window_id);
	constexpr auto observe_history =
		UiAction("tests.window-shortcuts.observe-history",
				 [](std::uint64_t& request_count) { ++request_count; });
	const ActionCall history_notification{
		ui.actions().uiActions().make(observe_history, fixture.history_request_count)};
	Clay_ElementDeclaration panel{};
	panel.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
	panel.layout.padding = CLAY_PADDING_ALL(24);
	panel.layout.childGap = 16;
	panel.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	panel.backgroundColor = Flow_Color("#141C28");
	CLAY(ui.toClaySID("window-shortcuts/panel"), panel) {
		Clay_TextElementConfig text_style{};
		text_style.fontSize = 18;
		text_style.textColor = Flow_Color("#EDF4FF");
		CLAY_TEXT(ui.toClayString(fixture.window_id == application.mainWindowId()
									  ? "Shortcut verification · Main"
									  : "Shortcut verification · Secondary"),
				  CLAY_TEXT_CONFIG(text_style));
		text_style.fontSize = 14;
		CLAY_TEXT(CLAY_STRING("F8 close · F9 minimize · Ctrl+Shift+F10 maximize · F11 fullscreen"),
				  CLAY_TEXT_CONFIG(text_style));
		CLAY_TEXT(CLAY_STRING("Editable field"), CLAY_TEXT_CONFIG(text_style));
		ui.createElement(FSEL::kTextInput, "editable")
			.setParameters(FSEL::TextInputParameters{
				.actions = {.onUndoRequested = history_notification,
							.onRedoRequested = history_notification},
				.value = &fixture.editable,
				.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(42)}})
			.draw();
		CLAY_TEXT(CLAY_STRING("Read-only field"), CLAY_TEXT_CONFIG(text_style));
		ui.createElement(FSEL::kTextInput, "read_only")
			.setParameters(FSEL::TextInputParameters{
				.actions = {.onUndoRequested = history_notification,
							.onRedoRequested = history_notification},
				.value = &fixture.read_only,
				.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(42)},
				.readOnly = true})
			.draw();
	}
	FLOWUI_CHECK(application.endFrame(fixture.window_id));
	FLOWUI_CHECK(application.drawFrame(fixture.window_id));
}

[[nodiscard]] agentic::Json window_state(App& application, const WindowFixture& fixture) {
	auto* native_window =
		static_cast<GLFWwindow*>(application.nativeWindowHandle(fixture.window_id));
	int position_x = 0;
	int position_y = 0;
	int width = 0;
	int height = 0;
	glfwGetWindowPos(native_window, &position_x, &position_y);
	glfwGetWindowSize(native_window, &width, &height);
	return agentic::Json::make_object(
		{{"window_id", agentic::decimal(fixture.window_id)},
		 {"close_requested", application.shouldClose(fixture.window_id)},
		 {"fullscreen", glfwGetWindowMonitor(native_window) != nullptr},
		 {"maximized", glfwGetWindowAttrib(native_window, GLFW_MAXIMIZED) == GLFW_TRUE},
		 {"minimized", glfwGetWindowAttrib(native_window, GLFW_ICONIFIED) == GLFW_TRUE},
		 {"x", agentic::Json(static_cast<double>(position_x))},
		 {"y", agentic::Json(static_cast<double>(position_y))},
		 {"width", agentic::Json::number(width)},
		 {"height", agentic::Json::number(height)},
		 {"editable", fixture.editable},
		 {"read_only", fixture.read_only}});
}

void register_fixture_actions(App& application, Fixture& fixture) {
	const auto runtime = application.agentic_workflow().interaction_runtime();
	FLOWUI_CHECK(runtime);
	for (const bool reset : {false, true}) {
		agentic::AgenticActionDeclaration declaration{};
		declaration.action_id = reset ? "window_shortcuts.reset" : "window_shortcuts.state";
		declaration.description =
			reset ? "Clear native close flags and restore minimized test windows."
				  : "Report actual native state and text values for both shortcut test windows.";
		declaration.mutating = reset;
		declaration.asynchronous = reset;
		declaration.argument_schema =
			agentic::Json::make_object({{"type", "object"}, {"additionalProperties", false}});
		declaration.result_schema = agentic::Json::make_object({{"type", "object"}});
		agentic::AgenticActionAdapter adapter{};
		adapter.decode_arguments = [](const agentic::Json& arguments) -> Result<agentic::Json> {
			return arguments;
		};
		adapter.invoke = [&fixture,
						  reset](App& app, const agentic::AgenticActionInvocation&,
								 const agentic::Json&) -> Result<agentic::AgenticActionReceipt> {
			if (reset)
				fixture.reset_requested = true;
			agentic::AgenticActionReceipt receipt{};
			receipt.result = agentic::Json::make_object(
				{{"main", window_state(app, fixture.windows[0])},
				 {"secondary", window_state(app, fixture.windows[1])},
				 {"manual_history_count", agentic::decimal(fixture.manual_history_count)},
				 {"history_request_count",
				  agentic::decimal(fixture.windows[0].history_request_count +
								   fixture.windows[1].history_request_count)}});
			receipt.completed = !reset;
			receipt.completion_succeeded = !reset;
			return receipt;
		};
		if (reset) {
			adapter.poll_completion = [&fixture](App&,
												 const agentic::AgenticActionReceipt& previous)
				-> Result<agentic::AgenticActionReceipt> {
				auto receipt = previous;
				receipt.completed = !fixture.reset_requested;
				receipt.completion_succeeded = receipt.completed;
				return receipt;
			};
		}
		FLOWUI_CHECK(
			(*runtime)->actions().register_action(std::move(declaration), std::move(adapter)));
	}
}

int run_agentic_fixture(const std::filesystem::path& store_path) {
	auto configuration = shortcut_configuration();
	configuration.agentic_workflow.enabled = true;
	configuration.agentic_workflow.local_attachment_enabled = true;
	configuration.agentic_workflow.input_control_enabled = true;
	configuration.agentic_workflow.action_control_enabled = true;
	configuration.agentic_workflow.visual_capture_enabled = true;
	configuration.agentic_workflow.application_id = "flowui.window-shortcuts.fixture";
	configuration.agentic_workflow.store_path = store_path;
	auto application = makeApplication(configuration);
#if FLOW_UI_DEV_MODE
	application.devTooling().bakePipeline().setManifestPath(store_path / "fixture.flowchanges");
#endif
	FLOWUI_CHECK(
		application.themes().registerTheme<FSEL::FSELTheme>("default", FSEL::FSELTheme{}, true));
	Fixture fixture{};
	fixture.windows[0].window_id = application.mainWindowId();
	const auto secondary = application.createWindow(
		WindowConfigOverrides{.width = 640, .height = 400, .title = "FlowUi shortcuts secondary"});
	FLOWUI_CHECK(secondary);
	fixture.windows[1].window_id = *secondary;
	FLOWUI_CHECK(application.ui().shortcuts().registerShortcut(
		ShortcutChord{.key = GLFW_KEY_Z, .ctrl = true}, ShortcutScope::Global, 0,
		[&fixture](ShortcutContext&) {
			++fixture.manual_history_count;
			return true;
		}));
	register_fixture_actions(application, fixture);
	while (true) {
		FLOWUI_CHECK(application.pollEvents());
		if (fixture.reset_requested) {
			for (const auto& window_fixture : fixture.windows) {
				application.setShouldClose(window_fixture.window_id, 0);
				auto* native_window = static_cast<GLFWwindow*>(
					application.nativeWindowHandle(window_fixture.window_id));
				if (glfwGetWindowAttrib(native_window, GLFW_ICONIFIED) == GLFW_TRUE)
					glfwRestoreWindow(native_window);
			}
			fixture.reset_requested = false;
		}
		for (auto& window_fixture : fixture.windows)
			draw_fixture_window(application, window_fixture);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}
#endif
} // namespace

int main(int argument_count, char* arguments[]) {
	try {
#if FLOW_UI_AGENTIC_WORKFLOW && COMPILE_FSELI
		if (argument_count == 3 && std::string_view(arguments[1]) == "--agentic-store")
			return run_agentic_fixture(arguments[2]);
#endif
		if (argument_count != 1)
			return 2;
		FlowUi::test::Runner runner;
		runner.run(
			"window shortcut routing, exact modifiers, Press, App move and deferred lifetime",
			test_window_shortcut_dispatch);
		runner.run("window shortcut disabling, inherited chords and configuration validation",
				   test_window_configuration_validation);
		return runner.finish();
	} catch (const FlowUi::FlowUiException& exception) {
		std::cerr << "Window shortcut integration: " << exception.what() << '\n';
		return exception.error().code == FlowUi::ErrorCode::PlatformInitializationFailed ||
					   exception.error().code == FlowUi::ErrorCode::WindowCreationFailed
				   ? 77
				   : 1;
	} catch (const std::exception& exception) {
		std::cerr << "Window shortcut integration: " << exception.what() << '\n';
		return 1;
	}
}
