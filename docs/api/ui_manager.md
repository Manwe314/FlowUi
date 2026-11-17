# UI Manager API

[UiManager.hpp](include/FlowUi/Public/managers/UiManager.hpp) is the frame-scoped application authoring surface. `App` owns its lifecycle. Obtain it from the checked `App::ui()` accessor; once acquired, `ui.actions()` and `ui.elements()` return direct borrowed references. The `App::actions()` and `App::elements()` accessors themselves return checked wrappers.

```cpp
auto uiResult = app.ui();
if (!uiResult) {
    throw FlowUi::FlowUiException(uiResult.error());
}
FlowUi::UiManager& ui = uiResult->get();
auto status = ui.setClipboardText("Copied text");
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

Frame storage and bridge helpers return direct values and throw on operational failure. `resolveFont()` returns a direct `FontId` without throwing for an attached manager. Theme lookups, clipboard operations and frame status retain their declared checked results. Builder creation, drawing, manager borrowing and action invocation remain direct throwing APIs. In custom composition, retain the first failure with `recordFrameError()` and cancel a failed frame. Exceptions from application callbacks are allowed to reach the enclosing application frame boundary; output from that failed frame must not be submitted. See [result handling](docs/api/result_handling.md).

`Clay_String`, image payload pointers and frame bridge data borrow current-frame storage. Input/interaction references borrow this manager; do not keep them as frozen snapshots across later frames. Font aliases are `FontId = uint16_t` and `FontFamilyId = uint32_t`; `CursorType` names platform cursor shapes. [UI types](include/FlowUi/Public/managerStructs/UiTypes.hpp) define `FrameInput`, `InteractionSnapshot` and `TextureRef`.

The `createElement()` examples use the complete `Counter` definition in [the element API](docs/api/elements.md). Development builds append an optional `std::source_location` argument to every `createElement()` overload. `FLOW_UI_DEV_MODE` and `FLOW_UI_AGENTIC_WORKFLOW` also expose observation/configuration helpers for tooling; those conditional implementation-facing services are outside the application reference below.

### **UiManager::frameStatus**

#### `[[nodiscard]] Status frameStatus() const noexcept;`

Return this frame's latched failure with UI, then input-field, then popup priority. Each manager retains its first error; this observer does not select the earliest failure across managers. Check before committing custom frame composition.

**Example:**

```cpp
auto result = ui.frameStatus();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::recordFrameError**

#### `void recordFrameError(FlowUiError error) const noexcept;`

Latch the first error for this frame so partially built output cannot be submitted. Later calls preserve the earlier error.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::toClayString**

#### `[[nodiscard]] Clay_String toClayString(std::string_view s);`

Copy text into current-frame storage and return a Clay_String borrowing that storage. ResourceKey supplies the name text and explicit resource attribution.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const std::string_view text = "Save";
const Clay_String clayText = ui.toClayString(text);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::toClayString**

#### `[[nodiscard]] Clay_String toClayString(ResourceKey key);`

Copy text into current-frame storage and return a Clay_String borrowing that storage. ResourceKey supplies the name text and explicit resource attribution.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const FlowUi::ResourceKey text{.name = "Save"};
const Clay_String clayText = ui.toClayString(text);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::imageData**

#### `[[nodiscard]] TextureRef* imageData(TextureRef textureRef);`

Copy a texture reference into current-frame storage and return the pointer used by Clay image configuration. The pointee is valid for this frame only.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
auto result = ui.imageData(texture);
Clay_ImageElementConfig image{};
image.imageData = result;
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::inputContentElement**

#### `[[nodiscard]] Clay_ElementDeclaration inputContentElement(const Clay_TextElementConfig& textConfig) const;`

Return an input-content declaration with growing width and a height derived from the resolved font metrics. The declaration can be passed directly to Clay.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const Clay_ElementDeclaration content = ui.inputContentElement(textConfig);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::toClaySID**

#### `[[nodiscard]] Clay_ElementId toClaySID(std::string_view s);`

Copy the supplied string identity into current-frame storage and return its Clay string ID. ResourceKey carries explicit attribution.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const std::string_view name = "toolbar/save";
const Clay_ElementId clayId = ui.toClaySID(name);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::toClaySID**

#### `[[nodiscard]] Clay_ElementId toClaySID(ResourceKey key);`

Copy the supplied string identity into current-frame storage and return its Clay string ID. ResourceKey carries explicit attribution.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const FlowUi::ResourceKey name{.name = "toolbar/save"};
const Clay_ElementId clayId = ui.toClaySID(name);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::toClayEID**

#### `[[nodiscard]] Clay_ElementId toClayEID(FlowElementID id);`

Validate and bridge a strong local, global, or semantic-part identity into a Clay ID for this window.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const Clay_ElementId clayId = ui.toClayEID(id);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::toClayEID**

#### `[[nodiscard]] Clay_ElementId toClayEID(GlobalFlowID id);`

Validate and bridge a strong local, global, or semantic-part identity into a Clay ID for this window.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const Clay_ElementId clayId = ui.toClayEID(id);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::toClayEID**

#### `[[nodiscard]] Clay_ElementId toClayEID(FlowElementPartID id);`

Validate and bridge a strong local, global, or semantic-part identity into a Clay ID for this window.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const Clay_ElementId clayId = ui.toClayEID(id);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::createElement**

#### `template <FlowElement Element> [[nodiscard]] ElementBuilder<Element> createElement(const Element&, LocalElementName localName );`

Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.

Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.

**Example:**

```cpp
ui.createElement(counter, "counter")
    .setParameters({.width = 160.0f})
    .draw();
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::createElement**

#### `template <FlowElement Element> [[nodiscard]] ElementBuilder<Element> createElement(const Element&, RuntimeElementName localName );`

Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.

Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.

**Example:**

```cpp
ui.createElement(counter, FlowUi::RuntimeName("counter"))
    .setParameters({.width = 160.0f})
    .draw();
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::createElement**

#### `template <FlowElement Element> [[nodiscard]] ElementBuilder<Element> createElement(const Element&, IndexedElementName indexedName );`

Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.

Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.

**Example:**

```cpp
ui.createElement(counter, FlowUi::Indexed("counter", 0))
    .setParameters({.width = 160.0f})
    .draw();
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::createElement**

#### `template <FlowElement Element> [[nodiscard]] ElementBuilder<Element> createElement(const Element&, AutoElementName automaticName = AutoID() );`

Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.

Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.

**Example:**

```cpp
ui.createElement(counter, FlowUi::AutoID())
    .setParameters({.width = 160.0f})
    .draw();
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::createElement**

#### `template <FlowElement Element> [[nodiscard]] ElementBuilder<Element> createElement(const Element&, GlobalFlowID globalId );`

Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.

Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.

**Example:**

```cpp
ui.createElement(counter, id)
    .setParameters({.width = 160.0f})
    .draw();
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::createElement**

#### `template <FlowElement Element> [[nodiscard]] ElementBuilder<Element> createElement(const Element&, FlowElementID resolvedId );`

Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.

Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.

**Example:**

```cpp
ui.createElement(counter, id)
    .setParameters({.width = 160.0f})
    .draw();
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::createElement**

#### `template <FlowElement Element> [[nodiscard]] ElementBuilder<Element> createElement(const Element&, FlowElementPartID partId );`

Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.

Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.

**Example:**

```cpp
ui.createElement(counter, id)
    .setParameters({.width = 160.0f})
    .draw();
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::drawConstructed**

#### `void drawConstructed();`

Close the most recent root opened by ElementBuilder::construct(), including its FlowUi scope and development capture. Each successful construct needs one close.

Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.

**Example:**

```cpp
ui.createElement(counter, "panel").construct();
ui.createElement(counter, "child").draw();
ui.drawConstructed();
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::getPreviousFramesInteraction**

#### `[[nodiscard]] const InteractionSnapshot& getPreviousFramesInteraction() const noexcept;`

Borrow the previous completed frame interaction snapshot. It remains the stable source for current element pointer callbacks.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::getCurrentFrameInput**

#### `[[nodiscard]] const FrameInput& getCurrentFrameInput() const noexcept;`

Borrow this frame input in scaled FlowUi layout coordinates; do not retain this reference as an immutable historical snapshot.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::getPreviousFrameInput**

#### `[[nodiscard]] const FrameInput& getPreviousFrameInput() const noexcept;`

Borrow the previous layout-frame input, useful with current input for edge and drag calculations.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::inputFields**

#### `[[nodiscard]] InputFieldManager& inputFields() noexcept;`

Borrow this window InputFieldManager directly; the const overload exposes read-only access.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::inputFields**

#### `[[nodiscard]] const InputFieldManager& inputFields() const noexcept;`

Borrow this window InputFieldManager directly; the const overload exposes read-only access.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::shortcuts**

#### `[[nodiscard]] ShortcutManager& shortcuts() noexcept;`

Borrow this window ShortcutManager directly; the const overload exposes read-only access.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::shortcuts**

#### `[[nodiscard]] const ShortcutManager& shortcuts() const noexcept;`

Borrow this window ShortcutManager directly; the const overload exposes read-only access.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::popups**

#### `[[nodiscard]] PopupManager& popups() noexcept;`

Borrow this window popup placement and dismissal manager directly; the const overload exposes read-only access.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::popups**

#### `[[nodiscard]] const PopupManager& popups() const noexcept;`

Borrow this window popup placement and dismissal manager directly; the const overload exposes read-only access.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::actions**

#### `[[nodiscard]] ActionManager& actions();`

Borrow the attached app-wide action manager directly. A detached UiManager throws FlowUiException.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::actions**

#### `[[nodiscard]] const ActionManager& actions() const;`

Borrow the attached app-wide action manager directly. A detached UiManager throws FlowUiException.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::invoke**

#### `[[nodiscard]] ActionInvocationStatus invoke(ActionCall call);`

Invoke an action synchronously and return Invoked, Empty, Unbound or Disabled. Arbitrary callback exceptions propagate unchanged; application effects are not rolled back.

**Example:**

```cpp
const FlowUi::ActionInvocationStatus outcome = actions.invoke(call);
const bool invoked = outcome == FlowUi::ActionInvocationStatus::Invoked;
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::setClipboardText**

#### `[[nodiscard]] Status setClipboardText(std::string_view text) const noexcept;`

Copy text to the attached platform clipboard. Detached, unsupported, and failed platform operations are returned as errors; check the Status.

**Example:**

```cpp
auto result = ui.setClipboardText("Copied text");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::clipboardText**

#### `[[nodiscard]] Result<std::string> clipboardText() const noexcept;`

Return an owned copy of clipboard text. An empty successful string is valid text; detached/platform failures are errors.

**Example:**

```cpp
auto result = ui.clipboardText();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const std::string pasted = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::hasClipboardAccess**

#### `[[nodiscard]] bool hasClipboardAccess() const noexcept;`

Return whether the attached window currently supports immediate clipboard reads and writes; detached managers return false.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::requestCursor**

#### `void requestCursor(CursorType cursorType, uint8_t priority = 0) noexcept;`

Request this frame cursor shape. Highest priority wins; a later equal-priority request may replace the earlier one. Requests reset at frame start.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::resolveFont**

#### `[[nodiscard]] FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const noexcept;`

Return the resolved concrete font ID from the attached frame font view. An unavailable family/style resolves to the documented zero/fallback ID without throwing; this does not load a new font.

**Example:**

```cpp
const FlowUi::FontId font = ui.resolveFont(family, 400, FlowUi::FontStyle::Normal);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::resolveFont**

#### `[[nodiscard]] FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const noexcept;`

Return the resolved concrete font ID from the attached frame font view. An unavailable family/style resolves to the documented zero/fallback ID without throwing; this does not load a new font.

**Example:**

```cpp
const FlowUi::FontId font = ui.resolveFont(family, 400, FlowUi::FontStyle::Normal);
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::theme**

#### `template <typename T> [[nodiscard]] Result<std::reference_wrapper<const T>> theme() const noexcept;`

Borrow the active or explicitly named registered theme through Result<reference_wrapper<const T>>. Active lookup respects a development local theme scope; named lookup bypasses it.

Check the Result before result->get(). The wrapper and extracted reference do not own the payload. Reacquire after theme registry changes; a development override also limits the borrow to its enclosing scope.

**Example:**

```cpp
auto result = ui.theme<FlowUi::FlowUiTheme>();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowUiTheme& theme = result->get();
const float fontSize = theme.fontSizeMedium;
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::theme**

#### `template <typename T> [[nodiscard]] Result<std::reference_wrapper<const T>> theme(std::string_view variantName) const noexcept;`

Borrow the active or explicitly named registered theme through Result<reference_wrapper<const T>>. Active lookup respects a development local theme scope; named lookup bypasses it.

Check the Result before result->get(). The wrapper and extracted reference do not own the payload. Reacquire after theme registry changes; a development override also limits the borrow to its enclosing scope.

**Example:**

```cpp
auto result = ui.theme<FlowUi::FlowUiTheme>("light");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowUiTheme& theme = result->get();
const float fontSize = theme.fontSizeMedium;
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::flowTheme**

#### `[[nodiscard]] Result<std::reference_wrapper<const FlowUiTheme>> flowTheme() const noexcept;`

Borrow the active built-in FlowUiTheme through Result<reference_wrapper<const FlowUiTheme>>, including a development local theme scope.

Check the Result before result->get(). The wrapper and extracted reference do not own the payload. Reacquire after theme registry changes; a development override also limits the borrow to its enclosing scope.

**Example:**

```cpp
auto result = ui.flowTheme();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowUiTheme& theme = result->get();
const float fontSize = theme.fontSizeMedium;
```

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::elements**

#### `[[nodiscard]] ElementManager& elements();`

Borrow the attached app-wide element manager directly. A detached UiManager throws FlowUiException.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::elements**

#### `[[nodiscard]] const ElementManager& elements() const;`

Borrow the attached app-wide element manager directly. A detached UiManager throws FlowUiException.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

### **UiManager::windowId**

#### `[[nodiscard]] WindowId windowId() const noexcept;`

Return the window whose frame is built by this manager.

See: [Owning header](include/FlowUi/Public/managers/UiManager.hpp).

## Interaction snapshots and texture values

`InteractionSnapshot` contains contiguous `hoveredElementIds`, `pressedElementIds`, `heldElementIds` and `releasedElementIds` vectors. These store Clay numeric identities for one completed frame. The primary pointer can appear in several lists when the recorded transitions permit it. All four observers below accept Clay, local Flow, global Flow and semantic-part Flow IDs directly; strong Flow identities are folded using the bridge identity convention. A false observation is ordinary absence, with no Result wrapper.

### **InteractionSnapshot::isHovered**

#### `[[nodiscard]] bool isHovered(Clay_ElementId id) const noexcept;`

Return whether this snapshot records the given element as hovered. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isHovered**

#### `[[nodiscard]] bool isHovered(FlowElementID id) const noexcept;`

Return whether this snapshot records the given element as hovered. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isHovered**

#### `[[nodiscard]] bool isHovered(GlobalFlowID id) const noexcept;`

Return whether this snapshot records the given element as hovered. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isHovered**

#### `[[nodiscard]] bool isHovered(FlowElementPartID id) const noexcept;`

Return whether this snapshot records the given element as hovered. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isPressed**

#### `[[nodiscard]] bool isPressed(Clay_ElementId id) const noexcept;`

Return whether this snapshot records the given element as pressed. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isPressed**

#### `[[nodiscard]] bool isPressed(FlowElementID id) const noexcept;`

Return whether this snapshot records the given element as pressed. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isPressed**

#### `[[nodiscard]] bool isPressed(GlobalFlowID id) const noexcept;`

Return whether this snapshot records the given element as pressed. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isPressed**

#### `[[nodiscard]] bool isPressed(FlowElementPartID id) const noexcept;`

Return whether this snapshot records the given element as pressed. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isHeld**

#### `[[nodiscard]] bool isHeld(Clay_ElementId id) const noexcept;`

Return whether this snapshot records the given element as held. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isHeld**

#### `[[nodiscard]] bool isHeld(FlowElementID id) const noexcept;`

Return whether this snapshot records the given element as held. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isHeld**

#### `[[nodiscard]] bool isHeld(GlobalFlowID id) const noexcept;`

Return whether this snapshot records the given element as held. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isHeld**

#### `[[nodiscard]] bool isHeld(FlowElementPartID id) const noexcept;`

Return whether this snapshot records the given element as held. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isReleased**

#### `[[nodiscard]] bool isReleased(Clay_ElementId id) const noexcept;`

Return whether this snapshot records the given element as released. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isReleased**

#### `[[nodiscard]] bool isReleased(FlowElementID id) const noexcept;`

Return whether this snapshot records the given element as released. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isReleased**

#### `[[nodiscard]] bool isReleased(GlobalFlowID id) const noexcept;`

Return whether this snapshot records the given element as released. The snapshot is read without modifying the current frame.

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **InteractionSnapshot::isReleased**

#### `[[nodiscard]] bool isReleased(FlowElementPartID id) const noexcept;`

Return whether this snapshot records the given element as released. The snapshot is read without modifying the current frame.

**Example:**

```cpp
const FlowUi::InteractionSnapshot& snapshot = ui.getPreviousFramesInteraction();
const bool invoked = snapshot.isPressed(counterId);
```

See: [Owning header](include/FlowUi/Public/managerStructs/UiTypes.hpp).

### **TextureRef::fromStable**

#### `[[nodiscard]] static constexpr TextureRef fromStable( ResourceDomain domain, std::string_view key, TextureFitMode fit = TextureFitMode::Contain, bool tint = false) noexcept;`

Create an unresolved reference from a borrowed logical manager key. This neither loads nor validates an asset, and owns no GPU resource. Keep key text alive while retaining the reference. `fitMode` and `tintEnabled` are application-adjustable; resolved handles, UV coordinates and source dimensions are populated by the owning manager. `TextureFitMode` supports `Stretch`, `Contain`, `Cover` and `None`.

**Example:**

```cpp
const FlowUi::TextureRef texture = FlowUi::TextureRef::fromStable(
    FlowUi::ResourceDomain::Image, "branding/logo", FlowUi::TextureFitMode::Contain, false);
```

See: [Owning header](include/FlowUi/Public/TextureRef.hpp).
