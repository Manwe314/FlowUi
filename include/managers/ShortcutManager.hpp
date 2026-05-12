#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "window/Inputs.hpp"

namespace FlowUi {

class UiManager;

/** @addtogroup flowui_shortcut_manager
 * @{
 */

/** @brief Scope that controls when a shortcut is eligible to run. */
enum class ShortcutScope : uint8_t {
	FocusedInput = 0,
	FocusedElement = 1,
	Global = 2,
};

/** @brief Input transition used to trigger a shortcut. */
enum class ShortcutTrigger : uint8_t {
	Press = 0,
	Release = 1,
	Down = 2,
};

/** @brief Keyboard chord registered with ShortcutManager. */
struct ShortcutChord {
	/** @brief Platform key code. */
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

/** @brief Runtime data passed to shortcut callbacks. */
struct ShortcutContext {
	/** @brief UI manager for the active frame. */
	UiManager& ui;
	/** @brief Current frame input. */
	const FrameInput& currentInput;
	/** @brief Previous frame input. */
	const FrameInput& previousInput;
	/** @brief Focused Clay element id. */
	Clay_ElementId focusedElementId{};
};

/** @brief Shortcut callback. Return true when the shortcut is handled. */
using ShortcutCallback = std::function<bool(ShortcutContext&)>;

/** @brief Opaque shortcut registration id. */
using ShortcutId = uint32_t;

/** @brief Registers and dispatches keyboard shortcuts. */
class ShortcutManager {
public:
	/** @brief Register a shortcut and return its id. */
	ShortcutId registerShortcut(
		const ShortcutChord& chord,
		ShortcutScope scope,
		int32_t priority,
		ShortcutCallback callback);
	/** @brief Unregister a shortcut by id. */
	bool unregisterShortcut(ShortcutId id);
	/** @brief Remove all registered shortcuts. */
	void clear();

	/** @brief Set the currently focused Clay element id. */
	void setFocusedElement(Clay_ElementId elementId);
	/** @brief Clear focused element state. */
	void clearFocusedElement();
	/** @brief Return the currently focused Clay element id. */
	Clay_ElementId focusedElement() const { return focusedElementId_; }

	/** @brief Dispatch shortcuts for the current frame. */
	void beginFrame(UiManager& ui, const FrameInput& currentInput, const FrameInput& previousInput);

private:
	struct ShortcutExecutable {
		ShortcutScope scope = ShortcutScope::Global;
		int32_t priority = 0;
		ShortcutId id = 0u;
		uint64_t registrationOrder = 0u;
		ShortcutCallback callback{};
	};

	using ShortcutBucket = std::vector<ShortcutExecutable>;

	static uint8_t modsMaskFromChord(const ShortcutChord& chord);
	static uint8_t modsMaskFromInput(const FrameInput& input);
	static bool keyDown(const FrameInput& input, int key);
	static uint32_t packChord(int key, uint8_t modsMask, ShortcutTrigger trigger);
	static int unpackKey(uint32_t packedChord);
	static bool executableOrderLess(const ShortcutExecutable& a, const ShortcutExecutable& b);

	bool dispatchPackedChord(ShortcutContext& context, UiManager& ui, uint32_t packedChord) const;
	bool scopeIsActive(const ShortcutExecutable& executable, const ShortcutContext& context, UiManager& ui) const;

	std::unordered_map<uint32_t, ShortcutBucket> chordBuckets_{};
	std::unordered_map<ShortcutId, uint32_t> shortcutIdToChord_{};
	std::unordered_map<int, uint32_t> registeredKeyRefCount_{};
	uint64_t nextShortcutId_ = 1u;
	uint64_t nextRegistrationOrder_ = 1u;
	Clay_ElementId focusedElementId_{};
};

/** @} */

} // namespace FlowUi
