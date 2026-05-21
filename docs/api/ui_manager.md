# UI Manager API

## Aliases

### **FontId**
---

#### `using FontId = uint16_t`

Concrete font id consumed by Clay text configuration. UiManager can resolve this through the connected `FlowUi::FontManager`.

### **FontFamilyId**
---

#### `using FontFamilyId = uint32_t`

Logical font family id returned by `FlowUi::FontManager`. UiManager accepts this for font resolution.

## Enums

### **CursorType**
---

#### `enum class CursorType : uint8_t`

Cursor shape requested by UI code. UiManager collects per-frame cursor requests and forwards the winning shape to the window backend.

## Public Structs

### **FrameInput**
---

#### `struct FrameInput`

Current or previous normalized input snapshot exposed by UiManager. Custom elements use it for low-level pointer, keyboard, scroll, and timing behavior.

### **InteractionSnapshot**
---

#### `struct InteractionSnapshot`

Previous-frame interaction data returned by UiManager. Custom elements use it to query hover, press, hold, and release state.

### **TextureRef**
---

#### `struct TextureRef`

Texture handle copied into frame storage by storeTexture. It is used as Clay image data for images, icons, and viewports.

## Public API

### **toClayString**
---

#### `Clay_String toClayString(std::string_view s)`

- **Returns:** `Clay_String`
- **Arguments:** `s` string data to copy into the current frame arena.

Copies dynamic text into frame-owned storage and returns a Clay string pointing at that copy. Use this for any string emitted to Clay when the original data may not live until frame end.

**Example:**

```cpp
CLAY_TEXT(context.uiManager.toClayString(context.params.label), CLAY_TEXT_CONFIG(textConfig));
```

### **storeTexture**
---

#### `TextureRef* storeTexture(const TextureRef& textureRef)`

- **Returns:** `TextureRef*`
- **Arguments:** `textureRef` texture reference to copy into frame storage.

Stores a texture reference in the current frame arena and returns a pointer suitable for `Clay_ImageElementConfig::imageData`. Use this instead of taking the address of a temporary or local `TextureRef`.

**Example:**

```cpp
imageConfig.imageData = context.uiManager.storeTexture(app.images().getTexture("logo"));
```

### **toClaySID**
---

#### `Clay_ElementId toClaySID(std::string_view s)`

- **Returns:** `Clay_ElementId`
- **Arguments:** `s` id string.

Converts a string into a Clay string id using FlowUi frame storage. This is useful when you want Clay's string-id path directly.

**Example:**

```cpp
Clay_ElementId overlayId = ui.toClaySID("overlay/root");
```

### **toClayEID**
---

#### `Clay_ElementId toClayEID(std::string_view s)`

- **Returns:** `Clay_ElementId`
- **Arguments:** `s` element id string.

Converts a FlowUi element id string into a Clay element id. This is the normal helper for root and child Clay nodes inside FlowUi element callbacks.

**Example:**

```cpp
Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
```

### **createElement**
---

#### `template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal> ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal> createElement(const ElementDefinition<Parameters, State, Resources, DefinitionId, IsDevInternal>& elementDefinition, std::string_view elementID)`

- **Returns:** `ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal>`
- **Arguments:** `elementDefinition` typed definition to invoke, `elementID` stable instance id string.

Creates a builder for one typed FlowUi element invocation. Chain parameter setup and finish with `draw()` or `construct()`.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").draw();
```

### **drawConstructed**
---

#### `void drawConstructed()`

- **Returns:** `void`
- **Arguments:** none.

Closes the current element opened by `ElementBuilder::construct()`. Call this after emitting the manually supplied child Clay nodes for a constructed element flow.

**Example:**

```cpp
ui.drawConstructed();
```

### **getPreviousFramesInteraction**
---

#### `const InteractionSnapshot& getPreviousFramesInteraction() const`

- **Returns:** `const InteractionSnapshot&`
- **Arguments:** none.

Returns the previous completed frame's interaction snapshot. Use it for stable hover, press, hold, and release queries while building the current frame.

**Example:**

```cpp
const bool wasPressed = ui.getPreviousFramesInteraction().isPressed(buttonId);
```

### **getCurrentFrameInput**
---

#### `const FrameInput& getCurrentFrameInput() const`

- **Returns:** `const FrameInput&`
- **Arguments:** none.

Returns the current frame input in FlowUi layout space. Custom elements can use this for low-level pointer, scroll, keyboard, or timing behavior.

**Example:**

```cpp
const FlowUi::FrameInput& input = ui.getCurrentFrameInput();
```

### **getPreviousFrameInput**
---

#### `const FrameInput& getPreviousFrameInput() const`

- **Returns:** `const FrameInput&`
- **Arguments:** none.

Returns the previous frame input in FlowUi layout space. Compare it with `getCurrentFrameInput()` for custom edge detection or drag calculations.

**Example:**

```cpp
const bool pressedThisFrame = ui.getCurrentFrameInput().mouseDown[0] && !ui.getPreviousFrameInput().mouseDown[0];
```

### **inputFields** `1/2`
---

#### `InputFieldManager& inputFields()`

- **Returns:** `InputFieldManager&`
- **Arguments:** none.

Returns the mutable input field manager owned by the UI manager. Custom editable text elements use it to request fields, focus, carets, and text edits.

**Example:**

```cpp
context.uiManager.inputFields().requestCaret(context.elementID, FlowUi::CaretRequestKind::SetPrimary);
```

### **inputFields** `2/2`
---

#### `const InputFieldManager& inputFields() const`

- **Returns:** `const InputFieldManager&`
- **Arguments:** none.

Returns the immutable input field manager. Use it for read-only input focus and selection checks.

**Example:**

```cpp
context.uiManager.inputFields().requestCaret(context.elementID, FlowUi::CaretRequestKind::SetPrimary);
```

### **shortcuts** `1/2`
---

#### `ShortcutManager& shortcuts()`

- **Returns:** `ShortcutManager&`
- **Arguments:** none.

Returns the mutable shortcut manager owned by the UI manager. Use it to register app or element keyboard shortcuts.

**Example:**

```cpp
FlowUi::ShortcutManager& shortcuts = app.ui().shortcuts();
```

### **shortcuts** `2/2`
---

#### `const ShortcutManager& shortcuts() const`

- **Returns:** `const ShortcutManager&`
- **Arguments:** none.

Returns the immutable shortcut manager. Use it for read-only focused element inspection.

**Example:**

```cpp
FlowUi::ShortcutManager& shortcuts = app.ui().shortcuts();
```

### **devRuntime** `1/2`
---

#### `devMode::DevRuntime& devRuntime()`

- **Returns:** `devMode::DevRuntime&`
- **Arguments:** none.

Returns the mutable developer runtime when `FLOW_UI_DEV_MODE` is enabled. This is mainly for developer tooling and custom dev integrations.

**Example:**

```cpp
auto& devRuntime = app.ui().devRuntime();
```

### **devRuntime** `2/2`
---

#### `const devMode::DevRuntime& devRuntime() const`

- **Returns:** `const devMode::DevRuntime&`
- **Arguments:** none.

Returns the immutable developer runtime when `FLOW_UI_DEV_MODE` is enabled. Use it for read-only inspection of dev-mode state.

**Example:**

```cpp
auto& devRuntime = app.ui().devRuntime();
```

### **devToolsConfig** `1/2`
---

#### `DevToolsConfig& devToolsConfig()`

- **Returns:** `DevToolsConfig&`
- **Arguments:** none.

Returns mutable developer tooling configuration when `FLOW_UI_DEV_MODE` is enabled. This allows runtime updates to developer panel and capture behavior.

**Example:**

```cpp
app.ui().devToolsConfig().panelOpenByDefault = true;
```

### **devToolsConfig** `2/2`
---

#### `const DevToolsConfig& devToolsConfig() const`

- **Returns:** `const DevToolsConfig&`
- **Arguments:** none.

Returns immutable developer tooling configuration when `FLOW_UI_DEV_MODE` is enabled. Use it for read-only access to current dev settings.

**Example:**

```cpp
app.ui().devToolsConfig().panelOpenByDefault = true;
```

### **setClipboardText**
---

#### `void setClipboardText(std::string_view text) const`

- **Returns:** `void`
- **Arguments:** `text` text to copy.

Writes clipboard text through the clipboard accessor installed by `App`. If no accessor is installed, this function does nothing.

**Example:**

```cpp
context.uiManager.setClipboardText(selectedText);
```

### **clipboardText**
---

#### `std::string clipboardText() const`

- **Returns:** `std::string`
- **Arguments:** none.

Reads clipboard text through the installed clipboard accessor. Returns an empty string when no getter is installed.

**Example:**

```cpp
std::string pasted = context.uiManager.clipboardText();
```

### **hasClipboardAccess**
---

#### `bool hasClipboardAccess() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether both clipboard read and write accessors are installed. Use this before exposing clipboard-dependent UI behavior.

**Example:**

```cpp
if (context.uiManager.hasClipboardAccess()) { context.uiManager.setClipboardText(selectedText); }
```

### **requestCursor**
---

#### `void requestCursor(CursorType cursorType, uint8_t priority = 0)`

- **Returns:** `void`
- **Arguments:** `cursorType` requested cursor shape, `priority` ordering priority for competing requests.

Requests a cursor shape for the current frame. Cursor requests reset each frame, and higher-priority requests win when multiple UI elements request different cursors.

**Example:**

```cpp
context.uiManager.requestCursor(FlowUi::CursorType::PointingHand, 10);
```

### **resolveFont** `1/2`
---

#### `FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyId` logical family id, `weight` requested font weight, `style` requested font style.

Resolves a logical family/style request to a concrete Clay font id through the connected font manager. Returns `0` when no font manager is attached or the family cannot be resolved.

**Example:**

```cpp
textConfig.fontId = context.uiManager.resolveFont("Body", 700, FlowUi::FontStyle::Normal);
```

### **resolveFont** `2/2`
---

#### `FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyName` logical family name, `weight` requested font weight, `style` requested font style.

Named-family overload for font resolution. Use it when you want a concise lookup by family name rather than caching a `FontFamilyId`.

**Example:**

```cpp
textConfig.fontId = context.uiManager.resolveFont("Body", 700, FlowUi::FontStyle::Normal);
```
