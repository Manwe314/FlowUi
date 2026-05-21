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

## Public Structs

### **ShortcutChord**


#### `struct ShortcutChord`

Keyboard chord registered with ShortcutManager. It stores backend key code, modifier requirements, and trigger mode.

### **ShortcutContext**


#### `struct ShortcutContext`

Runtime data passed to shortcut callbacks. It exposes UiManager, current and previous input, and the focused element id.

## Public API

### **registerShortcut**


#### `ShortcutId registerShortcut(const ShortcutChord& chord, ShortcutScope scope, int32_t priority, ShortcutCallback callback)`

- **Returns:** `ShortcutId`
- **Arguments:** `chord` key/modifier/trigger match, `scope` eligibility scope, `priority` ordering value, `callback` handler.

Registers a keyboard shortcut and returns an opaque id. Matching callbacks run by scope and priority, and a callback returning `true` stops later handlers for the same chord.

**Example:**

```cpp
FlowUi::ShortcutId saveShortcut = app.ui().shortcuts().registerShortcut({.key = GLFW_KEY_S, .ctrl = true}, FlowUi::ShortcutScope::Global, 100, saveCallback);
```

See: [Full Doxygen reference](classFlowUi_1_1ShortcutManager.html#a185118dced81d2cccdc61bbc5e74307f).

### **unregisterShortcut**


#### `bool unregisterShortcut(ShortcutId id)`

- **Returns:** `bool`
- **Arguments:** `id` shortcut id returned by `registerShortcut()`.

Removes a registered shortcut. It is valid to unregister a shortcut from inside a shortcut callback.

**Example:**

```cpp
const bool removed = app.ui().shortcuts().unregisterShortcut(saveShortcut);
```

See: [Full Doxygen reference](classFlowUi_1_1ShortcutManager.html#aa1243fa0e00dffc89ec688db82954a5e).

### **clear**


#### `void clear()`

- **Returns:** `void`
- **Arguments:** none.

Removes every registered shortcut and resets focused-element shortcut state. Use this for app-level shortcut reloads or teardown.

**Example:**

```cpp
app.ui().shortcuts().clear();
```

See: [Full Doxygen reference](classFlowUi_1_1ShortcutManager.html#af08e7e2bb696cec1bf476a3afda5d6b1).

### **setFocusedElement**


#### `void setFocusedElement(Clay_ElementId elementId)`

- **Returns:** `void`
- **Arguments:** `elementId` Clay element id to treat as shortcut-focused.

Sets the focused element marker used by `ShortcutScope::FocusedElement`. Application or element code decides when an element should become shortcut-focused.

**Example:**

```cpp
app.ui().shortcuts().setFocusedElement(context.uiManager.toClayEID(context.elementID));
```

See: [Full Doxygen reference](classFlowUi_1_1ShortcutManager.html#a8a496694eeaf4fdf03166ecf9c273212).

### **clearFocusedElement**


#### `void clearFocusedElement()`

- **Returns:** `void`
- **Arguments:** none.

Clears the focused element marker. After this, focused-element shortcuts are not eligible until another element id is set.

**Example:**

```cpp
app.ui().shortcuts().clearFocusedElement();
```

See: [Full Doxygen reference](classFlowUi_1_1ShortcutManager.html#abf2f8245061c1fd51116e79c615ce52d).

### **focusedElement**


#### `Clay_ElementId focusedElement() const`

- **Returns:** `Clay_ElementId`
- **Arguments:** none.

Returns the currently focused Clay element id for shortcut dispatch. A zero id means no shortcut-focused element is active.

**Example:**

```cpp
Clay_ElementId focused = app.ui().shortcuts().focusedElement();
```

See: [Full Doxygen reference](classFlowUi_1_1ShortcutManager.html#a2d2a678228d439ea9889a30b1a6c94b2).
