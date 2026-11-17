# Shortcut Manager API

[Public declarations](include/FlowUi/Public/managers/ShortcutManager.hpp) · [Shortcut values](include/FlowUi/Public/managerStructs/ShortcutTypes.hpp)

Shortcuts are window-owned. A `ShortcutChord` uses a backend key code (use `keyCode(Key::C)`), exact Ctrl/Shift/Alt/Super requirements, and a Press/Release/Down trigger. Eligible scopes dispatch in order: FocusedInput, FocusedElement, Global. Within a scope, higher priority precedes lower priority, then registration order breaks ties. A callback's returned bool decides consumption; it is independent of registration's Result success.

`ShortcutContext` borrows the active UI manager and current/previous frame input only during dispatch. Do not retain these references. Capture application state by value or with a lifetime that exceeds registration; unregister before destroying borrowed targets.

## Acquire the manager

`App` getters return `Result<std::reference_wrapper<T>>`. Check the wrapper, then name the borrowed object as a normal reference:

**Example:**

```cpp
auto result = app.ui();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::UiManager& manager = result->get();
```

Call `ui.shortcuts()` on that `UiManager&` to obtain the direct manager reference. The reference is borrowed; the App must outlive it, and window-owned managers must also stay within their owning window lifetime. The examples use an already acquired manager reference and existing field/window identities where needed. They choose an application policy of throwing `FlowUiException` after a failed Result; returning/reporting the error is equally possible. Include the relevant manager/value headers and `FlowUi/Public/errorManagement/Exception.hpp`.

For `Result<bool>`, `if (!result)` tests operational failure; `*result` is the separate operation outcome. Never treat a successful `false` as an error.

## Public API

### **registerShortcut**

#### `[[nodiscard]] Result<ShortcutId> registerShortcut(const ShortcutChord& chord, ShortcutScope scope, int32_t priority, ShortcutCallback callback) noexcept`

- **Returns:** `Result<ShortcutId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `chord` key, modifiers and transition, `scope` shortcut eligibility scope, `priority` higher values dispatch first, `callback` retained callable

Register a keyboard chord and return its window-local registration ID. Success contains a nonzero ShortcutId; invalid manager/chord/callback/action or allocation failures are errors.

Matching uses exact modifier requirements and the trigger transition. FocusedInput precedes FocusedElement, then Global; higher priority first, then registration order. Callbacks are owned by the registration, so captured references must outlive it. A callback returning true consumes the chord; false allows subsequent matches. Callback exceptions follow the dispatch/frame boundary policy, not the registration call.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ShortcutChord chord{.key = FlowUi::keyCode(FlowUi::Key::S), .ctrl = true};
bool saveRequested = false;
FlowUi::ShortcutCallback callback = [&saveRequested](const FlowUi::ShortcutContext&) {
    saveRequested = true;
    return true;
};
auto result = manager.registerShortcut(chord, FlowUi::ShortcutScope::Global, 100, std::move(callback));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto shortcutId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ShortcutManager.hpp).

### **registerShortcut**

#### `[[nodiscard]] Result<ShortcutId> registerShortcut(const ShortcutChord& chord, ShortcutScope scope, int32_t priority, AppActionCall action, ShortcutHandling handling = ShortcutHandling::Consume) noexcept`

- **Returns:** `Result<ShortcutId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `chord` key, modifiers and transition, `scope` shortcut eligibility scope, `priority` higher values dispatch first, `action` retained application action, `handling` consume or pass through

Register a retained semantic app action for a keyboard chord. Success contains a nonzero ShortcutId; invalid manager/chord/callback/action or allocation failures are errors.

The action is resolved at dispatch; unbound/disabled actions are skipped. handling defaults to Consume; PassThrough allows later matching shortcuts. Referenced action resources follow AppActions::bind() lifetime rules. Scope, priority, and chord matching match callback registration.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ShortcutChord chord{.key = FlowUi::keyCode(FlowUi::Key::S), .ctrl = true};
auto result = manager.registerShortcut(chord, FlowUi::ShortcutScope::Global, 100, action, FlowUi::ShortcutHandling::Consume);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto shortcutId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ShortcutManager.hpp).

### **unregisterShortcut**

#### `[[nodiscard]] Result<bool> unregisterShortcut(ShortcutId id) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `id` window-scoped identity

Remove a shortcut registration. Success contains true if removed or false for zero/unknown IDs.

It is valid to unregister from inside a shortcut callback. Callback-owned captures are released when their retained registration is no longer needed by dispatch.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.unregisterShortcut(shortcutId);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto removed = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ShortcutManager.hpp).

### **clear**

#### `[[nodiscard]] Status clear() noexcept`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** none.

Remove every shortcut and clear the focused-element marker. Success means the reset completed.

Stored callbacks/actions are released and the registration epoch advances; the monotonic ID counter is not reset. Discard IDs for cleared registrations.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.clear();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ShortcutManager.hpp).

### **setFocusedElement**

#### `[[nodiscard]] Status setFocusedElement(Clay_ElementId elementId) noexcept`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `elementId` Clay focus marker

Set the lightweight Clay focus marker used by FocusedElement shortcuts. Success means the marker was stored; a zero element ID represents no focused element.

This does not assign input-field focus or decide when your application considers an element focused.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.setFocusedElement(elementId);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ShortcutManager.hpp).

### **clearFocusedElement**

#### `[[nodiscard]] Status clearFocusedElement() noexcept`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** none.

Clear the Clay focus marker. Success means FocusedElement registrations are ineligible until a nonzero marker is set.

Input-field focus is independent.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.clearFocusedElement();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ShortcutManager.hpp).

### **focusedElement**

#### `[[nodiscard]] Result<Clay_ElementId> focusedElement() const noexcept`

- **Returns:** `Result<Clay_ElementId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** none.

Read the current lightweight Clay focus marker. Success contains the tracked Clay_ElementId; id == 0 means no marker, not an error.

The returned ID is a value, not a borrowed element pointer.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.focusedElement();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto focusedElement = *result;
```

## Build-gated development hooks

`FLOW_UI_DEV_MODE` and the development memory-level macros expose additional inspection/recording hooks in the header. They are development infrastructure, not the application API described here. Their existing build gates and signatures are unchanged.

## Checked development inspection examples

These hooks require `FLOW_UI_DEV_MODE`; viewport hooks also require `FLOWUI_PUBLIC_VULKAN_INTEROP`. They are observation/tooling APIs. A visitor returns `true` to continue; a successful returned `false` is distinct from a failed `Result`. Callback arguments and their resource views are borrowed. The examples use the already acquired manager for the owning App/window.

See: [Owning header](include/FlowUi/Public/managers/ShortcutManager.hpp).

### **visitDevShortcuts**

#### `[[nodiscard]] Result<bool> visitDevShortcuts(void* target, DevShortcutVisitor visitor) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.

Observe development catalogue entries without invoking application operations. Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.

Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts live shortcut registrations; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
std::size_t count = 0;
auto result = manager.visitDevShortcuts(&count,
    [](void* target, const FlowUi::ShortcutManager::DevShortcutView& shortcut) {
        auto& observedCount = *static_cast<std::size_t*>(target);
        if (shortcut.id != 0) {
            ++observedCount;
        }
        return true;
    });
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool completed = *result;
#endif
```

See: [Owning header](include/FlowUi/Public/managers/ShortcutManager.hpp).
