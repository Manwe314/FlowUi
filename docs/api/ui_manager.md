# UI Manager API

## Aliases

### **FontId**


#### `using FontId = uint16_t`

Concrete font id consumed by Clay text configuration. UiManager can resolve this through the connected `FlowUi::FontManager`.

### **FontFamilyId**


#### `using FontFamilyId = uint32_t`

Logical font family id returned by `FlowUi::FontManager`. UiManager accepts this for font resolution.

## Enums

### **CursorType**


#### `enum class CursorType : uint8_t`

Cursor shape requested by UI code. UiManager collects per-frame cursor requests and forwards the winning shape to the window backend.

## Public Structs

### **FrameInput**


#### `struct FrameInput`

Current or previous normalized input snapshot exposed by UiManager. Custom elements use it for low-level pointer, keyboard, scroll, and timing behavior.

### **InteractionSnapshot**


#### `struct InteractionSnapshot`

Previous-frame interaction data returned by UiManager. Custom elements use it to query hover, press, hold, and release state.

### **TextureRef**


#### `struct TextureRef`

Texture payload copied into frame storage by `imageData()`. Storage retains the logical GPU texture separately through submission.

## Public API

### **toClayString**


#### `Clay_String toClayString(std::string_view s)`

- **Returns:** `Clay_String`
- **Arguments:** `s` string data to copy into the current frame arena.

Copies dynamic text into frame-owned storage and returns a Clay string pointing at that copy. Use this for any string emitted to Clay when the original data may not live until frame end.

**Example:**

```cpp
CLAY_TEXT(context.uiManager.toClayString(context.params.label), CLAY_TEXT_CONFIG(textConfig));
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **imageData**


#### `TextureRef* imageData(TextureRef textureRef)`

- **Returns:** `TextureRef*`
- **Arguments:** `textureRef` texture reference to copy into frame storage.

Stores a texture reference in the current frame arena and returns a pointer suitable for `Clay_ImageElementConfig::imageData`. Use this instead of taking the address of a temporary or local `TextureRef`.

**Example:**

```cpp
imageConfig.imageData = context.uiManager.imageData(app.images().getTexture("logo"));
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **inputContentElement**


#### `Clay_ElementDeclaration inputContentElement(const Clay_TextElementConfig& textConfig) const`

- **Returns:** `Clay_ElementDeclaration`
- **Arguments:** `textConfig` text configuration used by the input field text.

Creates a stable inner content element declaration for input fields. The returned declaration grows horizontally and uses a fixed height based on the resolved font line height.

**Example:**

```cpp
CLAY(contentId, context.uiManager.inputContentElement(textConfig)) {
    CLAY_TEXT(context.uiManager.toClayString(field.text.copy()), CLAY_TEXT_CONFIG(textConfig));
}
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **toClaySID**


#### `Clay_ElementId toClaySID(std::string_view s)`

- **Returns:** `Clay_ElementId`
- **Arguments:** `s` id string.

Converts a string into a Clay string id using FlowUi frame storage. This is useful when you want Clay's string-id path directly.

**Example:**

```cpp
Clay_ElementId overlayId = ui.toClaySID("overlay/root");
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **toClayEID**


#### `Clay_ElementId toClayEID(FlowElementID id)`

- **Returns:** `Clay_ElementId`
- **Arguments:** `id` strong element identity.

Bridges a strong FlowUi element identity into a Clay element id. Overloads also accept `GlobalFlowID` and `FlowElementPartID`. This is the normal helper for root and child Clay nodes inside FlowUi element callbacks.

**Example:**

```cpp
Clay_ElementId rootId = context.clayID();
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **createElement**


#### `template <FlowElement Element> ElementBuilder<Element> createElement(const Element&, LocalElementName localName)`

- **Returns:** `ElementBuilder<Element>`.
- **Arguments:** an empty element tag and a literal local name resolved under the active parent scope.

Overloads accept runtime, indexed/keyed, automatic, resolved local, global, and semantic-part identity tokens. Omitting a name uses `AutoID()`; loops need distinct names or keys. See [element identity](elements.md#identity).

Creates a builder for one typed FlowUi element invocation. Chain parameter setup and finish with `draw()` or `construct()`.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").draw();
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **theme** `1/2`


#### `template <typename T> const T& theme() const`

- **Returns:** `const T&`
- **Arguments:** none.

Queries the active variant for theme struct type `T`. Use this inside custom element `buildElement` or `constructElement` callbacks to retrieve design tokens.

**Example:**

```cpp
const auto& appTheme = context.uiManager.theme<AppTheme>();
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **theme** `2/2`


#### `template <typename T> const T& theme(std::string_view variantName) const`

- **Returns:** `const T&`
- **Arguments:** `variantName` registered variant name.

Queries a specific named variant for theme struct type `T`.

**Example:**

```cpp
const auto& lightTheme = context.uiManager.theme<AppTheme>("light");
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **flowTheme**


#### `const FlowUiTheme& flowTheme() const`

- **Returns:** `const FlowUiTheme&`
- **Arguments:** none.

Convenience shortcut returning the active built-in `FlowUiTheme` design system tokens (colors, font sizes, spacing, corner radii).

**Example:**

```cpp
const auto& flowTheme = context.uiManager.flowTheme();
root.backgroundColor = flowTheme.surface;
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **drawConstructed**


#### `void drawConstructed()`

- **Returns:** `void`
- **Arguments:** none.

Closes the current element opened by `ElementBuilder::construct()`. Call this after emitting the manually supplied child Clay nodes for a constructed element flow.

**Example:**

```cpp
ui.drawConstructed();
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **getPreviousFramesInteraction**


#### `const InteractionSnapshot& getPreviousFramesInteraction() const`

- **Returns:** `const InteractionSnapshot&`
- **Arguments:** none.

Returns the previous completed frame's interaction snapshot. Use it for stable hover, press, hold, and release queries while building the current frame.

**Example:**

```cpp
const bool wasPressed = ui.getPreviousFramesInteraction().isPressed(buttonId);
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **getCurrentFrameInput**


#### `const FrameInput& getCurrentFrameInput() const`

- **Returns:** `const FrameInput&`
- **Arguments:** none.

Returns the current frame input in FlowUi layout space. Custom elements can use this for low-level pointer, scroll, keyboard, or timing behavior.

**Example:**

```cpp
const FlowUi::FrameInput& input = ui.getCurrentFrameInput();
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **getPreviousFrameInput**


#### `const FrameInput& getPreviousFrameInput() const`

- **Returns:** `const FrameInput&`
- **Arguments:** none.

Returns the previous frame input in FlowUi layout space. Compare it with `getCurrentFrameInput()` for custom edge detection or drag calculations.

**Example:**

```cpp
const bool pressedThisFrame = ui.getCurrentFrameInput().mouseDown[0] && !ui.getPreviousFrameInput().mouseDown[0];
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **inputFields** `1/2`


#### `InputFieldManager& inputFields()`

- **Returns:** `InputFieldManager&`
- **Arguments:** none.

Returns the mutable input field manager owned by the UI manager. Custom editable text elements use it to request fields, focus, carets, and text edits.

**Example:**

```cpp
context.uiManager.inputFields().requestCaret(context.id, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **inputFields** `2/2`


#### `const InputFieldManager& inputFields() const`

- **Returns:** `const InputFieldManager&`
- **Arguments:** none.

Returns the immutable input field manager. Use it for read-only input focus and selection checks.

**Example:**

```cpp
context.uiManager.inputFields().requestCaret(context.id, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **shortcuts** `1/2`


#### `ShortcutManager& shortcuts()`

- **Returns:** `ShortcutManager&`
- **Arguments:** none.

Returns the mutable shortcut manager owned by the UI manager. Use it to register app or element keyboard shortcuts.

**Example:**

```cpp
FlowUi::ShortcutManager& shortcuts = app.ui().shortcuts();
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **shortcuts** `2/2`


#### `const ShortcutManager& shortcuts() const`

- **Returns:** `const ShortcutManager&`
- **Arguments:** none.

Returns the immutable shortcut manager. Use it for read-only focused element inspection.

**Example:**

```cpp
FlowUi::ShortcutManager& shortcuts = app.ui().shortcuts();
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **popups** `1/2`


#### `PopupManager& popups()`

- **Returns:** `PopupManager&`
- **Arguments:** none.

Returns the mutable window-scoped popup manager. Custom popup elements use it for placement, measurement, overflow correction, z-order, and dismissal behavior.

**Example:**

```cpp
FlowUi::PopupFrame frame = context.uiManager.popups().request(
    context.id,
    context.params.popupRequest);
```

See: [Popup Manager API](popup_manager.md).

### **popups** `2/2`


#### `const PopupManager& popups() const`

- **Returns:** `const PopupManager&`
- **Arguments:** none.

Returns the immutable window-scoped popup manager.

See: [Popup Manager API](popup_manager.md).

### Modern developer services

Available only when `FLOW_UI_DEV_MODE` is compiled in:

| Query | Result and lifetime |
| --- | --- |
| `devSchemaRegistry()` | Pointer to the app-owned `devSystems::tooling::DevSchemaRegistry`, or null when unattached; const and mutable overloads are `noexcept`. |
| `devOverrideEngine()` | Pointer to the app-owned `devSystems::tooling::DevOverrideEngine`, or null when unattached; const and mutable overloads are `noexcept`. |
| `devTreeSnapshot() const noexcept` | Const reference to this window's completed `DevTreeSnapshot`. |
| `devTreeCapture() noexcept` | Window-local modern `DevTreeCapture`; const and mutable overloads. |

The application normally owns publication and capture sequencing. Use `app.devTooling()` for schemas, catalogues, overrides, and baking. See [Developer Systems](../tutorials/developer_mode.md) and [UiManager.hpp](../../include/FlowUi/Public/managers/UiManager.hpp).

### **devToolsConfig** `1/2`


#### `DevToolsConfig& devToolsConfig()`

- **Returns:** `DevToolsConfig&`
- **Arguments:** none.

Returns mutable developer tooling configuration when `FLOW_UI_DEV_MODE` is enabled. This allows runtime updates to capture behavior.

**Example:**

```cpp
app.ui().devToolsConfig().excludeInternalDevElementsFromCapture = true;
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **devToolsConfig** `2/2`


#### `const DevToolsConfig& devToolsConfig() const`

- **Returns:** `const DevToolsConfig&`
- **Arguments:** none.

Returns immutable developer tooling configuration when `FLOW_UI_DEV_MODE` is enabled. Use it for read-only access to current dev settings.

**Example:**

```cpp
const bool excludesInternal = app.ui().devToolsConfig().excludeInternalDevElementsFromCapture;
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **setClipboardText**


#### `void setClipboardText(std::string_view text) const`

- **Returns:** `void`
- **Arguments:** `text` text to copy.

Writes clipboard text through the clipboard accessor installed by `App`. If no accessor is installed, this function does nothing.

**Example:**

```cpp
context.uiManager.setClipboardText(selectedText);
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **clipboardText**


#### `std::string clipboardText() const`

- **Returns:** `std::string`
- **Arguments:** none.

Reads clipboard text through the installed clipboard accessor. Returns an empty string when no getter is installed.

**Example:**

```cpp
std::string pasted = context.uiManager.clipboardText();
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **hasClipboardAccess**


#### `bool hasClipboardAccess() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether both clipboard read and write accessors are installed. Use this before exposing clipboard-dependent UI behavior.

**Example:**

```cpp
if (context.uiManager.hasClipboardAccess()) { context.uiManager.setClipboardText(selectedText); }
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **requestCursor**


#### `void requestCursor(CursorType cursorType, uint8_t priority = 0)`

- **Returns:** `void`
- **Arguments:** `cursorType` requested cursor shape, `priority` ordering priority for competing requests.

Requests a cursor shape for the current frame. Cursor requests reset each frame, and higher-priority requests win when multiple UI elements request different cursors.

**Example:**

```cpp
context.uiManager.requestCursor(FlowUi::CursorType::PointingHand, 10);
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **resolveFont** `1/2`


#### `FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyId` logical family id, `weight` requested font weight, `style` requested font style.

Resolves a logical family/style request to a concrete Clay font id through the connected font manager. Returns `0` when no font manager is attached or the family cannot be resolved.

**Example:**

```cpp
textConfig.fontId = context.uiManager.resolveFont("Body", 700, FlowUi::FontStyle::Normal);
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).

### **resolveFont** `2/2`


#### `FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyName` logical family name, `weight` requested font weight, `style` requested font style.

Named-family overload for font resolution. Use it when you want a concise lookup by family name rather than caching a `FontFamilyId`.

**Example:**

```cpp
textConfig.fontId = context.uiManager.resolveFont("Body", 700, FlowUi::FontStyle::Normal);
```

See: [Owning header](../../include/FlowUi/Public/managers/UiManager.hpp).
