# Shortcut Manager API

## Aliases

### **ShortcutCallback**


#### `using ShortcutCallback = std::function<bool(ShortcutContext&)>`

Callback invoked when a shortcut chord matches. Return true to mark the shortcut handled and stop later callbacks for the same chord.

### **ShortcutId**


#### `using ShortcutId = uint32_t`

Opaque shortcut registration id returned by registerShortcut. Value 0 is reserved as invalid.

## Enums

### **ShortcutScope**


#### `enum class ShortcutScope : uint8_t`

Scope controlling when a shortcut is eligible to run. Focused input and focused element scopes run before global shortcuts.

### **ShortcutTrigger**


#### `enum class ShortcutTrigger : uint8_t`

Input transition used to trigger a shortcut. It supports press, release, and every-frame down behavior.

### **ShortcutHandling**

Explicit consumption policy for the `AppActionCall` registration overload. `Consume` stops later matches after successful invocation; `PassThrough` continues dispatch. Disabled and unbound actions fall through.

## Public Structs

### **ShortcutChord**


#### `struct ShortcutChord`

Keyboard chord registered with ShortcutManager. It stores backend key code, modifier requirements, and trigger mode.

### **ShortcutContext**


#### `struct ShortcutContext`

Runtime data passed to shortcut callbacks. It exposes UiManager, current and previous input, and the focused element id.

## Public API

### **registerShortcut**


#### `Result<ShortcutId> registerShortcut(const ShortcutChord& chord, ShortcutScope scope, int32_t priority, ShortcutCallback callback)`

- **Returns:** `Result<ShortcutId>`; check success before reading the value, and use `.error()` on failure.
- **Arguments:** `chord` key/modifier/trigger match, `scope` eligibility scope, `priority` ordering value, `callback` handler.

Registers a keyboard shortcut and returns an opaque id. Matching callbacks run by scope and priority, and a callback returning `true` stops later handlers for the same chord.

**Example:**

```cpp
const auto shortcut = app.ui().shortcuts().registerShortcut({.key = GLFW_KEY_S, .ctrl = true}, FlowUi::ShortcutScope::Global, 100, saveCallback);
if (!shortcut) {
    throw FlowUi::FlowUiException(shortcut.error());
}
const FlowUi::ShortcutId saveShortcut = *shortcut;
```

See: [Owning header](../../include/FlowUi/Public/managers/ShortcutManager.hpp).

#### `Result<ShortcutId> registerShortcut(const ShortcutChord& chord, ShortcutScope scope, int32_t priority, AppActionCall action, ShortcutHandling handling = ShortcutHandling::Consume)`

Registers a semantic app action without retaining a local UI recipe or arbitrary callback. Return values never determine consumption.

### **unregisterShortcut**


#### `bool unregisterShortcut(ShortcutId id)`

- **Returns:** `bool`
- **Arguments:** `id` shortcut id returned by `registerShortcut()`.

Removes a registered shortcut. It is valid to unregister a shortcut from inside a shortcut callback.

**Example:**

```cpp
const bool removed = app.ui().shortcuts().unregisterShortcut(saveShortcut);
```

See: [Owning header](../../include/FlowUi/Public/managers/ShortcutManager.hpp).

### **clear**


#### `void clear()`

- **Returns:** `void`
- **Arguments:** none.

Removes every registered shortcut and resets focused-element shortcut state. Use this for app-level shortcut reloads or teardown.

**Example:**

```cpp
app.ui().shortcuts().clear();
```

See: [Owning header](../../include/FlowUi/Public/managers/ShortcutManager.hpp).

### **setFocusedElement**


#### `void setFocusedElement(Clay_ElementId elementId)`

- **Returns:** `void`
- **Arguments:** `elementId` Clay element id to treat as shortcut-focused.

Sets the focused element marker used by `ShortcutScope::FocusedElement`. Application or element code decides when an element should become shortcut-focused.

**Example:**

```cpp
app.ui().shortcuts().setFocusedElement(context.uiManager.toClayEID(context.elementID));
```

See: [Owning header](../../include/FlowUi/Public/managers/ShortcutManager.hpp).

### **clearFocusedElement**


#### `void clearFocusedElement()`

- **Returns:** `void`
- **Arguments:** none.

Clears the focused element marker. After this, focused-element shortcuts are not eligible until another element id is set.

**Example:**

```cpp
app.ui().shortcuts().clearFocusedElement();
```

See: [Owning header](../../include/FlowUi/Public/managers/ShortcutManager.hpp).

### **focusedElement**


#### `Clay_ElementId focusedElement() const`

- **Returns:** `Clay_ElementId`
- **Arguments:** none.

Returns the currently focused Clay element id for shortcut dispatch. A zero id means no shortcut-focused element is active.

**Example:**

```cpp
Clay_ElementId focused = app.ui().shortcuts().focusedElement();
```

See: [Owning header](../../include/FlowUi/Public/managers/ShortcutManager.hpp).
