#pragma once

#include "FlowUi/BuildConfig.hpp"

#include <cstdint>
#include <clay.h>

#include "FlowUi/WindowId.hpp"
#include "managers/structs/ShortcutManagerStructs.hpp"

namespace FlowUi {

class UiManager;
namespace detail::storage { class IStorageSystem; }
namespace detail::manager_storage { struct ShortcutManagerState; }
namespace devSystems { class MemorySampleSink; }

/** @addtogroup flowui_shortcut_manager
 * @{
 */

/**
 * @brief Registers and dispatches keyboard shortcuts.
 *
 * ShortcutManager stores keyboard chords and evaluates them once per FlowUi
 * frame from the current and previous FrameInput. Matching shortcuts are
 * filtered by ShortcutScope, then invoked in priority order. A callback that
 * returns true handles the chord and stops later callbacks for that same match.
 *
 * ShortcutId value 0 is reserved as an invalid id. registerShortcut() returns 0
 * when registration fails, so application code can treat 0 as "not registered".
 *
 * @see @ref md_docs_2tutorials_2input__fields__and__shortcuts "Input Fields and Shortcuts"
 */
class ShortcutManager {
public:
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Register a shortcut and return its id.
	 *
	 * The chord is matched against frame input using its key, modifier flags,
	 * and trigger mode. The scope controls when the callback is eligible to run.
	 * When multiple callbacks match the same chord, focused scopes are considered
	 * before global shortcuts, higher priority values run first, and callbacks
	 * with the same scope and priority run in registration order.
	 *
	 * A callback should return true after it handles the shortcut. Returning
	 * false allows the next matching callback for the same chord to run.
	 *
	 * @param chord Key, modifier requirements, and trigger transition to match.
	 * @param scope Focus condition required before the shortcut may run.
	 * @param priority Ordering value within the same scope. Higher values run
	 * first.
	 * @param callback Function invoked when the chord and scope match.
	 * @return Opaque registration id used with unregisterShortcut(). Returns 0
	 * when callback is empty or chord.key is outside FrameInput's keyboard range.
	 *
	 * @code{.cpp}
	 * const FlowUi::ShortcutId copyShortcut =
	 *     app.ui().shortcuts().registerShortcut(
	 *         FlowUi::ShortcutChord{
	 *             .key = GLFW_KEY_C,
	 *             .ctrl = true,
	 *             .trigger = FlowUi::ShortcutTrigger::Press,
	 *         },
	 *         FlowUi::ShortcutScope::FocusedInput,
	 *         100,
	 *         [](FlowUi::ShortcutContext& context) {
	 *             const std::string selected(context.ui.inputFields().getSelectedText());
	 *             if (selected.empty()) {
	 *                 return false;
	 *             }
	 *             context.ui.setClipboardText(selected);
	 *             return true;
	 *         });
	 *
	 * const FlowUi::ShortcutId pasteShortcut =
	 *     app.ui().shortcuts().registerShortcut(
	 *         FlowUi::ShortcutChord{
	 *             .key = GLFW_KEY_V,
	 *             .ctrl = true,
	 *             .trigger = FlowUi::ShortcutTrigger::Press,
	 *         },
	 *         FlowUi::ShortcutScope::FocusedInput,
	 *         100,
	 *         [](FlowUi::ShortcutContext& context) {
	 *             return context.ui.inputFields().insertTextAtPrimaryCaret(
	 *                 context.ui.clipboardText());
	 *         });
	 *
	 * if (copyShortcut == 0 || pasteShortcut == 0) {
	 *     // Registration failed; key values or callbacks should be checked.
	 * }
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2input__fields__and__shortcuts "Input Fields and Shortcuts"
	 */
	ShortcutId registerShortcut(
		const ShortcutChord& chord,
		ShortcutScope scope,
		int32_t priority,
		ShortcutCallback callback);

	/** Register a retained semantic app action for this window's shortcut scope. */
	ShortcutId registerShortcut(
		const ShortcutChord& chord,
		ShortcutScope scope,
		int32_t priority,
		AppActionCall action,
		ShortcutHandling handling = ShortcutHandling::Consume);

	/**
	 * @brief Unregister a shortcut by id.
	 *
	 * Removes the registered callback and releases the manager's internal
	 * reference to the chord key. It is valid to unregister from inside a
	 * shortcut callback.
	 *
	 * @param id Registration id returned by registerShortcut().
	 * @retval true A shortcut with id existed and was removed.
	 * @retval false id was 0 or did not match an active registration.
	 *
	 * @code{.cpp}
	 * if (copyShortcut != 0) {
	 *     (void)app.ui().shortcuts().unregisterShortcut(copyShortcut);
	 * }
	 * @endcode
	 */
	bool unregisterShortcut(ShortcutId id);

	/**
	 * @brief Remove all registered shortcuts.
	 *
	 * Clears every chord registration, resets registration ids, and clears the
	 * focused element marker.
	 *
	 * @code{.cpp}
	 * app.ui().shortcuts().clear();
	 * @endcode
	 */
	void clear();

	/**
	 * @brief Set the currently focused Clay element id.
	 *
	 * The focused element marker is used by ShortcutScope::FocusedElement. It is
	 * a lightweight shortcut classification value; callbacks can inspect
	 * ShortcutContext::focusedElementId when they need element-specific behavior.
	 * Conditions with which an element becomes focused is up to the user to decide by calling this function.
	 *
	 * @param elementId Clay element id to treat as focused.
	 *
	 * @code{.cpp}
	 * app.ui().shortcuts().setFocusedElement(context.uiManager.toClayEID(context.id));
	 * @endcode
	 */
	void setFocusedElement(Clay_ElementId elementId);

	/**
	 * @brief Clear focused element state.
	 *
	 * After this call, FocusedElement shortcuts are not eligible until another
	 * non-zero focused element id is set.
	 *
	 * @code{.cpp}
	 * app.ui().shortcuts().clearFocusedElement();
	 * @endcode
	 */
	void clearFocusedElement();

	/**
	 * @brief Return the currently focused Clay element id.
	 *
	 * @return Clay element id currently tracked by ShortcutManager. A zero id
	 * means no shortcut-focused element is set.
	 *
	 * @code{.cpp}
	 * const Clay_ElementId focused = app.ui().shortcuts().focusedElement();
	 * if (focused.id != 0u) {
	 *     // FocusedElement shortcuts can be eligible this frame.
	 * }
	 * @endcode
	 */
	Clay_ElementId focusedElement() const;

private:
	friend class UiManager;

	void init(detail::storage::IStorageSystem& storage, WindowId window);
	void installDefaultTextShortcuts(const DefaultTextShortcutConfig& config);
	void destroy() noexcept;
	/** @brief Dispatch shortcuts for the current frame. */
	void beginFrame(UiManager& ui, const FrameInput& currentInput, const FrameInput& previousInput);

	detail::manager_storage::ShortcutManagerState& state();
	const detail::manager_storage::ShortcutManagerState& state() const;

	detail::storage::IStorageSystem* storage_ = nullptr;
	WindowId window_ = InvalidWindowId;
	uint64_t stateHandle_ = 0;
};

/** @} */

} // namespace FlowUi
