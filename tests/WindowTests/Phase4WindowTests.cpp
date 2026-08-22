#include "TestHarness.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <GLFW/glfw3.h>

#include "FlowUi.hpp"
#include "managers/ActionManager.hpp"
#include "managers/UiManager.hpp"
#include "managers/FontManager.hpp"
#if FLOWUI_PUBLIC_VULKAN_INTEROP
#include "managers/ViewPortManager.hpp"
#endif

namespace {

void emptyFrame(FlowUi::App& app, FlowUi::WindowId window) {
	FLOWUI_CHECK(app.beginFrame(window));
	FLOWUI_CHECK(app.endFrame(window));
	FLOWUI_CHECK(app.drawFrame(window));
}

bool environmentUnavailable(std::string_view message) {
	constexpr std::string_view reasons[] = {
		"The platform window system could not be initialized.",
		"The FlowUi window could not be created.",
		"No compatible Vulkan device is available.",
		"A required Vulkan extension is unavailable.",
		"The selected Vulkan device lacks a required feature.",
		"Failed to initialize GLFW",
		"Failed to create GLFW window",
		"No Vulkan-capable GPU",
		"Failed to find a suitable Vulkan",
		"Missing required instance extension",
		"does not support descriptor indexing features",
		"does not support dynamic rendering",
		"does not support the secondary-window surface",
		"Secondary-window surface has no usable swapchain",
		"Independent secondary-window retirement requires",
	};
	for (const std::string_view reason : reasons) {
		if (message.find(reason) != std::string_view::npos) return true;
	}
	return false;
}

} // namespace

int main() {
	const char* stage = "startup";
	try {
		stage = "main creation";
		FlowUi::AppConfig config{};
		config.window.title = "FlowUi Phase 4 main";
		config.window.width = 320;
		config.window.height = 240;
		config.vk.enableValidation = true;
		config.vk.framesInFlight = 2;
		FlowUi::App app = FlowUi::makeApplication(config);
		FLOWUI_CHECK(app.mainWindowId() == FlowUi::MainWindowId);
		FLOWUI_CHECK(app.hasWindow(app.mainWindowId()));
		FLOWUI_CHECK(!app.hasWindow(FlowUi::InvalidWindowId));
		// The default 2048 px font atlas must fit the StorageSystem GPU budget.
		// Without a concrete face, all UI text (including input fields) is invisible.
		FLOWUI_CHECK(app.fonts().getFontById(0) != nullptr);

		stage = "secondary creation";
		FlowUi::WindowConfig firstConfig{};
		firstConfig.title = "FlowUi Phase 4 secondary A";
		firstConfig.width = 280;
		firstConfig.height = 180;
		FlowUi::WindowConfig secondConfig = firstConfig;
		secondConfig.title = "FlowUi Phase 4 secondary B";
		FlowUi::WindowConfig invalidConfig = firstConfig;
		invalidConfig.width = 0;
		const auto invalidWindow = app.createWindow(invalidConfig);
		FLOWUI_CHECK(!invalidWindow);
		FLOWUI_CHECK(invalidWindow.error().code == FlowUi::ErrorCode::InvalidWindowConfiguration);
		FLOWUI_CHECK(app.hasWindow(app.mainWindowId()));
		const auto firstResult = app.createWindow(firstConfig);
		const auto secondResult = app.createWindow(secondConfig);
		FLOWUI_CHECK(firstResult);
		FLOWUI_CHECK(secondResult);
		const FlowUi::WindowId first = firstResult.value();
		const FlowUi::WindowId second = secondResult.value();
		FLOWUI_CHECK(first > app.mainWindowId());
		FLOWUI_CHECK(second > first);
		FLOWUI_CHECK(app.hasWindow(first));
		FLOWUI_CHECK(&app.ui(first) != &app.ui(second));
		FLOWUI_CHECK(&app.ui().actions() == &app.actions());
		FLOWUI_CHECK(&app.ui(first).actions() == &app.actions());
		FLOWUI_CHECK(&app.ui(second).actions() == &app.actions());
		FLOWUI_CHECK(app.nativeWindowHandle(first) != app.nativeWindowHandle(second));
#if FLOWUI_PUBLIC_VULKAN_INTEROP
		const FlowUi::ResourceKey automaticViewport{.name = "window-local"};
		FLOWUI_CHECK(app.viewPorts(first).create(automaticViewport));
		FLOWUI_CHECK(app.viewPorts(second).create(automaticViewport));
		FLOWUI_CHECK(app.viewPorts(first).getTexture("window-local").handle !=
			app.viewPorts(second).getTexture("window-local").handle);
		FLOWUI_CHECK_THROWS(app.viewPorts(first).contains(FlowUi::ResourceKey{
			.name = "window-local", .window = second,
		}));
#endif
		FLOWUI_CHECK(app.fonts().getFamilyId(FlowUi::ResourceKey{.name = "Default"}) ==
			app.fonts().getFamilyId("Default"));
		FLOWUI_CHECK_THROWS(app.fonts().getFamilyId(FlowUi::ResourceKey{
			.name = "Default", .domain = FlowUi::ResourceDomain::Image,
		}));

		stage = "lifecycle gate";
		FLOWUI_CHECK(app.pollEvents());
		FLOWUI_CHECK(app.beginFrame(app.mainWindowId()));
		FLOWUI_CHECK(!app.beginFrame(first));
		FLOWUI_CHECK(!app.pollEvents());
		FLOWUI_CHECK(!app.destroyWindow(first));
		FLOWUI_CHECK(app.endFrame(app.mainWindowId()));
		FLOWUI_CHECK(app.drawFrame(app.mainWindowId()));
		FLOWUI_CHECK(!app.endFrame(first));

		stage = "different-rate frames";
		for (int tick = 0; tick < 4; ++tick) {
			FLOWUI_CHECK(app.pollEvents());
			emptyFrame(app, app.mainWindowId());
			emptyFrame(app, first);
			if ((tick % 2) == 0) emptyFrame(app, second);
		}

		stage = "window-local accessors";
		app.setWindowTitle(first, "FlowUi Phase 4 renamed");
		FlowUi::WindowInputConfig input = app.windowInputConfig(first);
		input.stickyKeys = !input.stickyKeys;
		app.setWindowInputConfig(first, input);
		FLOWUI_CHECK(app.windowInputConfig(first).stickyKeys == input.stickyKeys);
		FLOWUI_CHECK(app.windowSize(first).first > 0);
		FLOWUI_CHECK(app.framebufferSize(first).second > 0);

		stage = "independent resize";
		auto* firstNative = static_cast<GLFWwindow*>(app.nativeWindowHandle(first));
		glfwSetWindowSize(firstNative, 360, 220);
		FLOWUI_CHECK(app.pollEvents());
		emptyFrame(app, first);
		emptyFrame(app, app.mainWindowId());
		stage = "minimized isolation";
		glfwIconifyWindow(firstNative);
		FLOWUI_CHECK(app.pollEvents());
		emptyFrame(app, first);
		emptyFrame(app, app.mainWindowId());
		glfwRestoreWindow(firstNative);
		FLOWUI_CHECK(app.pollEvents());

		stage = "secondary close";
		app.setShouldClose(second, 1);
		FLOWUI_CHECK(app.shouldClose(second));
		FLOWUI_CHECK(app.destroyWindow(second));
		FLOWUI_CHECK(!app.hasWindow(second));
		FLOWUI_CHECK_THROWS(app.shouldClose(second));
		const auto mainDestroy = app.destroyWindow(app.mainWindowId());
		FLOWUI_CHECK(!mainDestroy);
		FLOWUI_CHECK(mainDestroy.error().code == FlowUi::ErrorCode::MainWindowDestructionForbidden);
		stage = "non-reused ID";
		FlowUi::WindowConfig replacementConfig = firstConfig;
		replacementConfig.title = "FlowUi Phase 4 non-reused ID";
		const auto replacementResult = app.createWindow(replacementConfig);
		FLOWUI_CHECK(replacementResult);
		const FlowUi::WindowId replacement = replacementResult.value();
		FLOWUI_CHECK(replacement > second);
		FLOWUI_CHECK(app.destroyWindow(replacement));

		// Destroy immediately after submission to exercise target-window drain and
		// exact present completion while another public window remains alive.
		stage = "in-flight close";
		FLOWUI_CHECK(app.pollEvents());
		stage = "in-flight frame";
		FLOWUI_CHECK(app.beginFrame(first));
		stage = "in-flight end";
		FLOWUI_CHECK(app.endFrame(first));
		stage = "in-flight draw";
		FLOWUI_CHECK(app.drawFrame(first));
		stage = "in-flight destroy";
		FLOWUI_CHECK(app.destroyWindow(first));
		FLOWUI_CHECK(!app.hasWindow(first));
		stage = "remaining main frame";
		FLOWUI_CHECK(app.pollEvents());
		emptyFrame(app, app.mainWindowId());
		return 0;
	} catch (const std::exception& error) {
		if (environmentUnavailable(error.what())) {
			std::cerr << "Phase 4 real-window validation unavailable at " << stage << ": "
				<< error.what() << '\n';
			return 77;
		}
		std::cerr << "Phase 4 real-window validation failed at " << stage << ": " << error.what() << '\n';
		return 1;
	}
}
