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
 * @brief Configures FlowUi developer tooling and capture behavior.
 *
 * These options are only used when developer mode support is compiled in.
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
	/** @brief Enable developer tooling at runtime. */
	bool enabled = false;

	/**
	 * @brief Capture timing from the first application tick until the timing ring is full
	 * or the developer interface is opened. Requires enabled developer tooling.
	 *
	 * The completed startup snapshot is preserved for the first investigation; filling
	 * the ring does not automatically open the developer interface. Defaults to off.
	 */
	bool capture_timing_on_startup = false;

	/**
	 * @brief Register the developer-interface toggle chord with ShortcutManager.
	 *
	 * Disable this when the application wants to own panel toggling itself.
	 */
	bool useShortcutManagerForPanelToggle = true;

	/**
	 * @brief Hide FlowUi's internal developer elements from captured UI data.
	 *
	 * @note Keeping this enabled is strongly recommended. Capturing the developer
	 * panel itself can add noisy internal elements to inspection/export data and
	 * make user-authored UI harder to reason about.
	 */
	bool excludeInternalDevElementsFromCapture = true;
};

} // namespace FlowUi
