# Popup Manager API

[Public declarations](include/FlowUi/Public/managers/PopupManager.hpp) · [Popup values](include/FlowUi/Public/managerStructs/PopupTypes.hpp)

Submit each open popup once during the active window UI build. `PopupRequest` defaults to parent anchoring, bottom-left to top-left placement, flip/shift overflow handling, hidden first-frame measurement, casual layer, pointer capture, anchor-blocking outside-press dismissal, and Escape dismissal. `expectedSize` supplies an optional positive finite layout-pixel estimate. Missing-anchor, duplicate, and capacity behavior follows the window error policy.

A successful `PopupFrame` can be visible, hidden for measurement (`measureOnly`), or entirely hidden. Emit its floating root for either visible or measure-only frames. Placement/dismissal state commits with the window build and rolls back if the build is canceled; dismissals requested outside a build update committed state immediately.

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

Call `ui.popups()` on that `UiManager&` to obtain the direct manager reference. The reference is borrowed; the App must outlive it, and window-owned managers must also stay within their owning window lifetime. The examples use an already acquired manager reference and existing field/window identities where needed. They choose an application policy of throwing `FlowUiException` after a failed Result; returning/reporting the error is equally possible. Include the relevant manager/value headers and `FlowUi/Public/errorManagement/Exception.hpp`.

`request()` retains `Result<PopupFrame>` because configured duplicate rejection can be handled locally. `dismiss()` returns void and `consumeDismissed()` returns bool directly; their operational failures throw and latch frame failure. A false dismissal notification is an ordinary answer.

## Value helpers: FlowUi

### **toClayAttachment**

#### `[[nodiscard]] constexpr Clay_FloatingAttachPointType toClayAttachment( PopupAttachmentPoint point) noexcept`

- **Returns:** `Clay_FloatingAttachPointType`
- **Arguments:** `point`

Convert a popup attachment point to Clay's alignment enum. An unrecognized enum value maps to CLAY_ATTACH_POINT_CENTER_CENTER.

This is a constexpr/noexcept value conversion.

**Example:**

```cpp
const auto attachment = FlowUi::toClayAttachment(FlowUi::PopupAttachmentPoint::BottomLeft);
```

See: [Owning header](include/FlowUi/Public/managerStructs/PopupTypes.hpp).

## Value helpers: PopupAnchor

### **parent**

#### `[[nodiscard]] static constexpr PopupAnchor parent() noexcept`

- **Returns:** `PopupAnchor`
- **Arguments:** none.

Create an anchor for the current Clay parent. Returns a value descriptor; placement is resolved during popup layout.

This constexpr/noexcept factory owns no UI object.

**Example:**

```cpp
const auto anchor = FlowUi::PopupAnchor::parent();
```

See: [Owning header](include/FlowUi/Public/managerStructs/PopupTypes.hpp).

### **element**

#### `[[nodiscard]] static constexpr PopupAnchor element(FlowElementID id) noexcept`

- **Returns:** `PopupAnchor`
- **Arguments:** `id` window-scoped identity

Create an anchor for a Flow element or semantic part in the current window. Returns an Element descriptor containing the supplied identity value.

Local, global, and semantic part IDs all resolve in the popup's current window; a global name does not select a different window.

**Example:**

```cpp
const auto anchor = FlowUi::PopupAnchor::element(fieldId);
```

See: [Owning header](include/FlowUi/Public/managerStructs/PopupTypes.hpp).

### **element**

#### `[[nodiscard]] static constexpr PopupAnchor element(GlobalFlowID id) noexcept`

- **Returns:** `PopupAnchor`
- **Arguments:** `id` window-scoped identity

Create an anchor for a Flow element or semantic part in the current window. Returns an Element descriptor containing the supplied identity value.

Local, global, and semantic part IDs all resolve in the popup's current window; a global name does not select a different window.

**Example:**

```cpp
const auto anchor = FlowUi::PopupAnchor::element(fieldId);
```

See: [Owning header](include/FlowUi/Public/managerStructs/PopupTypes.hpp).

### **element**

#### `[[nodiscard]] static constexpr PopupAnchor element(FlowElementPartID id) noexcept`

- **Returns:** `PopupAnchor`
- **Arguments:** `id` window-scoped identity

Create an anchor for a Flow element or semantic part in the current window. Returns an Element descriptor containing the supplied identity value.

Local, global, and semantic part IDs all resolve in the popup's current window; a global name does not select a different window.

**Example:**

```cpp
const auto anchor = FlowUi::PopupAnchor::element(fieldId);
```

See: [Owning header](include/FlowUi/Public/managerStructs/PopupTypes.hpp).

### **pointerSnapshot**

#### `[[nodiscard]] static constexpr PopupAnchor pointerSnapshot() noexcept`

- **Returns:** `PopupAnchor`
- **Arguments:** none.

Create an anchor at the pointer position first resolved for this popup. Returns a value descriptor; the resolved coordinate is retained while the popup remains open.

This constexpr/noexcept factory does not sample input immediately.

**Example:**

```cpp
const auto anchor = FlowUi::PopupAnchor::pointerSnapshot();
```

See: [Owning header](include/FlowUi/Public/managerStructs/PopupTypes.hpp).

### **pointerFollow**

#### `[[nodiscard]] static constexpr PopupAnchor pointerFollow() noexcept`

- **Returns:** `PopupAnchor`
- **Arguments:** none.

Create an anchor that follows the current frame pointer. Returns a value descriptor sampled when submitted.

This constexpr/noexcept factory owns no pointer state.

**Example:**

```cpp
const auto anchor = FlowUi::PopupAnchor::pointerFollow();
```

See: [Owning header](include/FlowUi/Public/managerStructs/PopupTypes.hpp).

### **position**

#### `[[nodiscard]] static constexpr PopupAnchor position(Clay_Vector2 value) noexcept`

- **Returns:** `PopupAnchor`
- **Arguments:** `value`

Create an anchor at an explicit window-layout coordinate. Returns a Position descriptor storing the coordinate.

Coordinates use logical layout pixels, not framebuffer pixels.

**Example:**

```cpp
const auto anchor = FlowUi::PopupAnchor::position(Clay_Vector2{24.0f, 48.0f});
```

See: [Owning header](include/FlowUi/Public/managerStructs/PopupTypes.hpp).

### **viewport**

#### `[[nodiscard]] static constexpr PopupAnchor viewport() noexcept`

- **Returns:** `PopupAnchor`
- **Arguments:** none.

Create an anchor for the current window viewport rectangle. Returns a value descriptor resolved during layout.

This constexpr/noexcept factory owns no viewport object.

**Example:**

```cpp
const auto anchor = FlowUi::PopupAnchor::viewport();
```

See: [Owning header](include/FlowUi/Public/managerStructs/PopupTypes.hpp).

## Public API

### **frameStatus**

#### `[[nodiscard]] Status frameStatus() const noexcept`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** none.

Read a caught failure requiring cancellation of the popup frame. Success means no popup frame error has been latched; an error requires cancellation of the surrounding frame.

UiManager checks the latch during frame completion.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.frameStatus();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).

### **request**

#### `[[nodiscard]] Result<PopupFrame> request(FlowElementID popupId, const PopupRequest& request) noexcept`

- **Returns:** `Result<PopupFrame>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `popupId`, `request` operation configuration

Submit an open popup for the active window UI build. Success contains PopupFrame, which may be hidden or measureOnly; a zero ID yields an empty frame. Inactive frame or configured duplicate/placement failures return errors.

Request each popup once per active build. Emit the floating root when visible or measureOnly, using its floating configuration. Placement/dismissal participate in commit; canceling the build rolls them back. Default unknown-size policy uses a hidden measurement frame.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
FlowUi::PopupRequest request{};
request.anchor = FlowUi::PopupAnchor::parent();
request.expectedSize = Clay_Dimensions{240.0f, 160.0f};
auto result = manager.request(fieldId, request);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto popup = *result;
```

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).

### **request**

#### `[[nodiscard]] Result<PopupFrame> request(GlobalFlowID popupId, const PopupRequest& request) noexcept`

- **Returns:** `Result<PopupFrame>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `popupId`, `request` operation configuration

Submit an open popup for the active window UI build. Success contains PopupFrame, which may be hidden or measureOnly; a zero ID yields an empty frame. Inactive frame or configured duplicate/placement failures return errors.

Request each popup once per active build. Emit the floating root when visible or measureOnly, using its floating configuration. Placement/dismissal participate in commit; canceling the build rolls them back. Default unknown-size policy uses a hidden measurement frame.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
FlowUi::PopupRequest request{};
request.anchor = FlowUi::PopupAnchor::parent();
request.expectedSize = Clay_Dimensions{240.0f, 160.0f};
auto result = manager.request(fieldId, request);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto popup = *result;
```

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).

### **request**

#### `[[nodiscard]] Result<PopupFrame> request(FlowElementPartID popupId, const PopupRequest& request) noexcept`

- **Returns:** `Result<PopupFrame>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `popupId`, `request` operation configuration

Submit an open popup for the active window UI build. Success contains PopupFrame, which may be hidden or measureOnly; a zero ID yields an empty frame. Inactive frame or configured duplicate/placement failures return errors.

Request each popup once per active build. Emit the floating root when visible or measureOnly, using its floating configuration. Placement/dismissal participate in commit; canceling the build rolls them back. Default unknown-size policy uses a hidden measurement frame.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
FlowUi::PopupRequest request{};
request.anchor = FlowUi::PopupAnchor::parent();
request.expectedSize = Clay_Dimensions{240.0f, 160.0f};
auto result = manager.request(fieldId, request);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto popup = *result;
```

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).

### **dismiss**

#### `void dismiss(FlowElementID popupId)`

- **Returns:** `void` (no payload).
- **Arguments:** `popupId`

Dismiss a popup and record a consumable notification. Success means the dismissal request completed; zero IDs are ignored.

During a build the change commits with the frame and is rolled back on cancel. Outside a build it commits immediately. An already submitted popup becomes hidden.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
manager.dismiss(fieldId);
```

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).

### **dismiss**

#### `void dismiss(GlobalFlowID popupId)`

- **Returns:** `void` (no payload).
- **Arguments:** `popupId`

Dismiss a popup and record a consumable notification. Success means the dismissal request completed; zero IDs are ignored.

During a build the change commits with the frame and is rolled back on cancel. Outside a build it commits immediately. An already submitted popup becomes hidden.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
manager.dismiss(fieldId);
```

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).

### **dismiss**

#### `void dismiss(FlowElementPartID popupId)`

- **Returns:** `void` (no payload).
- **Arguments:** `popupId`

Dismiss a popup and record a consumable notification. Success means the dismissal request completed; zero IDs are ignored.

During a build the change commits with the frame and is rolled back on cancel. Outside a build it commits immediately. An already submitted popup becomes hidden.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
manager.dismiss(fieldId);
```

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).

### **consumeDismissed**

#### `[[nodiscard]] bool consumeDismissed(FlowElementID popupId)`

- **Returns:** `bool`.
- **Arguments:** `popupId`

Read and consume one pending popup dismissal. Success contains true once per notification, or false for zero, unknown, or already-consumed IDs.

Consumption during a canceled build is rolled back.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto dismissed = manager.consumeDismissed(fieldId);
```

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).

### **consumeDismissed**

#### `[[nodiscard]] bool consumeDismissed(GlobalFlowID popupId)`

- **Returns:** `bool`.
- **Arguments:** `popupId`

Read and consume one pending popup dismissal. Success contains true once per notification, or false for zero, unknown, or already-consumed IDs.

Consumption during a canceled build is rolled back.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto dismissed = manager.consumeDismissed(fieldId);
```

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).

### **consumeDismissed**

#### `[[nodiscard]] bool consumeDismissed(FlowElementPartID popupId)`

- **Returns:** `bool`.
- **Arguments:** `popupId`

Read and consume one pending popup dismissal. Success contains true once per notification, or false for zero, unknown, or already-consumed IDs.

Consumption during a canceled build is rolled back.

Operational failures propagate as exceptions and latch the frame failure where manager mutation or frame storage is involved. Leave the failed authoring operation; `App::runFrame()` contains it and cancels the frame. Catching an exception does not clear a latched frame error.

**Example:**

```cpp
const auto dismissed = manager.consumeDismissed(fieldId);
```

## Build-gated development hooks

`FLOW_UI_DEV_MODE` and the development memory-level macros expose additional inspection/recording hooks in the header. They are development infrastructure, not the application API described here. Their existing build gates and signatures are unchanged.

See: [Owning header](include/FlowUi/Public/managers/PopupManager.hpp).
