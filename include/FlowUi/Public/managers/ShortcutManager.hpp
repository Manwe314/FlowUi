#pragma once

#include "FlowUi/Private/ApiBoundary.hpp"

#include "FlowUi/Public/configurationStructs/ShortcutManagerConfig.hpp"
#include "FlowUi/Public/input/Key.hpp"

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"

#include <cstdint>
#include <clay.h>

#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Public/managerStructs/ShortcutTypes.hpp"

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
 * ShortcutId value 0 is reserved as an invalid id. registerShortcut() returns an error
 * when registration fails, so application code can treat 0 as "not registered".
 *
 * @see @ref md_docs_2tutorials_2input__fields__and__shortcuts "Input Fields and Shortcuts"
 */
class ShortcutManager {
public:
#if FLOW_UI_DEV_MODE
	/** Callable-free development observation, preserving window registration identity. */
	struct DevShortcutView {
		/** @brief Registered key, modifiers, and trigger transition. */
		ShortcutChord chord{};
		/** @brief Semantic action identity; zero for callback registrations. */
		AppActionID action{};
		/** @brief Registration generation and ordering sequence, respectively. */
		/** @var uint64_t order
		 * @brief Registration order used to break equal-scope and equal-priority ties.
		 */
		/** @var uint64_t epoch
		 * @brief Registration generation distinguishing reused window-local IDs.
		 */
		uint64_t epoch = 0, order = 0;
		/** @brief Window-local registration ID; zero is invalid. */
		ShortcutId id = 0;
		/** @brief Higher values dispatch earlier within the same scope. */
		int32_t priority = 0;
		/** @brief Focus condition required by this registration. */
		ShortcutScope scope = ShortcutScope::Global;
		/** @brief Consumption behavior for successful semantic action invocation. */
		ShortcutHandling handling = ShortcutHandling::Consume;
		/** @brief Whether this registration holds a user callback rather than an app action. */
		bool callback = false;
	};
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevShortcutVisitor = bool (*)(void*, const DevShortcutView&);
	/**
	 * @brief Observe development catalogue entries without invoking application operations.
	 *
	 * Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts live shortcut registrations; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure.
	 *
	 * @return Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * std::size_t count = 0;
	 * auto result = manager.visitDevShortcuts(&count,
	 *     [](void* target, const FlowUi::ShortcutManager::DevShortcutView& shortcut) {
	 *         auto& observedCount = *static_cast<std::size_t*>(target);
	 *         if (shortcut.id != 0) {
	 *             ++observedCount;
	 *         }
	 *         return true;
	 *     });
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool completed = *result;
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<bool> visitDevShortcuts(void* target, DevShortcutVisitor visitor) const noexcept;
	/** Pure dispatcher chord/scope matching, without input injection or callback execution. */
	[[nodiscard]] static bool devMatches(const DevShortcutView& candidate, const ShortcutChord& chord,
		bool focused_input, bool focused_element) noexcept;
	/** Pure dispatcher precedence over copied registration metadata. */
	[[nodiscard]] static bool devPrecedes(const DevShortcutView& left, const DevShortcutView& right) noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only memory accounting to sink without transferring storage ownership. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Register a keyboard chord and return its window-local registration ID.
	 *
	 * Matching uses exact modifier requirements and the trigger transition. FocusedInput precedes FocusedElement, then Global; higher priority first, then registration order. Callbacks are owned by the registration, so captured references must outlive it. A callback returning true consumes the chord; false allows subsequent matches. Callback exceptions follow the dispatch/frame boundary policy, not the registration call.
	 *
	 * @return Success contains a nonzero ShortcutId; invalid manager/chord/callback/action or allocation failures are errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ShortcutChord chord{.key = FlowUi::keyCode(FlowUi::Key::S), .ctrl = true};
	 * bool saveRequested = false;
	 * FlowUi::ShortcutCallback callback = [&saveRequested](const FlowUi::ShortcutContext&) {
	 *     saveRequested = true;
	 *     return true;
	 * };
	 * auto result = manager.registerShortcut(chord, FlowUi::ShortcutScope::Global, 100, std::move(callback));
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto shortcutId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<ShortcutId> registerShortcut(const ShortcutChord& chord,
													  ShortcutScope scope, int32_t priority,
													  ShortcutCallback callback) noexcept;

	/**
	 * @brief Register a retained semantic app action for a keyboard chord.
	 *
	 * The action is resolved at dispatch; unbound/disabled actions are skipped. handling defaults to Consume; PassThrough allows later matching shortcuts. Referenced action resources follow AppActions::bind() lifetime rules. Scope, priority, and chord matching match callback registration.
	 *
	 * @return Success contains a nonzero ShortcutId; invalid manager/chord/callback/action or allocation failures are errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ShortcutChord chord{.key = FlowUi::keyCode(FlowUi::Key::S), .ctrl = true};
	 * auto result = manager.registerShortcut(chord, FlowUi::ShortcutScope::Global, 100, action, FlowUi::ShortcutHandling::Consume);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto shortcutId = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<ShortcutId>
	registerShortcut(const ShortcutChord& chord, ShortcutScope scope, int32_t priority,
					 AppActionCall action, ShortcutHandling handling = ShortcutHandling::Consume) noexcept;

	/**
	 * @brief Remove a shortcut registration.
	 *
	 * It is valid to unregister from inside a shortcut callback. Callback-owned captures are released when their retained registration is no longer needed by dispatch.
	 *
	 * @return Success contains true if removed or false for zero/unknown IDs.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.unregisterShortcut(shortcutId);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> unregisterShortcut(ShortcutId id) noexcept;

	/**
	 * @brief Remove every shortcut and clear the focused-element marker.
	 *
	 * Stored callbacks/actions are released and the registration epoch advances; the monotonic ID counter is not reset. Discard IDs for cleared registrations.
	 *
	 * @return Success means the reset completed.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.clear();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status clear() noexcept;

	/**
	 * @brief Set the lightweight Clay focus marker used by FocusedElement shortcuts.
	 *
	 * This does not assign input-field focus or decide when your application considers an element focused.
	 *
	 * @return Success means the marker was stored; a zero element ID represents no focused element.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.setFocusedElement(elementId);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setFocusedElement(Clay_ElementId elementId) noexcept;

	/**
	 * @brief Clear the Clay focus marker.
	 *
	 * Input-field focus is independent.
	 *
	 * @return Success means FocusedElement registrations are ineligible until a nonzero marker is set.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.clearFocusedElement();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status clearFocusedElement() noexcept;

	/**
	 * @brief Read the current lightweight Clay focus marker.
	 *
	 * The returned ID is a value, not a borrowed element pointer.
	 *
	 * @return Success contains the tracked Clay_ElementId; id == 0 means no marker, not an error.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.focusedElement();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto focusedElement = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<Clay_ElementId> focusedElement() const noexcept;

private:
	friend class UiManager;

	void init(detail::storage::IStorageSystem& storage, WindowId window);
	[[nodiscard]] Status installDefaultTextShortcuts(const DefaultTextShortcutConfig& config);
	void destroy() noexcept;
	// Dispatch shortcuts for the current frame.
	void beginFrame(UiManager& ui, const FrameInput& currentInput, const FrameInput& previousInput);

	detail::manager_storage::ShortcutManagerState& state();
	const detail::manager_storage::ShortcutManagerState& state() const;

	detail::storage::IStorageSystem* storage_ = nullptr;
	WindowId window_ = InvalidWindowId;
	uint64_t stateHandle_ = 0;
};

/** @} */

} // namespace FlowUi
