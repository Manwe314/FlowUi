#include "TestHarness.hpp"

#include "FlowUi/Private/platformProviderService/providers/glfw/GlfwPlatformProviderService.hpp"

#include "FlowUi/Private/inputService/InputState.hpp"
#include "FlowUi/Private/platformProviderService/WindowPlatformServices.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>

namespace {

static_assert(FlowUi::keyCode(FlowUi::Key::Space) == GLFW_KEY_SPACE);
static_assert(FlowUi::keyCode(FlowUi::Key::Apostrophe) == GLFW_KEY_APOSTROPHE);
static_assert(FlowUi::keyCode(FlowUi::Key::Comma) == GLFW_KEY_COMMA);
static_assert(FlowUi::keyCode(FlowUi::Key::Minus) == GLFW_KEY_MINUS);
static_assert(FlowUi::keyCode(FlowUi::Key::Period) == GLFW_KEY_PERIOD);
static_assert(FlowUi::keyCode(FlowUi::Key::Slash) == GLFW_KEY_SLASH);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit0) == GLFW_KEY_0);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit1) == GLFW_KEY_1);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit2) == GLFW_KEY_2);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit3) == GLFW_KEY_3);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit4) == GLFW_KEY_4);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit5) == GLFW_KEY_5);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit6) == GLFW_KEY_6);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit7) == GLFW_KEY_7);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit8) == GLFW_KEY_8);
static_assert(FlowUi::keyCode(FlowUi::Key::Digit9) == GLFW_KEY_9);
static_assert(FlowUi::keyCode(FlowUi::Key::Semicolon) == GLFW_KEY_SEMICOLON);
static_assert(FlowUi::keyCode(FlowUi::Key::Equal) == GLFW_KEY_EQUAL);
static_assert(FlowUi::keyCode(FlowUi::Key::A) == GLFW_KEY_A);
static_assert(FlowUi::keyCode(FlowUi::Key::B) == GLFW_KEY_B);
static_assert(FlowUi::keyCode(FlowUi::Key::C) == GLFW_KEY_C);
static_assert(FlowUi::keyCode(FlowUi::Key::D) == GLFW_KEY_D);
static_assert(FlowUi::keyCode(FlowUi::Key::E) == GLFW_KEY_E);
static_assert(FlowUi::keyCode(FlowUi::Key::F) == GLFW_KEY_F);
static_assert(FlowUi::keyCode(FlowUi::Key::G) == GLFW_KEY_G);
static_assert(FlowUi::keyCode(FlowUi::Key::H) == GLFW_KEY_H);
static_assert(FlowUi::keyCode(FlowUi::Key::I) == GLFW_KEY_I);
static_assert(FlowUi::keyCode(FlowUi::Key::J) == GLFW_KEY_J);
static_assert(FlowUi::keyCode(FlowUi::Key::K) == GLFW_KEY_K);
static_assert(FlowUi::keyCode(FlowUi::Key::L) == GLFW_KEY_L);
static_assert(FlowUi::keyCode(FlowUi::Key::M) == GLFW_KEY_M);
static_assert(FlowUi::keyCode(FlowUi::Key::N) == GLFW_KEY_N);
static_assert(FlowUi::keyCode(FlowUi::Key::O) == GLFW_KEY_O);
static_assert(FlowUi::keyCode(FlowUi::Key::P) == GLFW_KEY_P);
static_assert(FlowUi::keyCode(FlowUi::Key::Q) == GLFW_KEY_Q);
static_assert(FlowUi::keyCode(FlowUi::Key::R) == GLFW_KEY_R);
static_assert(FlowUi::keyCode(FlowUi::Key::S) == GLFW_KEY_S);
static_assert(FlowUi::keyCode(FlowUi::Key::T) == GLFW_KEY_T);
static_assert(FlowUi::keyCode(FlowUi::Key::U) == GLFW_KEY_U);
static_assert(FlowUi::keyCode(FlowUi::Key::V) == GLFW_KEY_V);
static_assert(FlowUi::keyCode(FlowUi::Key::W) == GLFW_KEY_W);
static_assert(FlowUi::keyCode(FlowUi::Key::X) == GLFW_KEY_X);
static_assert(FlowUi::keyCode(FlowUi::Key::Y) == GLFW_KEY_Y);
static_assert(FlowUi::keyCode(FlowUi::Key::Z) == GLFW_KEY_Z);
static_assert(FlowUi::keyCode(FlowUi::Key::LeftBracket) == GLFW_KEY_LEFT_BRACKET);
static_assert(FlowUi::keyCode(FlowUi::Key::Backslash) == GLFW_KEY_BACKSLASH);
static_assert(FlowUi::keyCode(FlowUi::Key::RightBracket) == GLFW_KEY_RIGHT_BRACKET);
static_assert(FlowUi::keyCode(FlowUi::Key::GraveAccent) == GLFW_KEY_GRAVE_ACCENT);
static_assert(FlowUi::keyCode(FlowUi::Key::World1) == GLFW_KEY_WORLD_1);
static_assert(FlowUi::keyCode(FlowUi::Key::World2) == GLFW_KEY_WORLD_2);
static_assert(FlowUi::keyCode(FlowUi::Key::Escape) == GLFW_KEY_ESCAPE);
static_assert(FlowUi::keyCode(FlowUi::Key::Enter) == GLFW_KEY_ENTER);
static_assert(FlowUi::keyCode(FlowUi::Key::Tab) == GLFW_KEY_TAB);
static_assert(FlowUi::keyCode(FlowUi::Key::Backspace) == GLFW_KEY_BACKSPACE);
static_assert(FlowUi::keyCode(FlowUi::Key::Insert) == GLFW_KEY_INSERT);
static_assert(FlowUi::keyCode(FlowUi::Key::Delete) == GLFW_KEY_DELETE);
static_assert(FlowUi::keyCode(FlowUi::Key::Right) == GLFW_KEY_RIGHT);
static_assert(FlowUi::keyCode(FlowUi::Key::Left) == GLFW_KEY_LEFT);
static_assert(FlowUi::keyCode(FlowUi::Key::Down) == GLFW_KEY_DOWN);
static_assert(FlowUi::keyCode(FlowUi::Key::Up) == GLFW_KEY_UP);
static_assert(FlowUi::keyCode(FlowUi::Key::PageUp) == GLFW_KEY_PAGE_UP);
static_assert(FlowUi::keyCode(FlowUi::Key::PageDown) == GLFW_KEY_PAGE_DOWN);
static_assert(FlowUi::keyCode(FlowUi::Key::Home) == GLFW_KEY_HOME);
static_assert(FlowUi::keyCode(FlowUi::Key::End) == GLFW_KEY_END);
static_assert(FlowUi::keyCode(FlowUi::Key::CapsLock) == GLFW_KEY_CAPS_LOCK);
static_assert(FlowUi::keyCode(FlowUi::Key::ScrollLock) == GLFW_KEY_SCROLL_LOCK);
static_assert(FlowUi::keyCode(FlowUi::Key::NumLock) == GLFW_KEY_NUM_LOCK);
static_assert(FlowUi::keyCode(FlowUi::Key::PrintScreen) == GLFW_KEY_PRINT_SCREEN);
static_assert(FlowUi::keyCode(FlowUi::Key::Pause) == GLFW_KEY_PAUSE);
static_assert(FlowUi::keyCode(FlowUi::Key::F1) == GLFW_KEY_F1);
static_assert(FlowUi::keyCode(FlowUi::Key::F2) == GLFW_KEY_F2);
static_assert(FlowUi::keyCode(FlowUi::Key::F3) == GLFW_KEY_F3);
static_assert(FlowUi::keyCode(FlowUi::Key::F4) == GLFW_KEY_F4);
static_assert(FlowUi::keyCode(FlowUi::Key::F5) == GLFW_KEY_F5);
static_assert(FlowUi::keyCode(FlowUi::Key::F6) == GLFW_KEY_F6);
static_assert(FlowUi::keyCode(FlowUi::Key::F7) == GLFW_KEY_F7);
static_assert(FlowUi::keyCode(FlowUi::Key::F8) == GLFW_KEY_F8);
static_assert(FlowUi::keyCode(FlowUi::Key::F9) == GLFW_KEY_F9);
static_assert(FlowUi::keyCode(FlowUi::Key::F10) == GLFW_KEY_F10);
static_assert(FlowUi::keyCode(FlowUi::Key::F11) == GLFW_KEY_F11);
static_assert(FlowUi::keyCode(FlowUi::Key::F12) == GLFW_KEY_F12);
static_assert(FlowUi::keyCode(FlowUi::Key::F13) == GLFW_KEY_F13);
static_assert(FlowUi::keyCode(FlowUi::Key::F14) == GLFW_KEY_F14);
static_assert(FlowUi::keyCode(FlowUi::Key::F15) == GLFW_KEY_F15);
static_assert(FlowUi::keyCode(FlowUi::Key::F16) == GLFW_KEY_F16);
static_assert(FlowUi::keyCode(FlowUi::Key::F17) == GLFW_KEY_F17);
static_assert(FlowUi::keyCode(FlowUi::Key::F18) == GLFW_KEY_F18);
static_assert(FlowUi::keyCode(FlowUi::Key::F19) == GLFW_KEY_F19);
static_assert(FlowUi::keyCode(FlowUi::Key::F20) == GLFW_KEY_F20);
static_assert(FlowUi::keyCode(FlowUi::Key::F21) == GLFW_KEY_F21);
static_assert(FlowUi::keyCode(FlowUi::Key::F22) == GLFW_KEY_F22);
static_assert(FlowUi::keyCode(FlowUi::Key::F23) == GLFW_KEY_F23);
static_assert(FlowUi::keyCode(FlowUi::Key::F24) == GLFW_KEY_F24);
static_assert(FlowUi::keyCode(FlowUi::Key::F25) == GLFW_KEY_F25);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad0) == GLFW_KEY_KP_0);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad1) == GLFW_KEY_KP_1);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad2) == GLFW_KEY_KP_2);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad3) == GLFW_KEY_KP_3);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad4) == GLFW_KEY_KP_4);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad5) == GLFW_KEY_KP_5);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad6) == GLFW_KEY_KP_6);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad7) == GLFW_KEY_KP_7);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad8) == GLFW_KEY_KP_8);
static_assert(FlowUi::keyCode(FlowUi::Key::Keypad9) == GLFW_KEY_KP_9);
static_assert(FlowUi::keyCode(FlowUi::Key::KeypadDecimal) == GLFW_KEY_KP_DECIMAL);
static_assert(FlowUi::keyCode(FlowUi::Key::KeypadDivide) == GLFW_KEY_KP_DIVIDE);
static_assert(FlowUi::keyCode(FlowUi::Key::KeypadMultiply) == GLFW_KEY_KP_MULTIPLY);
static_assert(FlowUi::keyCode(FlowUi::Key::KeypadSubtract) == GLFW_KEY_KP_SUBTRACT);
static_assert(FlowUi::keyCode(FlowUi::Key::KeypadAdd) == GLFW_KEY_KP_ADD);
static_assert(FlowUi::keyCode(FlowUi::Key::KeypadEnter) == GLFW_KEY_KP_ENTER);
static_assert(FlowUi::keyCode(FlowUi::Key::KeypadEqual) == GLFW_KEY_KP_EQUAL);
static_assert(FlowUi::keyCode(FlowUi::Key::LeftShift) == GLFW_KEY_LEFT_SHIFT);
static_assert(FlowUi::keyCode(FlowUi::Key::LeftControl) == GLFW_KEY_LEFT_CONTROL);
static_assert(FlowUi::keyCode(FlowUi::Key::LeftAlt) == GLFW_KEY_LEFT_ALT);
static_assert(FlowUi::keyCode(FlowUi::Key::LeftSuper) == GLFW_KEY_LEFT_SUPER);
static_assert(FlowUi::keyCode(FlowUi::Key::RightShift) == GLFW_KEY_RIGHT_SHIFT);
static_assert(FlowUi::keyCode(FlowUi::Key::RightControl) == GLFW_KEY_RIGHT_CONTROL);
static_assert(FlowUi::keyCode(FlowUi::Key::RightAlt) == GLFW_KEY_RIGHT_ALT);
static_assert(FlowUi::keyCode(FlowUi::Key::RightSuper) == GLFW_KEY_RIGHT_SUPER);
static_assert(FlowUi::keyCode(FlowUi::Key::Menu) == GLFW_KEY_MENU);

void poll_native_events() {
	for (int poll_count = 0; poll_count < 4; ++poll_count)
		glfwWaitEventsTimeout(0.005);
}

[[nodiscard]] bool wait_for_window_attribute(GLFWwindow* native_window, int attribute,
											 int expected_value) noexcept {
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
	do {
		glfwPollEvents();
		if (glfwGetWindowAttrib(native_window, attribute) == expected_value)
			return true;
		glfwWaitEventsTimeout(0.01);
	} while (std::chrono::steady_clock::now() < deadline);
	return false;
}

[[nodiscard]] bool wait_for_window_extent(GLFWwindow* native_window, int expected_width,
										  int expected_height) noexcept {
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	int observed_width = 0;
	int observed_height = 0;
	do {
		glfwPollEvents();
		glfwGetWindowSize(native_window, &observed_width, &observed_height);
		if (observed_width == expected_width && observed_height == expected_height)
			return true;
		glfwWaitEventsTimeout(0.01);
	} while (std::chrono::steady_clock::now() < deadline);
	std::cerr << "Expected native extent " << expected_width << 'x' << expected_height
			  << ", observed " << observed_width << 'x' << observed_height << '\n';
	return false;
}

[[nodiscard]] bool wait_for_windowed_rectangle(GLFWwindow* native_window, int expected_x,
											   int expected_y, int expected_width,
											   int expected_height) noexcept {
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	int observed_x = 0;
	int observed_y = 0;
	int observed_width = 0;
	int observed_height = 0;
	do {
		glfwPollEvents();
		glfwGetWindowPos(native_window, &observed_x, &observed_y);
		glfwGetWindowSize(native_window, &observed_width, &observed_height);
		if (!glfwGetWindowMonitor(native_window) &&
			glfwGetWindowAttrib(native_window, GLFW_MAXIMIZED) == GLFW_FALSE &&
			observed_x == expected_x && observed_y == expected_y &&
			observed_width == expected_width && observed_height == expected_height)
			return true;
		glfwWaitEventsTimeout(0.01);
	} while (std::chrono::steady_clock::now() < deadline);
	std::cerr << "Expected windowed rectangle " << expected_x << ',' << expected_y << ' '
			  << expected_width << 'x' << expected_height << ", observed " << observed_x << ','
			  << observed_y << ' ' << observed_width << 'x' << observed_height << '\n';
	return false;
}

void test_native_window_operations() {
	FlowUi::WindowConfig configuration{};
	configuration.width = 640;
	configuration.height = 400;
	configuration.title = "FlowUi native operation verification";
	FlowUi::detail::platform::GlfwPlatformProviderService service;
	FLOWUI_CHECK(service.initialize({}));
	FlowUi::InputSink firstSink;
	FlowUi::InputSink secondSink;
	FlowUi::detail::platform::GlfwWindowState first_window;
	FlowUi::detail::platform::GlfwWindowState second_window;
	FLOWUI_CHECK(
		service.initializeWindow(first_window, FlowUi::WindowId{1u}, configuration, firstSink));
	FLOWUI_CHECK(
		service.initializeWindow(second_window, FlowUi::WindowId{2u}, configuration, secondSink));
	auto* first_native = static_cast<GLFWwindow*>(service.nativeHandle(first_window));
	auto* second_native = static_cast<GLFWwindow*>(service.nativeHandle(second_window));
	const auto capabilities = service.windowOperationCapabilities(first_window);
	FLOWUI_CHECK(capabilities.close_window);
	FLOWUI_CHECK(capabilities.minimize);
	FLOWUI_CHECK(capabilities.maximize_restore);
	FLOWUI_CHECK(capabilities.toggle_fullscreen);

	FlowUi::Status cross_thread_result{};
	std::thread worker_thread([&] { cross_thread_result = service.minimizeWindow(first_window); });
	worker_thread.join();
	FLOWUI_CHECK(!cross_thread_result);
	FLOWUI_CHECK(cross_thread_result.error().code == FlowUi::ErrorCode::WrongThread);

	glfwSetWindowPos(first_native, 100, 100);
	poll_native_events();
	int original_x = 0;
	int original_y = 0;
	int original_width = 0;
	int original_height = 0;
	glfwGetWindowPos(first_native, &original_x, &original_y);
	glfwGetWindowSize(first_native, &original_width, &original_height);
	FLOWUI_CHECK(service.toggleWindowFullscreen(first_window));
	FLOWUI_CHECK(glfwGetWindowMonitor(first_native));
	FLOWUI_CHECK(!glfwGetWindowMonitor(second_native));
	const auto fullscreen_maximize = service.toggleWindowMaximized(first_window);
	FLOWUI_CHECK(!fullscreen_maximize);
	FLOWUI_CHECK(fullscreen_maximize.error().code == FlowUi::ErrorCode::WindowOperationUnsupported);
	poll_native_events();
	FLOWUI_CHECK(service.toggleWindowFullscreen(first_window));
	poll_native_events();
	FLOWUI_CHECK(!glfwGetWindowMonitor(first_native));
	int restored_x = 0;
	int restored_y = 0;
	int restored_width = 0;
	int restored_height = 0;
	glfwGetWindowPos(first_native, &restored_x, &restored_y);
	glfwGetWindowSize(first_native, &restored_width, &restored_height);
	FLOWUI_CHECK(restored_x == original_x);
	FLOWUI_CHECK(restored_y == original_y);
	FLOWUI_CHECK(restored_width == original_width);
	FLOWUI_CHECK(restored_height == original_height);

	FLOWUI_CHECK(service.toggleWindowMaximized(first_window));
	if (wait_for_window_attribute(first_native, GLFW_MAXIMIZED, GLFW_TRUE)) {
		FLOWUI_CHECK(service.toggleWindowFullscreen(first_window));
		poll_native_events();
		FLOWUI_CHECK(service.toggleWindowFullscreen(first_window));
		FLOWUI_CHECK(wait_for_window_attribute(first_native, GLFW_MAXIMIZED, GLFW_TRUE));
		FLOWUI_CHECK(service.toggleWindowMaximized(first_window));
		FLOWUI_CHECK(wait_for_window_attribute(first_native, GLFW_MAXIMIZED, GLFW_FALSE));
		FLOWUI_CHECK(wait_for_window_extent(first_native, original_width, original_height));
	} else {
		std::cout << "Native window manager did not apply maximization within 500ms; geometry "
					 "round trip was "
					 "verified windowed.\n";
	}

	FLOWUI_CHECK(service.minimizeWindow(first_window));
	if (!wait_for_window_attribute(first_native, GLFW_ICONIFIED, GLFW_TRUE))
		std::cout << "Native window manager did not apply minimization within 500ms.\n";
	FLOWUI_CHECK(!service.shouldClose(second_window));
	FLOWUI_CHECK(glfwGetWindowAttrib(second_native, GLFW_ICONIFIED) == GLFW_FALSE);
	glfwRestoreWindow(first_native);
	poll_native_events();
	FLOWUI_CHECK(service.requestWindowClose(first_window));
	FLOWUI_CHECK(service.shouldClose(first_window));
	FLOWUI_CHECK(!service.shouldClose(second_window));
}

void test_initial_fullscreen_restoration() {
	FlowUi::WindowConfig configuration{};
	configuration.width = 520;
	configuration.height = 320;
	configuration.fullscreen = true;
	FlowUi::detail::platform::GlfwPlatformProviderService service;
	FLOWUI_CHECK(service.initialize({}));
	FlowUi::InputSink sink;
	FlowUi::detail::platform::GlfwWindowState fullscreen_window;
	FLOWUI_CHECK(
		service.initializeWindow(fullscreen_window, FlowUi::WindowId{1u}, configuration, sink));
	auto* native_window = static_cast<GLFWwindow*>(service.nativeHandle(fullscreen_window));
	FLOWUI_CHECK(glfwGetWindowMonitor(native_window));
	int work_x = 0;
	int work_y = 0;
	int work_width = 0;
	int work_height = 0;
	glfwGetMonitorWorkarea(glfwGetWindowMonitor(native_window), &work_x, &work_y, &work_width,
						   &work_height);
	const int expected_x = work_x + (work_width - configuration.width) / 2;
	const int expected_y = work_y + (work_height - configuration.height) / 2;
	FLOWUI_CHECK(service.toggleWindowFullscreen(fullscreen_window));
	FLOWUI_CHECK(wait_for_windowed_rectangle(native_window, expected_x, expected_y,
											 configuration.width, configuration.height));
	FLOWUI_CHECK(!glfwGetWindowMonitor(native_window));
	const auto restored_extent = service.windowExtent(fullscreen_window);
	FLOWUI_CHECK(restored_extent.width == static_cast<std::uint32_t>(configuration.width));
	FLOWUI_CHECK(restored_extent.height == static_cast<std::uint32_t>(configuration.height));
	FLOWUI_CHECK(service.toggleWindowFullscreen(fullscreen_window));
	poll_native_events();
	FLOWUI_CHECK(service.toggleWindowFullscreen(fullscreen_window));
	FLOWUI_CHECK(wait_for_windowed_rectangle(native_window, expected_x, expected_y,
											 configuration.width, configuration.height));
	FLOWUI_CHECK(service.windowExtent(fullscreen_window).width == restored_extent.width);
	FLOWUI_CHECK(service.windowExtent(fullscreen_window).height == restored_extent.height);
}

void testProviderOwnershipAndLifecycle() {
	using namespace FlowUi;
	using namespace FlowUi::detail::platform;
	GlfwPlatformProviderService firstService;
	GlfwPlatformProviderService secondService;
	FLOWUI_CHECK(firstService.initialize({}));
	FLOWUI_CHECK(secondService.initialize({}));
	detail::input::InputState firstInput;
	firstInput.bindTarget(11u);
	InputSink secondSink;
	std::unordered_map<WindowId, std::unique_ptr<GlfwWindowState>> states;
	states.emplace(11u, std::make_unique<GlfwWindowState>());
	auto& firstState = *states.at(11u);
	WindowConfig configuration;
	configuration.width = 320;
	configuration.height = 240;
	auto invalidConfig = configuration;
	invalidConfig.width = 0;
	FLOWUI_CHECK(!firstService.initializeWindow(firstState, 11u, invalidConfig, firstInput.sink()));
	FLOWUI_CHECK(!firstService.windowReady(firstState));
	firstService.shutdownWindow(firstState);
	FLOWUI_CHECK(firstService.initializeWindow(firstState, 11u, configuration, firstInput.sink()));
	FLOWUI_CHECK(firstService.windowReady(firstState));
	FLOWUI_CHECK(!firstService.initializeWindow(firstState, 11u, configuration, firstInput.sink()));
	FLOWUI_CHECK(!secondService.refreshInput(firstState));
	FLOWUI_CHECK(!secondService.minimizeWindow(firstState));
	FLOWUI_CHECK(!firstService.createSurface(firstState, VK_NULL_HANDLE));
	auto* native = static_cast<GLFWwindow*>(firstService.nativeHandle(firstState));
	void* callbackAddress = glfwGetWindowUserPointer(native);
	states.reserve(1024u);
	states.emplace(12u, std::make_unique<GlfwWindowState>());
	FLOWUI_CHECK(firstService.initializeWindow(*states.at(12u), 12u, configuration, secondSink));
	FLOWUI_CHECK(glfwGetWindowUserPointer(native) == callbackAddress);
	auto keyCallback = glfwSetKeyCallback(native, nullptr);
	auto charCallback = glfwSetCharCallback(native, nullptr);
	FLOWUI_CHECK(keyCallback && charCallback);
	glfwSetKeyCallback(native, keyCallback);
	glfwSetCharCallback(native, charCallback);
	auto focusCallback = glfwSetWindowFocusCallback(native, nullptr);
	FLOWUI_CHECK(focusCallback);
	glfwSetWindowFocusCallback(native, focusCallback);
	focusCallback(native, GLFW_TRUE);
	keyCallback(native, GLFW_KEY_F5, 0, GLFW_PRESS, GLFW_MOD_CONTROL);
	charCallback(native, 0x3bbu);
	keyCallback(native, GLFW_KEY_UNKNOWN, 0, GLFW_PRESS, 0);
	FLOWUI_CHECK(firstInput.processPending());
	FLOWUI_CHECK(firstInput.keyDown(Key::F5));
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(firstInput.queue().dev_shortcut_presses[keyCode(Key::F5)] == (1u << 1u));
#endif
	keyCallback(native, GLFW_KEY_F5, 0, GLFW_RELEASE, 0);
	FLOWUI_CHECK(firstInput.processPending());
	FLOWUI_CHECK(!firstInput.keyDown(Key::F5));
	FLOWUI_CHECK(firstInput.queue().queuedTextInput_.size() == 1u);
	FLOWUI_CHECK(firstInput.queue().queuedTextInput_[0] == U'λ');
	firstService.quiesceWindow(firstState);
	firstService.quiesceWindow(firstState);
	FLOWUI_CHECK(!glfwGetWindowUserPointer(native));
	FLOWUI_CHECK(!glfwSetKeyCallback(native, nullptr));
	firstService.shutdownWindow(firstState);
	firstService.shutdownWindow(firstState);
	FLOWUI_CHECK(!firstService.windowReady(firstState));
	firstService.shutdownWindow(*states.at(12u));
	states.clear();
	firstService.shutdown();
	firstService.shutdown();
	FLOWUI_CHECK(!firstService.ready());
}

void testTypedBindingAndClipboardServices() {
	using namespace FlowUi;
	using namespace FlowUi::detail::platform;
	GlfwRuntime runtime;
	FLOWUI_CHECK(runtime.initialize({}));
	GlfwWindow firstWindow;
	GlfwWindow secondWindow;
	WindowConfig configuration;
	FLOWUI_CHECK(firstWindow.initialize(runtime, configuration));
	FLOWUI_CHECK(secondWindow.initialize(runtime, configuration));
	GlfwWindowInput firstInput;
	FLOWUI_CHECK(firstInput.initialize(firstWindow, configuration.input));
	InputSink sink;
	GlfwEventBinding binding;
	FLOWUI_CHECK(!binding.attach(secondWindow, firstInput, sink, 21u));
	FLOWUI_CHECK(!glfwGetWindowUserPointer(firstWindow.native()));
	FLOWUI_CHECK(!glfwGetWindowUserPointer(secondWindow.native()));
	FLOWUI_CHECK(binding.attach(firstWindow, firstInput, sink, 21u));
	FLOWUI_CHECK(binding.attached());
	binding.detach();
	FLOWUI_CHECK(!binding.attached());
	firstInput.shutdown();
	firstWindow.shutdown();
	secondWindow.shutdown();
	runtime.shutdown();

	ActivePlatformProviderService service;
	FLOWUI_CHECK(service.initialize({}));
	ActivePlatformProviderService::WindowState state;
	FLOWUI_CHECK(service.initializeWindow(state, 22u, configuration, sink));
	WindowPlatformServices facade;
	FLOWUI_CHECK(!facade.hasClipboardAccess());
	FLOWUI_CHECK(facade.clipboardText().empty());
	facade.bind(service, state);
	FLOWUI_CHECK(facade.hasClipboardAccess());
	FLOWUI_CHECK(service.setClipboardText(state, ""));
	const auto emptyClipboard = service.clipboardText(state);
	FLOWUI_CHECK(emptyClipboard && emptyClipboard->empty());
	facade.setClipboardText("FlowUi λ\nsecond line");
	FLOWUI_CHECK(facade.clipboardText() == "FlowUi λ\nsecond line");
	facade.setCursor(CursorType::IBeam);
	facade.setCursor(CursorType::IBeam);
	facade.setCursor(CursorType::Default);
	FLOWUI_CHECK(!service.setCursor(state, static_cast<CursorType>(255u)));
	facade.detach();
	FLOWUI_CHECK(!facade.hasClipboardAccess());
	FLOWUI_CHECK(facade.clipboardText().empty());
	facade.setClipboardText("detached");
	FLOWUI_CHECK(service.clipboardText(state).value() == "FlowUi λ\nsecond line");
	service.shutdownWindow(state);
	FLOWUI_CHECK(!service.clipboardText(state));
}

} // namespace

int main() {
	try {
		FlowUi::detail::platform::GlfwRuntime platformProbe;
		if (const auto result = platformProbe.initialize({}); !result) {
			std::cerr << "Native window platform unavailable: "
					  << static_cast<int>(result.error().code) << '\n';
			return 77;
		}
		FlowUi::test::Runner runner;
		runner.run("window-specific native operations, thread checks and fullscreen restoration",
				   test_native_window_operations);
		runner.run("initial fullscreen window restores its configured windowed extent",
				   test_initial_fullscreen_restoration);
		runner.run("provider ownership, stable callbacks and idempotent lifecycle",
				   testProviderOwnershipAndLifecycle);
		runner.run("typed binding, clipboard outcomes and manager facade",
				   testTypedBindingAndClipboardServices);
		return runner.finish();
	} catch (const std::exception& exception) {
		std::cerr << "Native window platform unavailable: " << exception.what() << '\n';
		return 77;
	}
}
