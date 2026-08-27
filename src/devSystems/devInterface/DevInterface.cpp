#include "devSystems/devInterface/DevInterface.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/AppElementWindows.hpp"
#include "devSystems/devInterface/Elements/DevRoot.hpp"
#include "managers/ShortcutManager.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems {
namespace {

ShortcutTrigger toShortcutTrigger(DevShortcutTrigger trigger) {
	switch (trigger) {
	case DevShortcutTrigger::Press: return ShortcutTrigger::Press;
	case DevShortcutTrigger::Release: return ShortcutTrigger::Release;
	case DevShortcutTrigger::Down: return ShortcutTrigger::Down;
	}
	return ShortcutTrigger::Press;
}

} // namespace

Status DevInterface::initialize(UiManager& mainUi, const DevToolsConfig& config) {
	enabled_ = config.enabled;
	shortcutEnabled_ = config.useShortcutManagerForPanelToggle;
	toggleChord_ = config.panelToggleChord;
	return attachWindow(mainUi);
}

Status DevInterface::attachWindow(UiManager& ui) {
	if (!enabled_ || !shortcutEnabled_) return {};

	const ShortcutChord chord{
		.key = toggleChord_.key,
		.ctrl = toggleChord_.ctrl,
		.shift = toggleChord_.shift,
		.alt = toggleChord_.alt,
		.super = toggleChord_.super,
		.trigger = toShortcutTrigger(toggleChord_.trigger),
	};
	auto registered = ui.shortcuts().registerShortcut(
		chord, ShortcutScope::Global, 1000,
		[this](ShortcutContext&) {
			if (!enabled_) return false;
			toggleRequested_ = true;
			return true;
		});
	if (!registered) return unexpectedError(registered.error());
	return {};
}

Status DevInterface::synchronize(App& app) {
	if (windowId_ != InvalidWindowId && !app.hasWindow(windowId_)) {
		windowId_ = InvalidWindowId;
	}
	if (!enabled_ || !toggleRequested_) return {};
	toggleRequested_ = false;

	if (windowId_ != InvalidWindowId) {
		const WindowId closing = windowId_;
		windowId_ = InvalidWindowId;
		return app.destroyWindow(closing);
	}

	WindowConfigOverrides overrides{};
	overrides.title = "FlowUi Developer Interface";
	overrides.width = 1100;
	overrides.height = 720;
	auto created = app.createWindow(overrides, interface_elements::kDevRoot);
	if (!created) return unexpectedError(created.error());
	windowId_ = *created;
	return {};
}

} // namespace FlowUi::devSystems

#endif
