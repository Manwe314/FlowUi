# Input Field Manager API

## Enums

### **CaretRequestKind**
---

#### `enum class CaretRequestKind : uint8_t`

Requested caret operation for an input field. It supports setting primary focus, adding a caret, or clearing all carets and focus.

## Public Structs

### **InputManagerConfig**
---

#### `struct InputManagerConfig`

Caret and selection rendering configuration. It controls caret dimensions, caret color, selection color, and selected text color.

### **FieldConfig**
---

#### `struct FieldConfig`

Behavior configuration for an editable field. It controls read-only state, newline insertion, arrow navigation, and maximum stored bytes.

### **FieldRequest**
---

#### `struct FieldRequest`

Per-frame request submitted by a custom input element. It marks a field present, provides initial text, config, and Clay ids for text and content areas.

### **FieldQueryResult**
---

#### `struct FieldQueryResult`

Current manager-owned field state returned by requestField. It exposes text, primary caret state, and selection state.

## Public API


### **requestField**
---

#### `FieldQueryResult requestField(const FieldRequest& request)`

- **Returns:** `FieldQueryResult`
- **Arguments:** `request` stable field id, initial text, config, and Clay element ids.

Registers or updates an input field for the current frame and returns its current manager-owned state. Call this once per frame from the element that draws the editable field.

### **requestCaret**
---

#### `void requestCaret(std::string_view fieldId, CaretRequestKind kind)`

- **Returns:** `void`
- **Arguments:** `fieldId` field to focus or edit, `kind` requested caret operation.

Requests focus or caret changes for an input field. `SetPrimary` is the common operation for clicked fields, while `ClearAll` removes text focus globally.

### **removeField**
---

#### `bool removeField(std::string_view fieldId)`

- **Returns:** `bool`
- **Arguments:** `fieldId` field state to remove.

Deletes stored text, config, caret, and selection state for one field. Use this when a dynamic field is removed or when external state should replace the edited text.

### **clear**
---

#### `void clear()`

- **Returns:** `void`
- **Arguments:** none.

Clears all managed input field state. This resets fields, focus, key repeat, pointer drag, and frame render overrides.

### **hasPrimaryFieldFocus**
---

#### `bool hasPrimaryFieldFocus() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether any input field currently owns primary text focus. This is useful for suppressing global shortcuts while the user is editing text.

### **getSelectedText**
---

#### `std::string_view getSelectedText() const`

- **Returns:** `std::string_view`
- **Arguments:** none.

Returns selected text from the primary field, or an empty view when no selection exists. The view points into manager-owned storage and is invalidated when that field text changes or is removed.

### **insertTextAtPrimaryCaret**
---

#### `bool insertTextAtPrimaryCaret(std::string_view utf8Text)`

- **Returns:** `bool`
- **Arguments:** `utf8Text` text to insert.

Inserts UTF-8 text at the primary caret, replacing active selections. The operation respects read-only state and `FieldConfig::maxBytes`.
