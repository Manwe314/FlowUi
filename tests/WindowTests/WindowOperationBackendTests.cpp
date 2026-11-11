#include "TestHarness.hpp"

#include "FlowUi/Private/windowService/Window.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace {

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
	FlowUi::detail::GlfwWindowBackend first_window(configuration, nullptr);
	FlowUi::detail::GlfwWindowBackend second_window(configuration, nullptr);
	auto* first_native = static_cast<GLFWwindow*>(first_window.nativeHandle());
	auto* second_native = static_cast<GLFWwindow*>(second_window.nativeHandle());
	const auto capabilities = first_window.window_operation_capabilities();
	FLOWUI_CHECK(capabilities.close_window);
	FLOWUI_CHECK(capabilities.minimize);
	FLOWUI_CHECK(capabilities.maximize_restore);
	FLOWUI_CHECK(capabilities.toggle_fullscreen);

	FlowUi::Status cross_thread_result{};
	std::thread worker_thread([&] { cross_thread_result = first_window.minimize_window(); });
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
	FLOWUI_CHECK(first_window.toggle_window_fullscreen());
	FLOWUI_CHECK(glfwGetWindowMonitor(first_native));
	FLOWUI_CHECK(!glfwGetWindowMonitor(second_native));
	const auto fullscreen_maximize = first_window.toggle_window_maximized();
	FLOWUI_CHECK(!fullscreen_maximize);
	FLOWUI_CHECK(fullscreen_maximize.error().code == FlowUi::ErrorCode::WindowOperationUnsupported);
	poll_native_events();
	FLOWUI_CHECK(first_window.toggle_window_fullscreen());
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

	FLOWUI_CHECK(first_window.toggle_window_maximized());
	if (wait_for_window_attribute(first_native, GLFW_MAXIMIZED, GLFW_TRUE)) {
		FLOWUI_CHECK(first_window.toggle_window_fullscreen());
		poll_native_events();
		FLOWUI_CHECK(first_window.toggle_window_fullscreen());
		FLOWUI_CHECK(wait_for_window_attribute(first_native, GLFW_MAXIMIZED, GLFW_TRUE));
		FLOWUI_CHECK(first_window.toggle_window_maximized());
		FLOWUI_CHECK(wait_for_window_attribute(first_native, GLFW_MAXIMIZED, GLFW_FALSE));
		FLOWUI_CHECK(wait_for_window_extent(first_native, original_width, original_height));
	} else {
		std::cout << "Native window manager did not apply maximization within 500ms; geometry "
					 "round trip was "
					 "verified windowed.\n";
	}

	FLOWUI_CHECK(first_window.minimize_window());
	if (!wait_for_window_attribute(first_native, GLFW_ICONIFIED, GLFW_TRUE))
		std::cout << "Native window manager did not apply minimization within 500ms.\n";
	FLOWUI_CHECK(!second_window.shouldClose());
	FLOWUI_CHECK(glfwGetWindowAttrib(second_native, GLFW_ICONIFIED) == GLFW_FALSE);
	glfwRestoreWindow(first_native);
	poll_native_events();
	FLOWUI_CHECK(first_window.request_window_close());
	FLOWUI_CHECK(first_window.shouldClose());
	FLOWUI_CHECK(!second_window.shouldClose());
}

void test_initial_fullscreen_restoration() {
	FlowUi::WindowConfig configuration{};
	configuration.width = 520;
	configuration.height = 320;
	configuration.fullscreen = true;
	FlowUi::detail::GlfwWindowBackend fullscreen_window(configuration, nullptr);
	auto* native_window = static_cast<GLFWwindow*>(fullscreen_window.nativeHandle());
	FLOWUI_CHECK(glfwGetWindowMonitor(native_window));
	int work_x = 0;
	int work_y = 0;
	int work_width = 0;
	int work_height = 0;
	glfwGetMonitorWorkarea(glfwGetWindowMonitor(native_window), &work_x, &work_y, &work_width,
						   &work_height);
	const int expected_x = work_x + (work_width - configuration.width) / 2;
	const int expected_y = work_y + (work_height - configuration.height) / 2;
	FLOWUI_CHECK(fullscreen_window.toggle_window_fullscreen());
	FLOWUI_CHECK(wait_for_windowed_rectangle(native_window, expected_x, expected_y,
											 configuration.width, configuration.height));
	FLOWUI_CHECK(!glfwGetWindowMonitor(native_window));
	const auto restored_extent = fullscreen_window.windowExtent();
	FLOWUI_CHECK(restored_extent.width == static_cast<std::uint32_t>(configuration.width));
	FLOWUI_CHECK(restored_extent.height == static_cast<std::uint32_t>(configuration.height));
	FLOWUI_CHECK(fullscreen_window.toggle_window_fullscreen());
	poll_native_events();
	FLOWUI_CHECK(fullscreen_window.toggle_window_fullscreen());
	FLOWUI_CHECK(wait_for_windowed_rectangle(native_window, expected_x, expected_y,
											 configuration.width, configuration.height));
	FLOWUI_CHECK(fullscreen_window.windowExtent().width == restored_extent.width);
	FLOWUI_CHECK(fullscreen_window.windowExtent().height == restored_extent.height);
}

} // namespace

int main() {
	try {
		FlowUi::detail::GlfwLibrary platform_probe;
		FlowUi::test::Runner runner;
		runner.run("window-specific native operations, thread checks and fullscreen restoration",
				   test_native_window_operations);
		runner.run("initial fullscreen window restores its configured windowed extent",
				   test_initial_fullscreen_restoration);
		return runner.finish();
	} catch (const FlowUi::FlowUiException& exception) {
		std::cerr << "Native window platform unavailable: " << exception.what() << '\n';
		return 77;
	}
}
