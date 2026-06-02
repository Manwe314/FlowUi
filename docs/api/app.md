# App API

## Aliases

### **FlowElementId**


#### `using FlowElementId = uint64_t`

Stable hashed id for a FlowUi element instance. It is used to key per-instance element state and generated child ids.

### **FlowDefinitionId**


#### `using FlowDefinitionId = uint64_t`

Stable hashed id for a FlowUi element definition. It identifies the definition type rather than one rendered instance.

## Public Structs

### **AppConfig**


#### `struct AppConfig`

Top-level configuration passed to makeApplication. It groups window, Vulkan, UI, icon manager, and developer tooling settings.

### **WindowInputConfig**


#### `struct WindowInputConfig`

Low-level window input behavior shared by app creation and runtime input updates. It controls cursor mode, sticky input, lock modifiers, and raw mouse motion.

### **TextureRef**


#### `struct TextureRef`

Renderer texture handle and draw options returned by image, icon, and viewport managers. App code can adjust fit, sampling, and tint fields while leaving manager-owned id and UV fields alone.

## Public API

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
