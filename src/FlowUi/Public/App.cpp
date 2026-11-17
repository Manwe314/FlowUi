#define FLOWUI_INTERNAL_VIEWPORT_MANAGER 1
#include "FlowUi/Public/App.hpp"
#include "FlowUi/Private/AppRuntime.hpp"
#include "FlowUi/Private/ApiBoundary.hpp"

namespace FlowUi {

using detail::app_runtime::requireSuccess;
using detail::app_runtime::runLocalOperation;

App::App() noexcept = default;

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

App::~App() noexcept = default;
#if FLOW_UI_AGENTIC_WORKFLOW
Result<std::reference_wrapper<devSystems::agentic::AgenticWorkflowService>> App::agentic_workflow() noexcept try {
	if (!impl_ || !impl_->agentic_service)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return std::ref(*impl_->agentic_service);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}
Result<std::reference_wrapper<const devSystems::agentic::AgenticWorkflowService>> App::agentic_workflow() const noexcept try {
	if (!impl_ || !impl_->agentic_service)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return std::cref(*impl_->agentic_service);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
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

Result<WindowId> App::createWindow(const WindowConfig& config) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppCreateWindow));
	try {
		return impl_->createWindow(config);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppCreateWindow));
}

Result<WindowId> App::createWindowLikeMain() noexcept try {
	return createWindow(WindowConfigOverrides{});
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppCreateWindow));
}

Result<WindowId> App::createWindow(const WindowConfigOverrides& overrides) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppCreateWindow));
	WindowConfig config = mergeWindowConfig(impl_->config.window, overrides);
	if (!overrides.title) {
		config.title += " - Window " + std::to_string(impl_->nextWindowId);
	}
	return createWindow(config);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppCreateWindow));
}

Result<WindowId> App::createWindow(std::string_view title, int width, int height) noexcept try {
	WindowConfigOverrides overrides{};
	overrides.title = std::string(title);
	if (width != 0)
		overrides.width = width;
	if (height != 0)
		overrides.height = height;
	return createWindow(overrides);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppCreateWindow));
}

Result<WindowId> App::createWindow(const WindowConfigOverrides& overrides, const UiBuildCallback& buildUi,
								   ManagedWindowFlags flags) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppCreateWindow));
	impl_->requirePlatformThread();
	impl_->requireQuiescent();
	if (!buildUi) {
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	// Prepare every allocating callback-registration step before publishing native state.
	Impl::ManagedWindowEntry managed{.buildUi = buildUi, .flags = flags};
	impl_->managedWindows.reserve(impl_->managedWindows.size() + 1u);
	auto created = createWindow(overrides);
	if (!created)
		return created;
	managed.id = *created;
	impl_->managedWindows.emplace_back(std::move(managed));
	return created;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppCreateWindow));
}

Status App::setWindowUiCallback(WindowId id, const UiBuildCallback& buildUi, ManagedWindowFlags flags) noexcept try {
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
		UiBuildCallback preparedCallback = buildUi;
		if (found != impl_->managedWindows.end()) {
			found->buildUi = std::move(preparedCallback);
			found->flags = flags;
			return;
		}
		impl_->managedWindows.push_back(Impl::ManagedWindowEntry{
			.id = id,
			.buildUi = std::move(preparedCallback),
			.flags = flags,
		});
	}, ErrorSite::AppFrameDispatch);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppFrameDispatch));
}

Status App::removeWindowUiCallback(WindowId id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	impl_->requirePlatformThread();
	std::erase_if(impl_->managedWindows,
				  [id](const Impl::ManagedWindowEntry& entry) { return entry.id == id; });
	return {};
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::dispatchManagedWindows() noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppFrameDispatch));
	try {
		impl_->requirePlatformThread();
		impl_->requireQuiescent();
		impl_->requireGraphicsRunning();
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category != ErrorCategory::Local)
			throw;
		return unexpectedError(exception.error());
	}

	std::vector<WindowId> recursive_dispatch_order;
	const bool recursive =
		impl_->dispatch_flags.has_flag(Impl::DispatchFlag::dispatching_managed_windows);
	struct DispatchScope {
		detail::EnumFlags<Impl::DispatchFlag>& flags;
		bool previous;
		~DispatchScope() {
			flags.set_flag(Impl::DispatchFlag::dispatching_managed_windows, previous);
		}
	} dispatch_scope{impl_->dispatch_flags, recursive};
	impl_->dispatch_flags.set_flag(Impl::DispatchFlag::dispatching_managed_windows);
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
			requireSuccess(removeWindowUiCallback(id));
			continue;
		}
		if (windowIt->second->phase == AppWindow::Phase::Stopped ||
			windowIt->second->phase == AppWindow::Phase::Closing)
			continue;
#if FLOW_UI_DEV_MODE
		if (windowIt->second->dev_suspended)
			continue;
#endif
		if (impl_->platformService.shouldClose(windowIt->second->platformState)) {
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
			FlowUiError error = detail::currentExceptionError(ErrorSite::AppFrameDispatch);
			const auto current = impl_->windows.find(id);
			if (current != impl_->windows.end()) {
				if (const auto status = current->second->ui.frameStatus(); !status)
					error = status.error();
				impl_->cancelStorageFrame(*current->second);
			}
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
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppFrameDispatch));
}

Status App::runFrame(const UiBuildCallback& buildUi) noexcept {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppBeginFrame));
	if (!buildUi)
		return unexpectedError(makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppFrameDispatch));
	const auto begun = beginFrame();
	if (!begun)
		return begun;
	const WindowId id = impl_->mainWindowId;
	try {
		auto& window = impl_->requireWindow(id);
		buildUi(window.ui, id);
		const auto built = window.ui.frameStatus();
		if (!built) {
			impl_->cancelStorageFrame(window);
			return built;
		}
		const auto ended = endFrame(id);
		if (!ended)
			return ended;
		return drawFrame();
	} catch (...) {
		auto error = detail::currentExceptionError(ErrorSite::AppFrameDispatch);
		const auto found = impl_->windows.find(id);
		if (found != impl_->windows.end()) {
			if (const auto status = found->second->ui.frameStatus(); !status)
				error = status.error();
			impl_->cancelStorageFrame(*found->second);
		}
		return unexpectedError(error);
	}
}

Status App::runFrame(WindowId id, const UiBuildCallback& buildUi) noexcept {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppBeginFrame));
	if (!buildUi)
		return unexpectedError(makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppFrameDispatch));
	const auto begun = beginFrame(id);
	if (!begun)
		return begun;
	try {
		auto& window = impl_->requireWindow(id);
		buildUi(window.ui, id);
		const auto built = window.ui.frameStatus();
		if (!built) {
			impl_->cancelStorageFrame(window);
			return built;
		}
		const auto ended = endFrame(id);
		if (!ended)
			return ended;
		return drawFrame(id);
	} catch (...) {
		auto error = detail::currentExceptionError(ErrorSite::AppFrameDispatch);
		const auto found = impl_->windows.find(id);
		if (found != impl_->windows.end()) {
			if (const auto status = found->second->ui.frameStatus(); !status)
				error = status.error();
			impl_->cancelStorageFrame(*found->second);
		}
		return unexpectedError(error);
	}
}

Status App::destroyWindow(WindowId id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppDestroyWindow));
	return runLocalOperation([&] { impl_->destroyWindow(id); }, ErrorSite::AppDestroyWindow);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppDestroyWindow));
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
	const devSystems::PerformanceCaptureSettings& settings) noexcept try {
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
	if (impl_->agentic_service && detail::requireApiValue(impl_->agentic_service->capture_lease_active()))
		return unexpectedError(makeError(ErrorCode::AgenticBusy, ErrorSite::AgenticCapture));
#endif
	return impl_->devMonitoring.performance_capture().request_capture(settings);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}
Status App::set_dev_window_suspended(WindowId window_id, bool suspended) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return runLocalOperation([&] {
		impl_->requirePlatformThread();
		impl_->requireQuiescent();
		auto& window = impl_->requireWindow(window_id);
		if (suspended)
			impl_->drainWindowGraphics(window);
		impl_->platformService.setShouldClose(window.platformState, false);
		requireSuccess(impl_->platformService.setVisible(window.platformState, !suspended));
		window.dev_suspended = suspended;
		impl_->inputService.setTargetEnabled(window.id, !suspended);
		if (!suspended)
			requireSuccess(impl_->platformService.requestFocus(window.platformState));
	}, ErrorSite::AppAccessWindow);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}
Result<bool> App::dev_shortcut_pressed(const DevShortcutChord& chord) const noexcept try {
	if (!impl_ || chord.key < 32 || chord.key >= 349)
		return false;
	const auto modifiers =
		(chord.ctrl ? 1 : 0) | (chord.shift ? 2 : 0) | (chord.alt ? 4 : 0) | (chord.super ? 8 : 0);
	for (const auto& [window_id, window] : impl_->windows)
		if (!window->dev_suspended &&
#if FLOW_UI_AGENTIC_WORKFLOW
			!(impl_->agentic_service &&
			  detail::requireApiValue(impl_->agentic_service->interaction_controls_window(window_id))) &&
#endif
			(window->inputState.queue().dev_shortcut_presses[static_cast<size_t>(chord.key)] &
			 (1u << modifiers)))
			return true;
	return false;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}
Result<bool> App::dev_shortcut_down(const DevShortcutChord& chord) const noexcept try {
	if (!impl_ || chord.key < 32 || chord.key >= 349)
		return false;
	for (const auto& [window_id, window] : impl_->windows) {
#if FLOW_UI_AGENTIC_WORKFLOW
		if (impl_->agentic_service &&
			detail::requireApiValue(impl_->agentic_service->interaction_controls_window(window_id)))
			continue;
#endif
		if (window->dev_suspended)
			continue;
		if (!impl_->platformService.isFocused(window->platformState))
			continue;
		const auto down = [&](int key) noexcept {
			const auto semanticKey = keyFromCode(key);
			return semanticKey && impl_->inputService.keyDown(window->inputState, *semanticKey);
		};
		if (down(chord.key) &&
			(down(keyCode(Key::LeftControl)) || down(keyCode(Key::RightControl))) == chord.ctrl &&
			(down(keyCode(Key::LeftShift)) || down(keyCode(Key::RightShift))) == chord.shift &&
			(down(keyCode(Key::LeftAlt)) || down(keyCode(Key::RightAlt))) == chord.alt &&
			(down(keyCode(Key::LeftSuper)) || down(keyCode(Key::RightSuper))) == chord.super)
			return true;
	}
	return false;
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}
Status App::advance_dev_capture() noexcept try {
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
		impl_->dispatch_flags.set_flag(Impl::DispatchFlag::advancing_agentic, true);
		impl_->agentic_service->advance_safe_point(
			{tick, impl_->devMonitoring.timingReporting().current_boundary_ns(),
			 devSystems::agentic::frame_capture::pending_bytes(impl_->vk),
			 devSystems::agentic::frame_capture::failure_count(impl_->vk)});
		impl_->dispatch_flags.set_flag(Impl::DispatchFlag::advancing_agentic, false);
	}
#endif
	if (capture.status().phase == devSystems::PerformanceCapturePhase::Sealed)
		return impl_->devInterface.synchronize(*this);
	return {};
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}
#endif

Status App::pollEvents() noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return runLocalOperation([&] {
		impl_->pollEventsAndAdvanceSharedManagers();
#if FLOW_UI_DEV_MODE
		auto capture_status = advance_dev_capture();
		if (!capture_status)
			throw FlowUiException(capture_status.error());
#endif
	}, ErrorSite::AppPollEvents);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppPollEvents));
}

Result<bool> App::shouldClose() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return shouldClose(impl_->mainWindowId);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<bool> App::shouldClose(WindowId id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	impl_->requirePlatformThread();
	const auto found = impl_->windows.find(id);
	if (found != impl_->windows.end() && found->second &&
		(found->second->phase == AppWindow::Phase::Stopped ||
		 found->second->phase == AppWindow::Phase::Closing))
		return true;
	const AppWindow& window = impl_->requireWindow(id);
	return impl_->platformService.shouldClose(window.platformState);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::setShouldClose(int value) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return setShouldClose(impl_->mainWindowId, value);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::setShouldClose(WindowId id, int value) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	impl_->requirePlatformThread();
	AppWindow& window = impl_->requireWindow(id);
	impl_->platformService.setShouldClose(window.platformState, value != 0);
	return {};
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<WindowOperationCapabilities> App::window_operation_capabilities(WindowId window_id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return impl_->window_operation_capabilities(window_id);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::request_window_close(WindowId window_id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return impl_->request_window_operation(window_id, Impl::NativeWindowOperation::Close);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::minimize_window(WindowId window_id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return impl_->request_window_operation(window_id, Impl::NativeWindowOperation::Minimize);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::toggle_window_maximized(WindowId window_id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return impl_->request_window_operation(window_id, Impl::NativeWindowOperation::MaximizeRestore);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::toggle_window_fullscreen(WindowId window_id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return impl_->request_window_operation(window_id, Impl::NativeWindowOperation::Fullscreen);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::beginFrame() noexcept try {
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
	}, ErrorSite::AppBeginFrame);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppBeginFrame));
}

Status App::beginFrame(WindowId id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppBeginFrame));
	return runLocalOperation([&] { impl_->beginFrame(id); }, ErrorSite::AppBeginFrame);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppBeginFrame));
}

Status App::endFrame() noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppEndFrame));
	return runLocalOperation([&] { impl_->endFrame(impl_->mainWindowId); }, ErrorSite::AppEndFrame);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppEndFrame));
}

Status App::endFrame(WindowId id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppEndFrame));
	return runLocalOperation([&] { impl_->endFrame(id); }, ErrorSite::AppEndFrame);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppEndFrame));
}

Status App::drawFrame() noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppDrawFrame));
	Status mainStatus = drawFrame(impl_->mainWindowId);
	if (!mainStatus)
		return mainStatus;
	return dispatchManagedWindows();
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppDrawFrame));
}

Status App::drawFrame(WindowId id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppDrawFrame));
	return runLocalOperation([&] { impl_->drawFrame(id); }, ErrorSite::AppDrawFrame);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppDrawFrame));
}

Result<std::reference_wrapper<FontManager>> App::fonts() noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessFonts));
	}
	return std::ref(impl_->fonts);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessFonts));
}

Result<std::reference_wrapper<const FontManager>> App::fonts() const noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessFonts));
	}
	return std::cref(impl_->fonts);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessFonts));
}

Result<std::reference_wrapper<ImageManager>> App::images() noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessImages));
	}
	return std::ref(impl_->imageManager);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessImages));
}

Result<std::reference_wrapper<const ImageManager>> App::images() const noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessImages));
	}
	return std::cref(impl_->imageManager);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessImages));
}

Result<std::reference_wrapper<ThemeManager>> App::themes() noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessThemes));
	}
	return std::ref(impl_->themeManager);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessThemes));
}

Result<std::reference_wrapper<const ThemeManager>> App::themes() const noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessThemes));
	}
	return std::cref(impl_->themeManager);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessThemes));
}

Result<std::reference_wrapper<ElementManager>> App::elements() noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessElements));
	return std::ref(impl_->elementManager);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessElements));
}

Result<std::reference_wrapper<const ElementManager>> App::elements() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessElements));
	return std::cref(impl_->elementManager);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessElements));
}

Result<std::reference_wrapper<ActionManager>> App::actions() noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessActions));
	return std::ref(impl_->actionManager);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessActions));
}

Result<std::reference_wrapper<const ActionManager>> App::actions() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessActions));
	return std::cref(impl_->actionManager);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessActions));
}

#if FLOW_UI_DEV_MODE
Result<std::reference_wrapper<devSystems::DevMonitoringAndReporting>> App::devMonitoring() noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return std::ref(impl_->devMonitoring);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessDevMonitoring));
}

Result<std::reference_wrapper<const devSystems::DevMonitoringAndReporting>> App::devMonitoring() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return std::cref(impl_->devMonitoring);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessDevMonitoring));
}

Result<std::reference_wrapper<devSystems::DevTooling>> App::devTooling() noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return std::ref(impl_->devTooling);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessDevMonitoring));
}

Result<std::reference_wrapper<const devSystems::DevTooling>> App::devTooling() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return std::cref(impl_->devTooling);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessDevMonitoring));
}

Result<std::vector<DevWindowInfo>> App::devWindowSnapshot() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
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
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<std::vector<DevViewportInfo>> App::devViewportSnapshot(WindowId id) const noexcept try {
	std::vector<DevViewportInfo> result;
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const auto found = impl_->windows.find(id);
	if (found == impl_->windows.end() || !found->second)
		return unexpectedError(makeError(ErrorCode::InvalidWindowId, ErrorSite::AppAccessWindow, id));
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
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::request_dev_viewport_capture(WindowId window, std::string_view key) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::ViewportPrepare));
	const auto found = impl_->windows.find(window);
	if (found == impl_->windows.end() || !found->second)
		return unexpectedError(makeError(ErrorCode::ViewportNotFound, ErrorSite::ViewportPrepare));
	return found->second->viewPorts.request_dev_capture(key);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

void App::release_dev_viewport_capture(WindowId window, std::string_view key) noexcept {
	if (!impl_)
		return;
	const auto found = impl_->windows.find(window);
	if (found != impl_->windows.end() && found->second)
		found->second->viewPorts.release_dev_capture(key);
}

Result<TextureHandle> App::retain_dev_viewport_capture(WindowId window, std::string_view key,
													   TextureHandle expected_capture) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::ViewportPublish));
	const auto found = impl_->windows.find(window);
	if (found == impl_->windows.end() || !found->second)
		return unexpectedError(makeError(ErrorCode::ViewportNotFound, ErrorSite::ViewportPublish));
	return found->second->viewPorts.retain_dev_capture(key, expected_capture);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::release_dev_viewport_capture_texture(TextureHandle texture) noexcept try {
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
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
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

Status App::requestDevUiReplay(WindowId id, uint64_t requestKey,
							 std::span<const uint32_t> clayCommandIds,
							 Clay_BoundingBox sourceRootBounds) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const auto found = impl_->windows.find(id);
	if (found == impl_->windows.end() || !found->second)
		return unexpectedError(makeError(ErrorCode::InvalidWindowId, ErrorSite::AppAccessWindow, id));
	AppWindow& window = *found->second;
	if (clayCommandIds.empty() || requestKey == 0u) {
		window.devReplayRequest = {};
		window.devReplayPacket.clearCommands();
		window.devReplayPacket.requestKey = 0u;
		return {};
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
	return {};
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}
#endif

#if FLOWUI_INCLUDE_ICON_MANAGER
Result<std::reference_wrapper<IconManager>> App::icons() noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessIcons));
	}
	return std::ref(impl_->icons);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessIcons));
}

Result<std::reference_wrapper<const IconManager>> App::icons() const noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessIcons));
	}
	return std::cref(impl_->icons);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessIcons));
}
#endif

#if FLOWUI_PUBLIC_VULKAN_INTEROP
Result<std::reference_wrapper<ViewPortManager>> App::viewPorts() noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	}
	return std::ref(impl_->mainWindow().viewPorts);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessViewports));
}

Result<std::reference_wrapper<ViewPortManager>> App::viewPorts(WindowId id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	return std::ref(impl_->requireWindow(id).viewPorts);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessViewports));
}

Result<std::reference_wrapper<const ViewPortManager>> App::viewPorts() const noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	}
	return std::cref(impl_->mainWindow().viewPorts);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessViewports));
}

Result<std::reference_wrapper<const ViewPortManager>> App::viewPorts(WindowId id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	return std::cref(impl_->requireWindow(id).viewPorts);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessViewports));
}
#endif

Result<std::reference_wrapper<UiManager>> App::ui() noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	}
	return std::ref(impl_->mainWindow().ui);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessUi));
}

Result<std::reference_wrapper<UiManager>> App::ui(WindowId id) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	return std::ref(impl_->requireWindow(id).ui);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessUi));
}

Result<std::reference_wrapper<const UiManager>> App::ui() const noexcept try {
	if (!impl_) {
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	}
	return std::cref(impl_->mainWindow().ui);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessUi));
}

Result<std::reference_wrapper<const UiManager>> App::ui(WindowId id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	return std::cref(impl_->requireWindow(id).ui);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessUi));
}

Status App::setWindowTitle(std::string_view title) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return setWindowTitle(impl_->mainWindowId, title);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::WindowSetTitle));
}

Status App::setWindowTitle(WindowId id, std::string_view title) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::WindowSetTitle));
	impl_->requirePlatformThread();
	AppWindow& window = impl_->requireWindow(id);
	std::string pendingTitle(title);
	requireSuccess(impl_->platformService.setTitle(window.platformState, pendingTitle));
	window.config.native.title = std::move(pendingTitle);
	return {};
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::WindowSetTitle));
}

Result<void*> App::nativeWindowHandle() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return nativeWindowHandle(impl_->mainWindowId);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<void*> App::nativeWindowHandle(WindowId id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	impl_->requirePlatformThread();
	const AppWindow& window = impl_->requireWindow(id);
	return impl_->platformService.nativeHandle(window.platformState);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::setWindowInputConfig(const WindowInputConfig& config) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return setWindowInputConfig(impl_->mainWindowId, config);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::setWindowInputConfig(WindowId id, const WindowInputConfig& config) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	impl_->requirePlatformThread();
	AppWindow& window = impl_->requireWindow(id);
	requireSuccess(impl_->platformService.setInputConfig(window.platformState, config));
	window.config.native.input = config;
	return {};
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<WindowInputConfig> App::windowInputConfig() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return windowInputConfig(impl_->mainWindowId);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<WindowInputConfig> App::windowInputConfig(WindowId id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	impl_->requirePlatformThread();
	const AppWindow& window = impl_->requireWindow(id);
	return impl_->platformService.inputConfig(window.platformState);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<bool> App::supportsRawMouseMotion() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return supportsRawMouseMotion(impl_->mainWindowId);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<bool> App::supportsRawMouseMotion(WindowId id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	impl_->requirePlatformThread();
	const AppWindow& window = impl_->requireWindow(id);
	return impl_->platformService.supportsRawMouseMotion(window.platformState);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Status App::setClipboardText(std::string_view text) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return setClipboardText(impl_->mainWindowId, text);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::WindowWriteClipboard));
}

Status App::setClipboardText(WindowId id, std::string_view text) noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::WindowWriteClipboard));
	impl_->requirePlatformThread();
	AppWindow& window = impl_->requireWindow(id);
	return impl_->platformService.setClipboardText(window.platformState, text);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::WindowWriteClipboard));
}

Result<std::string> App::clipboardText() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return clipboardText(impl_->mainWindowId);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::WindowReadClipboard));
}

Result<std::string> App::clipboardText(WindowId id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::WindowReadClipboard));
	impl_->requirePlatformThread();
	const AppWindow& window = impl_->requireWindow(id);
	return impl_->platformService.clipboardText(window.platformState);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::WindowReadClipboard));
}

Result<std::pair<int, int>> App::windowSize() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return windowSize(impl_->mainWindowId);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<std::pair<int, int>> App::windowSize(WindowId id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	impl_->requirePlatformThread();
	const AppWindow& window = impl_->requireWindow(id);
	const VkExtent2D extent = impl_->platformService.windowExtent(window.platformState);
	return std::pair{static_cast<int>(extent.width), static_cast<int>(extent.height)};
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<std::pair<int, int>> App::framebufferSize() const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return framebufferSize(impl_->mainWindowId);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<std::pair<int, int>> App::framebufferSize(WindowId id) const noexcept try {
	if (!impl_)
		return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	impl_->requirePlatformThread();
	const AppWindow& window = impl_->requireWindow(id);
	const VkExtent2D extent = impl_->platformService.framebufferExtent(window.platformState);
	return std::pair{static_cast<int>(extent.width), static_cast<int>(extent.height)};
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppAccessWindow));
}

Result<App> makeApplication(const AppConfig& cfg) noexcept try {
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
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::AppInitialize));
}

} // namespace FlowUi
