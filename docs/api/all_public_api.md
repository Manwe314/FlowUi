# All Public API

This page is a scan-first cheat sheet for public FlowUi functions. It sits between the generated Doxygen reference and the tutorial docs: function names are visible, signatures are explicit, and descriptions stay short.

## FlowUi Namespace Functions

### **toFlowId** `1/2`
---

#### `constexpr FlowElementId toFlowId(std::string_view elementName) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `elementName` string to hash into an element instance id.

Hashes a runtime string into a stable FlowUi element id. Use this when looking up or managing state for an element instance outside the builder path.

**Example:**

```cpp
const FlowUi::FlowElementId saveButtonId = FlowUi::toFlowId("toolbar/save");
```

### **toFlowId** `2/2`
---

#### `template <std::size_t N> constexpr FlowElementId toFlowId(const char (&elementName)[N]) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `elementName` string literal to hash into an element instance id.

String-literal overload for `toFlowId`. It avoids counting the terminating null byte and can be used in constant expressions.

**Example:**

```cpp
const FlowUi::FlowElementId saveButtonId = FlowUi::toFlowId("toolbar/save");
```

### **toFlowDefinitionId** `1/2`
---

#### `constexpr FlowDefinitionId toFlowDefinitionId(std::string_view definitionName) noexcept`

- **Returns:** `FlowDefinitionId`
- **Arguments:** `definitionName` string to hash into an element definition id.

Hashes a runtime string into a stable FlowUi element definition id. This is the function behind definition ids used by `ElementDefinition`.

**Example:**

```cpp
constexpr FlowUi::FlowDefinitionId buttonDefinitionId = FlowUi::toFlowDefinitionId("button");
```

### **toFlowDefinitionId** `2/2`
---

#### `template <std::size_t N> constexpr FlowDefinitionId toFlowDefinitionId(const char (&definitionName)[N]) noexcept`

- **Returns:** `FlowDefinitionId`
- **Arguments:** `definitionName` string literal to hash into an element definition id.

String-literal overload for `toFlowDefinitionId`. Prefer this through `FLOW_DEF_ID("name")` when declaring custom element definition types.

**Example:**

```cpp
constexpr FlowUi::FlowDefinitionId buttonDefinitionId = FlowUi::toFlowDefinitionId("button");
```

### **createIndexedFlowId** `1/3`
---

#### `constexpr FlowElementId createIndexedFlowId(FlowElementId rootId, uint64_t index) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `rootId` parent/root Flow id, `index` numeric child/index value.

Creates a stable child-style id by mixing an existing Flow id with an index. This is useful for repeated UI rows or generated children where a string id would be awkward.

**Example:**

```cpp
const FlowUi::FlowElementId rowId = FlowUi::createIndexedFlowId("asset-list/row", rowIndex);
```

### **createIndexedFlowId** `2/3`
---

#### `constexpr FlowElementId createIndexedFlowId(std::string_view rootName, uint64_t index) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `rootName` parent/root name, `index` numeric child/index value.

Hashes the root name and then mixes in the numeric index. Use this when generating stable ids from a named collection or repeated layout section.

**Example:**

```cpp
const FlowUi::FlowElementId rowId = FlowUi::createIndexedFlowId("asset-list/row", rowIndex);
```

### **createIndexedFlowId** `3/3`
---

#### `template <std::size_t N> constexpr FlowElementId createIndexedFlowId(const char (&rootName)[N], uint64_t index) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `rootName` string literal parent/root name, `index` numeric child/index value.

String-literal overload for indexed id creation. It is useful for compile-time root names paired with runtime loop indexes.

**Example:**

```cpp
const FlowUi::FlowElementId rowId = FlowUi::createIndexedFlowId("asset-list/row", rowIndex);
```

### **Flow_Color**
---

#### `Clay_Color Flow_Color(std::string_view hexRgba)`

- **Returns:** `Clay_Color`
- **Arguments:** `hexRgba` color string in `#RRGGBBAA` form.

Converts a hex RGBA string into a Clay color. The input must include the leading `#` and eight hex digits; invalid input throws `std::invalid_argument`.

**Example:**

```cpp
Clay_Color panelColor = FlowUi::Flow_Color("#20242cff");
```

### **makeApplication**
---

#### `App makeApplication(const AppConfig& cfg)`

- **Returns:** `App`
- **Arguments:** `cfg` top-level FlowUi app configuration.

Creates and initializes a running FlowUi application. Use this instead of manually constructing `App`; it wires the window, managers, renderer, and configuration together.

**Example:**

```cpp
FlowUi::App app = FlowUi::makeApplication(config);
```

### **operator|**
---

#### `ElementDrawOptions operator|(ElementDrawOptions a, ElementDrawOptions b)`

- **Returns:** `ElementDrawOptions`
- **Arguments:** `a` first option flag, `b` second option flag.

Combines draw-option flags for `ElementBuilder::draw()` and `ElementBuilder::construct()`. This is the normal way to skip multiple callback phases in one builder call.

**Example:**

```cpp
auto options = FlowUi::ElementDrawOptions::SkipEventCallbacks | FlowUi::ElementDrawOptions::SkipLogicCallback;
```

### **elementDrawOptionsHas**
---

#### `bool elementDrawOptionsHas(ElementDrawOptions value, ElementDrawOptions flag)`

- **Returns:** `bool`
- **Arguments:** `value` combined option value, `flag` flag to test.

Checks whether an `ElementDrawOptions` value contains a specific flag. This is mostly useful inside infrastructure or advanced element helper code.

**Example:**

```cpp
const bool skipsLogic = FlowUi::elementDrawOptionsHas(options, FlowUi::ElementDrawOptions::SkipLogicCallback);
```

## FlowUi::App

### **App** `1/2`
---

#### `App()`

- **Returns:** `App`
- **Arguments:** none.

Constructs an empty app handle. Public for move/handle mechanics, but normal application code should create an initialized app with `makeApplication()`.

**Example:**

```cpp
FlowUi::App emptyHandle{};
```

### **App** `2/2`
---

#### `App(App&&) noexcept`

- **Returns:** `App`
- **Arguments:** rvalue `App` to move from.

Moves an app handle and its owned runtime implementation. Copying is disabled because the app owns unique window, renderer, and manager resources.

**Example:**

```cpp
FlowUi::App emptyHandle{};
```

### **operator=**
---

#### `App& operator=(App&&) noexcept`

- **Returns:** `App&`
- **Arguments:** rvalue `App` to move from.

Move-assigns an app handle. The target takes ownership of the source runtime resources, and the source becomes a moved-from handle.

**Example:**

```cpp
runningApp = std::move(replacementApp);
```

### **~App**
---

#### `~App()`

- **Returns:** none.
- **Arguments:** none.

Destroys the app runtime and releases owned resources. This includes managers, renderer state, window backend state, and GPU resources owned by the app.

**Example:**

```cpp
{ FlowUi::App app = FlowUi::makeApplication(config); }
```

### **shouldClose**
---

#### `bool shouldClose() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the window backend has requested shutdown. Use this as the condition for the main application loop.

**Example:**

```cpp
while (!app.shouldClose()) { app.beginFrame(); app.endFrame(); app.drawFrame(); }
```

### **beginFrame**
---

#### `void beginFrame()`

- **Returns:** `void`
- **Arguments:** none.

Begins one FlowUi frame. It polls input, prepares frame-local UI state, and sets up the layout/input snapshot used while building UI.

**Example:**

```cpp
app.beginFrame();
```

### **endFrame**
---

#### `void endFrame()`

- **Returns:** `void`
- **Arguments:** none.

Ends UI construction for the current frame. It finalizes Clay render commands and prepares frame-dependent resources before rendering.

**Example:**

```cpp
app.endFrame();
```

### **drawFrame**
---

#### `void drawFrame()`

- **Returns:** `void`
- **Arguments:** none.

Submits and presents the frame produced by `endFrame()`. Call it once after UI construction has been finalized.

**Example:**

```cpp
app.drawFrame();
```

### **fonts** `1/2`
---

#### `FontManager& fonts()`

- **Returns:** `FontManager&`
- **Arguments:** none.

Returns the mutable font manager owned by the app. Use it to create font families, add faces, resolve fonts, or inspect the atlas resource.

**Example:**

```cpp
FlowUi::FontFamilyId body = app.fonts().getFamilyId("Body");
```

### **fonts** `2/2`
---

#### `const FontManager& fonts() const`

- **Returns:** `const FontManager&`
- **Arguments:** none.

Returns the immutable font manager owned by the app. Use this for read-only font lookup from const app contexts.

**Example:**

```cpp
FlowUi::FontFamilyId body = app.fonts().getFamilyId("Body");
```

### **images** `1/2`
---

#### `ImageManager& images()`

- **Returns:** `ImageManager&`
- **Arguments:** none.

Returns the mutable image manager owned by the app. Use it to register image files and resolve texture references for UI image drawing.

**Example:**

```cpp
app.images().registerImage("logo", "assets/logo.png");
```

### **images** `2/2`
---

#### `const ImageManager& images() const`

- **Returns:** `const ImageManager&`
- **Arguments:** none.

Returns the immutable image manager owned by the app. Use this when only checking or resolving already registered image keys.

**Example:**

```cpp
app.images().registerImage("logo", "assets/logo.png");
```

### **icons** `1/2`
---

#### `IconManager& icons()`

- **Returns:** `IconManager&`
- **Arguments:** none.

Returns the mutable icon manager when icon support is compiled in. Use it to register SVG icons and request texture references for UI rendering.

**Example:**

```cpp
app.icons().registerFromFile("save", "assets/icons/save.svg");
```

### **icons** `2/2`
---

#### `const IconManager& icons() const`

- **Returns:** `const IconManager&`
- **Arguments:** none.

Returns the immutable icon manager when icon support is compiled in. Use this for read-only icon lookup paths.

**Example:**

```cpp
app.icons().registerFromFile("save", "assets/icons/save.svg");
```

### **viewPorts** `1/2`
---

#### `ViewPortManager& viewPorts()`

- **Returns:** `ViewPortManager&`
- **Arguments:** none.

Returns the mutable viewport manager when public Vulkan interop is enabled. Use it to create offscreen UI viewports and attach custom Vulkan render callbacks.

**Example:**

```cpp
app.viewPorts().create("scene-preview");
```

### **viewPorts** `2/2`
---

#### `const ViewPortManager& viewPorts() const`

- **Returns:** `const ViewPortManager&`
- **Arguments:** none.

Returns the immutable viewport manager when public Vulkan interop is enabled. Use this for read-only viewport lookup and texture access.

**Example:**

```cpp
app.viewPorts().create("scene-preview");
```

### **ui** `1/2`
---

#### `UiManager& ui()`

- **Returns:** `UiManager&`
- **Arguments:** none.

Returns the mutable UI manager for frame construction. This is the main surface for creating FlowUi elements and accessing frame-scoped UI services.

**Example:**

```cpp
FlowUi::UiManager& ui = app.ui();
```

### **ui** `2/2`
---

#### `const UiManager& ui() const`

- **Returns:** `const UiManager&`
- **Arguments:** none.

Returns the immutable UI manager. Use it for read-only access to UI frame state and managers from const contexts.

**Example:**

```cpp
FlowUi::UiManager& ui = app.ui();
```

### **setWindowTitle**
---

#### `void setWindowTitle(std::string_view title)`

- **Returns:** `void`
- **Arguments:** `title` new native window title.

Updates the native window title after app creation. The initial title comes from `WindowConfig::title`.

**Example:**

```cpp
app.setWindowTitle("Project - Saved");
```

### **windowSize**
---

#### `std::pair<int, int> windowSize() const`

- **Returns:** `std::pair<int, int>`
- **Arguments:** none.

Returns the current window size in screen coordinates. This is separate from framebuffer pixel size on high-DPI systems.

**Example:**

```cpp
auto [windowWidth, windowHeight] = app.windowSize();
```

### **framebufferSize**
---

#### `std::pair<int, int> framebufferSize() const`

- **Returns:** `std::pair<int, int>`
- **Arguments:** none.

Returns the current framebuffer size in pixels. Use this for renderer-facing size logic where pixel dimensions matter.

**Example:**

```cpp
auto [fbWidth, fbHeight] = app.framebufferSize();
```

### **setWindowInputConfig**
---

#### `void setWindowInputConfig(const WindowInputConfig& config)`

- **Returns:** `void`
- **Arguments:** `config` low-level window input configuration.

Applies cursor, sticky input, lock modifier, and raw mouse settings to the window backend. The initial input configuration comes from `WindowConfig::input`.

**Example:**

```cpp
app.setWindowInputConfig(FlowUi::WindowInputConfig{.cursorMode = FlowUi::CursorMode::Normal});
```

### **windowInputConfig**
---

#### `WindowInputConfig windowInputConfig() const`

- **Returns:** `WindowInputConfig`
- **Arguments:** none.

Returns the currently active low-level window input configuration. Use this when temporarily changing input behavior and later restoring it.

**Example:**

```cpp
FlowUi::WindowInputConfig inputConfig = app.windowInputConfig();
```

### **nativeWindowHandle**
---

#### `void* nativeWindowHandle() const`

- **Returns:** `void*`
- **Arguments:** none.

Returns the backend native window handle when available. The concrete pointed-to type depends on the active window backend.

**Example:**

```cpp
void* nativeWindow = app.nativeWindowHandle();
```

### **supportsRawMouseMotion**
---

#### `bool supportsRawMouseMotion() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the current backend and platform support raw mouse motion. Check this before enabling raw motion for camera-like or pointer-lock input.

**Example:**

```cpp
if (app.supportsRawMouseMotion()) { app.setWindowInputConfig({.rawMouseMotion = true}); }
```

### **setClipboardText**
---

#### `void setClipboardText(std::string_view text)`

- **Returns:** `void`
- **Arguments:** `text` value to write to the system clipboard.

Writes clipboard text through the window backend. This is the app-level clipboard path; UI code can also use `UiManager` clipboard helpers.

**Example:**

```cpp
app.setClipboardText("Copied from FlowUi");
```

### **clipboardText**
---

#### `std::string clipboardText() const`

- **Returns:** `std::string`
- **Arguments:** none.

Reads clipboard text through the window backend. Returns the current clipboard text as an owning string.

**Example:**

```cpp
std::string pastedText = app.clipboardText();
```

## FlowUi::UiManager

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

## FontManager

### **createFamily**
---

#### `FontFamilyId createFamily(const FontFamilyCreateInfo& createInfo)`

- **Returns:** `FontFamilyId`
- **Arguments:** `createInfo` logical family name and initial concrete faces.

Creates a logical font family and immediately loads its listed faces. Family names must be unique, and face paths load baked `.arfont` files unless runtime font baking is enabled for `.ttf`.

**Example:**

```cpp
FlowUi::FontFamilyId bodyFamily = app.fonts().createFamily({.name = "Body"});
```

### **getFamilyId**
---

#### `FontFamilyId getFamilyId(std::string_view familyName) const`

- **Returns:** `FontFamilyId`
- **Arguments:** `familyName` logical family name.

Looks up a previously registered family id by name. Missing families return `UINT32_MAX`, making this a non-throwing cache-friendly lookup.

**Example:**

```cpp
FlowUi::FontFamilyId bodyFamily = app.fonts().getFamilyId("Body");
```

### **addFamilyFace** `1/2`
---

#### `FontId addFamilyFace(FontFamilyId familyId, const FontFaceCreateInfo& createInfo)`

- **Returns:** `FontId`
- **Arguments:** `familyId` existing family id, `createInfo` concrete face source and style data.

Adds a concrete face to an existing family by id. The new face becomes available to `resolveFont()` for its weight and style.

**Example:**

```cpp
FlowUi::FontId boldFace = app.fonts().addFamilyFace("Body", {.path = "assets/fonts/Inter-Bold.arfont", .weight = 700});
```

### **addFamilyFace** `2/2`
---

#### `FontId addFamilyFace(std::string_view familyName, const FontFaceCreateInfo& createInfo)`

- **Returns:** `FontId`
- **Arguments:** `familyName` existing family name, `createInfo` concrete face source and style data.

Adds a concrete face to an existing family by name. Use this when the caller has not cached the family id.

**Example:**

```cpp
FlowUi::FontId boldFace = app.fonts().addFamilyFace("Body", {.path = "assets/fonts/Inter-Bold.arfont", .weight = 700});
```

### **resolveFont** `1/2`
---

#### `FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyId` existing family id, `weight` requested weight, `style` requested style.

Resolves a logical font request to the best concrete face in a family. It prefers matching style and closest weight, with fallback behavior for missing variants.

**Example:**

```cpp
FlowUi::FontId bodyFont = app.fonts().resolveFont("Body", 400, FlowUi::FontStyle::Normal);
```

### **resolveFont** `2/2`
---

#### `FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyName` existing family name, `weight` requested weight, `style` requested style.

Named-family overload for resolving a concrete Clay font id. Returns `0` when the family is missing or empty.

**Example:**

```cpp
FlowUi::FontId bodyFont = app.fonts().resolveFont("Body", 400, FlowUi::FontStyle::Normal);
```

### **getFontById**
---

#### `const FlowUi::Font::FontFaceData* getFontById(FontId fontId) const`

- **Returns:** `const FlowUi::Font::FontFaceData*`
- **Arguments:** `fontId` concrete font id.

Returns loaded font metrics, glyphs, kerning, and atlas placement for a concrete face. Normal UI code usually only needs `resolveFont()`, but renderer or advanced layout integrations may need this data.

**Example:**

```cpp
const FlowUi::Font::FontFaceData* face = app.fonts().getFontById(bodyFont);
```

### **getAtlasResource**
---

#### `const FlowUi::Font::AtlasArrayResource& getAtlasResource() const`

- **Returns:** `const FlowUi::Font::AtlasArrayResource&`
- **Arguments:** none.

Returns the Vulkan atlas array resource used by FlowUi text rendering. Use `bindingRevision` to decide when external descriptors need refreshing.

**Example:**

```cpp
const FlowUi::Font::AtlasArrayResource& atlas = app.fonts().getAtlasResource();
```

## FlowUi::ImageManager

### **registerImage**
---

#### `bool registerImage(std::string_view key, std::string_view filePath)`

- **Returns:** `bool`
- **Arguments:** `key` application image key, `filePath` image file path.

Loads an image file, uploads it, and registers it under the provided key. Returns `true` for a new key and `false` when replacing an existing key.

**Example:**

```cpp
app.images().registerImage("avatar", "assets/avatar.png");
```

### **removeImage**
---

#### `bool removeImage(std::string_view key)`

- **Returns:** `bool`
- **Arguments:** `key` registered image key.

Removes an image registration and retires the GPU resource safely. Existing `TextureRef` values for that key should be treated as invalid after removal.

**Example:**

```cpp
const bool removed = app.images().removeImage("avatar");
```

### **contains**
---

#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` image key to test.

Checks whether an image key is currently registered. This performs no file IO or GPU work.

**Example:**

```cpp
if (!app.images().contains("avatar")) { app.images().registerImage("avatar", "assets/avatar.png"); }
```

### **getTexture**
---

#### `TextureRef getTexture(std::string_view key) const`

- **Returns:** `TextureRef`
- **Arguments:** `key` registered image key.

Returns the texture reference for a registered image. Missing keys return fallback texture id `0` and log a warning once for that key.

**Example:**

```cpp
FlowUi::TextureRef avatar = app.images().getTexture("avatar");
```

## FlowUi::IconManager

### **registerSvg**
---

#### `bool registerSvg(std::string_view key, std::string_view svgSource)`

- **Returns:** `bool`
- **Arguments:** `key` icon key, `svgSource` complete SVG source text.

Parses and registers an SVG document from memory. Raster variants are created lazily later when the icon is drawn at a particular size.

**Example:**

```cpp
app.icons().registerSvg("check", checkSvgSource);
```

### **registerFromFile**
---

#### `bool registerFromFile(std::string_view key, std::string_view filePath)`

- **Returns:** `bool`
- **Arguments:** `key` icon key, `filePath` SVG file path.

Parses and registers an SVG document from disk. Returns `false` if the key already exists and the current icon is left unchanged.

**Example:**

```cpp
app.icons().registerFromFile("save", "assets/icons/save.svg");
```

### **remove**
---

#### `bool remove(std::string_view key)`

- **Returns:** `bool`
- **Arguments:** `key` registered icon key.

Removes a registered SVG document and its cached atlas variants. Previously returned texture references for the key should be discarded.

**Example:**

```cpp
const bool removed = app.icons().remove("save");
```

### **contains**
---

#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` icon key to test.

Checks whether an SVG document key is registered. This does not force rasterization or atlas allocation.

**Example:**

```cpp
if (!app.icons().contains("save")) { app.icons().registerFromFile("save", "assets/icons/save.svg"); }
```

### **textureRef**
---

#### `TextureRef textureRef(std::string_view key)`

- **Returns:** `TextureRef`
- **Arguments:** `key` registered icon key.

Returns a texture request reference for a registered icon. FlowUi later resolves the request into a cached atlas variant sized to the rendered UI image area.

**Example:**

```cpp
FlowUi::TextureRef saveIcon = app.icons().textureRef("save");
```

## FlowUi::InputFieldManager

### **requestField**
---

#### `FieldQueryResult requestField(const FieldRequest& request)`

- **Returns:** `FieldQueryResult`
- **Arguments:** `request` stable field id, initial text, config, and Clay element ids.

Registers or updates an input field for the current frame and returns its current manager-owned state. Call this once per frame from the element that draws the editable field.

**Example:**

```cpp
FlowUi::FieldQueryResult field = context.uiManager.inputFields().requestField({.fieldId = context.elementID, .initialText = "Search", .textElementId = textId, .contentElementId = contentId});
```

### **requestCaret**
---

#### `void requestCaret(std::string_view fieldId, CaretRequestKind kind)`

- **Returns:** `void`
- **Arguments:** `fieldId` field to focus or edit, `kind` requested caret operation.

Requests focus or caret changes for an input field. `SetPrimary` is the common operation for clicked fields, while `ClearAll` removes text focus globally.

**Example:**

```cpp
context.uiManager.inputFields().requestCaret(context.elementID, FlowUi::CaretRequestKind::SetPrimary);
```

### **removeField**
---

#### `bool removeField(std::string_view fieldId)`

- **Returns:** `bool`
- **Arguments:** `fieldId` field state to remove.

Deletes stored text, config, caret, and selection state for one field. Use this when a dynamic field is removed or when external state should replace the edited text.

**Example:**

```cpp
const bool removed = app.ui().inputFields().removeField("settings/name");
```

### **clear**
---

#### `void clear()`

- **Returns:** `void`
- **Arguments:** none.

Clears all managed input field state. This resets fields, focus, key repeat, pointer drag, and frame render overrides.

**Example:**

```cpp
app.ui().inputFields().clear();
```

### **hasPrimaryFieldFocus**
---

#### `bool hasPrimaryFieldFocus() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether any input field currently owns primary text focus. This is useful for suppressing global shortcuts while the user is editing text.

**Example:**

```cpp
if (!app.ui().inputFields().hasPrimaryFieldFocus()) { runGlobalShortcut(); }
```

### **getSelectedText**
---

#### `std::string_view getSelectedText() const`

- **Returns:** `std::string_view`
- **Arguments:** none.

Returns selected text from the primary field, or an empty view when no selection exists. The view points into manager-owned storage and is invalidated when that field text changes or is removed.

**Example:**

```cpp
std::string selected(app.ui().inputFields().getSelectedText());
```

### **insertTextAtPrimaryCaret**
---

#### `bool insertTextAtPrimaryCaret(std::string_view utf8Text)`

- **Returns:** `bool`
- **Arguments:** `utf8Text` text to insert.

Inserts UTF-8 text at the primary caret, replacing active selections. The operation respects read-only state and `FieldConfig::maxBytes`.

**Example:**

```cpp
const bool pasted = app.ui().inputFields().insertTextAtPrimaryCaret(app.ui().clipboardText());
```

## FlowUi::ShortcutManager

### **registerShortcut**
---

#### `ShortcutId registerShortcut(const ShortcutChord& chord, ShortcutScope scope, int32_t priority, ShortcutCallback callback)`

- **Returns:** `ShortcutId`
- **Arguments:** `chord` key/modifier/trigger match, `scope` eligibility scope, `priority` ordering value, `callback` handler.

Registers a keyboard shortcut and returns an opaque id. Matching callbacks run by scope and priority, and a callback returning `true` stops later handlers for the same chord.

**Example:**

```cpp
FlowUi::ShortcutId saveShortcut = app.ui().shortcuts().registerShortcut({.key = GLFW_KEY_S, .ctrl = true}, FlowUi::ShortcutScope::Global, 100, saveCallback);
```

### **unregisterShortcut**
---

#### `bool unregisterShortcut(ShortcutId id)`

- **Returns:** `bool`
- **Arguments:** `id` shortcut id returned by `registerShortcut()`.

Removes a registered shortcut. It is valid to unregister a shortcut from inside a shortcut callback.

**Example:**

```cpp
const bool removed = app.ui().shortcuts().unregisterShortcut(saveShortcut);
```

### **clear**
---

#### `void clear()`

- **Returns:** `void`
- **Arguments:** none.

Removes every registered shortcut and resets focused-element shortcut state. Use this for app-level shortcut reloads or teardown.

**Example:**

```cpp
app.ui().shortcuts().clear();
```

### **setFocusedElement**
---

#### `void setFocusedElement(Clay_ElementId elementId)`

- **Returns:** `void`
- **Arguments:** `elementId` Clay element id to treat as shortcut-focused.

Sets the focused element marker used by `ShortcutScope::FocusedElement`. Application or element code decides when an element should become shortcut-focused.

**Example:**

```cpp
app.ui().shortcuts().setFocusedElement(context.uiManager.toClayEID(context.elementID));
```

### **clearFocusedElement**
---

#### `void clearFocusedElement()`

- **Returns:** `void`
- **Arguments:** none.

Clears the focused element marker. After this, focused-element shortcuts are not eligible until another element id is set.

**Example:**

```cpp
app.ui().shortcuts().clearFocusedElement();
```

### **focusedElement**
---

#### `Clay_ElementId focusedElement() const`

- **Returns:** `Clay_ElementId`
- **Arguments:** none.

Returns the currently focused Clay element id for shortcut dispatch. A zero id means no shortcut-focused element is active.

**Example:**

```cpp
Clay_ElementId focused = app.ui().shortcuts().focusedElement();
```

## FlowUi::ViewPort

### **getKey**
---

#### `std::string_view getKey() const`

- **Returns:** `std::string_view`
- **Arguments:** none.

Returns the stable key used to create and look up this viewport. The key is owned by the viewport object.

**Example:**

```cpp
std::string_view viewportKey = viewport->getKey();
```

### **hasValidSize**
---

#### `bool hasValidSize() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the viewport currently has positive width and height. A viewport may be invalid before its texture has been referenced and sized by UI image commands.

**Example:**

```cpp
if (viewport->hasValidSize()) { renderScene(viewport->getSize()); }
```

### **getSize**
---

#### `VkExtent2D getSize() const`

- **Returns:** `VkExtent2D`
- **Arguments:** none.

Returns the current render target size in pixels. FlowUi derives this from the largest UI image area using the viewport texture.

**Example:**

```cpp
VkExtent2D extent = viewport->getSize();
```

### **textureRef**
---

#### `TextureRef textureRef() const`

- **Returns:** `TextureRef`
- **Arguments:** none.

Returns a texture reference for this viewport's current frame image. `ViewPortManager::getTexture()` is usually more convenient when drawing by key.

**Example:**

```cpp
FlowUi::TextureRef sceneTexture = viewport->textureRef();
```

### **setRenderCallback** `1/2`
---

#### `void setRenderCallback(RenderCallback callback)`

- **Returns:** `void`
- **Arguments:** `callback` function invoked for viewport rendering.

Installs a custom render callback for this viewport. FlowUi begins and ends the provided secondary command buffer, so callback code records commands but does not begin or end the buffer.

**Example:**

```cpp
viewport->setRenderCallback([](const FlowUi::ViewPortRenderContext& ctx) { recordSceneCommands(ctx); });
```

### **setRenderCallback** `2/2`
---

#### `template <typename T, typename Fn> void setRenderCallback(std::shared_ptr<T> userData, Fn&& callback)`

- **Returns:** `void`
- **Arguments:** `userData` retained callback payload, `callback` callable invoked with render context and `T&`.

Installs a render callback that keeps typed shared user data alive. This is useful for binding scene renderers or other stateful rendering helpers to a viewport.

**Example:**

```cpp
viewport->setRenderCallback([](const FlowUi::ViewPortRenderContext& ctx) { recordSceneCommands(ctx); });
```

### **clearRenderCallback**
---

#### `void clearRenderCallback()`

- **Returns:** `void`
- **Arguments:** none.

Clears the viewport render callback. FlowUi continues to manage the viewport texture, but no custom draw commands are recorded.

**Example:**

```cpp
viewport->clearRenderCallback();
```

### **hasRenderCallback**
---

#### `bool hasRenderCallback() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether a render callback is currently installed. Use this to avoid redundant setup or to display fallback content.

**Example:**

```cpp
if (!viewport->hasRenderCallback()) { viewport->setRenderCallback(renderScene); }
```

### **setClearColor**
---

#### `void setClearColor(float r, float g, float b, float a)`

- **Returns:** `void`
- **Arguments:** `r`, `g`, `b`, `a` clear color channels.

Sets the viewport clear color. The color is used by the viewport render pass when clearing is enabled.

**Example:**

```cpp
viewport->setClearColor(0.02f, 0.02f, 0.03f, 1.0f);
```

### **clearColor**
---

#### `std::array<float, 4> clearColor() const`

- **Returns:** `std::array<float, 4>`
- **Arguments:** none.

Returns the current viewport clear color. The values are RGBA channels.

**Example:**

```cpp
std::array<float, 4> color = viewport->clearColor();
```

### **setClearEveryFrame**
---

#### `void setClearEveryFrame(bool enabled)`

- **Returns:** `void`
- **Arguments:** `enabled` true to clear each rendered frame.

Controls whether FlowUi clears the viewport image every frame. Disabling clear can be useful for persistent render target effects.

**Example:**

```cpp
viewport->setClearEveryFrame(false);
```

### **clearEveryFrame**
---

#### `bool clearEveryFrame() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the viewport clears before rendering each frame. When false, FlowUi loads previous image contents after initialization.

**Example:**

```cpp
const bool clears = viewport->clearEveryFrame();
```

## FlowUi::ViewPortManager

### **create**
---

#### `bool create(std::string_view key, const ViewPortCreateInfo& createInfo = {})`

- **Returns:** `bool`
- **Arguments:** `key` viewport key, `createInfo` initial format and clear settings.

Creates a named offscreen viewport. Newly created viewports resize automatically when their texture is referenced by UI image commands.

**Example:**

```cpp
app.viewPorts().create("scene", {.clearColor = {0.02f, 0.02f, 0.03f, 1.0f}});
```

### **remove**
---

#### `bool remove(std::string_view key)`

- **Returns:** `bool`
- **Arguments:** `key` viewport key.

Removes a viewport and destroys its per-frame resources. This can block because the implementation waits for the Vulkan device to become idle before releasing resources.

**Example:**

```cpp
const bool removed = app.viewPorts().remove("scene");
```

### **contains**
---

#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` viewport key.

Checks whether a viewport exists for the key. This is a lightweight lookup with no rendering side effects.

**Example:**

```cpp
if (!app.viewPorts().contains("scene")) { app.viewPorts().create("scene"); }
```

### **getViewPort** `1/2`
---

#### `ViewPort* getViewPort(std::string_view key)`

- **Returns:** `ViewPort*`
- **Arguments:** `key` viewport key.

Returns a mutable viewport pointer, or `nullptr` when missing. Use this to set callbacks, clear color, or persistent clear behavior.

**Example:**

```cpp
FlowUi::ViewPort* scene = app.viewPorts().getViewPort("scene");
```

### **getViewPort** `2/2`
---

#### `const ViewPort* getViewPort(std::string_view key) const`

- **Returns:** `const ViewPort*`
- **Arguments:** `key` viewport key.

Returns an immutable viewport pointer, or `nullptr` when missing. Use this for read-only viewport inspection.

**Example:**

```cpp
FlowUi::ViewPort* scene = app.viewPorts().getViewPort("scene");
```

### **getTexture**
---

#### `TextureRef getTexture(std::string_view key) const`

- **Returns:** `TextureRef`
- **Arguments:** `key` viewport key.

Returns a texture reference for the current frame's viewport image. Missing keys return fallback texture id `0` and log a warning once.

**Example:**

```cpp
FlowUi::TextureRef sceneTexture = app.viewPorts().getTexture("scene");
```

### **getVulkanInterop**
---

#### `const ViewPortVulkanInterop& getVulkanInterop() const`

- **Returns:** `const ViewPortVulkanInterop&`
- **Arguments:** none.

Returns shared Vulkan handles owned by the FlowUi app. Use these handles only to create compatible resources or record viewport work; do not destroy them.

**Example:**

```cpp
const FlowUi::ViewPortVulkanInterop& vk = app.viewPorts().getVulkanInterop();
```

## FlowUi::ElementBuilder

### **ElementBuilder**
---

#### `ElementBuilder(UiManager& uiManager, const DefinitionType* definition, std::string elementID)`

- **Returns:** `ElementBuilder`
- **Arguments:** `uiManager` active UI manager, `definition` element definition pointer, `elementID` stable element instance id.

Constructs a builder for one element invocation. User code normally gets builders from `UiManager::createElement()` rather than calling this constructor directly.

**Example:**

```cpp
auto builder = app.ui().createElement(kButton, "toolbar/save");
```

### **setParameters** `1/2`
---

#### `ElementBuilder& setParameters(const ParametersType& parameters)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `parameters` parameter values to copy.

Copies parameter values into the builder. The stored parameters are passed to interaction, logic, construct, and build callbacks.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").setParameters(ButtonParams{.label = "Save"}).draw();
```

### **setParameters** `2/2`
---

#### `ElementBuilder& setParameters(ParametersType&& parameters)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `parameters` parameter values to move.

Moves parameter values into the builder. Use this when parameter construction is dynamic or owns heavier data.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").setParameters(ButtonParams{.label = "Save"}).draw();
```

### **mergeParams**
---

#### `template <typename MergeFn> ElementBuilder& mergeParams(MergeFn&& mergeFn)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `mergeFn` callable invoked with `ParametersType&`.

Mutates the builder's existing parameter storage. This is useful when defaults are mostly correct and only a few fields need to be changed.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").mergeParams([](ButtonParams& params) { params.enabled = false; }).draw();
```

### **withElementID**
---

#### `ElementBuilder& withElementID(std::string_view elementID)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `elementID` replacement element id string.

Replaces the id stored by the builder. Use it when a builder is created before the final stable id is known.

**Example:**

```cpp
buttonBuilder.withElementID("toolbar/save-secondary").draw();
```

### **setDevInternalCapture**
---

#### `ElementBuilder& setDevInternalCapture(bool isDevInternal = true)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `isDevInternal` whether dev capture should mark this invocation internal.

Controls how this element invocation is captured in developer mode. Normal user elements usually do not need this.

**Example:**

```cpp
app.ui().createElement(kDevPanel, "flowui/dev/panel").setDevInternalCapture(true).draw();
```

### **construct**
---

#### `void construct(ElementDrawOptions options = ElementDrawOptions::Default)`

- **Returns:** `void`
- **Arguments:** `options` callback phases to skip.

Runs enabled callbacks and opens a constructed Clay root using the definition's `constructElement` callback. Emit child nodes after this call, then close the element with `UiManager::drawConstructed()`.

**Example:**

```cpp
app.ui().createElement(kPanel, "settings").construct();
```

### **draw**
---

#### `void draw(ElementDrawOptions options = ElementDrawOptions::Default)`

- **Returns:** `void`
- **Arguments:** `options` callback phases to skip.

Runs enabled callbacks and emits the element through its `buildElement` callback. This is the most common final call for a FlowUi element builder.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").draw();
```

## FlowUi::InteractionSnapshot

### **contains**
---

#### `static bool contains(const std::vector<Clay_ElementId>& list, Clay_ElementId id)`

- **Returns:** `bool`
- **Arguments:** `list` interaction id list, `id` Clay element id to find.

Checks whether a Clay element id exists in an interaction list. Comparison uses the underlying Clay id value.

**Example:**

```cpp
const bool hasButton = FlowUi::InteractionSnapshot::contains(snapshot.pressedElementIds, buttonId);
```

### **isHovered**
---

#### `bool isHovered(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element was hovered in this snapshot. Element callbacks commonly use this for child interaction checks.

**Example:**

```cpp
const bool hovered = context.previousInteraction.isHovered(rootId);
```

### **isPressed**
---

#### `bool isPressed(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element received a primary pointer press in this snapshot. This is based on the previous completed frame when used from element callbacks.

**Example:**

```cpp
const bool pressed = context.previousInteraction.isPressed(rootId);
```

### **isHeld**
---

#### `bool isHeld(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element was held by the primary pointer in this snapshot. Use it for drag-like or continuous pressed behavior.

**Example:**

```cpp
const bool held = context.previousInteraction.isHeld(rootId);
```

### **isReleased**
---

#### `bool isReleased(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element received a primary pointer release in this snapshot. Use it for release-triggered actions.

**Example:**

```cpp
const bool released = context.previousInteraction.isReleased(rootId);
```

## FlowUi::ElementBuildContext

### **createChildElementId**
---

#### `std::string createChildElementId(std::string_view localChildId) const`

- **Returns:** `std::string`
- **Arguments:** `localChildId` child id segment or relative child path.

Creates a stable child id by appending the local child id to the current element id. Use this for child Clay nodes or nested Flow elements owned by the current element.

**Example:**

```cpp
Clay_ElementId labelId = context.uiManager.toClayEID(context.createChildElementId("label"));
```

## FlowUi::ElementInteractionContext

### **createChildElementId**
---

#### `std::string createChildElementId(std::string_view localChildId) const`

- **Returns:** `std::string`
- **Arguments:** `localChildId` child id segment or relative child path.

Creates a stable child id from inside interaction or logic callbacks. This is useful when querying previous interaction for child elements.

**Example:**

```cpp
Clay_ElementId labelId = context.uiManager.toClayEID(context.createChildElementId("label"));
```

## FlowUi::ElementDefinition

### **getResources**
---

#### `static ResourcesType& getResources(App& app)`

- **Returns:** `ResourcesType&`
- **Arguments:** `app` active FlowUi app used for resource construction.

Lazily creates and returns the shared resources instance for this element definition specialization. Available only when the `Resources` template argument is not `void`.

**Example:**

```cpp
ButtonResources& resources = ButtonDefinition::getResources(app);
```

### **getOrCreateState**
---

#### `static StateType& getOrCreateState(uint64_t elementFlowId)`

- **Returns:** `StateType&`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Returns existing state for an element instance or creates default state when missing. Available only when the `State` template argument is not `void`.

**Example:**

```cpp
ButtonState& state = ButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
```

### **tryGetState**
---

#### `static StateType* tryGetState(uint64_t elementFlowId)`

- **Returns:** `StateType*`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Looks up mutable state without creating it. Returns `nullptr` when the element instance has no stored state.

**Example:**

```cpp
if (ButtonState* state = ButtonDefinition::tryGetState(FLOW_ID("toolbar/save"))) { state->pressed = false; }
```

### **tryGetStateConst**
---

#### `static const StateType* tryGetStateConst(uint64_t elementFlowId)`

- **Returns:** `const StateType*`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Looks up immutable state without creating it. Use this for read-only checks outside element callbacks.

**Example:**

```cpp
const ButtonState* state = ButtonDefinition::tryGetStateConst(FLOW_ID("toolbar/save"));
```

### **eraseState**
---

#### `static bool eraseState(uint64_t elementFlowId)`

- **Returns:** `bool`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Erases stored state for one element instance. FlowUi does not automatically garbage-collect custom element state, so dynamic UI can use this to keep state pools bounded.

**Example:**

```cpp
const bool erased = ButtonDefinition::eraseState(FLOW_ID("toolbar/save"));
```

## FlowUi::Font::FontVariantData

### **kerningKey**
---

#### `static uint64_t kerningKey(uint32_t leftCodepoint, uint32_t rightCodepoint)`

- **Returns:** `uint64_t`
- **Arguments:** `leftCodepoint` left glyph codepoint, `rightCodepoint` right glyph codepoint.

Packs two codepoints into the key used by the kerning lookup table. This is mainly useful when inspecting or extending font metric data.

**Example:**

```cpp
uint64_t key = FlowUi::Font::FontVariantData::kerningKey(U'A', U'V');
```

### **kerningAdvance**
---

#### `float kerningAdvance(uint32_t leftCodepoint, uint32_t rightCodepoint) const`

- **Returns:** `float`
- **Arguments:** `leftCodepoint` left glyph codepoint, `rightCodepoint` right glyph codepoint.

Returns kerning advance for a codepoint pair. Missing pairs return `0.0f`.

**Example:**

```cpp
float advance = variant.kerningAdvance(U'A', U'V');
```

## FlowUi::Font::FontFaceData

### **defaultVariant**
---

#### `const FontVariantData* defaultVariant() const`

- **Returns:** `const FontVariantData*`
- **Arguments:** none.

Returns the default baked variant for a loaded font face. Returns `nullptr` when the face has no variants or the default index is invalid.

**Example:**

```cpp
const FlowUi::Font::FontVariantData* variant = face->defaultVariant();
```
