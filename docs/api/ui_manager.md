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

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#ac1694a665b71269470ff22a2adc00437).

### **imageData**


#### `TextureRef* imageData(TextureRef textureRef)`

- **Returns:** `TextureRef*`
- **Arguments:** `textureRef` texture reference to copy into frame storage.

Stores a texture reference in the current frame arena and returns a pointer suitable for `Clay_ImageElementConfig::imageData`. Use this instead of taking the address of a temporary or local `TextureRef`.

**Example:**

```cpp
imageConfig.imageData = context.uiManager.imageData(app.images().getTexture("logo"));
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#ad796d47d74be0f2b856012a8d48305a9).

### **inputContentElement**


#### `Clay_ElementDeclaration inputContentElement(const Clay_TextElementConfig& textConfig) const`

- **Returns:** `Clay_ElementDeclaration`
- **Arguments:** `textConfig` text configuration used by the input field text.

Creates a stable inner content element declaration for input fields. The returned declaration grows horizontally and uses a fixed height based on the resolved font line height.

**Example:**

```cpp
CLAY(contentId, context.uiManager.inputContentElement(textConfig)) {
    CLAY_TEXT(context.uiManager.toClayString(field.text), CLAY_TEXT_CONFIG(textConfig));
}
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html).

### **toClaySID**


#### `Clay_ElementId toClaySID(std::string_view s)`

- **Returns:** `Clay_ElementId`
- **Arguments:** `s` id string.

Converts a string into a Clay string id using FlowUi frame storage. This is useful when you want Clay's string-id path directly.

**Example:**

```cpp
Clay_ElementId overlayId = ui.toClaySID("overlay/root");
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#ab5a7131ff83f731ff6f99006f4563d29).

### **toClayEID**


#### `Clay_ElementId toClayEID(std::string_view s)`

- **Returns:** `Clay_ElementId`
- **Arguments:** `s` element id string.

Converts a FlowUi element id string into a Clay element id. This is the normal helper for root and child Clay nodes inside FlowUi element callbacks.

**Example:**

```cpp
Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a67ed60b989b4c54ca6aae7aad7bed4d0).

### **createElement**


#### `template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal> ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal> createElement(const ElementDefinition<Parameters, State, Resources, DefinitionId, IsDevInternal>& elementDefinition, std::string_view elementID)`

- **Returns:** `ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal>`
- **Arguments:** `elementDefinition` typed definition to invoke, `elementID` stable instance id string.

Creates a builder for one typed FlowUi element invocation. Chain parameter setup and finish with `draw()` or `construct()`.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").draw();
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a2a9c6c1f0719e6dd203ad4de09007ee4).

### **theme** `1/2`


#### `template <typename T> const T& theme() const`

- **Returns:** `const T&`
- **Arguments:** none.

Queries the active variant for theme struct type `T`. Use this inside custom element `buildElement` or `constructElement` callbacks to retrieve design tokens.

**Example:**

```cpp
const auto& appTheme = context.uiManager.theme<AppTheme>();
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html).

### **theme** `2/2`


#### `template <typename T> const T& theme(std::string_view variantName) const`

- **Returns:** `const T&`
- **Arguments:** `variantName` registered variant name.

Queries a specific named variant for theme struct type `T`.

**Example:**

```cpp
const auto& lightTheme = context.uiManager.theme<AppTheme>("light");
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html).

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

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html).

### **drawConstructed**


#### `void drawConstructed()`

- **Returns:** `void`
- **Arguments:** none.

Closes the current element opened by `ElementBuilder::construct()`. Call this after emitting the manually supplied child Clay nodes for a constructed element flow.

**Example:**

```cpp
ui.drawConstructed();
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a857a971efa5a8e904806b9398e0e4883).

### **getPreviousFramesInteraction**


#### `const InteractionSnapshot& getPreviousFramesInteraction() const`

- **Returns:** `const InteractionSnapshot&`
- **Arguments:** none.

Returns the previous completed frame's interaction snapshot. Use it for stable hover, press, hold, and release queries while building the current frame.

**Example:**

```cpp
const bool wasPressed = ui.getPreviousFramesInteraction().isPressed(buttonId);
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a428e81a65fec2e616b90be54b58744c4).

### **getCurrentFrameInput**


#### `const FrameInput& getCurrentFrameInput() const`

- **Returns:** `const FrameInput&`
- **Arguments:** none.

Returns the current frame input in FlowUi layout space. Custom elements can use this for low-level pointer, scroll, keyboard, or timing behavior.

**Example:**

```cpp
const FlowUi::FrameInput& input = ui.getCurrentFrameInput();
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a0caf0ecce99a1beb956ce76179b2f06a).

### **getPreviousFrameInput**


#### `const FrameInput& getPreviousFrameInput() const`

- **Returns:** `const FrameInput&`
- **Arguments:** none.

Returns the previous frame input in FlowUi layout space. Compare it with `getCurrentFrameInput()` for custom edge detection or drag calculations.

**Example:**

```cpp
const bool pressedThisFrame = ui.getCurrentFrameInput().mouseDown[0] && !ui.getPreviousFrameInput().mouseDown[0];
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a12cb4ceeafcca80fea278337586ae2ac).

### **inputFields** `1/2`


#### `InputFieldManager& inputFields()`

- **Returns:** `InputFieldManager&`
- **Arguments:** none.

Returns the mutable input field manager owned by the UI manager. Custom editable text elements use it to request fields, focus, carets, and text edits.

**Example:**

```cpp
context.uiManager.inputFields().requestCaret(context.id, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a1a718742e52372ebd62f57980ef501ba).

### **inputFields** `2/2`


#### `const InputFieldManager& inputFields() const`

- **Returns:** `const InputFieldManager&`
- **Arguments:** none.

Returns the immutable input field manager. Use it for read-only input focus and selection checks.

**Example:**

```cpp
context.uiManager.inputFields().requestCaret(context.id, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#ad9373da210911028ada7f4559cc760b3).

### **shortcuts** `1/2`


#### `ShortcutManager& shortcuts()`

- **Returns:** `ShortcutManager&`
- **Arguments:** none.

Returns the mutable shortcut manager owned by the UI manager. Use it to register app or element keyboard shortcuts.

**Example:**

```cpp
FlowUi::ShortcutManager& shortcuts = app.ui().shortcuts();
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a61893809f24d2017db5642a3a780065f).

### **shortcuts** `2/2`


#### `const ShortcutManager& shortcuts() const`

- **Returns:** `const ShortcutManager&`
- **Arguments:** none.

Returns the immutable shortcut manager. Use it for read-only focused element inspection.

**Example:**

```cpp
FlowUi::ShortcutManager& shortcuts = app.ui().shortcuts();
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a0e207879dfd35c910841bc23509c0abc).

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

### **devRuntime** `1/2`


#### `devMode::DevRuntime& devRuntime()`

- **Returns:** `devMode::DevRuntime&`
- **Arguments:** none.

Returns the mutable developer runtime when `FLOW_UI_DEV_MODE` is enabled. This is mainly for developer tooling and custom dev integrations.

**Example:**

```cpp
auto& devRuntime = app.ui().devRuntime();
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a27ecba8061cf9a454320450d533c2b0c).

### **devRuntime** `2/2`


#### `const devMode::DevRuntime& devRuntime() const`

- **Returns:** `const devMode::DevRuntime&`
- **Arguments:** none.

Returns the immutable developer runtime when `FLOW_UI_DEV_MODE` is enabled. Use it for read-only inspection of dev-mode state.

**Example:**

```cpp
auto& devRuntime = app.ui().devRuntime();
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#af44b4564bc6f0af50ab7ea3c678a7deb).

### **devToolsConfig** `1/2`


#### `DevToolsConfig& devToolsConfig()`

- **Returns:** `DevToolsConfig&`
- **Arguments:** none.

Returns mutable developer tooling configuration when `FLOW_UI_DEV_MODE` is enabled. This allows runtime updates to developer panel and capture behavior.

**Example:**

```cpp
app.ui().devToolsConfig().panelOpenByDefault = true;
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a15f634a57bdcc1cc0838007554144c5d).

### **devToolsConfig** `2/2`


#### `const DevToolsConfig& devToolsConfig() const`

- **Returns:** `const DevToolsConfig&`
- **Arguments:** none.

Returns immutable developer tooling configuration when `FLOW_UI_DEV_MODE` is enabled. Use it for read-only access to current dev settings.

**Example:**

```cpp
app.ui().devToolsConfig().panelOpenByDefault = true;
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a3d1d0dcbcbdc6c177a80ed4bdd3ef1b9).

### **setClipboardText**


#### `void setClipboardText(std::string_view text) const`

- **Returns:** `void`
- **Arguments:** `text` text to copy.

Writes clipboard text through the clipboard accessor installed by `App`. If no accessor is installed, this function does nothing.

**Example:**

```cpp
context.uiManager.setClipboardText(selectedText);
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#af951ad4db8a17064009ed8ff9ac9f79a).

### **clipboardText**


#### `std::string clipboardText() const`

- **Returns:** `std::string`
- **Arguments:** none.

Reads clipboard text through the installed clipboard accessor. Returns an empty string when no getter is installed.

**Example:**

```cpp
std::string pasted = context.uiManager.clipboardText();
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a4b3ec28e336b11fc18d35639a360544e).

### **hasClipboardAccess**


#### `bool hasClipboardAccess() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether both clipboard read and write accessors are installed. Use this before exposing clipboard-dependent UI behavior.

**Example:**

```cpp
if (context.uiManager.hasClipboardAccess()) { context.uiManager.setClipboardText(selectedText); }
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a64bfd45ccd65d7eb70792e13a4559a8f).

### **requestCursor**


#### `void requestCursor(CursorType cursorType, uint8_t priority = 0)`

- **Returns:** `void`
- **Arguments:** `cursorType` requested cursor shape, `priority` ordering priority for competing requests.

Requests a cursor shape for the current frame. Cursor requests reset each frame, and higher-priority requests win when multiple UI elements request different cursors.

**Example:**

```cpp
context.uiManager.requestCursor(FlowUi::CursorType::PointingHand, 10);
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a73558c0199600f70651c54dba7211498).

### **resolveFont** `1/2`


#### `FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyId` logical family id, `weight` requested font weight, `style` requested font style.

Resolves a logical family/style request to a concrete Clay font id through the connected font manager. Returns `0` when no font manager is attached or the family cannot be resolved.

**Example:**

```cpp
textConfig.fontId = context.uiManager.resolveFont("Body", 700, FlowUi::FontStyle::Normal);
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#adbbbc7d049cee3b9e97242fc44585c2d).

### **resolveFont** `2/2`


#### `FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyName` logical family name, `weight` requested font weight, `style` requested font style.

Named-family overload for font resolution. Use it when you want a concise lookup by family name rather than caching a `FontFamilyId`.

**Example:**

```cpp
textConfig.fontId = context.uiManager.resolveFont("Body", 700, FlowUi::FontStyle::Normal);
```

See: [Full Doxygen reference](classFlowUi_1_1UiManager.html#a43b32fe49c09bc873a6691764164ba6b).
