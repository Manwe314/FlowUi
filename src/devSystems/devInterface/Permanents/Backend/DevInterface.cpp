#include "devSystems/devInterface/Permanents/Backend/DevInterface.hpp"

#if FLOW_UI_DEV_MODE

#include <functional>
#include <algorithm>
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"

#include "FlowUi/AppElementWindows.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevInterface.hpp"
#include "managers/ShortcutManager.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems {
DevInterface::DevInterface() noexcept = default;
DevInterface::~DevInterface() = default;
Status DevInterface::initialize(UiManager& mainUi, const DevToolsConfig& config) {
	session_ = std::make_unique<DevInterfaceState>();
	enabled_ = config.enabled;
	shortcutEnabled_ = config.useShortcutManagerForPanelToggle;
	toggleChord_ = config.panelToggleChord;
	toggle_down_ = mainUi.getCurrentFrameInput()
					   .keyDown[static_cast<size_t>(std::clamp(toggleChord_.key, 0, 348))];
	return attachWindow(mainUi);
}
Status DevInterface::attachWindow(UiManager& ui) {
	if (!enabled_ || !shortcutEnabled_)
		return {};
	const ShortcutTrigger trigger =
		toggleChord_.trigger == DevShortcutTrigger::Release ? ShortcutTrigger::Release
		: toggleChord_.trigger == DevShortcutTrigger::Down	? ShortcutTrigger::Down
															: ShortcutTrigger::Press;
	auto registered = ui.shortcuts().registerShortcut(
		ShortcutChord{.key = toggleChord_.key,
					  .ctrl = toggleChord_.ctrl,
					  .shift = toggleChord_.shift,
					  .alt = toggleChord_.alt,
					  .super = toggleChord_.super,
					  .trigger = trigger},
		ShortcutScope::Global, 1000, [this](ShortcutContext&) { return enabled_; });
	if (!registered)
		return unexpectedError(registered.error());
	return {};
}

Status DevInterface::synchronize(App& app) {
	if (!enabled_)
		return {};
	auto& capture = app.devMonitoring().performance_capture();
	const auto& progress = capture.status();
	if (progress.generation && progress.generation != session_->capture_metadata_generation) {
		session_->capture_windows = session_->pending_capture_windows.empty()
										? app.devWindowSnapshot()
										: std::move(session_->pending_capture_windows);
		std::erase_if(session_->capture_windows,
					  [&](const auto& window) { return window.id == windowId_; });
		session_->capture_metadata_generation = progress.generation;
	}
	const auto tick = app.devMonitoring().timingReporting().current_app_tick();
	const auto now = app.devMonitoring().timingReporting().current_boundary_ns();
	const bool toggle_down = shortcutEnabled_ && app.dev_shortcut_down(toggleChord_);
	const bool requested_toggle = std::exchange(toggleRequested_, false);
	const bool native_toggle_pressed =
		toggleChord_.trigger == DevShortcutTrigger::Release ? !toggle_down && toggle_down_
		: toggleChord_.trigger == DevShortcutTrigger::Down ? toggle_down
														   : app.dev_shortcut_pressed(toggleChord_);
	const bool toggle_pressed = requested_toggle || native_toggle_pressed;
	const bool start_pressed = app.dev_shortcut_pressed(progress.settings.start_chord);
	toggle_down_ = toggle_down;
	if (windowId_ != InvalidWindowId && !app.hasWindow(windowId_))
		windowId_ = InvalidWindowId;
	if (windowId_ != InvalidWindowId && !suspended_ && app.shouldClose(windowId_)) {
		auto closed = app.set_dev_window_suspended(windowId_, true);
		if (!closed)
			return closed;
		suspended_ = true;
	}
	const bool capture_close = progress.phase == PerformanceCapturePhase::ClosingInterface;
	const bool reopen = capture.take_reopen_request();
	if (capture_close && windowId_ != InvalidWindowId) {
		auto closed = app.set_dev_window_suspended(windowId_, true);
		if (!closed) {
			capture.request_interface_open(tick, now);
			session_->capture_error = "Could not suspend developer interface.";
			return closed;
		}
		suspended_ = true;
		capture.interface_closed(tick);
		// Native presses from this closure tick cannot start the next clean tick.
		// Repeats are excluded by InputQueue, so a held key must be released/repressed.
	}
	bool open = reopen;
	if (toggle_pressed) {
		if (windowId_ != InvalidWindowId && !suspended_) {
			auto closed = app.set_dev_window_suspended(windowId_, true);
			if (!closed)
				return closed;
			suspended_ = true;
		} else {
			capture.request_interface_open(tick, now);
			open = capture.status().phase != PerformanceCapturePhase::Finalizing;
		}
	}
	if (open) {
		if (reopen || (capture.startup_capture() && windowId_ == InvalidWindowId))
			session_->activeTab = static_cast<uint64_t>(DevInterfaceTab::Performance);
		if (windowId_ != InvalidWindowId) {
			auto resumed = app.set_dev_window_suspended(windowId_, false);
			if (!resumed) {
				capture.retry_reopen();
				return resumed;
			}
			suspended_ = false;
		} else {
			WindowConfigOverrides overrides{};
			overrides.title = "FlowUi Developer Interface";
			overrides.width = 1100;
			overrides.height = 720;
			auto created = app.createWindow(
				overrides, interface_elements::kDevInterface,
				std::function<void(ElementBuilder<interface_elements::DevInterface>&, WindowId)>{
					[this, &app](ElementBuilder<interface_elements::DevInterface>& builder,
								 WindowId window) {
						builder.setParameters(interface_elements::DevInterfaceParameters{
							.app = &app,
							.interface_state = session_.get(),
							.interfaceWindowId = window,
							.mainWindowId = app.mainWindowId()});
					}});
			if (!created) {
				capture.retry_reopen();
				return unexpectedError(created.error());
			}
			windowId_ = *created;
			suspended_ = false;
		}
	}
	// Input edges are passed once to the controller by the shared polling path.
	start_requested_ = start_pressed && !toggle_pressed;
	return {};
}

} // namespace FlowUi::devSystems
#endif
