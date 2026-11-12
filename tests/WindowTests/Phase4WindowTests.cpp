#include "TestHarness.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>

#include "FlowUi.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/FontManager.hpp"
#include "FlowUi/Public/managers/ShortcutManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#if FLOWUI_PUBLIC_VULKAN_INTEROP
#include "FlowUi/Public/managers/ViewPortManager.hpp"
#endif

namespace {

struct ErrorCapture {
	FlowUi::ErrorEventView event{};
	FlowUi::App* application = nullptr;
	FlowUi::WindowId destroy_window_on_error = FlowUi::InvalidWindowId;
	std::uint32_t count = 0u;
	bool destroy_attempted = false;
	bool destroy_succeeded = false;
	bool destroy_threw = false;
};

void captureError(void* userData, const FlowUi::ErrorEventView& event) noexcept {
	auto& capture = *static_cast<ErrorCapture*>(userData);
	capture.event = event;
	++capture.count;
	if (capture.application && event.error.code == FlowUi::ErrorCode::WindowOperationUnsupported &&
		event.error.subject == capture.destroy_window_on_error) {
		const auto window_to_destroy = capture.destroy_window_on_error;
		capture.destroy_window_on_error = FlowUi::InvalidWindowId;
		capture.destroy_attempted = true;
		try {
			capture.destroy_succeeded =
				capture.application->destroyWindow(window_to_destroy).has_value();
		} catch (...) {
			capture.destroy_threw = true;
		}
	}
}

void emptyFrame(FlowUi::App& app, FlowUi::WindowId window) {
	FLOWUI_CHECK(app.beginFrame(window));
	FLOWUI_CHECK(app.endFrame(window));
	FLOWUI_CHECK(app.drawFrame(window));
}

void test_popup_flag_lifecycle(FlowUi::App& application) {
	using namespace FlowUi;
	const WindowId window_id = application.mainWindowId();
	auto* native_window = static_cast<GLFWwindow*>(application.nativeWindowHandle(window_id));
	const auto finish_frame = [&] {
		FLOWUI_CHECK(application.endFrame(window_id));
		FLOWUI_CHECK(application.drawFrame(window_id));
	};
	const auto inject_key = [&](int action) {
		const GLFWkeyfun callback = glfwSetKeyCallback(native_window, nullptr);
		FLOWUI_CHECK(callback);
		glfwSetKeyCallback(native_window, callback);
		callback(native_window, GLFW_KEY_ESCAPE, 0, action, 0);
	};
	const auto move_pointer = [&](double pointer_x, double pointer_y) {
		// App refreshes pointer state directly from GLFW at each frame boundary.
		glfwSetCursorPos(native_window, pointer_x, pointer_y);
	};
	const auto emit_popup_root = [](FlowElementID popup_id, const PopupFrame& frame) {
		if (!frame.visible && !frame.measureOnly)
			return;
		const auto clay_id = FlowIDToClayID(popup_id);
		const Clay_ElementId clay_element_id{.id = clay_id, .baseId = clay_id};
		CLAY(clay_element_id, {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(80),
								   .height = CLAY_SIZING_FIXED(40)}},
			  .backgroundColor = {20, 40, 60, 255},
			  .floating = frame.floating}) {}
	};

	const FlowElementID measured_popup{.value = 0x48514f8a1001u};
	PopupRequest request{.anchor = PopupAnchor::position({20, 20})};
	FLOWUI_CHECK(application.beginFrame(window_id));
	auto frame = application.ui(window_id).popups().request(measured_popup, request);
	FLOWUI_CHECK(frame && !frame->visible && frame->measureOnly);
	emit_popup_root(measured_popup, *frame);
	finish_frame();

	FLOWUI_CHECK(application.beginFrame(window_id));
	frame = application.ui(window_id).popups().request(measured_popup, request);
	FLOWUI_CHECK(frame && frame->visible && !frame->measureOnly);
	emit_popup_root(measured_popup, *frame);
	finish_frame();

	inject_key(GLFW_PRESS);
	FLOWUI_CHECK(application.beginFrame(window_id));
	auto& popups = application.ui(window_id).popups();
	FLOWUI_CHECK(popups.consumeDismissed(measured_popup));
	FLOWUI_CHECK(!popups.consumeDismissed(measured_popup));
	frame = popups.request(measured_popup, request);
	FLOWUI_CHECK(frame && !frame->visible && !frame->measureOnly);
	finish_frame();
	inject_key(GLFW_RELEASE);
	emptyFrame(application, window_id);

	request.dismissOnEscape = false;
	request.expectedSize = Clay_Dimensions{80, 40};
	FLOWUI_CHECK(application.beginFrame(window_id));
	frame = popups.request(measured_popup, request);
	FLOWUI_CHECK(frame && frame->visible);
	emit_popup_root(measured_popup, *frame);
	finish_frame();
	inject_key(GLFW_PRESS);
	FLOWUI_CHECK(application.beginFrame(window_id));
	FLOWUI_CHECK(!popups.consumeDismissed(measured_popup));
	frame = popups.request(measured_popup, request);
	FLOWUI_CHECK(frame && frame->visible);
	emit_popup_root(measured_popup, *frame);
	finish_frame();
	popups.dismiss(measured_popup);
	FLOWUI_CHECK(popups.consumeDismissed(measured_popup));
	FLOWUI_CHECK(!popups.consumeDismissed(measured_popup));
	inject_key(GLFW_RELEASE);
	emptyFrame(application, window_id);

	const FlowElementID snapshot_popup{.value = 0x48514f8a1002u};
	request.anchor = PopupAnchor::pointerSnapshot();
	request.outsidePress = PopupOutsidePressPolicy::DismissAndConsume;
	move_pointer(40, 45);
	FLOWUI_CHECK(application.beginFrame(window_id));
	frame = popups.request(snapshot_popup, request);
	FLOWUI_CHECK(frame && frame->visible);
	const Clay_Vector2 snapshot_position = frame->floating.offset;
	FLOWUI_CHECK(application.ui(window_id).getCurrentFrameInput().mouseX == 40.0f);
	FLOWUI_CHECK(application.ui(window_id).getCurrentFrameInput().mouseY == 45.0f);
	FLOWUI_CHECK(snapshot_position.x == 40.0f && snapshot_position.y == 45.0f);
	emit_popup_root(snapshot_popup, *frame);
	finish_frame();
	move_pointer(60, 65);
	FLOWUI_CHECK(application.beginFrame(window_id));
	frame = popups.request(snapshot_popup, request);
	FLOWUI_CHECK(frame && frame->visible);
	FLOWUI_CHECK(application.ui(window_id).getCurrentFrameInput().mouseX == 60.0f);
	FLOWUI_CHECK(application.ui(window_id).getCurrentFrameInput().mouseY == 65.0f);
	FLOWUI_CHECK(frame->floating.offset.x == snapshot_position.x);
	FLOWUI_CHECK(frame->floating.offset.y == snapshot_position.y);
	emit_popup_root(snapshot_popup, *frame);
	finish_frame();

	popups.dismiss(snapshot_popup);
	FLOWUI_CHECK(popups.consumeDismissed(snapshot_popup));
	FLOWUI_CHECK(!popups.consumeDismissed(snapshot_popup));
	emptyFrame(application, window_id);
}

void test_deferred_error_observer_destroys_window(FlowUi::App& application,
												  ErrorCapture& error_capture) {
	const auto victim_result = application.createWindow(FlowUi::WindowConfigOverrides{
		.width = 280, .height = 180, .title = "FlowUi deferred error victim"});
	const auto surviving_result = application.createWindow(FlowUi::WindowConfigOverrides{
		.width = 280, .height = 180, .title = "FlowUi deferred surviving operation"});
	FLOWUI_CHECK(victim_result);
	FLOWUI_CHECK(surviving_result);
	const auto victim_window = *victim_result;
	const auto surviving_window = *surviving_result;
	FLOWUI_CHECK(application.toggle_window_fullscreen(victim_window));
	FLOWUI_CHECK(glfwGetWindowMonitor(
		static_cast<GLFWwindow*>(application.nativeWindowHandle(victim_window))));

	error_capture = {};
	error_capture.application = &application;
	error_capture.destroy_window_on_error = victim_window;
	FLOWUI_CHECK(application.beginFrame());
	FLOWUI_CHECK(application.toggle_window_maximized(victim_window));
	FLOWUI_CHECK(application.toggle_window_fullscreen(victim_window));
	FLOWUI_CHECK(application.toggle_window_fullscreen(surviving_window));
	FLOWUI_CHECK(application.endFrame());
	FLOWUI_CHECK(application.drawFrame());
	FLOWUI_CHECK(application.pollEvents());
	FLOWUI_CHECK(error_capture.destroy_attempted);
	FLOWUI_CHECK(error_capture.destroy_succeeded);
	FLOWUI_CHECK(!error_capture.destroy_threw);
	FLOWUI_CHECK(error_capture.count == 1);
	FLOWUI_CHECK(error_capture.event.error.code == FlowUi::ErrorCode::WindowOperationUnsupported);
	FLOWUI_CHECK(error_capture.event.error.subject == victim_window);
	FLOWUI_CHECK(!application.hasWindow(victim_window));
	FLOWUI_CHECK(application.hasWindow(surviving_window));
	FLOWUI_CHECK(glfwGetWindowMonitor(
		static_cast<GLFWwindow*>(application.nativeWindowHandle(surviving_window))));
	FLOWUI_CHECK(!glfwGetWindowMonitor(static_cast<GLFWwindow*>(application.nativeWindowHandle())));
	error_capture = {};
	FLOWUI_CHECK(application.toggle_window_fullscreen(surviving_window));
	FLOWUI_CHECK(application.destroyWindow(surviving_window));
	FLOWUI_CHECK(application.pollEvents());
	emptyFrame(application, application.mainWindowId());
}

#if FLOW_UI_DEV_MODE
void test_default_text_shortcuts(FlowUi::App& application) {
	using namespace FlowUi;
	std::vector<ShortcutManager::DevShortcutView> registrations;
	registrations.reserve(15);
	const auto collect = +[](void* target, const ShortcutManager::DevShortcutView& registration) {
		static_cast<std::vector<ShortcutManager::DevShortcutView>*>(target)->emplace_back(
			registration);
		return true;
	};
	auto& shortcuts = application.ui().shortcuts();
	FLOWUI_CHECK(shortcuts.visitDevShortcuts(&registrations, collect));
	FLOWUI_CHECK(registrations.size() == 14);
	for (const auto& registration : registrations) {
		FLOWUI_CHECK(registration.scope == ShortcutScope::FocusedInput);
		FLOWUI_CHECK(registration.chord.trigger == ShortcutTrigger::Press);
		FLOWUI_CHECK(registration.callback);
		FLOWUI_CHECK(registration.chord.key != GLFW_KEY_Z);
		FLOWUI_CHECK(registration.chord.key != GLFW_KEY_Y);
		FLOWUI_CHECK(!registration.chord.ctrl || !registration.chord.super);
	}
	for (const int key : {GLFW_KEY_A, GLFW_KEY_C, GLFW_KEY_X, GLFW_KEY_V}) {
		const auto matching_count = std::count_if(
			registrations.begin(), registrations.end(), [key](const auto& registration) {
				return registration.chord.key == key && !registration.chord.shift &&
					   !registration.chord.alt &&
#if defined(__APPLE__)
					   registration.chord.super && !registration.chord.ctrl;
#else
					registration.chord.ctrl && !registration.chord.super;
#endif
			});
		FLOWUI_CHECK(matching_count == 1);
	}
	const auto manual_history = shortcuts.registerShortcut(
		ShortcutChord{.key = GLFW_KEY_Z, .ctrl = true}, ShortcutScope::FocusedInput, 1,
		[](ShortcutContext& context) {
			return context.ui.inputFields().enqueueCommand(TextCommand::RequestUndo);
		});
	FLOWUI_CHECK(manual_history);
	registrations.clear();
	FLOWUI_CHECK(shortcuts.visitDevShortcuts(&registrations, collect));
	FLOWUI_CHECK(registrations.size() == 15);
	FLOWUI_CHECK(shortcuts.unregisterShortcut(*manual_history));
}
#endif

void test_text_command_pipeline(FlowUi::App& application) {
	using namespace FlowUi;
	auto& manager = application.ui().inputFields();
	const ResourceKey first_key{.name = "default-shortcuts/first"};
	const ResourceKey second_key{.name = "default-shortcuts/second"};
	FieldRequest first_request{.initialText = "first value"};
	const FieldRequest second_request{.initialText = "other"};
	const auto finish_frame = [&application] {
		FLOWUI_CHECK(application.endFrame());
		FLOWUI_CHECK(application.drawFrame());
	};
	FLOWUI_CHECK(application.beginFrame());
	FLOWUI_CHECK(manager.requestField(first_key, first_request).text.copy() == "first value");
	FLOWUI_CHECK(manager.requestField(second_key, second_request).text.copy() == "other");
	manager.requestCaret(first_key, CaretRequestKind::SetPrimary);
	finish_frame();

	FLOWUI_CHECK(application.beginFrame());
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::SelectAll));
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::Copy));
	const auto selected = manager.requestField(first_key, first_request);
	FLOWUI_CHECK(selected.hasSelection);
	FLOWUI_CHECK(selected.commandRequests.empty());
	FLOWUI_CHECK(manager.getSelectedText() == "first value");
	FLOWUI_CHECK(application.ui().clipboardText() == "first value");
	finish_frame();

	FLOWUI_CHECK(application.beginFrame());
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::Cut));
	const auto cut = manager.requestField(first_key, first_request);
	FLOWUI_CHECK(cut.text.copy().empty());
	FLOWUI_CHECK(cut.transactions.size() == 1);
	FLOWUI_CHECK(cut.transactions.front().origin == EditOrigin::Cut);
	FLOWUI_CHECK(application.ui().clipboardText() == "first value");
	finish_frame();

	FLOWUI_CHECK(application.beginFrame());
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::Paste));
	FLOWUI_CHECK(manager.requestField(first_key, first_request).text.copy() == "first value");
	finish_frame();

	FLOWUI_CHECK(application.beginFrame());
	first_request.config.readOnly = true;
	for (const auto command : {TextCommand::SelectAll, TextCommand::Cut, TextCommand::Paste,
							   TextCommand::DeleteWordBackward, TextCommand::DeleteWordForward}) {
		FLOWUI_CHECK(manager.enqueueCommand(command));
	}
	const auto read_only = manager.requestField(first_key, first_request);
	FLOWUI_CHECK(read_only.text.copy() == "first value");
	FLOWUI_CHECK(read_only.hasSelection);
	FLOWUI_CHECK(read_only.transactions.empty());
	FLOWUI_CHECK(read_only.commandRequests.empty());
	finish_frame();

	FLOWUI_CHECK(application.beginFrame());
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::Copy));
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::MoveDocumentStart));
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::MoveWordRight));
	FLOWUI_CHECK(!manager.requestField(first_key, first_request).hasSelection);
	FLOWUI_CHECK(application.ui().clipboardText() == "first value");
	finish_frame();

	FLOWUI_CHECK(application.beginFrame());
	first_request.config.readOnly = false;
	FLOWUI_CHECK(manager.requestField(first_key, first_request).text.copy() == "first value");
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::SelectAll));
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::Paste, "captured"));
	manager.requestCaret(second_key, CaretRequestKind::SetPrimary);
	FLOWUI_CHECK(manager.requestField(first_key, first_request).text.copy() == "captured");
	FLOWUI_CHECK(manager.requestField(second_key, second_request).text.copy() == "other");
	manager.requestCaret(first_key, CaretRequestKind::SetPrimary);
	FLOWUI_CHECK(manager.requestField(first_key, first_request).text.copy() == "captured");
	finish_frame();

	FLOWUI_CHECK(application.beginFrame());
	FLOWUI_CHECK(manager.enqueueCommand(TextCommand::RequestUndo));
	const auto manual_history = manager.requestField(first_key, first_request);
	FLOWUI_CHECK(manual_history.commandRequests.size() == 1);
	FLOWUI_CHECK(manual_history.commandRequests.front() == FieldCommandRequest::Undo);
	FLOWUI_CHECK(manager.removeField(first_key));
	FLOWUI_CHECK(manager.removeField(second_key));
	finish_frame();
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
#if FLOW_UI_DEV_MODE
		test_default_text_shortcuts(app);
#endif
		test_text_field_modes(app);
		test_text_command_pipeline(app);
		stage = "popup measurement, dismissal and pointer flags";
		test_popup_flag_lifecycle(app);
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
		FlowUi::WindowInputConfig rejectedInput = input;
		rejectedInput.stickyKeys = !input.stickyKeys;
		rejectedInput.stickyMouseButtons = !input.stickyMouseButtons;
		FlowUi::FlowUiError titleThreadError{};
		FlowUi::FlowUiError inputThreadError{};
		std::exception_ptr unexpectedSetterFailure;
		std::thread setterWorker([&] {
			try {
				app.setWindowTitle(first, "Rejected worker-thread title");
			} catch (const FlowUi::FlowUiException& exception) {
				titleThreadError = exception.error();
			} catch (...) {
				unexpectedSetterFailure = std::current_exception();
			}
			try {
				app.setWindowInputConfig(first, rejectedInput);
			} catch (const FlowUi::FlowUiException& exception) {
				inputThreadError = exception.error();
			} catch (...) {
				unexpectedSetterFailure = std::current_exception();
			}
		});
		setterWorker.join();
		if (unexpectedSetterFailure)
			std::rethrow_exception(unexpectedSetterFailure);
		FLOWUI_CHECK(titleThreadError.code == FlowUi::ErrorCode::WrongThread);
		FLOWUI_CHECK(inputThreadError.code == FlowUi::ErrorCode::WrongThread);
		const FlowUi::WindowInputConfig retainedInput = app.windowInputConfig(first);
		FLOWUI_CHECK(retainedInput.cursorMode == input.cursorMode);
		FLOWUI_CHECK(retainedInput.stickyKeys == input.stickyKeys);
		FLOWUI_CHECK(retainedInput.stickyMouseButtons == input.stickyMouseButtons);
		FLOWUI_CHECK(retainedInput.lockKeyMods == input.lockKeyMods);
		FLOWUI_CHECK(retainedInput.rawMouseMotion == input.rawMouseMotion);
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
		stage = "deferred error observer reentrant window destruction";
		test_deferred_error_observer_destroys_window(app, errorCapture);
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
