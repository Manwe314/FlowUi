#pragma once

#include <cstdint>
#include <functional>

#include <clay.h>

#include "FlowUi/PublicStructs.hpp"
#include "managers/structs/InputStructs.hpp"
#include "managers/structs/ActionManagerStructs.hpp"

namespace FlowUi {

class UiManager;

/** @addtogroup flowui_shortcut_manager
 * @{
 */

/**
 * @brief Scope that controls when a shortcut is eligible to run.
 *
 * ShortcutManager groups matching chords by scope before invoking callbacks.
 * More focused scopes are evaluated before global shortcuts, allowing local UI
 * behavior to handle a chord before application-wide fallback behavior runs.
 */
enum class ShortcutScope : uint8_t {
	/** @brief Run only while an input field owns primary text focus. */
	FocusedInput = 0,

	/** @brief Run only while ShortcutManager has a focused Clay element id. */
	FocusedElement = 1,

	/** @brief Run regardless of focused input or focused element state. */
	Global = 2,
};

/**
 * @brief Input transition used to trigger a shortcut.
 *
 * The trigger is evaluated against the current and previous frame input. Use
 * Press for most commands, Release for key-up behavior, and Down for repeated
 * per-frame behavior while the chord remains held.
 */
enum class ShortcutTrigger : uint8_t {
	/** @brief Fire on the frame where the key changes from up to down. */
	Press = 0,

	/** @brief Fire on the frame where the key changes from down to up. */
	Release = 1,

	/** @brief Fire every frame while the key is down. */
	Down = 2,
};

/** Whether a registered app action consumes a matching shortcut. */
enum class ShortcutHandling : uint8_t {
	Consume = 0,
	PassThrough,
};

/**
 * @brief Keyboard chord registered with ShortcutManager.
 *
 * A chord is a key plus an exact modifier requirement and a trigger mode. The
 * key value is the platform/backend key code stored in FrameInput.
 */
struct ShortcutChord {
	/** @brief Platform/backend key code. */
	int key = -1;

	/** @brief Whether Ctrl is required. */
	bool ctrl = false;

	/** @brief Whether Shift is required. */
	bool shift = false;

	/** @brief Whether Alt is required. */
	bool alt = false;

	/** @brief Whether Super/Command is required. */
	bool super = false;

	/** @brief Trigger mode for the chord. */
	ShortcutTrigger trigger = ShortcutTrigger::Press;
};

/**
 * @brief Runtime data passed to shortcut callbacks.
 *
 * ShortcutContext provides access to the frame input used for dispatch and to
 * the active UiManager. focusedElementId is ShortcutManager's lightweight focus
 * marker for FocusedElement shortcuts; callbacks may inspect it when they need
 * to distinguish which focused element should handle a shared chord.
 */
struct ShortcutContext {
	/** @brief UI manager for the active frame. */
	UiManager& ui;

	/** @brief Current frame input. */
	const FrameInput& currentInput;

	/** @brief Previous frame input. */
	const FrameInput& previousInput;

	/** @brief Focused Clay element id tracked by ShortcutManager. */
	Clay_ElementId focusedElementId{};
};

/**
 * @brief Shortcut callback.
 *
 * Return true when the shortcut was handled. A handled shortcut stops further
 * callbacks for the same matching chord.
 */
using ShortcutCallback = std::function<bool(ShortcutContext&)>;

/** @brief Opaque shortcut registration id. */
using ShortcutId = uint32_t;

/** @} */

} // namespace FlowUi
