# Viewport Manager API

## Enums

### **TextureFitMode**


#### `enum class TextureFitMode : uint8_t`

Texture layout mode used when drawing viewport textures into UI rectangles. Viewports often use contain or cover depending on preview behavior.

## Public Structs

### **TextureRef**


#### `struct TextureRef`

Texture reference returned for viewport images. It points at the current frame viewport target or fallback texture id 0 when unavailable.

### **ViewPortVulkanInterop**


#### `struct ViewPortVulkanInterop`

Shared Vulkan handles exposed for viewport integrations. The handles are owned by FlowUi App and must not be destroyed by user code.

### **ViewPortRenderContext**


#### `struct ViewPortRenderContext`

Per-frame context passed to viewport render callbacks. It contains the begun secondary command buffer, extent, color format, frame index, key, and interop handles.

### **ViewPortCreateInfo**


#### `struct ViewPortCreateInfo`

Creation settings for a viewport render target. It controls color format, clear color, and whether the target clears every frame.

### **ViewPort**


#### `class ViewPort`

Named offscreen render target managed by ViewPortManager. It stores render callback, clear behavior, current size, key, and texture access.

### **ViewPortManager**


#### `class ViewPortManager`

App-owned manager for named offscreen viewports. It creates, tracks, sizes, renders, and exposes viewport textures to the UI renderer.

## Public API

### **getKey**


#### `std::string_view getKey() const`

- **Returns:** `std::string_view`
- **Arguments:** none.

Returns the stable key used to create and look up this viewport. The key is owned by the viewport object.

**Example:**

```cpp
std::string_view viewportKey = viewport->getKey();
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **hasValidSize**


#### `bool hasValidSize() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the viewport currently has positive width and height. A viewport may be invalid before its texture has been referenced and sized by UI image commands.

**Example:**

```cpp
if (viewport->hasValidSize()) { renderScene(viewport->getSize()); }
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getSize**


#### `VkExtent2D getSize() const`

- **Returns:** `VkExtent2D`
- **Arguments:** none.

Returns the current render target size in pixels. FlowUi derives this from the largest UI image area using the viewport texture.

**Example:**

```cpp
VkExtent2D extent = viewport->getSize();
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **textureRef**


#### `TextureRef textureRef() const`

- **Returns:** `TextureRef`
- **Arguments:** none.

Returns a texture reference for this viewport's current frame image. `ViewPortManager::getTexture()` is usually more convenient when drawing by key.

**Example:**

```cpp
FlowUi::TextureRef sceneTexture = viewport->textureRef();
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **setRenderCallback** `1/2`


#### `void setRenderCallback(RenderCallback callback)`

- **Returns:** `void`
- **Arguments:** `callback` function invoked for viewport rendering.

Installs a custom render callback for this viewport. FlowUi begins and ends the provided secondary command buffer, so callback code records commands but does not begin or end the buffer.

**Example:**

```cpp
viewport->setRenderCallback([](const FlowUi::ViewPortRenderContext& ctx) { recordSceneCommands(ctx); });
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **setRenderCallback** `2/2`


#### `template <typename T, typename Fn> void setRenderCallback(std::shared_ptr<T> userData, Fn&& callback)`

- **Returns:** `void`
- **Arguments:** `userData` retained callback payload, `callback` callable invoked with render context and `T&`.

Installs a render callback that keeps typed shared user data alive. This is useful for binding scene renderers or other stateful rendering helpers to a viewport.

**Example:**

```cpp
viewport->setRenderCallback([](const FlowUi::ViewPortRenderContext& ctx) { recordSceneCommands(ctx); });
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **clearRenderCallback**


#### `void clearRenderCallback()`

- **Returns:** `void`
- **Arguments:** none.

Clears the viewport render callback. FlowUi continues to manage the viewport texture, but no custom draw commands are recorded.

**Example:**

```cpp
viewport->clearRenderCallback();
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **hasRenderCallback**


#### `bool hasRenderCallback() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether a render callback is currently installed. Use this to avoid redundant setup or to display fallback content.

**Example:**

```cpp
if (!viewport->hasRenderCallback()) { viewport->setRenderCallback(renderScene); }
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **setClearColor**


#### `void setClearColor(float r, float g, float b, float a)`

- **Returns:** `void`
- **Arguments:** `r`, `g`, `b`, `a` clear color channels.

Sets the viewport clear color. The color is used by the viewport render pass when clearing is enabled.

**Example:**

```cpp
viewport->setClearColor(0.02f, 0.02f, 0.03f, 1.0f);
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **clearColor**


#### `std::array<float, 4> clearColor() const`

- **Returns:** `std::array<float, 4>`
- **Arguments:** none.

Returns the current viewport clear color. The values are RGBA channels.

**Example:**

```cpp
std::array<float, 4> color = viewport->clearColor();
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **setClearEveryFrame**


#### `void setClearEveryFrame(bool enabled)`

- **Returns:** `void`
- **Arguments:** `enabled` true to clear each rendered frame.

Controls whether FlowUi clears the viewport image every frame. Disabling clear can be useful for persistent render target effects.

**Example:**

```cpp
viewport->setClearEveryFrame(false);
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **clearEveryFrame**


#### `bool clearEveryFrame() const`

- **Returns:** `bool`
- **Arguments:** none.

Reports whether the viewport clears before rendering each frame. When false, FlowUi loads previous image contents after initialization.

**Example:**

```cpp
const bool clears = viewport->clearEveryFrame();
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **create**


#### `Result<bool> create(std::string_view key, const ViewPortCreateInfo& createInfo = {})`

- **Returns:** `Result<bool>`; check success before reading the value, and use `.error()` on failure.
- **Arguments:** `key` viewport key, `createInfo` initial format and clear settings.

Creates a named offscreen viewport. Newly created viewports resize automatically when their texture is referenced by UI image commands.

**Example:**

```cpp
const auto registered = app.viewPorts().create("scene", {.clearColor = {0.02f, 0.02f, 0.03f, 1.0f}});
if (!registered) {
    throw FlowUi::FlowUiException(registered.error());
}
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **remove**


#### `Result<bool> remove(std::string_view key)`

- **Returns:** `Result<bool>`; check success before reading the value, and use `.error()` on failure.
- **Arguments:** `key` viewport key.

Removes a viewport and retires its per-frame resources after their recorded submissions complete. The successful result contains `false` when the key is absent.

**Example:**

```cpp
const auto removal = app.viewPorts().remove("scene");
if (!removal) {
    throw FlowUi::FlowUiException(removal.error());
}
const bool removed = *removal;
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **contains**


#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` viewport key.

Checks whether a viewport exists for the key. This is a lightweight lookup with no rendering side effects.

**Example:**

```cpp
if (!app.viewPorts().contains("scene")) {
    const auto created = app.viewPorts().create("scene");
    if (!created) {
        throw FlowUi::FlowUiException(created.error());
    }
}
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getViewPort** `1/2`


#### `ViewPort* getViewPort(std::string_view key)`

- **Returns:** `ViewPort*`
- **Arguments:** `key` viewport key.

Returns a mutable viewport pointer, or `nullptr` when missing. Use this to set callbacks, clear color, or persistent clear behavior.

**Example:**

```cpp
FlowUi::ViewPort* scene = app.viewPorts().getViewPort("scene");
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getViewPort** `2/2`


#### `const ViewPort* getViewPort(std::string_view key) const`

- **Returns:** `const ViewPort*`
- **Arguments:** `key` viewport key.

Returns an immutable viewport pointer, or `nullptr` when missing. Use this for read-only viewport inspection.

**Example:**

```cpp
FlowUi::ViewPort* scene = app.viewPorts().getViewPort("scene");
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getTexture**


#### `TextureRef getTexture(std::string_view key) const`

- **Returns:** `TextureRef`
- **Arguments:** `key` viewport key.

Returns a texture reference for the current frame's viewport image. Missing keys return fallback texture id `0` and log a warning once.

**Example:**

```cpp
FlowUi::TextureRef sceneTexture = app.viewPorts().getTexture("scene");
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getVulkanInterop**


#### `const ViewPortVulkanInterop& getVulkanInterop() const`

- **Returns:** `const ViewPortVulkanInterop&`
- **Arguments:** none.

Returns shared Vulkan handles owned by the FlowUi app. Use these handles only to create compatible resources or record viewport work; do not destroy them.

**Example:**

```cpp
const FlowUi::ViewPortVulkanInterop& vk = app.viewPorts().getVulkanInterop();
```

See: [Owning header](../../include/FlowUi/Public/managers/ViewPortManager.hpp).
