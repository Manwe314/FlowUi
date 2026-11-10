#define FLOWUI_INTERNAL_VIEWPORT_MANAGER 1
#include "FlowUi/Public/App.hpp"
#include "FlowUi/Private/AppRuntime.hpp"

namespace FlowUi {

using detail::app_runtime::runLocalOperation;

App::App() = default;

App::App(App&& other) noexcept : impl_(std::move(other.impl_)) {
	if (impl_) {
		impl_->elementManager.rebindOwner(*this);
		impl_->actionManager.rebindOwner(*this);
#if FLOW_UI_AGENTIC_WORKFLOW
		if (impl_->agentic_service)
			impl_->agentic_service->rebind_application(*this);
#endif
	}
}

App& App::operator=(App&& other) noexcept {
	if (this == &other)
		return *this;
	impl_ = std::move(other.impl_);
	if (impl_) {
		impl_->elementManager.rebindOwner(*this);
		impl_->actionManager.rebindOwner(*this);
#if FLOW_UI_AGENTIC_WORKFLOW
		if (impl_->agentic_service)
			impl_->agentic_service->rebind_application(*this);
#endif
	}
	return *this;
}

App::~App() = default;
#if FLOW_UI_AGENTIC_WORKFLOW
devSystems::agentic::AgenticWorkflowService& App::agentic_workflow() noexcept {
	return *impl_->agentic_service;
}
const devSystems::agentic::AgenticWorkflowService& App::agentic_workflow() const noexcept {
	return *impl_->agentic_service;
}
#endif

WindowId App::mainWindowId() const noexcept {
	return impl_ ? impl_->mainWindowId : MainWindowId;
}

void App::reportError(FlowUiError error) const noexcept {
	if (!impl_)
		return;
	detail::reportErrorEvent(ErrorEventView{
		.error = error,
		.kind = ErrorEventKind::Reported,
	});
}

Result<WindowId> App::createWindow(const WindowConfig& config) {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppCreateWindow));
	try {
		return impl_->createWindow(config);
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(exception.error());
		}
		return unexpectedError(exception.error());
	}
}

Result<WindowId> App::createWindowLikeMain() {
	return createWindow(WindowConfigOverrides{});
}

Result<WindowId> App::createWindow(const WindowConfigOverrides& overrides) {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppCreateWindow));
	WindowConfig config = mergeWindowConfig(impl_->config.window, overrides);
	if (!overrides.title) {
		config.title += " - Window " + std::to_string(impl_->nextWindowId);
	}
	return createWindow(config);
}

Result<WindowId> App::createWindow(std::string_view title, int width, int height) {
	WindowConfigOverrides overrides{};
	overrides.title = std::string(title);
	if (width != 0)
		overrides.width = width;
	if (height != 0)
		overrides.height = height;
	return createWindow(overrides);
}

Result<WindowId> App::createWindow(const WindowConfigOverrides& overrides, UiBuildCallback buildUi,
								   ManagedWindowFlags flags) {
	if (!buildUi) {
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	auto created = createWindow(overrides);
	if (!created)
		return created;
	Status registered = setWindowUiCallback(*created, std::move(buildUi), flags);
	if (!registered) {
		(void)destroyWindow(*created);
		return unexpectedError(registered.error());
	}
	return created;
}

Status App::setWindowUiCallback(WindowId id, UiBuildCallback buildUi, ManagedWindowFlags flags) {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppFrameDispatch));
	return runLocalOperation([&] {
		impl_->requirePlatformThread();
		impl_->requireQuiescent();
		(void)impl_->requireWindow(id);
		if (id == impl_->mainWindowId || !buildUi) {
			throw FlowUiException(
				makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppFrameDispatch, id));
		}
		const auto found =
			std::find_if(impl_->managedWindows.begin(), impl_->managedWindows.end(),
						 [id](const Impl::ManagedWindowEntry& entry) { return entry.id == id; });
		if (found != impl_->managedWindows.end()) {
			found->buildUi = std::move(buildUi);
			found->flags = flags;
			return;
		}
		impl_->managedWindows.push_back(Impl::ManagedWindowEntry{
			.id = id,
			.buildUi = std::move(buildUi),
			.flags = flags,
		});
	});
}

void App::removeWindowUiCallback(WindowId id) {
	if (!impl_)
		return;
	std::erase_if(impl_->managedWindows,
				  [id](const Impl::ManagedWindowEntry& entry) { return entry.id == id; });
}

Status App::dispatchManagedWindows() {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppFrameDispatch));
	try {
		impl_->requirePlatformThread();
		impl_->requireQuiescent();
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(exception.error());
		}
		if (exception.error().descriptor().category != ErrorCategory::Local)
			throw;
		return unexpectedError(exception.error());
	}

	std::vector<WindowId> recursive_dispatch_order;
	const bool recursive = impl_->dispatching_managed_windows;
	struct DispatchScope {
		bool& active;
		bool previous;
		~DispatchScope() { active = previous; }
	} dispatch_scope{impl_->dispatching_managed_windows,
					 std::exchange(impl_->dispatching_managed_windows, true)};
	auto& dispatchOrder = recursive ? recursive_dispatch_order : impl_->dispatch_order;
	FLOWUI_MEMORY_DEMAND_RECORD(impl_->capacity_demand,
								FlowUi::detail::InitialCapacityTarget::app_dispatch,
								dispatchOrder.size());
	dispatchOrder.clear();
	for (const Impl::ManagedWindowEntry& entry : impl_->managedWindows) {
		dispatchOrder.push_back(entry.id);
	}

	FlowUiError firstError{};
	const auto rememberError = [&](FlowUiError error) {
		if (!firstError)
			firstError = error;
	};
	const auto rememberException = [&](const FlowUiException& exception) {
		const FlowUiError error = exception.error();
		if (error.descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(error);
		}
		if (error.descriptor().category != ErrorCategory::Local)
			throw exception;
		rememberError(error);
	};

	for (WindowId id : dispatchOrder) {
		const auto managed =
			std::find_if(impl_->managedWindows.begin(), impl_->managedWindows.end(),
						 [id](const Impl::ManagedWindowEntry& entry) { return entry.id == id; });
		if (managed == impl_->managedWindows.end())
			continue;
		UiBuildCallback buildUi = managed->buildUi;
		const ManagedWindowFlags flags = managed->flags;
		const auto windowIt = impl_->windows.find(id);
		if (windowIt == impl_->windows.end()) {
			removeWindowUiCallback(id);
			continue;
		}
#if FLOW_UI_DEV_MODE
		if (windowIt->second->dev_suspended)
			continue;
#endif
		if (!windowIt->second->backend || windowIt->second->backend->shouldClose()) {
			if (flags.autoDestroyOnClose) {
				try {
					impl_->destroyWindow(id);
				} catch (const FlowUiException& exception) {
					rememberException(exception);
				}
			}
			continue;
		}

		try {
			impl_->beginFrame(id);
		} catch (const FlowUiException& exception) {
			rememberException(exception);
			continue;
		}

		try {
			buildUi(windowIt->second->ui, id);
		} catch (...) {
			const auto current = impl_->windows.find(id);
			if (current != impl_->windows.end())
				impl_->cancelStorageFrame(*current->second);
			const FlowUiError error =
				makeError(ErrorCode::UiBuildCallbackFailed, ErrorSite::AppFrameDispatch, id);
			reportError(error);
			rememberError(error);
			continue;
		}

		try {
			impl_->endFrame(id);
			impl_->drawFrame(id);
		} catch (const FlowUiException& exception) {
			rememberException(exception);
		}
	}

	if (firstError)
		return unexpectedError(firstError);
	return {};
}

Status App::destroyWindow(WindowId id) {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppDestroyWindow));
	return runLocalOperation([&] { impl_->destroyWindow(id); });
}

bool App::hasWindow(WindowId id) const noexcept {
	return impl_ && id != InvalidWindowId && impl_->windows.contains(id);
}

#if FLOW_UI_DEV_MODE
void App::request_dev_interface_toggle() noexcept {
	if (impl_)
		impl_->devInterface.request_toggle();
}
Status App::request_dev_performance_capture(
	const devSystems::PerformanceCaptureSettings& settings) noexcept {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const auto& toggle = impl_->config.dev.panelToggleChord;
	const auto& start = settings.start_chord;
	if (settings.start_mode == devSystems::PerformanceCaptureStartMode::Shortcut &&
		start.key == toggle.key && start.ctrl == toggle.ctrl && start.shift == toggle.shift &&
		start.alt == toggle.alt && start.super == toggle.super)
		return unexpectedError(makeError(ErrorCode::ShortcutInvalid, ErrorSite::ShortcutRegister));
	if (settings.end_mode == devSystems::PerformanceCaptureEndMode::BudgetEvent &&
		settings.budget_window &&
		(!impl_->windows.contains(settings.budget_window) ||
		 settings.budget_window == impl_->devInterface.windowId()))
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppAccessWindow));
#if FLOW_UI_AGENTIC_WORKFLOW
	if (impl_->agentic_service && impl_->agentic_service->capture_lease_active())
		return unexpectedError(makeError(ErrorCode::AgenticBusy, ErrorSite::AgenticCapture));
#endif
	return impl_->devMonitoring.performance_capture().request_capture(settings);
}
Status App::set_dev_window_suspended(WindowId window_id, bool suspended) {
	return runLocalOperation([&] {
		impl_->requirePlatformThread();
		impl_->requireQuiescent();
		auto& window = impl_->requireWindow(window_id);
		window.dev_suspended = suspended;
		window.backend->setShouldClose(0);
		auto* native = static_cast<GLFWwindow*>(window.backend->nativeHandle());
		if (suspended) {
			impl_->drainWindowGraphics(window);
			glfwHideWindow(native);
			window.inputQueue.clearKeyboardState();
			window.inputQueue.clearMouseButtonsState();
		} else {
			glfwShowWindow(native);
			glfwFocusWindow(native);
		}
	});
}
bool App::dev_shortcut_pressed(const DevShortcutChord& chord) const noexcept {
	if (!impl_ || chord.key < 32 || chord.key >= 349)
		return false;
	const auto modifiers =
		(chord.ctrl ? 1 : 0) | (chord.shift ? 2 : 0) | (chord.alt ? 4 : 0) | (chord.super ? 8 : 0);
	for (const auto& [window_id, window] : impl_->windows)
		if (!window->dev_suspended &&
#if FLOW_UI_AGENTIC_WORKFLOW
			!(impl_->agentic_service &&
			  impl_->agentic_service->interaction_controls_window(window_id)) &&
#endif
			(window->inputQueue.dev_shortcut_presses[static_cast<size_t>(chord.key)] &
			 (1u << modifiers)))
			return true;
	return false;
}
bool App::dev_shortcut_down(const DevShortcutChord& chord) const noexcept {
	if (!impl_ || chord.key < 32 || chord.key >= 349)
		return false;
	for (const auto& [window_id, window] : impl_->windows) {
#if FLOW_UI_AGENTIC_WORKFLOW
		if (impl_->agentic_service &&
			impl_->agentic_service->interaction_controls_window(window_id))
			continue;
#endif
		if (!window->backend || window->dev_suspended)
			continue;
		auto* native = static_cast<GLFWwindow*>(window->backend->nativeHandle());
		if (!native || glfwGetWindowAttrib(native, GLFW_FOCUSED) != GLFW_TRUE)
			continue;
		const auto down = [&](int key) { return glfwGetKey(native, key) == GLFW_PRESS; };
		if (down(chord.key) &&
			(down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL)) == chord.ctrl &&
			(down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT)) == chord.shift &&
			(down(GLFW_KEY_LEFT_ALT) || down(GLFW_KEY_RIGHT_ALT)) == chord.alt &&
			(down(GLFW_KEY_LEFT_SUPER) || down(GLFW_KEY_RIGHT_SUPER)) == chord.super)
			return true;
	}
	return false;
}
Status App::advance_dev_capture() {
	auto synchronized = impl_->devInterface.synchronize(*this);
	if (!synchronized)
		return synchronized;
	auto& capture = impl_->devMonitoring.performance_capture();
	const auto tick = impl_->appTick;
	if (capture.status().phase == devSystems::PerformanceCapturePhase::ClosingInterface &&
		(impl_->devInterface.windowId() == InvalidWindowId || impl_->devInterface.suspended()))
		capture.interface_closed(tick);
	const auto& settings = capture.status().settings;
	if (settings.end_mode == devSystems::PerformanceCaptureEndMode::BudgetEvent &&
		settings.budget_window && !impl_->windows.contains(settings.budget_window))
		capture.fail_capture(tick, impl_->devMonitoring.timingReporting().current_boundary_ns());
	const auto progress = capture.status();
	uint64_t pending = 0;
	if (progress.phase == devSystems::PerformanceCapturePhase::Finalizing) {
		for (auto& [window_id, window] : impl_->windows) {
			for (auto& frame : window->frames.frames) {
				auto& timing_slot = frame.gpuTiming;
				if (!timing_slot.submitted || timing_slot.appTick < progress.first_app_tick ||
					timing_slot.appTick >= progress.end_app_tick_exclusive)
					continue;
				if (vkGetFenceStatus(impl_->vk.device, frame.inFlight) == VK_SUCCESS)
					impl_->devMonitoring.gpuTiming().resolveCompleted(impl_->vk, timing_slot);
				else
					++pending;
			}
		}
		pending += impl_->devMonitoring.timing().pending_scope_count(
			progress.first_app_tick, progress.end_app_tick_exclusive);
		impl_->devMonitoring.timingReporting().consumeThrough(tick);
	}
	capture.advance_tick(tick, impl_->devMonitoring.timingReporting().current_boundary_ns(),
						 impl_->devInterface.windowId() != InvalidWindowId &&
							 !impl_->devInterface.suspended(),
						 impl_->devInterface.take_start_request(), pending);
#if FLOW_UI_AGENTIC_WORKFLOW
	if (impl_->agentic_service) {
		for (auto& [window_id, window] : impl_->windows)
			for (uint32_t frame_slot = 0; frame_slot < window->frames.frames.size(); ++frame_slot)
				if (vkGetFenceStatus(impl_->vk.device,
									 window->frames.frames[frame_slot].inFlight) == VK_SUCCESS)
					devSystems::agentic::frame_capture::complete_slot(impl_->vk, window_id,
																	  frame_slot);
		impl_->advancing_agentic = true;
		impl_->agentic_service->advance_safe_point(
			{tick, impl_->devMonitoring.timingReporting().current_boundary_ns(),
			 devSystems::agentic::frame_capture::pending_bytes(impl_->vk),
			 devSystems::agentic::frame_capture::failure_count(impl_->vk)});
		impl_->advancing_agentic = false;
	}
#endif
	if (capture.status().phase == devSystems::PerformanceCapturePhase::Sealed)
		return impl_->devInterface.synchronize(*this);
	return {};
}
#endif

Status App::pollEvents() {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return runLocalOperation([&] {
		impl_->pollEventsAndAdvanceSharedManagers();
#if FLOW_UI_DEV_MODE
		auto capture_status = advance_dev_capture();
		if (!capture_status)
			throw FlowUiException(capture_status.error());
#endif
	});
}

bool App::shouldClose() const {
	if (!impl_)
		return true;
	return shouldClose(impl_->mainWindowId);
}

bool App::shouldClose(WindowId id) const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	return !window.backend || window.backend->shouldClose();
}

void App::setShouldClose(int value) {
	if (!impl_)
		return;
	setShouldClose(impl_->mainWindowId, value);
}

void App::setShouldClose(WindowId id, int value) {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	AppWindow& window = impl_->requireWindow(id);
	if (window.backend)
		window.backend->setShouldClose(value);
}

Status App::beginFrame() {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppBeginFrame));
	return runLocalOperation([&] {
		impl_->pollEventsAndAdvanceSharedManagers();
#if FLOW_UI_DEV_MODE
		auto capture_status = advance_dev_capture();
		if (!capture_status)
			throw FlowUiException(capture_status.error());
#endif
		impl_->beginFrame(impl_->mainWindowId);
	});
}

Status App::beginFrame(WindowId id) {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppBeginFrame));
	return runLocalOperation([&] { impl_->beginFrame(id); });
}

Status App::endFrame() {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppEndFrame));
	return runLocalOperation([&] { impl_->endFrame(impl_->mainWindowId); });
}

Status App::endFrame(WindowId id) {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppEndFrame));
	return runLocalOperation([&] { impl_->endFrame(id); });
}

Status App::drawFrame() {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppDrawFrame));
	Status mainStatus = drawFrame(impl_->mainWindowId);
	if (!mainStatus)
		return mainStatus;
	return dispatchManagedWindows();
}

Status App::drawFrame(WindowId id) {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppDrawFrame));
	return runLocalOperation([&] { impl_->drawFrame(id); });
}

FontManager& App::fonts() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessFonts));
	}
	return impl_->fonts;
}

const FontManager& App::fonts() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessFonts));
	}
	return impl_->fonts;
}

ImageManager& App::images() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessImages));
	}
	return impl_->imageManager;
}

const ImageManager& App::images() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessImages));
	}
	return impl_->imageManager;
}

ThemeManager& App::themes() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessThemes));
	}
	return impl_->themeManager;
}

const ThemeManager& App::themes() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessThemes));
	}
	return impl_->themeManager;
}

ElementManager& App::elements() {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessElements));
	return impl_->elementManager;
}

const ElementManager& App::elements() const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessElements));
	return impl_->elementManager;
}

ActionManager& App::actions() {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessActions));
	return impl_->actionManager;
}

const ActionManager& App::actions() const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessActions));
	return impl_->actionManager;
}

#if FLOW_UI_DEV_MODE
devSystems::DevMonitoringAndReporting& App::devMonitoring() {
	if (!impl_)
		throw FlowUiException(
			makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return impl_->devMonitoring;
}

const devSystems::DevMonitoringAndReporting& App::devMonitoring() const {
	if (!impl_)
		throw FlowUiException(
			makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return impl_->devMonitoring;
}

devSystems::DevTooling& App::devTooling() {
	if (!impl_)
		throw FlowUiException(
			makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return impl_->devTooling;
}

const devSystems::DevTooling& App::devTooling() const {
	if (!impl_)
		throw FlowUiException(
			makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return impl_->devTooling;
}

std::vector<DevWindowInfo> App::devWindowSnapshot() const {
	if (!impl_)
		return {};
	std::vector<DevWindowInfo> result;
	result.reserve(impl_->windows.size());
	for (const auto& [id, window] : impl_->windows) {
		if (!window)
			continue;
		result.push_back(DevWindowInfo{
			.id = id,
			.title = window->config.native.title,
			.framesInFlight = window->config.vulkan.framesInFlight,
			.actual_present_mode = static_cast<uint32_t>(window->swapchain.swapchain.present_mode),
			.layout_dpi = window->config.ui.dpi,
			.font_scale = window->config.ui.fontScale,
		});
	}
	std::ranges::sort(result, {}, &DevWindowInfo::id);
	return result;
}

std::vector<DevViewportInfo> App::devViewportSnapshot(WindowId id) const {
	std::vector<DevViewportInfo> result;
	if (!impl_)
		return result;
	const auto found = impl_->windows.find(id);
	if (found == impl_->windows.end() || !found->second)
		return result;
	found->second->viewPorts.visitDevViewports(
		&result, [](void* target, const ViewPortManager::DevViewportView& view) {
			static_cast<std::vector<DevViewportInfo>*>(target)->emplace_back(
				DevViewportInfo{.key = std::string(view.key),
								.capture_texture = view.capture_texture,
								.generation = view.generation,
								.captured_frame = view.captured_frame,
								.captured_generation = view.captured_generation,
								.captured_submission = view.captured_submission,
								.width = view.width,
								.height = view.height,
								.format = view.format,
								.captured_width = view.captured_width,
								.captured_height = view.captured_height,
								.captured_format = view.captured_format,
								.has_callback = view.has_callback,
								.clear_every_frame = view.clear_every_frame,
								.capture_supported = view.capture_supported,
								.capture_pending = view.capture_pending,
								.capture_failed = view.capture_failed});
			return true;
		});
	std::ranges::sort(result, {}, &DevViewportInfo::key);
	return result;
}

Status App::request_dev_viewport_capture(WindowId window, std::string_view key) noexcept {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::ViewportPrepare));
	const auto found = impl_->windows.find(window);
	if (found == impl_->windows.end() || !found->second)
		return unexpectedError(makeError(ErrorCode::ViewportNotFound, ErrorSite::ViewportPrepare));
	return found->second->viewPorts.request_dev_capture(key);
}

void App::release_dev_viewport_capture(WindowId window, std::string_view key) noexcept {
	if (!impl_)
		return;
	const auto found = impl_->windows.find(window);
	if (found != impl_->windows.end() && found->second)
		found->second->viewPorts.release_dev_capture(key);
}

Result<TextureHandle> App::retain_dev_viewport_capture(WindowId window, std::string_view key,
													   TextureHandle expected_capture) noexcept {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::ViewportPublish));
	const auto found = impl_->windows.find(window);
	if (found == impl_->windows.end() || !found->second)
		return unexpectedError(makeError(ErrorCode::ViewportNotFound, ErrorSite::ViewportPublish));
	return found->second->viewPorts.retain_dev_capture(key, expected_capture);
}

Status App::release_dev_viewport_capture_texture(TextureHandle texture) noexcept {
	if (!impl_ || !impl_->storageSystem)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::ViewportPublish));
	try {
		impl_->storageSystem->releaseAnonymousTexture(texture);
		return {};
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::ViewportPublish));
	}
}

devSystems::DevUiReplaySource App::devUiReplaySource(WindowId id) noexcept {
	if (!impl_)
		return {};
	const auto found = impl_->windows.find(id);
	if (found == impl_->windows.end() || !found->second)
		return {};
	AppWindow& window = *found->second;
	const float configuredDpi = std::max(1.0f, window.config.ui.dpi);
	float pointsToPixelsScale =
		std::max(0.0f, window.config.ui.fontScale) * (configuredDpi / 72.0f);
	if (pointsToPixelsScale <= 0.0f)
		pointsToPixelsScale = configuredDpi / 72.0f;
	return devSystems::DevUiReplaySource{
		.renderer = &window.renderer,
		.fontFrameView = &window.fontFrameView,
		.commands = &window.devReplayPacket.commandArray,
		.textureBindings = window.devReplayPacket.textureBindings,
		.sourceRootBounds = window.devReplayPacket.sourceRootBounds,
		.requestKey = window.devReplayPacket.requestKey,
		.captureSerial = window.devReplayPacket.captureSerial,
		.textureFrameSlot = window.devReplayPacket.textureFrameSlot,
		.pointsToPixelsScale = pointsToPixelsScale,
	};
}

void App::requestDevUiReplay(WindowId id, uint64_t requestKey,
							 std::span<const uint32_t> clayCommandIds,
							 Clay_BoundingBox sourceRootBounds) {
	if (!impl_)
		return;
	const auto found = impl_->windows.find(id);
	if (found == impl_->windows.end() || !found->second)
		return;
	AppWindow& window = *found->second;
	if (clayCommandIds.empty() || requestKey == 0u) {
		window.devReplayRequest = {};
		window.devReplayPacket.clearCommands();
		window.devReplayPacket.requestKey = 0u;
		return;
	}
	if (requestKey != window.devReplayPacket.requestKey) {
		window.devReplayPacket.clearCommands();
		window.devReplayPacket.requestKey = 0u;
	}
	window.devReplayRequest.rootCommandId = clayCommandIds.front();
	window.devReplayRequest.commandIds.assign(clayCommandIds.begin(), clayCommandIds.end());
	std::ranges::sort(window.devReplayRequest.commandIds);
	const auto uniqueEnd = std::unique(window.devReplayRequest.commandIds.begin(),
									   window.devReplayRequest.commandIds.end());
	window.devReplayRequest.commandIds.erase(uniqueEnd, window.devReplayRequest.commandIds.end());
	window.devReplayRequest.sourceRootBounds = sourceRootBounds;
	window.devReplayRequest.key = requestKey;
}
#endif

#if FLOWUI_INCLUDE_ICON_MANAGER
IconManager& App::icons() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessIcons));
	}
	return impl_->icons;
}

const IconManager& App::icons() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessIcons));
	}
	return impl_->icons;
}
#endif

#if FLOWUI_PUBLIC_VULKAN_INTEROP
ViewPortManager& App::viewPorts() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	}
	return impl_->mainWindow().viewPorts;
}

ViewPortManager& App::viewPorts(WindowId id) {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	return impl_->requireWindow(id).viewPorts;
}

const ViewPortManager& App::viewPorts() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	}
	return impl_->mainWindow().viewPorts;
}

const ViewPortManager& App::viewPorts(WindowId id) const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	return impl_->requireWindow(id).viewPorts;
}
#endif

UiManager& App::ui() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	}
	return impl_->mainWindow().ui;
}

UiManager& App::ui(WindowId id) {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	return impl_->requireWindow(id).ui;
}

const UiManager& App::ui() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	}
	return impl_->mainWindow().ui;
}

const UiManager& App::ui(WindowId id) const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	return impl_->requireWindow(id).ui;
}

void App::setWindowTitle(std::string_view title) {
	if (!impl_)
		return;
	setWindowTitle(impl_->mainWindowId, title);
}

void App::setWindowTitle(WindowId id, std::string_view title) {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::WindowSetTitle));
	AppWindow& window = impl_->requireWindow(id);
	window.config.native.title.assign(title);
	if (window.backend)
		window.backend->setTitle(title);
}

void* App::nativeWindowHandle() const {
	if (!impl_)
		return nullptr;
	return nativeWindowHandle(impl_->mainWindowId);
}

void* App::nativeWindowHandle(WindowId id) const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend ? window.backend->nativeHandle() : nullptr;
}

void App::setWindowInputConfig(const WindowInputConfig& config) {
	if (!impl_)
		return;
	setWindowInputConfig(impl_->mainWindowId, config);
}

void App::setWindowInputConfig(WindowId id, const WindowInputConfig& config) {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	AppWindow& window = impl_->requireWindow(id);
	window.config.native.input = config;
	if (window.backend)
		window.backend->setInputConfig(config);
}

WindowInputConfig App::windowInputConfig() const {
	if (!impl_)
		return {};
	return windowInputConfig(impl_->mainWindowId);
}

WindowInputConfig App::windowInputConfig(WindowId id) const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend ? window.backend->getInputConfig() : WindowInputConfig{};
}

bool App::supportsRawMouseMotion() const {
	if (!impl_)
		return false;
	return supportsRawMouseMotion(impl_->mainWindowId);
}

bool App::supportsRawMouseMotion(WindowId id) const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend && window.backend->supportsRawMouseMotion();
}

void App::setClipboardText(std::string_view text) {
	if (!impl_)
		return;
	setClipboardText(impl_->mainWindowId, text);
}

void App::setClipboardText(WindowId id, std::string_view text) {
	if (!impl_)
		throw FlowUiException(
			makeError(ErrorCode::AppUnavailable, ErrorSite::WindowWriteClipboard));
	AppWindow& window = impl_->requireWindow(id);
	if (window.backend)
		window.backend->setClipboardText(text);
}

std::string App::clipboardText() const {
	if (!impl_)
		return {};
	return clipboardText(impl_->mainWindowId);
}

std::string App::clipboardText(WindowId id) const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::WindowReadClipboard));
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend ? window.backend->getClipboardText() : std::string{};
}

std::pair<int, int> App::windowSize() const {
	if (!impl_)
		return {0, 0};
	return windowSize(impl_->mainWindowId);
}

std::pair<int, int> App::windowSize(WindowId id) const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	if (!window.backend)
		return {0, 0};
	const VkExtent2D extent = window.backend->windowExtent();
	return {static_cast<int>(extent.width), static_cast<int>(extent.height)};
}

std::pair<int, int> App::framebufferSize() const {
	if (!impl_)
		return {0, 0};
	return framebufferSize(impl_->mainWindowId);
}

std::pair<int, int> App::framebufferSize(WindowId id) const {
	if (!impl_)
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	if (!window.backend)
		return {0, 0};
	const VkExtent2D extent = window.backend->framebufferExtent();
	return {static_cast<int>(extent.width), static_cast<int>(extent.height)};
}

App makeApplication(const AppConfig& cfg) {
	App app;
	AppConfig effective_config = cfg;
	const bool use_baked = cfg.prefer_baked_capacities && baked::has_baked_memory_capacity();
	auto resolved = FlowUi::detail::resolve_memory_capacity_profile(
		cfg.memoryCapacityProfile, use_baked ? baked::baked_memory_capacity_settings()
											 : std::span<const MemoryCapacitySetting>{});
	if (!resolved)
		throw FlowUiException(resolved.error());
	effective_config.memoryCapacityProfile = std::move(*resolved);
#if FLOW_UI_HAS_BAKED_CHANGES && __has_include("FlowUi/Private/BakedChanges.hpp")
	if (use_baked)
		baked::apply_baked_memory_policy(effective_config.memoryCapacityProfile);
#endif
#if FLOW_UI_DEV_MODE
	for (const auto& setting : effective_config.memoryCapacityProfile.settings) {
		const auto* definition = FlowUi::detail::find_initial_capacity(setting.target);
		const auto value = setting.value ? setting.value : definition->default_value;
		switch (definition->target) {
		case FlowUi::detail::InitialCapacityTarget::development_tree_flow_nodes:
			effective_config.dev.tooling.treeCapture.flowNodeReserve = static_cast<uint32_t>(value);
			break;
		case FlowUi::detail::InitialCapacityTarget::development_tree_strings:
			effective_config.dev.tooling.treeCapture.stringByteReserve =
				static_cast<uint32_t>(value);
			break;
		case FlowUi::detail::InitialCapacityTarget::development_tree_diagnostics:
			effective_config.dev.tooling.treeCapture.diagnosticReserve =
				static_cast<uint32_t>(value);
			break;
		default:
			break;
		}
	}
#endif
	app.impl_ = std::make_unique<App::Impl>(effective_config);
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE(app.impl_->timingRecorder(), devSystems::TimingCategory::Lifecycle,
						   devSystems::TimingZoneRole::Work, "flowui.app.make");
#endif
	app.impl_->init(app);
	return app;
}

} // namespace FlowUi
