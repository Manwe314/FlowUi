# Input Field Manager API

## Enums

### **CaretRequestKind**


#### `enum class CaretRequestKind : uint8_t`

Requested caret operation for an input field. It supports setting primary focus, adding a caret, or clearing all carets and focus.

## Public Structs

### **InputManagerConfig**


#### `struct InputManagerConfig`

Caret and selection rendering configuration. It controls caret dimensions, caret color, selection color, and selected text color.

### **FieldConfig**


#### `struct FieldConfig`

Behavior configuration for an editable field. It controls read-only state, newline insertion, arrow navigation, and maximum stored bytes.

### **FieldRequest**


#### `struct FieldRequest`

Per-frame request submitted by a custom input element. It marks a field present, provides initial text, config, and Clay ids for text and content areas.

### **FieldQueryResult**


#### `struct FieldQueryResult`

Current manager-owned field state returned by requestField. It exposes text, primary caret state, and selection state.

## Public API

### **requestField**


#### `FieldQueryResult requestField(const FieldRequest& request)`

- **Returns:** `FieldQueryResult`
- **Arguments:** `request` stable field id, initial text, config, and Clay element ids.

Registers or updates an input field for the current frame and returns its current manager-owned state. Call this once per frame from the element that draws the editable field.

**Example:**

```cpp
FlowUi::FieldQueryResult field = context.uiManager.inputFields().requestField({.fieldId = context.elementID, .initialText = "Search", .textElementId = textId, .contentElementId = contentId});
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#a6deadc46f16595277ae8e2258e63b787).

### **requestCaret**


#### `void requestCaret(std::string_view fieldId, CaretRequestKind kind)`

- **Returns:** `void`
- **Arguments:** `fieldId` field to focus or edit, `kind` requested caret operation.

Requests focus or caret changes for an input field. `SetPrimary` is the common operation for clicked fields, while `ClearAll` removes text focus globally.

**Example:**

```cpp
context.uiManager.inputFields().requestCaret(context.elementID, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#aad13550088f959cf6d948173d8afa446).

### **removeField**


#### `bool removeField(std::string_view fieldId)`

- **Returns:** `bool`
- **Arguments:** `fieldId` field state to remove.

Deletes stored text, config, caret, and selection state for one field. Use this when a dynamic field is removed or when external state should replace the edited text.

**Example:**

```cpp
const bool removed = app.ui().inputFields().removeField("settings/name");
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#a1e0319ffec372a95e5d129a5d8bda14a).

### **replaceText**


#### `bool replaceText(std::string_view fieldId, std::string_view text, bool preserveCaret = true)`

- **Returns:** `bool`
- **Arguments:** `fieldId` field state to update, `text` replacement text, `preserveCaret` whether to keep and clamp caret state.

Replaces stored text for an existing field. By default, active carets and selections are preserved and clamped to the new text; pass `false` to clear active carets from that field.

**Example:**

```cpp
const bool changed = app.ui().inputFields().replaceText("settings/name", externalName, false);
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html).

### **clear**


#### `void clear()`

- **Returns:** `void`
- **Arguments:** none.

Clears all managed input field state. This resets fields, focus, key repeat, pointer drag, and frame render overrides.

**Example:**

```cpp
app.ui().inputFields().clear();
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#a7ac0604f2391bfe2ea5d16b008b68d18).

### **hasPrimaryFieldFocus**


#### `bool hasPrimaryFieldFocus() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether any input field currently owns primary text focus. This is useful for suppressing global shortcuts while the user is editing text.

**Example:**

```cpp
if (!app.ui().inputFields().hasPrimaryFieldFocus()) { runGlobalShortcut(); }
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#a4f494c501874c594a05a8e28f8e9954f).

### **getSelectedText**


#### `std::string_view getSelectedText() const`

- **Returns:** `std::string_view`
- **Arguments:** none.

Returns selected text from the primary field, or an empty view when no selection exists. The view points into manager-owned storage and is invalidated when that field text changes or is removed.

**Example:**

```cpp
std::string selected(app.ui().inputFields().getSelectedText());
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#a997c4fea775ebe55ae427e901c38f50c).

### **insertTextAtPrimaryCaret**


#### `bool insertTextAtPrimaryCaret(std::string_view utf8Text)`

- **Returns:** `bool`
- **Arguments:** `utf8Text` text to insert.

Inserts UTF-8 text at the primary caret, replacing active selections. The operation respects read-only state and `FieldConfig::maxBytes`.

**Example:**

```cpp
const bool pasted = app.ui().inputFields().insertTextAtPrimaryCaret(app.ui().clipboardText());
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#a109309cb439eaa40a24f8ada6409da8e).
