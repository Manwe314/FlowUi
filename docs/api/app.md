# App API

## Aliases

### **FlowElementId**
---

#### `using FlowElementId = uint64_t`

Stable hashed id for a FlowUi element instance. It is used to key per-instance element state and generated child ids.

### **FlowDefinitionId**
---

#### `using FlowDefinitionId = uint64_t`

Stable hashed id for a FlowUi element definition. It identifies the definition type rather than one rendered instance.

## Public Structs

### **AppConfig**
---

#### `struct AppConfig`

Top-level configuration passed to makeApplication. It groups window, Vulkan, UI, icon manager, and developer tooling settings.

### **WindowInputConfig**
---

#### `struct WindowInputConfig`

Low-level window input behavior shared by app creation and runtime input updates. It controls cursor mode, sticky input, lock modifiers, and raw mouse motion.

### **TextureRef**
---

#### `struct TextureRef`

Renderer texture handle and draw options returned by image, icon, and viewport managers. App code can adjust fit, sampling, and tint fields while leaving manager-owned id and UV fields alone.

## Public API

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
