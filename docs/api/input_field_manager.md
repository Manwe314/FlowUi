# Input Field Manager API

[Public declarations](include/FlowUi/Public/managers/InputFieldManager.hpp) · [Field values](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp)

The window-owned manager retains editable text by strong Flow ID or window-local InputField `ResourceKey`. An element requests its snapshot once per UI build and draws its Clay text/content nodes. Initial text is copied only for a new field. Typed local/global/part IDs remain distinct identities; `ResourceKey` identities are normalized in the InputField domain.

Single-line fields expose contiguous text. Multiline fields expose visible lines and chunked `FieldTextView`; submit each emitted visible text line with the frame-scoped handle. Text, visible-line spans, selections, and edit/command reports borrow manager storage and must be copied before mutation/removal or frame advancement. Reversible transaction reports must be copied into application-owned undo history.

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

Call `ui.inputFields()` on that `UiManager&` to obtain the direct manager reference. The reference is borrowed; the App must outlive it, and window-owned managers must also stay within their owning window lifetime. The examples use an already acquired manager reference and existing field/window identities where needed. Field acquisition and mutations return direct values, booleans or `EditResult`; operational failures throw and must leave the failed invocation. `frameStatus()` and `FieldTextView::copy()` retain checked results. Include the relevant manager/value headers and `FlowUi/Public/Color.hpp` for the public `ValueOrThrow` convenience.

ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Direct `bool` return values describe the operation outcome, so `false` is an ordinary rejection/no-change answer. `EditResult` likewise distinguishes applied, no-change and rejected edits without an outer Result. Operational exceptions latch failure and belong to the enclosing frame boundary.

## Value helpers: FieldTextView

### **FieldTextView**

#### `FieldTextView() noexcept = default`

- **Returns:** `FieldTextView (construction)`
- **Arguments:** none.

Construct an empty detached text view. The view has zero bytes and no contiguous storage.

The view owns no text.

**Example:**

```cpp
FlowUi::FieldTextView textView;
const auto isEmpty = textView.empty();
```

See: [Owning header](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp).

### **sizeBytes**

#### `[[nodiscard]] size_t sizeBytes() const noexcept`

- **Returns:** `size_t`
- **Arguments:** none.

Read the viewed document's UTF-8 byte count. Returns zero when detached.

Valid only while the underlying field view remains alive and unmodified.

**Example:**

```cpp
const auto byteCount = value.sizeBytes();
```

See: [Owning header](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp).

### **empty**

#### `[[nodiscard]] bool empty() const noexcept`

- **Returns:** `bool`
- **Arguments:** none.

Test whether the viewed document has zero bytes. A detached view is empty.

Valid only while the underlying field view remains alive and unmodified.

**Example:**

```cpp
const auto isEmpty = value.empty();
```

See: [Owning header](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp).

### **contiguous**

#### `[[nodiscard]] std::optional<std::string_view> contiguous() const noexcept`

- **Returns:** `std::optional<std::string_view>`
- **Arguments:** none.

Borrow contiguous text storage if available. Returns nullopt for detached or noncontiguous text. A present empty string_view is valid empty contiguous storage.

The borrowed bytes are invalidated by field mutation/removal or manager destruction; do not assume multiline text is contiguous.

**Example:**

```cpp
const auto contiguousText = value.contiguous();
```

See: [Owning header](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp).

### **copy**

#### `[[nodiscard]] Result<std::string> copy() const noexcept`

- **Returns:** `Result<std::string>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** none.

Copy a full document or byte range to caller-owned text. Success contains an owning std::string. Bounds are clamped to the document; a detached view successfully copies an empty string. Allocation/storage failures are errors.

The range is half-open in UTF-8 bytes. Copy while the view is valid; the resulting string survives later field mutations.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = value.copy();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
std::string text = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp).

### **copy**

#### `[[nodiscard]] Result<std::string> copy(TextRange range) const noexcept`

- **Returns:** `Result<std::string>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `range` half-open UTF-8 byte interval

Copy a full document or byte range to caller-owned text. Success contains an owning std::string. Bounds are clamped to the document; a detached view successfully copies an empty string. Allocation/storage failures are errors.

The range is half-open in UTF-8 bytes. Copy while the view is valid; the resulting string survives later field mutations.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = value.copy(FlowUi::TextRange{0, value.sizeBytes()});
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
std::string text = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp).

### **forEachChunk**

#### `void forEachChunk(TextRange range, const TextChunkVisitor& visitor) const`

- **Returns:** none (`void`)
- **Arguments:** `range` half-open UTF-8 byte interval, `visitor` synchronous observation callback

Synchronously visit text chunks overlapping a byte range. Makes no callbacks for a detached view or empty clamped range.

Bounds are clamped. Chunks are borrowed only during each call; copy to retain them and do not mutate the field while visiting. Visitor exceptions propagate unchanged; earlier visitor side effects remain visible.

Exceptions: Visitor exceptions propagate unchanged.

**Example:**

```cpp
std::string ownedText;
value.forEachChunk(FlowUi::TextRange{0, value.sizeBytes()}, [&ownedText](std::string_view chunk) { ownedText.append(chunk); });
```

See: [Owning header](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp).

## Value helpers: FieldHandle

### **operator bool**

#### `[[nodiscard]] constexpr explicit operator bool() const noexcept`

- **Returns:** `explicit operator`
- **Arguments:** none.

Test whether the opaque handle contains a field slot. Returns value != 0; it does not validate frameEpoch or current field existence.

Only submitTextSpan() checks whether a handle is acceptable for the current frame.

**Example:**

```cpp
const auto valid = static_cast<bool>(value);
```

See: [Owning header](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp).

### **operator==**

#### `friend constexpr bool operator==(FieldHandle, FieldHandle) noexcept = default`

- **Returns:** `friend bool`
- **Arguments:** none.

Compare two field tokens by slot identity and frame epoch. Returns true only when both value and frameEpoch match.

Equality does not establish that either token is accepted by the current frame.

**Example:**

```cpp
const auto sameHandle = value == other;
```

See: [Owning header](include/FlowUi/Public/managerStructs/InputFieldTypes.hpp).

## Public API

### **frameStatus**

#### `[[nodiscard]] Status frameStatus() const noexcept`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** none.

Read the latched failure for the current input-field frame. Success means no failure was latched; an error means the surrounding frame must be canceled.

A successful later call does not clear an earlier frame failure. UiManager checks this status when completing the frame.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.frameStatus();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **requestField**

#### `[[nodiscard]] FieldQueryResult requestField(FlowElementID fieldId, const FieldRequest& request)`

- **Returns:** `FieldQueryResult`.
- **Arguments:** `fieldId`, `request` operation configuration

Register or update a field and borrow its current frame snapshot. Returns FieldQueryResult directly; an empty strong ID yields an empty snapshot. New fields copy initialText; later requests preserve edited text.

Call once per field during its active UI build. Update config and Clay text/content IDs each frame. An unrequested focused field loses primary focus at frame finalization. Text, visible lines, selections, and transaction spans are manager-owned; copy data before field mutation/removal or the next frame invalidates it.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
FlowUi::FieldRequest request{};
request.initialText = "Search";
request.textElementId = textElementId;
request.contentElementId = contentElementId;
const FlowUi::FieldQueryResult field = manager.requestField(fieldId, request);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **requestField**

#### `[[nodiscard]] FieldQueryResult requestField(GlobalFlowID fieldId, const FieldRequest& request)`

- **Returns:** `FieldQueryResult`.
- **Arguments:** `fieldId`, `request` operation configuration

Register or update a field and borrow its current frame snapshot. Returns FieldQueryResult directly; an empty strong ID yields an empty snapshot. New fields copy initialText; later requests preserve edited text.

Call once per field during its active UI build. Update config and Clay text/content IDs each frame. An unrequested focused field loses primary focus at frame finalization. Text, visible lines, selections, and transaction spans are manager-owned; copy data before field mutation/removal or the next frame invalidates it.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
FlowUi::FieldRequest request{};
request.initialText = "Search";
request.textElementId = textElementId;
request.contentElementId = contentElementId;
const FlowUi::FieldQueryResult field = manager.requestField(fieldId, request);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **requestField**

#### `[[nodiscard]] FieldQueryResult requestField(FlowElementPartID fieldId, const FieldRequest& request)`

- **Returns:** `FieldQueryResult`.
- **Arguments:** `fieldId`, `request` operation configuration

Register or update a field and borrow its current frame snapshot. Returns FieldQueryResult directly; an empty strong ID yields an empty snapshot. New fields copy initialText; later requests preserve edited text.

Call once per field during its active UI build. Update config and Clay text/content IDs each frame. An unrequested focused field loses primary focus at frame finalization. Text, visible lines, selections, and transaction spans are manager-owned; copy data before field mutation/removal or the next frame invalidates it.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
FlowUi::FieldRequest request{};
request.initialText = "Search";
request.textElementId = textElementId;
request.contentElementId = contentElementId;
const FlowUi::FieldQueryResult field = manager.requestField(fieldId, request);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **requestField**

#### `[[nodiscard]] FieldQueryResult requestField(ResourceKey key, const FieldRequest& request)`

- **Returns:** `FieldQueryResult`.
- **Arguments:** `key` logical resource identity, `request` operation configuration

Register or update a field and borrow its current frame snapshot. Returns FieldQueryResult directly; an empty strong ID yields an empty snapshot. New fields copy initialText; later requests preserve edited text.

Call once per field during its active UI build. Update config and Clay text/content IDs each frame. An unrequested focused field loses primary focus at frame finalization. Text, visible lines, selections, and transaction spans are manager-owned; copy data before field mutation/removal or the next frame invalidates it. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "search"};
FlowUi::FieldRequest request{};
request.initialText = "Search";
request.textElementId = textElementId;
request.contentElementId = contentElementId;
const FlowUi::FieldQueryResult field = manager.requestField(key, request);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **submitTextSpan**

#### `[[nodiscard]] bool submitTextSpan(FieldHandle field, const FieldTextSpanSubmission& span)`

- **Returns:** `bool`.
- **Arguments:** `field` current frame field handle, `span` visible-line text-node submission

Associate a visible multiline Clay text node with its document range. Returns true when accepted; false means an expired handle, unknown field, invalid text ID/range, or range not matching a visible line.

Submit every emitted VisibleTextLine during the same frame as requestField(). FieldHandle::operator bool only tests its slot and does not validate the epoch.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const FlowUi::FieldTextSpanSubmission span{
    .logicalRange = line.logicalRange,
    .textElementId = textElementId,
    .visualLineIndex = line.visualLineIndex,
};
const auto accepted = manager.submitTextSpan(field, span);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **requestCaret**

#### `void requestCaret(FlowElementID fieldId, CaretRequestKind kind)`

- **Returns:** `void` (no payload).
- **Arguments:** `fieldId`, `kind` caret operation

Apply a focus or caret request to persistent field state. Returns normally when the request completes. SetPrimary may create empty state before requestField(); ClearAll clears every field's focus and ignores a strong identity; ResourceKey overloads still validate their key before applying it.

SetPrimary is the normal click-focus operation; Add requests an additional caret. Changes take effect immediately in manager state.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
manager.requestCaret(fieldId, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **requestCaret**

#### `void requestCaret(GlobalFlowID fieldId, CaretRequestKind kind)`

- **Returns:** `void` (no payload).
- **Arguments:** `fieldId`, `kind` caret operation

Apply a focus or caret request to persistent field state. Returns normally when the request completes. SetPrimary may create empty state before requestField(); ClearAll clears every field's focus and ignores a strong identity; ResourceKey overloads still validate their key before applying it.

SetPrimary is the normal click-focus operation; Add requests an additional caret. Changes take effect immediately in manager state.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
manager.requestCaret(fieldId, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **requestCaret**

#### `void requestCaret(FlowElementPartID fieldId, CaretRequestKind kind)`

- **Returns:** `void` (no payload).
- **Arguments:** `fieldId`, `kind` caret operation

Apply a focus or caret request to persistent field state. Returns normally when the request completes. SetPrimary may create empty state before requestField(); ClearAll clears every field's focus and ignores a strong identity; ResourceKey overloads still validate their key before applying it.

SetPrimary is the normal click-focus operation; Add requests an additional caret. Changes take effect immediately in manager state.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
manager.requestCaret(fieldId, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **requestCaret**

#### `void requestCaret(ResourceKey key, CaretRequestKind kind)`

- **Returns:** `void` (no payload).
- **Arguments:** `key` logical resource identity, `kind` caret operation

Apply a focus or caret request to persistent field state. Returns normally when the request completes. SetPrimary may create empty state before requestField(); ClearAll clears every field's focus and ignores a strong identity; ResourceKey overloads still validate their key before applying it.

SetPrimary is the normal click-focus operation; Add requests an additional caret. Changes take effect immediately in manager state. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "search"};
manager.requestCaret(key, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **removeField**

#### `[[nodiscard]] bool removeField(FlowElementID fieldId)`

- **Returns:** `bool`.
- **Arguments:** `fieldId`

Remove a field's text, configuration, carets, and selection. Returns true if a field was removed or false for an empty strong ID/missing field.

Removing a focused field clears or transfers primary focus. All borrowed field text/views/reports become invalid.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto removed = manager.removeField(fieldId);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **removeField**

#### `[[nodiscard]] bool removeField(GlobalFlowID fieldId)`

- **Returns:** `bool`.
- **Arguments:** `fieldId`

Remove a field's text, configuration, carets, and selection. Returns true if a field was removed or false for an empty strong ID/missing field.

Removing a focused field clears or transfers primary focus. All borrowed field text/views/reports become invalid.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto removed = manager.removeField(fieldId);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **removeField**

#### `[[nodiscard]] bool removeField(FlowElementPartID fieldId)`

- **Returns:** `bool`.
- **Arguments:** `fieldId`

Remove a field's text, configuration, carets, and selection. Returns true if a field was removed or false for an empty strong ID/missing field.

Removing a focused field clears or transfers primary focus. All borrowed field text/views/reports become invalid.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto removed = manager.removeField(fieldId);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **removeField**

#### `[[nodiscard]] bool removeField(ResourceKey key)`

- **Returns:** `bool`.
- **Arguments:** `key` logical resource identity

Remove a field's text, configuration, carets, and selection. Returns true if a field was removed or false for an empty strong ID/missing field.

Removing a focused field clears or transfers primary focus. All borrowed field text/views/reports become invalid. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "search"};
const auto removed = manager.removeField(key);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **replaceText**

#### `[[nodiscard]] bool replaceText(FlowElementID fieldId, std::string_view text, bool preserveCaret = true)`

- **Returns:** `bool`.
- **Arguments:** `fieldId`, `text` UTF-8 text, `preserveCaret` whether to remap existing carets

Replace an existing field's text while retaining its configuration. Returns true if text changed; false for an empty strong ID, missing field, unchanged text, or a rejected edit (invalid UTF-8/range, newline policy, or maxBytes).

preserveCaret defaults to true: carets/selections are remapped through the smallest differing UTF-8 range and clamped. false clears active carets only after an applied replacement. Programmatic replacement bypasses readOnly, while UTF-8/newline/maxBytes validation still applies. Any field text mutation invalidates borrowed field text/views.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto changed = manager.replaceText(fieldId, "Updated text", true);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **replaceText**

#### `[[nodiscard]] bool replaceText(GlobalFlowID fieldId, std::string_view text, bool preserveCaret = true)`

- **Returns:** `bool`.
- **Arguments:** `fieldId`, `text` UTF-8 text, `preserveCaret` whether to remap existing carets

Replace an existing field's text while retaining its configuration. Returns true if text changed; false for an empty strong ID, missing field, unchanged text, or a rejected edit (invalid UTF-8/range, newline policy, or maxBytes).

preserveCaret defaults to true: carets/selections are remapped through the smallest differing UTF-8 range and clamped. false clears active carets only after an applied replacement. Programmatic replacement bypasses readOnly, while UTF-8/newline/maxBytes validation still applies. Any field text mutation invalidates borrowed field text/views.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto changed = manager.replaceText(fieldId, "Updated text", true);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **replaceText**

#### `[[nodiscard]] bool replaceText(FlowElementPartID fieldId, std::string_view text, bool preserveCaret = true)`

- **Returns:** `bool`.
- **Arguments:** `fieldId`, `text` UTF-8 text, `preserveCaret` whether to remap existing carets

Replace an existing field's text while retaining its configuration. Returns true if text changed; false for an empty strong ID, missing field, unchanged text, or a rejected edit (invalid UTF-8/range, newline policy, or maxBytes).

preserveCaret defaults to true: carets/selections are remapped through the smallest differing UTF-8 range and clamped. false clears active carets only after an applied replacement. Programmatic replacement bypasses readOnly, while UTF-8/newline/maxBytes validation still applies. Any field text mutation invalidates borrowed field text/views.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto changed = manager.replaceText(fieldId, "Updated text", true);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **replaceText**

#### `[[nodiscard]] bool replaceText(ResourceKey key, std::string_view text, bool preserveCaret = true)`

- **Returns:** `bool`.
- **Arguments:** `key` logical resource identity, `text` UTF-8 text, `preserveCaret` whether to remap existing carets

Replace an existing field's text while retaining its configuration. Returns true if text changed; false for an empty strong ID, missing field, unchanged text, or a rejected edit (invalid UTF-8/range, newline policy, or maxBytes).

preserveCaret defaults to true: carets/selections are remapped through the smallest differing UTF-8 range and clamped. false clears active carets only after an applied replacement. Programmatic replacement bypasses readOnly, while UTF-8/newline/maxBytes validation still applies. Any field text mutation invalidates borrowed field text/views. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "search"};
const auto changed = manager.replaceText(key, "Updated text", true);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **clear**

#### `void clear()`

- **Returns:** `void` (no payload).
- **Arguments:** none.

Clear every field and reset focus, repeat, drag, and frame overrides. Success means the reset completed.

All previously borrowed field views, spans, text, and handles must be discarded.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
manager.clear();
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **hasPrimaryFieldFocus**

#### `[[nodiscard]] bool hasPrimaryFieldFocus() const noexcept`

- **Returns:** `bool`
- **Arguments:** none.

Test whether a primary field has at least one caret. Returns false if detached, no primary field exists, or its caret list is empty.

This is a direct noexcept query; it does not report initialization errors.

**Example:**

```cpp
const auto hasFocus = manager.hasPrimaryFieldFocus();
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **canEditPrimaryField**

#### `[[nodiscard]] bool canEditPrimaryField() const noexcept`

- **Returns:** `bool`
- **Arguments:** none.

Test whether the primary field exists, has a caret, and is writable. Returns false for detached, unfocused, missing, caret-free, or read-only fields.

This is a direct noexcept query.

**Example:**

```cpp
const auto editable = manager.canEditPrimaryField();
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **getSelectedText**

#### `[[nodiscard]] std::string_view getSelectedText() const`

- **Returns:** `std::string_view`.
- **Arguments:** none.

Borrow the earliest selected range in the primary field. Returns an empty view if there is no primary field or active selection. Detached state and copy/allocation failures throw.

Single-line selections borrow field storage; chunked selections borrow scratch storage reused by a later getSelectedText() call. Copy to std::string immediately to retain it across another query, text mutation, removal, or frame/lifetime change.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
auto result = manager.getSelectedText();
const std::string selectedText{result};
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **insertTextAtPrimaryCaret**

#### `[[nodiscard]] bool insertTextAtPrimaryCaret(std::string_view utf8Text)`

- **Returns:** `bool`.
- **Arguments:** `utf8Text`

Insert UTF-8 text at every caret in the primary field, replacing selections. Returns true when text/caret state changed; false for empty text, absent focus/carets, read-only fields, unchanged text/carets, or rejected UTF-8/newline/size policies.

The input is consumed synchronously. FieldConfig readOnly/maxBytes rules apply; normal keyboard text already comes through the frame pipeline.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto changed = manager.insertTextAtPrimaryCaret("Pasted text");
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **enqueueCommand**

#### `[[nodiscard]] bool enqueueCommand(TextCommand command, std::string_view payload = {})`

- **Returns:** `bool`.
- **Arguments:** `command`, `payload`

Queue a semantic text command for the field focused now. Returns true when queued or false when no primary field owns focus.

payload is copied. The queue captures field identity and selection-extension state; commands execute during field processing, not necessarily at submission.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const FlowUi::TextCommand command = FlowUi::TextCommand::SelectAll;
const auto queued = manager.enqueueCommand(command);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **applyEdits**

#### `[[nodiscard]] EditResult applyEdits( FlowElementID fieldId, std::span<const TextReplacement> edits, EditOrigin origin = EditOrigin::Programmatic)`

- **Returns:** `EditResult`.
- **Arguments:** `fieldId`, `edits` replacement batch in original-document byte coordinates, `origin` edit attribution

Validate and atomically apply a replacement batch to an existing field. Returns EditResult directly: Applied, NoChange, or a Rejected* outcome. Rejection is an ordinary edit outcome; operational failures throw.

Ranges use original-document UTF-8 byte coordinates, must be in bounds, nonoverlapping, and on codepoint boundaries. Rejections leave text unchanged. No primary focus is needed; readOnly, newline policy, valid UTF-8, and maxBytes are enforced. Input spans/strings are consumed synchronously. origin defaults to Programmatic.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const std::array edits{FlowUi::TextReplacement{
    .oldRange = {0, 0},
    .insertedText = "Hello",
}};
const auto edit = manager.applyEdits(fieldId, edits, FlowUi::EditOrigin::Programmatic);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **applyEdits**

#### `[[nodiscard]] EditResult applyEdits( GlobalFlowID fieldId, std::span<const TextReplacement> edits, EditOrigin origin = EditOrigin::Programmatic)`

- **Returns:** `EditResult`.
- **Arguments:** `fieldId`, `edits` replacement batch in original-document byte coordinates, `origin` edit attribution

Validate and atomically apply a replacement batch to an existing field. Returns EditResult directly: Applied, NoChange, or a Rejected* outcome. Rejection is an ordinary edit outcome; operational failures throw.

Ranges use original-document UTF-8 byte coordinates, must be in bounds, nonoverlapping, and on codepoint boundaries. Rejections leave text unchanged. No primary focus is needed; readOnly, newline policy, valid UTF-8, and maxBytes are enforced. Input spans/strings are consumed synchronously. origin defaults to Programmatic.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const std::array edits{FlowUi::TextReplacement{
    .oldRange = {0, 0},
    .insertedText = "Hello",
}};
const auto edit = manager.applyEdits(fieldId, edits, FlowUi::EditOrigin::Programmatic);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **applyEdits**

#### `[[nodiscard]] EditResult applyEdits( FlowElementPartID fieldId, std::span<const TextReplacement> edits, EditOrigin origin = EditOrigin::Programmatic)`

- **Returns:** `EditResult`.
- **Arguments:** `fieldId`, `edits` replacement batch in original-document byte coordinates, `origin` edit attribution

Validate and atomically apply a replacement batch to an existing field. Returns EditResult directly: Applied, NoChange, or a Rejected* outcome. Rejection is an ordinary edit outcome; operational failures throw.

Ranges use original-document UTF-8 byte coordinates, must be in bounds, nonoverlapping, and on codepoint boundaries. Rejections leave text unchanged. No primary focus is needed; readOnly, newline policy, valid UTF-8, and maxBytes are enforced. Input spans/strings are consumed synchronously. origin defaults to Programmatic.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const std::array edits{FlowUi::TextReplacement{
    .oldRange = {0, 0},
    .insertedText = "Hello",
}};
const auto edit = manager.applyEdits(fieldId, edits, FlowUi::EditOrigin::Programmatic);
```

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).

### **applyEdits**

#### `[[nodiscard]] EditResult applyEdits( ResourceKey key, std::span<const TextReplacement> edits, EditOrigin origin = EditOrigin::Programmatic)`

- **Returns:** `EditResult`.
- **Arguments:** `key` logical resource identity, `edits` replacement batch in original-document byte coordinates, `origin` edit attribution

Validate and atomically apply a replacement batch to an existing field. Returns EditResult directly: Applied, NoChange, or a Rejected* outcome. Rejection is an ordinary edit outcome; operational failures throw.

Ranges use original-document UTF-8 byte coordinates, must be in bounds, nonoverlapping, and on codepoint boundaries. Rejections leave text unchanged. No primary focus is needed; readOnly, newline policy, valid UTF-8, and maxBytes are enforced. Input spans/strings are consumed synchronously. origin defaults to Programmatic. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "search"};
const std::array edits{FlowUi::TextReplacement{
    .oldRange = {0, 0},
    .insertedText = "Hello",
}};
const auto edit = manager.applyEdits(key, edits, FlowUi::EditOrigin::Programmatic);
```

## Build-gated development hooks

`FLOW_UI_DEV_MODE` and the development memory-level macros expose additional inspection/recording hooks in the header. They are development infrastructure, not the application API described here. Their existing build gates and signatures are unchanged.

See: [Owning header](include/FlowUi/Public/managers/InputFieldManager.hpp).
