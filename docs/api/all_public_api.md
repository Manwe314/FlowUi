# All Public API

This page is a scan-first cheat sheet for public FlowUi functions. It sits between the generated Doxygen reference and the tutorial docs: function names are visible, signatures are explicit, and descriptions stay short.

## FlowUi Namespace Functions

### **toFlowId** `1/2`


#### `constexpr FlowElementId toFlowId(std::string_view elementName) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `elementName` string to hash into an element instance id.

Hashes a runtime string into a stable FlowUi element id. Use this when looking up or managing state for an element instance outside the builder path.

**Example:**

```cpp
const FlowUi::FlowElementId saveButtonId = FlowUi::toFlowId("toolbar/save");
```

See: [Full Doxygen reference](group__flowui__app.html#ga49ddb3f056407bc53580f77d61664c2a).

### **toFlowId** `2/2`


#### `template <std::size_t N> constexpr FlowElementId toFlowId(const char (&elementName)[N]) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `elementName` string literal to hash into an element instance id.

String-literal overload for `toFlowId`. It avoids counting the terminating null byte and can be used in constant expressions.

**Example:**

```cpp
const FlowUi::FlowElementId saveButtonId = FlowUi::toFlowId("toolbar/save");
```

See: [Full Doxygen reference](group__flowui__app.html#ga0bedfbab552fbf49a5a1d31ed8abe9f8).

### **toFlowDefinitionId** `1/2`


#### `constexpr FlowDefinitionId toFlowDefinitionId(std::string_view definitionName) noexcept`

- **Returns:** `FlowDefinitionId`
- **Arguments:** `definitionName` string to hash into an element definition id.

Hashes a runtime string into a stable FlowUi element definition id. This is the function behind definition ids used by `ElementDefinition`.

**Example:**

```cpp
constexpr FlowUi::FlowDefinitionId buttonDefinitionId = FlowUi::toFlowDefinitionId("button");
```

See: [Full Doxygen reference](group__flowui__app.html#gad4ef9ba85d5584740ebdaba96078d2a6).

### **toFlowDefinitionId** `2/2`


#### `template <std::size_t N> constexpr FlowDefinitionId toFlowDefinitionId(const char (&definitionName)[N]) noexcept`

- **Returns:** `FlowDefinitionId`
- **Arguments:** `definitionName` string literal to hash into an element definition id.

String-literal overload for `toFlowDefinitionId`. Prefer this through `FLOW_DEF_ID("name")` when declaring custom element definition types.

**Example:**

```cpp
constexpr FlowUi::FlowDefinitionId buttonDefinitionId = FlowUi::toFlowDefinitionId("button");
```

See: [Full Doxygen reference](group__flowui__app.html#gadd3c481eff5accc8b1891802844a95a9).

### **createIndexedFlowId** `1/3`


#### `constexpr FlowElementId createIndexedFlowId(FlowElementId rootId, uint64_t index) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `rootId` parent/root Flow id, `index` numeric child/index value.

Creates a stable child-style id by mixing an existing Flow id with an index. This is useful for repeated UI rows or generated children where a string id would be awkward.

**Example:**

```cpp
const FlowUi::FlowElementId rowId = FlowUi::createIndexedFlowId("asset-list/row", rowIndex);
```

See: [Full Doxygen reference](group__flowui__app.html#gab3403318b43d5e0e47a7b32d0dddb12b).

### **createIndexedFlowId** `2/3`


#### `constexpr FlowElementId createIndexedFlowId(std::string_view rootName, uint64_t index) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `rootName` parent/root name, `index` numeric child/index value.

Hashes the root name and then mixes in the numeric index. Use this when generating stable ids from a named collection or repeated layout section.

**Example:**

```cpp
const FlowUi::FlowElementId rowId = FlowUi::createIndexedFlowId("asset-list/row", rowIndex);
```

See: [Full Doxygen reference](group__flowui__app.html#ga02a4d740ce7ac4121c6ba24f0b0bcd52).

### **createIndexedFlowId** `3/3`


#### `template <std::size_t N> constexpr FlowElementId createIndexedFlowId(const char (&rootName)[N], uint64_t index) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `rootName` string literal parent/root name, `index` numeric child/index value.

String-literal overload for indexed id creation. It is useful for compile-time root names paired with runtime loop indexes.

**Example:**

```cpp
const FlowUi::FlowElementId rowId = FlowUi::createIndexedFlowId("asset-list/row", rowIndex);
```

See: [Full Doxygen reference](group__flowui__app.html#gac4775bf6801619b1dc54427bc63d43a6).

### **Flow_Color**


#### `Clay_Color Flow_Color(std::string_view hexRgba)`

- **Returns:** `Clay_Color`
- **Arguments:** `hexRgba` color string in `#RRGGBB` or `#RRGGBBAA` form.

Converts a hex RGB/RGBA string into a Clay color. Six-digit RGB receives an implicit `ff` alpha channel. The leading `#` is required; invalid input throws `std::invalid_argument`.

**Example:**

```cpp
Clay_Color panelColor = FlowUi::Flow_Color("#20242cff");
Clay_Color opaquePanelColor = FlowUi::Flow_Color("#20242c");
```

See: [Full Doxygen reference](group__flowui__app.html#ga159d45f9b2b4441d66c814c58f809919).

### **makeApplication**


#### `App makeApplication(const AppConfig& cfg)`

- **Returns:** `App`
- **Arguments:** `cfg` top-level FlowUi app configuration.

Creates and initializes a running FlowUi application. Use this instead of manually constructing `App`; it wires the window, managers, renderer, and configuration together.

**Example:**

```cpp
FlowUi::App app = FlowUi::makeApplication(config);
```

See: [Full Doxygen reference](group__flowui__app.html#ga8afb464a3691ca644406ac41ff5281d2).

### **operator|**


#### `ElementDrawOptions operator|(ElementDrawOptions a, ElementDrawOptions b)`

- **Returns:** `ElementDrawOptions`
- **Arguments:** `a` first option flag, `b` second option flag.

Combines draw-option flags for `ElementBuilder::draw()` and `ElementBuilder::construct()`. This is the normal way to skip multiple callback phases in one builder call.

**Example:**

```cpp
auto options = FlowUi::ElementDrawOptions::SkipEventCallbacks | FlowUi::ElementDrawOptions::SkipLogicCallback;
```

See: [Full Doxygen reference](group__flowui__element__system.html#ga83272f87e6796154839aa8e2df16f531).

### **elementDrawOptionsHas**


#### `bool elementDrawOptionsHas(ElementDrawOptions value, ElementDrawOptions flag)`

- **Returns:** `bool`
- **Arguments:** `value` combined option value, `flag` flag to test.

Checks whether an `ElementDrawOptions` value contains a specific flag. This is mostly useful inside infrastructure or advanced element helper code.

**Example:**

```cpp
const bool skipsLogic = FlowUi::elementDrawOptionsHas(options, FlowUi::ElementDrawOptions::SkipLogicCallback);
```

See: [Full Doxygen reference](group__flowui__element__system.html#ga1a34831c629e5d93e711ca00c1f12bf2).

## FlowUi::App

### **App** `1/2`


#### `App()`

- **Returns:** `App`
- **Arguments:** none.

Constructs an empty app handle. Public for move/handle mechanics, but normal application code should create an initialized app with `makeApplication()`.

**Example:**

```cpp
FlowUi::App emptyHandle{};
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a9ea3c124f860be1faf8f5645d582943c).

### **App** `2/2`


#### `App(App&&) noexcept`

- **Returns:** `App`
- **Arguments:** rvalue `App` to move from.

Moves an app handle and its owned runtime implementation. Copying is disabled because the app owns unique window, renderer, and manager resources.

**Example:**

```cpp
FlowUi::App emptyHandle{};
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a9ea3c124f860be1faf8f5645d582943c).

### **operator=**


#### `App& operator=(App&&) noexcept`

- **Returns:** `App&`
- **Arguments:** rvalue `App` to move from.

Move-assigns an app handle. The target takes ownership of the source runtime resources, and the source becomes a moved-from handle.

**Example:**

```cpp
runningApp = std::move(replacementApp);
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#afb8878c9292ddcff6382a9e22fedb8bb).

### **~App**


#### `~App()`

- **Returns:** none.
- **Arguments:** none.

Destroys the app runtime and releases owned resources. This includes managers, renderer state, window backend state, and GPU resources owned by the app.

**Example:**

```cpp
{ FlowUi::App app = FlowUi::makeApplication(config); }
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#aaad78bc0186f550641ae5959d54f2e2c).

### **shouldClose**


#### `bool shouldClose() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the window backend has requested shutdown. Use this as the condition for the main application loop.

**Example:**

```cpp
while (!app.shouldClose()) { app.beginFrame(); app.endFrame(); app.drawFrame(); }
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a9d853f690168da16dc71e453c9616913).

### **setShouldClose**


#### `void setShouldClose(int value)`

- **Returns:** `void`
- **Arguments:** `value` close flag value to pass to the window backend.

Sets or clears the window close flag. With the GLFW backend, `0` clears the GLFW window close flag and a non-zero value sets the GLFW window close flag.

**Example:**

```cpp
app.setShouldClose(1);
```

See: [Full Doxygen reference](classFlowUi_1_1App.html).

### **beginFrame**


#### `void beginFrame()`

- **Returns:** `void`
- **Arguments:** none.

Begins one FlowUi frame. It polls input, prepares frame-local UI state, and sets up the layout/input snapshot used while building UI.

**Example:**

```cpp
app.beginFrame();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a2a1be3d0f458e4fdd5392dcdfe4555f1).

### **endFrame**


#### `void endFrame()`

- **Returns:** `void`
- **Arguments:** none.

Ends UI construction for the current frame. It finalizes Clay render commands and prepares frame-dependent resources before rendering.

**Example:**

```cpp
app.endFrame();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a5c6ee53de89f3dd55d43e4742905414a).

### **drawFrame**


#### `void drawFrame()`

- **Returns:** `void`
- **Arguments:** none.

Submits and presents the frame produced by `endFrame()`. Call it once after UI construction has been finalized.

**Example:**

```cpp
app.drawFrame();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#ad256201c79c7e07675e12b5bd0d8c8aa).

### **fonts** `1/2`


#### `FlowUi::FontManager& fonts()`

- **Returns:** `FlowUi::FontManager&`
- **Arguments:** none.

Returns the mutable font manager owned by the app. Use it to create font families, add faces, resolve fonts, or inspect the atlas resource.

**Example:**

```cpp
FlowUi::FontFamilyId body = app.fonts().getFamilyId("Body");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a1db6aceb22351a9a0c199e1faeb9405a).

### **fonts** `2/2`


#### `const FlowUi::FontManager& fonts() const`

- **Returns:** `const FlowUi::FontManager&`
- **Arguments:** none.

Returns the immutable font manager owned by the app. Use this for read-only font lookup from const app contexts.

**Example:**

```cpp
FlowUi::FontFamilyId body = app.fonts().getFamilyId("Body");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#abd633c521ed80977c1992cf3cccc0310).

### **images** `1/2`


#### `ImageManager& images()`

- **Returns:** `ImageManager&`
- **Arguments:** none.

Returns the mutable image manager owned by the app. Use it to register image files and resolve texture references for UI image drawing.

**Example:**

```cpp
app.images().registerImage("logo", "assets/logo.png");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a5cf823f81759a819dc7fce58e75054f9).

### **images** `2/2`


#### `const ImageManager& images() const`

- **Returns:** `const ImageManager&`
- **Arguments:** none.

Returns the immutable image manager owned by the app. Use this when only checking or resolving already registered image keys.

**Example:**

```cpp
app.images().registerImage("logo", "assets/logo.png");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a04135a9974e2ea4aa8563083ebe51d40).

### **icons** `1/2`


#### `IconManager& icons()`

- **Returns:** `IconManager&`
- **Arguments:** none.

Returns the mutable icon manager when icon support is compiled in. Use it to register SVG icons and request texture references for UI rendering.

**Example:**

```cpp
app.icons().registerFromFile("save", "assets/icons/save.svg");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a7ef478693b7f8354db690c2ffc2e4cac).

### **icons** `2/2`


#### `const IconManager& icons() const`

- **Returns:** `const IconManager&`
- **Arguments:** none.

Returns the immutable icon manager when icon support is compiled in. Use this for read-only icon lookup paths.

**Example:**

```cpp
app.icons().registerFromFile("save", "assets/icons/save.svg");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a4aea0f22867850b6c6791ef76a827608).

### **viewPorts** `1/2`


#### `ViewPortManager& viewPorts()`

- **Returns:** `ViewPortManager&`
- **Arguments:** none.

Returns the mutable viewport manager when public Vulkan interop is enabled. Use it to create offscreen UI viewports and attach custom Vulkan render callbacks.

**Example:**

```cpp
app.viewPorts().create("scene-preview");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a704db5cbbabc33705f691362bfb275b8).

### **viewPorts** `2/2`


#### `const ViewPortManager& viewPorts() const`

- **Returns:** `const ViewPortManager&`
- **Arguments:** none.

Returns the immutable viewport manager when public Vulkan interop is enabled. Use this for read-only viewport lookup and texture access.

**Example:**

```cpp
app.viewPorts().create("scene-preview");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a28bca3bac1b8cba55d79950ad56515b2).

### **ui** `1/2`


#### `UiManager& ui()`

- **Returns:** `UiManager&`
- **Arguments:** none.

Returns the mutable UI manager for frame construction. This is the main surface for creating FlowUi elements and accessing frame-scoped UI services.

**Example:**

```cpp
FlowUi::UiManager& ui = app.ui();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a788f5bbe3b0d496a41a71494a5f485bc).

### **ui** `2/2`


#### `const UiManager& ui() const`

- **Returns:** `const UiManager&`
- **Arguments:** none.

Returns the immutable UI manager. Use it for read-only access to UI frame state and managers from const contexts.

**Example:**

```cpp
FlowUi::UiManager& ui = app.ui();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a4579cf4d39c726a5d2e625218bcfa059).

### **setWindowTitle**


#### `void setWindowTitle(std::string_view title)`

- **Returns:** `void`
- **Arguments:** `title` new native window title.

Updates the native window title after app creation. The initial title comes from `WindowConfig::title`.

**Example:**

```cpp
app.setWindowTitle("Project - Saved");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a02f684a2f6f2e3595ea0a674c95f4397).

### **windowSize**


#### `std::pair<int, int> windowSize() const`

- **Returns:** `std::pair<int, int>`
- **Arguments:** none.

Returns the current window size in screen coordinates. This is separate from framebuffer pixel size on high-DPI systems.

**Example:**

```cpp
auto [windowWidth, windowHeight] = app.windowSize();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#add8511c39a52e3d2bc9f5afedddf3228).

### **framebufferSize**


#### `std::pair<int, int> framebufferSize() const`

- **Returns:** `std::pair<int, int>`
- **Arguments:** none.

Returns the current framebuffer size in pixels. Use this for renderer-facing size logic where pixel dimensions matter.

**Example:**

```cpp
auto [fbWidth, fbHeight] = app.framebufferSize();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a304bb3c1528da2273dbb711d83fd6728).

### **setWindowInputConfig**


#### `void setWindowInputConfig(const WindowInputConfig& config)`

- **Returns:** `void`
- **Arguments:** `config` low-level window input configuration.

Applies cursor, sticky input, lock modifier, and raw mouse settings to the window backend. The initial input configuration comes from `WindowConfig::input`.

**Example:**

```cpp
app.setWindowInputConfig(FlowUi::WindowInputConfig{.cursorMode = FlowUi::CursorMode::Normal});
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#ac542187ec472fb6b9405fde9470ac098).

### **windowInputConfig**


#### `WindowInputConfig windowInputConfig() const`

- **Returns:** `WindowInputConfig`
- **Arguments:** none.

Returns the currently active low-level window input configuration. Use this when temporarily changing input behavior and later restoring it.

**Example:**

```cpp
FlowUi::WindowInputConfig inputConfig = app.windowInputConfig();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a9796415e95b720977162b8758e66f757).

### **nativeWindowHandle**


#### `void* nativeWindowHandle() const`

- **Returns:** `void*`
- **Arguments:** none.

Returns the backend native window handle when available. The concrete pointed-to type depends on the active window backend.

**Example:**

```cpp
void* nativeWindow = app.nativeWindowHandle();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a336d18e6c4db2aee3e904ffed6364ffb).

### **supportsRawMouseMotion**


#### `bool supportsRawMouseMotion() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the current backend and platform support raw mouse motion. Check this before enabling raw motion for camera-like or pointer-lock input.

**Example:**

```cpp
if (app.supportsRawMouseMotion()) { app.setWindowInputConfig({.rawMouseMotion = true}); }
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#a2717e2f03db8d86a85cc4c420547899b).

### **setClipboardText**


#### `void setClipboardText(std::string_view text)`

- **Returns:** `void`
- **Arguments:** `text` value to write to the system clipboard.

Writes clipboard text through the window backend. This is the app-level clipboard path; UI code can also use `UiManager` clipboard helpers.

**Example:**

```cpp
app.setClipboardText("Copied from FlowUi");
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#adf83dcd17ca6a6f93c8059af540035d0).

### **clipboardText**


#### `std::string clipboardText() const`

- **Returns:** `std::string`
- **Arguments:** none.

Reads clipboard text through the window backend. Returns the current clipboard text as an owning string.

**Example:**

```cpp
std::string pastedText = app.clipboardText();
```

See: [Full Doxygen reference](classFlowUi_1_1App.html#ab68d7f17584659d9f70a05af9e0f9fee).

## FlowUi::UiManager

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

### **storeTexture**


#### `TextureRef* storeTexture(const TextureRef& textureRef)`

- **Returns:** `TextureRef*`
- **Arguments:** `textureRef` texture reference to copy into frame storage.

Stores a texture reference in the current frame arena and returns a pointer suitable for `Clay_ImageElementConfig::imageData`. Use this instead of taking the address of a temporary or local `TextureRef`.

**Example:**

```cpp
imageConfig.imageData = context.uiManager.storeTexture(app.images().getTexture("logo"));
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

## FlowUi::FontManager

### **createFamily**


#### `FontFamilyId createFamily(const FontFamilyCreateInfo& createInfo)`

- **Returns:** `FontFamilyId`
- **Arguments:** `createInfo` logical family name and initial concrete faces.

Creates a logical font family and immediately loads its listed faces. Family names must be unique, and face paths load baked `.arfont` files unless runtime font baking is enabled for `.ttf`.

**Example:**

```cpp
FlowUi::FontFamilyId bodyFamily = app.fonts().createFamily({.name = "Body"});
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a2fd6b73942b1e8ffbccd4420de098b3f).

### **getFamilyId**


#### `FontFamilyId getFamilyId(std::string_view familyName) const`

- **Returns:** `FontFamilyId`
- **Arguments:** `familyName` logical family name.

Looks up a previously registered family id by name. Missing families return `UINT32_MAX`, making this a non-throwing cache-friendly lookup.

**Example:**

```cpp
FlowUi::FontFamilyId bodyFamily = app.fonts().getFamilyId("Body");
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a829f7e37438eef1eb8fa9981a8d511ec).

### **addFamilyFace** `1/2`


#### `FontId addFamilyFace(FontFamilyId familyId, const FontFaceCreateInfo& createInfo)`

- **Returns:** `FontId`
- **Arguments:** `familyId` existing family id, `createInfo` concrete face source and style data.

Adds a concrete face to an existing family by id. The new face becomes available to `resolveFont()` for its weight and style.

**Example:**

```cpp
FlowUi::FontId boldFace = app.fonts().addFamilyFace("Body", {.path = "assets/fonts/Inter-Bold.arfont", .weight = 700});
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#aaa09f54aa73849bba861457941bf1f19).

### **addFamilyFace** `2/2`


#### `FontId addFamilyFace(std::string_view familyName, const FontFaceCreateInfo& createInfo)`

- **Returns:** `FontId`
- **Arguments:** `familyName` existing family name, `createInfo` concrete face source and style data.

Adds a concrete face to an existing family by name. Use this when the caller has not cached the family id.

**Example:**

```cpp
FlowUi::FontId boldFace = app.fonts().addFamilyFace("Body", {.path = "assets/fonts/Inter-Bold.arfont", .weight = 700});
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a5e530f7fa3bcb6b0c4da4987fc804a62).

### **resolveFont** `1/2`


#### `FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyId` existing family id, `weight` requested weight, `style` requested style.

Resolves a logical font request to the best concrete face in a family. It prefers matching style and closest weight, with fallback behavior for missing variants.

**Example:**

```cpp
FlowUi::FontId bodyFont = app.fonts().resolveFont("Body", 400, FlowUi::FontStyle::Normal);
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a39c0eeaed6002691f954b1bcc6eadd83).

### **resolveFont** `2/2`


#### `FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyName` existing family name, `weight` requested weight, `style` requested style.

Named-family overload for resolving a concrete Clay font id. Returns `0` when the family is missing or empty.

**Example:**

```cpp
FlowUi::FontId bodyFont = app.fonts().resolveFont("Body", 400, FlowUi::FontStyle::Normal);
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a32e8291f5b08bc0d46d0884b8b4bad12).

### **getFontById**


#### `const FlowUi::Font::FontFaceData* getFontById(FontId fontId) const`

- **Returns:** `const FlowUi::Font::FontFaceData*`
- **Arguments:** `fontId` concrete font id.

Returns loaded font metrics, glyphs, kerning, and atlas placement for a concrete face. Normal UI code usually only needs `resolveFont()`, but renderer or advanced layout integrations may need this data.

**Example:**

```cpp
const FlowUi::Font::FontFaceData* face = app.fonts().getFontById(bodyFont);
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#ad67d18ee896d36b0cd0b4567cf79c311).

### **getAtlasResource**


#### `const FlowUi::Font::AtlasArrayResource& getAtlasResource() const`

- **Returns:** `const FlowUi::Font::AtlasArrayResource&`
- **Arguments:** none.

Returns the Vulkan atlas array resource used by FlowUi text rendering. Use `bindingRevision` to decide when external descriptors need refreshing.

**Example:**

```cpp
const FlowUi::Font::AtlasArrayResource& atlas = app.fonts().getAtlasResource();
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a20dada10a04d85cfb7b46f95c952085e).

## FlowUi::ImageManager

### **registerImage**


#### `bool registerImage(std::string_view key, std::string_view filePath)`

- **Returns:** `bool`
- **Arguments:** `key` application image key, `filePath` image file path.

Loads an image file, uploads it, and registers it under the provided key. Returns `true` for a new key and `false` when replacing an existing key.

**Example:**

```cpp
app.images().registerImage("avatar", "assets/avatar.png");
```

See: [Full Doxygen reference](classFlowUi_1_1ImageManager.html#a87c55cc08f33bb3fd8f83108aa15ea23).

### **removeImage**


#### `bool removeImage(std::string_view key)`

- **Returns:** `bool`
- **Arguments:** `key` registered image key.

Removes an image registration and retires the GPU resource safely. Existing `TextureRef` values for that key should be treated as invalid after removal.

**Example:**

```cpp
const bool removed = app.images().removeImage("avatar");
```

See: [Full Doxygen reference](classFlowUi_1_1ImageManager.html#ab1bb3be63b631b3947dc8d8a1950b7b5).

### **contains**


#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` image key to test.

Checks whether an image key is currently registered. This performs no file IO or GPU work.

**Example:**

```cpp
if (!app.images().contains("avatar")) { app.images().registerImage("avatar", "assets/avatar.png"); }
```

See: [Full Doxygen reference](classFlowUi_1_1ImageManager.html#a3ce30605daebf796adc6e757a93848c1).

### **getTexture**


#### `TextureRef getTexture(std::string_view key) const`

- **Returns:** `TextureRef`
- **Arguments:** `key` registered image key.

Returns the texture reference for a registered image. Missing keys return fallback texture id `0` and log a warning once for that key.

**Example:**

```cpp
FlowUi::TextureRef avatar = app.images().getTexture("avatar");
```

See: [Full Doxygen reference](classFlowUi_1_1ImageManager.html#a8db367968a237f11a7157d194a3720bc).

## FlowUi::IconManager

### **registerSvg**


#### `bool registerSvg(std::string_view key, std::string_view svgSource)`

- **Returns:** `bool`
- **Arguments:** `key` icon key, `svgSource` complete SVG source text.

Parses and registers an SVG document from memory. Raster variants are created lazily later when the icon is drawn at a particular size.

**Example:**

```cpp
app.icons().registerSvg("check", checkSvgSource);
```

See: [Full Doxygen reference](structFlowUi_1_1IconManager.html#af03e06a033403fd88d04412b6d11395c).

### **registerFromFile**


#### `bool registerFromFile(std::string_view key, std::string_view filePath)`

- **Returns:** `bool`
- **Arguments:** `key` icon key, `filePath` SVG file path.

Parses and registers an SVG document from disk. Returns `false` if the key already exists and the current icon is left unchanged.

**Example:**

```cpp
app.icons().registerFromFile("save", "assets/icons/save.svg");
```

See: [Full Doxygen reference](structFlowUi_1_1IconManager.html#a3014f0a3c8ae23869d23c107c6f469eb).

### **remove**


#### `bool remove(std::string_view key)`

- **Returns:** `bool`
- **Arguments:** `key` registered icon key.

Removes a registered SVG document and its cached atlas variants. Previously returned texture references for the key should be discarded.

**Example:**

```cpp
const bool removed = app.icons().remove("save");
```

See: [Full Doxygen reference](structFlowUi_1_1IconManager.html#a152fda978ab8ce39d2b33ab74cf7df29).

### **contains**


#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` icon key to test.

Checks whether an SVG document key is registered. This does not force rasterization or atlas allocation.

**Example:**

```cpp
if (!app.icons().contains("save")) { app.icons().registerFromFile("save", "assets/icons/save.svg"); }
```

See: [Full Doxygen reference](structFlowUi_1_1IconManager.html#aacefdec239a3b1fcd8aadae4eec05516).

### **textureRef**


#### `TextureRef textureRef(std::string_view key)`

- **Returns:** `TextureRef`
- **Arguments:** `key` registered icon key.

Returns a texture request reference for a registered icon. FlowUi later resolves the request into a cached atlas variant sized to the rendered UI image area.

**Example:**

```cpp
FlowUi::TextureRef saveIcon = app.icons().textureRef("save");
```

See: [Full Doxygen reference](structFlowUi_1_1IconManager.html#a0de1a60d027cd3b9985f73aebf128f48).

## FlowUi::InputFieldManager

### **requestField**


#### `FieldQueryResult requestField(ID fieldId, const FieldRequest& request)`

- **Returns:** `FieldQueryResult`
- **Arguments:** `fieldId` a typed Flow element ID or `ResourceKey`; `request` initial text, config, and Clay element ids.

Registers or updates an input field for the current frame and returns its current manager-owned state. Call this once per frame from the element that draws the editable field.

**Example:**

```cpp
FlowUi::FieldQueryResult field = context.uiManager.inputFields().requestField(
    context.id,
    {.initialText = "Search", .textElementId = textId, .contentElementId = contentId});
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#a6deadc46f16595277ae8e2258e63b787).

### **requestCaret**


#### `void requestCaret(ID fieldId, CaretRequestKind kind)`

- **Returns:** `void`
- **Arguments:** `fieldId` field to focus or edit, `kind` requested caret operation.

Requests focus or caret changes for an input field. `SetPrimary` is the common operation for clicked fields, while `ClearAll` removes text focus globally.

**Example:**

```cpp
context.uiManager.inputFields().requestCaret(context.id, FlowUi::CaretRequestKind::SetPrimary);
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#aad13550088f959cf6d948173d8afa446).

### **removeField**


#### `bool removeField(ID fieldId)`

- **Returns:** `bool`
- **Arguments:** `fieldId` field state to remove.

Deletes stored text, config, caret, and selection state for one field. Use this when a dynamic field is removed or when external state should replace the edited text.

**Example:**

```cpp
const bool removed = app.ui().inputFields().removeField(nameFieldId);
```

See: [Full Doxygen reference](classFlowUi_1_1InputFieldManager.html#a1e0319ffec372a95e5d129a5d8bda14a).

### **replaceText**


#### `bool replaceText(ID fieldId, std::string_view text, bool preserveCaret = true)`

- **Returns:** `bool`
- **Arguments:** `fieldId` field state to update, `text` replacement text, `preserveCaret` whether to keep and clamp caret state.

Replaces stored text for an existing field. By default, active carets and selections are preserved and clamped to the new text; pass `false` to clear active carets from that field.

**Example:**

```cpp
const bool changed = app.ui().inputFields().replaceText(nameFieldId, externalName, false);
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

## FlowUi::ShortcutManager

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

## FlowUi::ViewPort

### **getKey**


#### `std::string_view getKey() const`

- **Returns:** `std::string_view`
- **Arguments:** none.

Returns the stable key used to create and look up this viewport. The key is owned by the viewport object.

**Example:**

```cpp
std::string_view viewportKey = viewport->getKey();
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#ac2b642087b0c4fdf18ab3d41cd36c582).

### **hasValidSize**


#### `bool hasValidSize() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the viewport currently has positive width and height. A viewport may be invalid before its texture has been referenced and sized by UI image commands.

**Example:**

```cpp
if (viewport->hasValidSize()) { renderScene(viewport->getSize()); }
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#af9f3839dfa6d587a58dc0ec31e54d0aa).

### **getSize**


#### `VkExtent2D getSize() const`

- **Returns:** `VkExtent2D`
- **Arguments:** none.

Returns the current render target size in pixels. FlowUi derives this from the largest UI image area using the viewport texture.

**Example:**

```cpp
VkExtent2D extent = viewport->getSize();
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#af26b8ce43556bdf7af7718dbd33a31d4).

### **textureRef**


#### `TextureRef textureRef() const`

- **Returns:** `TextureRef`
- **Arguments:** none.

Returns a texture reference for this viewport's current frame image. `ViewPortManager::getTexture()` is usually more convenient when drawing by key.

**Example:**

```cpp
FlowUi::TextureRef sceneTexture = viewport->textureRef();
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#a16a189477b7c50b6d0415441f3dd72f1).

### **setRenderCallback** `1/2`


#### `void setRenderCallback(RenderCallback callback)`

- **Returns:** `void`
- **Arguments:** `callback` function invoked for viewport rendering.

Installs a custom render callback for this viewport. FlowUi begins and ends the provided secondary command buffer, so callback code records commands but does not begin or end the buffer.

**Example:**

```cpp
viewport->setRenderCallback([](const FlowUi::ViewPortRenderContext& ctx) { recordSceneCommands(ctx); });
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#a392c8372c68fd6562c4bc5f7ac4fc41f).

### **setRenderCallback** `2/2`


#### `template <typename T, typename Fn> void setRenderCallback(std::shared_ptr<T> userData, Fn&& callback)`

- **Returns:** `void`
- **Arguments:** `userData` retained callback payload, `callback` callable invoked with render context and `T&`.

Installs a render callback that keeps typed shared user data alive. This is useful for binding scene renderers or other stateful rendering helpers to a viewport.

**Example:**

```cpp
viewport->setRenderCallback([](const FlowUi::ViewPortRenderContext& ctx) { recordSceneCommands(ctx); });
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#a8ac28ba477958a85dff2a9af682d1431).

### **clearRenderCallback**


#### `void clearRenderCallback()`

- **Returns:** `void`
- **Arguments:** none.

Clears the viewport render callback. FlowUi continues to manage the viewport texture, but no custom draw commands are recorded.

**Example:**

```cpp
viewport->clearRenderCallback();
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#a41cbb5a21265263d5cb7bfab078b9b99).

### **hasRenderCallback**


#### `bool hasRenderCallback() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether a render callback is currently installed. Use this to avoid redundant setup or to display fallback content.

**Example:**

```cpp
if (!viewport->hasRenderCallback()) { viewport->setRenderCallback(renderScene); }
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#a0b98f73f56b90fefe2cac173f49aff0e).

### **setClearColor**


#### `void setClearColor(float r, float g, float b, float a)`

- **Returns:** `void`
- **Arguments:** `r`, `g`, `b`, `a` clear color channels.

Sets the viewport clear color. The color is used by the viewport render pass when clearing is enabled.

**Example:**

```cpp
viewport->setClearColor(0.02f, 0.02f, 0.03f, 1.0f);
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#a5041209fac6a58554ebe927bc70d194d).

### **clearColor**


#### `std::array<float, 4> clearColor() const`

- **Returns:** `std::array<float, 4>`
- **Arguments:** none.

Returns the current viewport clear color. The values are RGBA channels.

**Example:**

```cpp
std::array<float, 4> color = viewport->clearColor();
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#a36354a3627449b1c250cb31d63f26175).

### **setClearEveryFrame**


#### `void setClearEveryFrame(bool enabled)`

- **Returns:** `void`
- **Arguments:** `enabled` true to clear each rendered frame.

Controls whether FlowUi clears the viewport image every frame. Disabling clear can be useful for persistent render target effects.

**Example:**

```cpp
viewport->setClearEveryFrame(false);
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#a39b1ba8923020766f15ee701575aa0ca).

### **clearEveryFrame**


#### `bool clearEveryFrame() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the viewport clears before rendering each frame. When false, FlowUi loads previous image contents after initialization.

**Example:**

```cpp
const bool clears = viewport->clearEveryFrame();
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPort.html#aa397c0dfbcd5ebcf8b6b5290adcff367).

## FlowUi::ViewPortManager

### **create**


#### `bool create(std::string_view key, const ViewPortCreateInfo& createInfo = {})`

- **Returns:** `bool`
- **Arguments:** `key` viewport key, `createInfo` initial format and clear settings.

Creates a named offscreen viewport. Newly created viewports resize automatically when their texture is referenced by UI image commands.

**Example:**

```cpp
app.viewPorts().create("scene", {.clearColor = {0.02f, 0.02f, 0.03f, 1.0f}});
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPortManager.html#af6f5b9b1adf89a9ea58bec5fbf3ba86c).

### **remove**


#### `bool remove(std::string_view key)`

- **Returns:** `bool`
- **Arguments:** `key` viewport key.

Removes a viewport and destroys its per-frame resources. This can block because the implementation waits for the Vulkan device to become idle before releasing resources.

**Example:**

```cpp
const bool removed = app.viewPorts().remove("scene");
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPortManager.html#a393b43dacaeb66f2ebd66be2d15329c0).

### **contains**


#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` viewport key.

Checks whether a viewport exists for the key. This is a lightweight lookup with no rendering side effects.

**Example:**

```cpp
if (!app.viewPorts().contains("scene")) { app.viewPorts().create("scene"); }
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPortManager.html#a6ae77f78ebf36af55bf61ff75bef31ec).

### **getViewPort** `1/2`


#### `ViewPort* getViewPort(std::string_view key)`

- **Returns:** `ViewPort*`
- **Arguments:** `key` viewport key.

Returns a mutable viewport pointer, or `nullptr` when missing. Use this to set callbacks, clear color, or persistent clear behavior.

**Example:**

```cpp
FlowUi::ViewPort* scene = app.viewPorts().getViewPort("scene");
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPortManager.html#af6d427e4ed40d83a5485ab81ede41beb).

### **getViewPort** `2/2`


#### `const ViewPort* getViewPort(std::string_view key) const`

- **Returns:** `const ViewPort*`
- **Arguments:** `key` viewport key.

Returns an immutable viewport pointer, or `nullptr` when missing. Use this for read-only viewport inspection.

**Example:**

```cpp
FlowUi::ViewPort* scene = app.viewPorts().getViewPort("scene");
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPortManager.html#a0d6ce77f3c9c25cd6e1a8e99be0b031a).

### **getTexture**


#### `TextureRef getTexture(std::string_view key) const`

- **Returns:** `TextureRef`
- **Arguments:** `key` viewport key.

Returns a texture reference for the current frame's viewport image. Missing keys return fallback texture id `0` and log a warning once.

**Example:**

```cpp
FlowUi::TextureRef sceneTexture = app.viewPorts().getTexture("scene");
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPortManager.html#aa24f7ce00d8a59ecd9a1bc4a4a92c921).

### **getVulkanInterop**


#### `const ViewPortVulkanInterop& getVulkanInterop() const`

- **Returns:** `const ViewPortVulkanInterop&`
- **Arguments:** none.

Returns shared Vulkan handles owned by the FlowUi app. Use these handles only to create compatible resources or record viewport work; do not destroy them.

**Example:**

```cpp
const FlowUi::ViewPortVulkanInterop& vk = app.viewPorts().getVulkanInterop();
```

See: [Full Doxygen reference](classFlowUi_1_1ViewPortManager.html#ae7c0bc4fd533b02c7aee388d8ee272ec).

## FlowUi::ElementBuilder

### **ElementBuilder**


#### `ElementBuilder(UiManager& uiManager, const DefinitionType* definition, std::string elementID)`

- **Returns:** `ElementBuilder`
- **Arguments:** `uiManager` active UI manager, `definition` element definition pointer, `elementID` stable element instance id.

Constructs a builder for one element invocation. User code normally gets builders from `UiManager::createElement()` rather than calling this constructor directly.

**Example:**

```cpp
auto builder = app.ui().createElement(kButton, "toolbar/save");
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#a46780ec97486821e4b9f366bbab8fdab).

### **setParameters** `1/2`


#### `ElementBuilder& setParameters(const ParametersType& parameters)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `parameters` parameter values to copy.

Copies parameter values into the builder. The stored parameters are passed to interaction, logic, construct, and build callbacks.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").setParameters(ButtonParams{.label = "Save"}).draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#a55159e63934e32a22bd30889b5639a99).

### **setParameters** `2/2`


#### `ElementBuilder& setParameters(ParametersType&& parameters)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `parameters` parameter values to move.

Moves parameter values into the builder. Use this when parameter construction is dynamic or owns heavier data.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").setParameters(ButtonParams{.label = "Save"}).draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#a711158a45396b9b1a83522eaa051f26e).

### **mergeParams**


#### `template <typename MergeFn> ElementBuilder& mergeParams(MergeFn&& mergeFn)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `mergeFn` callable invoked with `ParametersType&`.

Mutates the builder's existing parameter storage. This is useful when defaults are mostly correct and only a few fields need to be changed.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").mergeParams([](ButtonParams& params) { params.enabled = false; }).draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#a0cb3694ec3a0bc7ac94feee7217f4788).

### **withElementID**


#### `ElementBuilder& withElementID(std::string_view elementID)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `elementID` replacement element id string.

Replaces the id stored by the builder. Use it when a builder is created before the final stable id is known.

**Example:**

```cpp
buttonBuilder.withElementID("toolbar/save-secondary").draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#addcbec2dd601b254be83f4114cb679f7).

### **setDevInternalCapture**


#### `ElementBuilder& setDevInternalCapture(bool isDevInternal = true)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `isDevInternal` whether dev capture should mark this invocation internal.

Controls how this element invocation is captured in developer mode. Normal user elements usually do not need this.

**Example:**

```cpp
app.ui().createElement(kDevPanel, "flowui/dev/panel").setDevInternalCapture(true).draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#ab2a74747085369a4b157d804c85f9699).

### **construct**


#### `void construct(ElementDrawOptions options = ElementDrawOptions::Default)`

- **Returns:** `void`
- **Arguments:** `options` callback phases to skip.

Runs enabled callbacks and opens a constructed Clay root using the definition's `constructElement` callback. Emit child nodes after this call, then close the element with `UiManager::drawConstructed()`.

**Example:**

```cpp
app.ui().createElement(kPanel, "settings").construct();
```

See: [Full Doxygen reference](group__flowui__element__system.html#gadd7adf768823bd01379c9054c516448f).

### **draw**


#### `void draw(ElementDrawOptions options = ElementDrawOptions::Default)`

- **Returns:** `void`
- **Arguments:** `options` callback phases to skip.

Runs enabled callbacks and emits the element through its `buildElement` callback. This is the most common final call for a FlowUi element builder.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").draw();
```

See: [Full Doxygen reference](group__flowui__element__system.html#ga7cefdbb08aab1d49879d08adc61d0509).

## FlowUi::InteractionSnapshot

### **contains**


#### `static bool contains(const std::vector<Clay_ElementId>& list, Clay_ElementId id)`

- **Returns:** `bool`
- **Arguments:** `list` interaction id list, `id` Clay element id to find.

Checks whether a Clay element id exists in an interaction list. Comparison uses the underlying Clay id value.

**Example:**

```cpp
const bool hasButton = FlowUi::InteractionSnapshot::contains(snapshot.pressedElementIds, buttonId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#a63d6503a6bcc503717b0ad8ee9fca8cd).

### **isHovered**


#### `bool isHovered(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element was hovered in this snapshot. Element callbacks commonly use this for child interaction checks.

**Example:**

```cpp
const bool hovered = context.previousInteraction.isHovered(rootId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#ad98a8c6a0e0664f27c24c97c11e1e031).

### **isPressed**


#### `bool isPressed(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element received a primary pointer press in this snapshot. This is based on the previous completed frame when used from element callbacks.

**Example:**

```cpp
const bool pressed = context.previousInteraction.isPressed(rootId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#a389ad759f1f175955c065a5fe6a88b5b).

### **isHeld**


#### `bool isHeld(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element was held by the primary pointer in this snapshot. Use it for drag-like or continuous pressed behavior.

**Example:**

```cpp
const bool held = context.previousInteraction.isHeld(rootId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#acb7231c76df9dbfbba2bdc1c02daeb02).

### **isReleased**


#### `bool isReleased(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element received a primary pointer release in this snapshot. Use it for release-triggered actions.

**Example:**

```cpp
const bool released = context.previousInteraction.isReleased(rootId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#ad70c5be92f52d3e35a7bea1d7c904874).

## FlowUi::ElementBuildContext

### **createChildElementId**


#### `std::string createChildElementId(std::string_view localChildId) const`

- **Returns:** `std::string`
- **Arguments:** `localChildId` child id segment or relative child path.

Creates a stable child id by appending the local child id to the current element id. Use this for child Clay nodes or nested Flow elements owned by the current element.

**Example:**

```cpp
Clay_ElementId labelId = context.uiManager.toClayEID(context.createChildElementId("label"));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementBuildContext.html#ac92999f9addc15917028cea0cbdf39a3).

## FlowUi::ElementInteractionContext

### **createChildElementId**


#### `std::string createChildElementId(std::string_view localChildId) const`

- **Returns:** `std::string`
- **Arguments:** `localChildId` child id segment or relative child path.

Creates a stable child id from inside interaction or logic callbacks. This is useful when querying previous interaction for child elements.

**Example:**

```cpp
Clay_ElementId labelId = context.uiManager.toClayEID(context.createChildElementId("label"));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementInteractionContext.html#ad1a4815b3e3cf5fdae8651902903b05a).

## FlowUi::ElementDefinition

### **getResources**


#### `static ResourcesType& getResources(App& app)`

- **Returns:** `ResourcesType&`
- **Arguments:** `app` active FlowUi app used for resource construction.

Lazily creates and returns the shared resources instance for this element definition specialization. Available only when the `Resources` template argument is not `void`.

**Example:**

```cpp
ButtonResources& resources = ButtonDefinition::getResources(app);
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#acd4597db7232a8da38104d1c14fd8abb).

### **getOrCreateState**


#### `static StateType& getOrCreateState(uint64_t elementFlowId)`

- **Returns:** `StateType&`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Returns existing state for an element instance or creates default state when missing. Available only when the `State` template argument is not `void`.

**Example:**

```cpp
ButtonState& state = ButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#ab8d37b94a61ee757c7656e01bd69849e).

### **tryGetState**


#### `static StateType* tryGetState(uint64_t elementFlowId)`

- **Returns:** `StateType*`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Looks up mutable state without creating it. Returns `nullptr` when the element instance has no stored state.

**Example:**

```cpp
if (ButtonState* state = ButtonDefinition::tryGetState(FLOW_ID("toolbar/save"))) { state->pressed = false; }
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#a8245c51a90d1d7e66df56670be390b05).

### **tryGetStateConst**


#### `static const StateType* tryGetStateConst(uint64_t elementFlowId)`

- **Returns:** `const StateType*`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Looks up immutable state without creating it. Use this for read-only checks outside element callbacks.

**Example:**

```cpp
const ButtonState* state = ButtonDefinition::tryGetStateConst(FLOW_ID("toolbar/save"));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#a1d8bacf80f61458c0b24b2b49b7d897d).

### **eraseState**


#### `static bool eraseState(uint64_t elementFlowId)`

- **Returns:** `bool`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Erases stored state for one element instance. FlowUi does not automatically garbage-collect custom element state, so dynamic UI can use this to keep state pools bounded.

**Example:**

```cpp
const bool erased = ButtonDefinition::eraseState(FLOW_ID("toolbar/save"));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#a86a3273921a616d575a46a3538b88382).

## FlowUi::Font::FontVariantData

### **kerningKey**


#### `static uint64_t kerningKey(uint32_t leftCodepoint, uint32_t rightCodepoint)`

- **Returns:** `uint64_t`
- **Arguments:** `leftCodepoint` left glyph codepoint, `rightCodepoint` right glyph codepoint.

Packs two codepoints into the key used by the kerning lookup table. This is mainly useful when inspecting or extending font metric data.

**Example:**

```cpp
uint64_t key = FlowUi::Font::FontVariantData::kerningKey(U'A', U'V');
```

See: [Full Doxygen reference](structFlowUi_1_1Font_1_1FontVariantData.html#aac641adb1ef8d9b2cfbaa10652b38986).

### **kerningAdvance**


#### `float kerningAdvance(uint32_t leftCodepoint, uint32_t rightCodepoint) const`

- **Returns:** `float`
- **Arguments:** `leftCodepoint` left glyph codepoint, `rightCodepoint` right glyph codepoint.

Returns kerning advance for a codepoint pair. Missing pairs return `0.0f`.

**Example:**

```cpp
float advance = variant.kerningAdvance(U'A', U'V');
```

See: [Full Doxygen reference](structFlowUi_1_1Font_1_1FontVariantData.html#acb58bcb53965391de0525cb6850a9d29).

## FlowUi::Font::FontFaceData

### **defaultVariant**


#### `const FontVariantData* defaultVariant() const`

- **Returns:** `const FontVariantData*`
- **Arguments:** none.

Returns the default baked variant for a loaded font face. Returns `nullptr` when the face has no variants or the default index is invalid.

**Example:**

```cpp
const FlowUi::Font::FontVariantData* variant = face->defaultVariant();
```

See: [Full Doxygen reference](structFlowUi_1_1Font_1_1FontFaceData.html#ad97f5e22e625cb9698ddd429fc1c63e1).
