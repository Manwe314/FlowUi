#include "TestHarness.hpp"

#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <GLFW/glfw3.h>

#include "FlowUi.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include "FlowUi/Public/managers/FontManager.hpp"
#if FLOWUI_PUBLIC_VULKAN_INTEROP
#include "FlowUi/Public/managers/ViewPortManager.hpp"
#endif

namespace {

struct ErrorCapture {
	FlowUi::ErrorEventView event{};
	std::uint32_t count = 0u;
};

void captureError(void* userData, const FlowUi::ErrorEventView& event) noexcept {
	auto& capture = *static_cast<ErrorCapture*>(userData);
	capture.event = event;
	++capture.count;
}

void emptyFrame(FlowUi::App& app, FlowUi::WindowId window) {
	FLOWUI_CHECK(app.beginFrame(window));
	FLOWUI_CHECK(app.endFrame(window));
	FLOWUI_CHECK(app.drawFrame(window));
}

void test_text_field_modes(FlowUi::App& application) {
	using namespace FlowUi;
	FLOWUI_CHECK(application.beginFrame());
	auto& manager = application.ui().inputFields();
	const ResourceKey single_key{.name = "cleanup/single-line"};
	const ResourceKey multiline_key{.name = "cleanup/multiline"};
	FieldRequest single_request{
		.initialText = "seed",
		.config = FieldConfig{.mode = TextFieldMode::SingleLine},
	};
	FieldRequest multiline_request{
		.initialText = "first",
		.config = FieldConfig{.mode = TextFieldMode::MultiLine},
	};
	const auto single = manager.requestField(single_key, single_request);
	FLOWUI_CHECK(single.mode == TextFieldMode::SingleLine);
	FLOWUI_CHECK(single.text.contiguous().value_or("") == "seed");
	const TextReplacement single_newline{.oldRange = {4, 4}, .insertedText = "\r\nnext"};
	FLOWUI_CHECK(manager.applyEdits(single_key, std::span{&single_newline, 1}) ==
				 EditResult::RejectedNewline);
	FLOWUI_CHECK(manager.requestField(single_key, single_request).text.copy() == "seed");

	FLOWUI_CHECK(manager.requestField(multiline_key, multiline_request).mode ==
				 TextFieldMode::MultiLine);
	const TextReplacement multiline_newlines{.oldRange = {5, 5},
											 .insertedText = "\r\nsecond\rthird"};
	FLOWUI_CHECK(manager.applyEdits(multiline_key, std::span{&multiline_newlines, 1}) ==
				 EditResult::Applied);
	const auto multiline = manager.requestField(multiline_key, multiline_request);
	FLOWUI_CHECK(!multiline.text.contiguous());
	FLOWUI_CHECK(multiline.text.copy() == "first\nsecond\nthird");
	std::string visited_text;
	visited_text.reserve(multiline.text.sizeBytes());
	multiline.text.forEachChunk({0, multiline.text.sizeBytes()},
								[&](std::string_view chunk) { visited_text.append(chunk); });
	FLOWUI_CHECK(visited_text == "first\nsecond\nthird");

	multiline_request.config.mode = TextFieldMode::SingleLine;
	const auto rejected_migration = manager.requestField(multiline_key, multiline_request);
	FLOWUI_CHECK(rejected_migration.modeChangeRejected);
	FLOWUI_CHECK(rejected_migration.mode == TextFieldMode::MultiLine);
	const size_t multiline_end = rejected_migration.text.sizeBytes();
	const TextReplacement retained_multiline_newline{.oldRange = {multiline_end, multiline_end},
													 .insertedText = "\r\nlast"};
	FLOWUI_CHECK(manager.applyEdits(multiline_key, std::span{&retained_multiline_newline, 1}) ==
				 EditResult::Applied);
	const auto multiline_with_last = manager.requestField(multiline_key, multiline_request);
	FLOWUI_CHECK(multiline_with_last.text.copy() == "first\nsecond\nthird\nlast");

	const TextReplacement flatten_multiline{.oldRange = {0, multiline_with_last.text.sizeBytes()},
											.insertedText = "plain"};
	FLOWUI_CHECK(manager.applyEdits(multiline_key, std::span{&flatten_multiline, 1}) ==
				 EditResult::Applied);
	const auto migrated_single = manager.requestField(multiline_key, multiline_request);
	FLOWUI_CHECK(!migrated_single.modeChangeRejected);
	FLOWUI_CHECK(migrated_single.mode == TextFieldMode::SingleLine);
	FLOWUI_CHECK(migrated_single.text.contiguous().value_or("") == "plain");
	const TextReplacement migrated_newline{.oldRange = {5, 5}, .insertedText = "\n"};
	FLOWUI_CHECK(manager.applyEdits(multiline_key, std::span{&migrated_newline, 1}) ==
				 EditResult::RejectedNewline);

	single_request.config.mode = TextFieldMode::MultiLine;
	const auto migrated_multiline = manager.requestField(single_key, single_request);
	FLOWUI_CHECK(migrated_multiline.mode == TextFieldMode::MultiLine);
	FLOWUI_CHECK(!migrated_multiline.modeChangeRejected);
	FLOWUI_CHECK(manager.applyEdits(single_key, std::span{&single_newline, 1}) ==
				 EditResult::Applied);
	FLOWUI_CHECK(manager.requestField(single_key, single_request).text.copy() == "seed\nnext");
	FLOWUI_CHECK(manager.removeField(single_key));
	FLOWUI_CHECK(manager.removeField(multiline_key));
	FLOWUI_CHECK(application.endFrame());
	FLOWUI_CHECK(application.drawFrame());
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

int main(int argument_count, char* arguments[]) {
	const bool text_modes_only =
		argument_count == 2 && std::string_view(arguments[1]) == "--text-modes";
	const char* stage = "startup";
	try {
		stage = "main creation";
		FlowUi::AppConfig config{};
		ErrorCapture errorCapture{};
		config.errors.observer.mode = FlowUi::ErrorReportingMode::SinkOnly;
		config.errors.observer.sink = FlowUi::ErrorSink{
			.userData = &errorCapture,
			.callback = &captureError,
		};
		config.window.title = "FlowUi Phase 4 main";
		config.window.width = 320;
		config.window.height = 240;
		config.vk.enableValidation = true;
		config.vk.framesInFlight = 2;
		FlowUi::App app = FlowUi::makeApplication(config);
		errorCapture = {};
		const FlowUi::FlowUiError reportedError = FlowUi::makeError(
			FlowUi::ErrorCode::InvalidWindowId,
			FlowUi::ErrorSite::AppAccessWindow,
			42u);
		app.reportError(reportedError);
		FLOWUI_CHECK(errorCapture.count == 1u);
		FLOWUI_CHECK(errorCapture.event.kind == FlowUi::ErrorEventKind::Reported);
		FLOWUI_CHECK(errorCapture.event.error.code == reportedError.code);
		FLOWUI_CHECK(errorCapture.event.error.site == reportedError.site);
		FLOWUI_CHECK(errorCapture.event.error.subject == reportedError.subject);
		FLOWUI_CHECK(app.mainWindowId() == FlowUi::MainWindowId);
		FLOWUI_CHECK(app.hasWindow(app.mainWindowId()));
		FLOWUI_CHECK(!app.hasWindow(FlowUi::InvalidWindowId));
		// The default 2048 px font atlas must fit the StorageSystem GPU budget.
		// Without a concrete face, all UI text (including input fields) is invisible.
		FLOWUI_CHECK(app.fonts().getFontById(0) != nullptr);

		stage = "text field mode and explicit text views";
		test_text_field_modes(app);
		if (text_modes_only)
			return 0;

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
		if (!firstResult && firstResult.error().code == FlowUi::ErrorCode::WindowPresentationUnsupported) {
			std::cerr << "Phase 4 secondary-window presentation is unavailable on this device\n";
			return 77;
		}
		FLOWUI_CHECK(firstResult);
		const auto secondResult = app.createWindow(secondConfig);
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

		stage = "managed window dispatch";
		std::vector<FlowUi::WindowId> managedOrder;
		const auto throwingManaged = app.createWindow(
			FlowUi::WindowConfigOverrides{
				.width = 240,
				.height = 160,
				.title = "FlowUi managed throwing",
			},
			[&](FlowUi::UiManager&, FlowUi::WindowId id) {
				managedOrder.push_back(id);
				throw std::runtime_error("managed callback failure");
			});
		const auto healthyManaged = app.createWindow(
			FlowUi::WindowConfigOverrides{
				.width = 240,
				.height = 160,
				.title = "FlowUi managed healthy",
			},
			[&](FlowUi::UiManager&, FlowUi::WindowId id) {
				managedOrder.push_back(id);
			});
		FLOWUI_CHECK(throwingManaged);
		FLOWUI_CHECK(healthyManaged);
		errorCapture = {};
		FLOWUI_CHECK(app.pollEvents());
		FLOWUI_CHECK(app.beginFrame(app.mainWindowId()));
		FLOWUI_CHECK(app.endFrame(app.mainWindowId()));
		const FlowUi::Status managedStatus = app.drawFrame();
		FLOWUI_CHECK(!managedStatus);
		FLOWUI_CHECK(managedStatus.error().code == FlowUi::ErrorCode::UiBuildCallbackFailed);
		FLOWUI_CHECK(errorCapture.count == 1u);
		FLOWUI_CHECK(errorCapture.event.error.code == FlowUi::ErrorCode::UiBuildCallbackFailed);
		FLOWUI_CHECK(managedOrder.size() == 2u);
		FLOWUI_CHECK(managedOrder[0] == *throwingManaged);
		FLOWUI_CHECK(managedOrder[1] == *healthyManaged);

		app.removeWindowUiCallback(*throwingManaged);
		FLOWUI_CHECK(app.destroyWindow(*throwingManaged));
		app.setShouldClose(*healthyManaged, 1);
		FLOWUI_CHECK(app.dispatchManagedWindows());
		FLOWUI_CHECK(!app.hasWindow(*healthyManaged));

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
