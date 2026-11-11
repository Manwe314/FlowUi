#pragma once

#include "FlowUi/Public/BuildConfig.hpp"
#include "devSystems/DevConfig.hpp"
#include <cstdint>

namespace FlowUi {

/** @brief Developer panel shortcut trigger mode.
 *
 * @see DevShortcutChord
 */
enum class DevShortcutTrigger : uint8_t {
	/** Trigger when the key transitions from up to down. */
	Press = 0,
	/** Trigger when the key transitions from down to up. */
	Release = 1,
	/** Trigger every frame while the key remains down. */
	Down = 2,
};

/** @brief Keyboard chord used by developer tooling.
 *
 * Defaults to Ctrl+Shift+D on the press transition. The chord uses backend
 * key codes and exact modifier matching.
 */
struct DevShortcutChord {
	/** @brief Platform key code consumed by FlowUi's ShortcutManager backend. */
	int key = 68;
	/** @brief Whether Ctrl is required. */
	bool ctrl = true;
	/** @brief Whether Shift is required. */
	bool shift = true;
	/** @brief Whether Alt is required. */
	bool alt = false;
	/** @brief Whether Super/Command is required. */
	bool super = false;
	/** @brief Shortcut trigger mode. */
	DevShortcutTrigger trigger = DevShortcutTrigger::Press;
};

/**
 * Developer-interface startup options; require FLOW_UI_DEV_MODE.
 * Monitoring/tooling configurations stay in devSystems; compiled capture levels limit runtime detail.
 */
struct DevToolsConfig {
#if FLOW_UI_DEV_MODE
	/** Capture, retention, and sampling controls for developer monitoring. */
	devSystems::DevMonitoringConfig monitoring{};
#endif

#if FLOW_UI_DEV_MODE
	/** Schema, override, and per-window tree-capture capacity controls. */
	devSystems::DevToolingConfig tooling{};
#endif

	/** @brief Keyboard chord used to toggle the developer-interface window. */
	DevShortcutChord panelToggleChord{};
	/** Enable the developer interface; monitoring has separate capture controls. */
	bool enabled = false;

	/**
	 * Capture startup timing until the ring fills or the developer interface opens; requires enabled.
	 * Preserve the snapshot for inspection without automatically opening the interface.
	 */
	bool capture_timing_on_startup = false;

	/**
	 * Register panelToggleChord with ShortcutManager; disable for application-controlled toggling.
	 */
	bool useShortcutManagerForPanelToggle = true;

	/**
	 * Exclude internal developer elements from inspection/export captures to focus on application UI.
	 */
	bool excludeInternalDevElementsFromCapture = true;
};

} // namespace FlowUi
