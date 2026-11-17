# Viewport Manager API

[Public declarations](include/FlowUi/Public/managers/ViewPortManager.hpp) · [Vulkan interop values](include/FlowUi/Public/interop/vulkan/Viewport.hpp)

This surface requires `FLOWUI_PUBLIC_VULKAN_INTEROP`. Viewports are window-local offscreen render targets with one image per frame slot. New targets start at 1x1; UI image usage drives resizing from the largest displayed area in framebuffer pixels. A callback runs only for frames that reference its viewport texture. Reacquire `TextureRef` each frame.

FlowUi owns the Vulkan instance/device/queue, viewport targets, and the secondary command buffer. A render callback records compatible commands into the already-begun buffer; FlowUi ends/submits it and controls the rendering pass and image transitions. User resources and borrowed captures must outlive all callbacks using them.

## Acquire the manager

`App` getters return `Result<std::reference_wrapper<T>>`. Check the wrapper, then name the borrowed object as a normal reference:

**Example:**

```cpp
auto result = app.viewPorts();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ViewPortManager& manager = result->get();
```

The reference is borrowed; the App must outlive it, and window-owned managers must also stay within their owning window lifetime. The examples use an already acquired manager reference and existing field/window identities where needed. They choose an application policy of throwing `FlowUiException` after a failed Result; returning/reporting the error is equally possible. Include the relevant manager/value headers and `FlowUi/Public/errorManagement/Exception.hpp`.

ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

For `Result<bool>`, `if (!result)` tests operational failure; `*result` is the separate operation outcome. Never treat a successful `false` as an error.

## ViewPort

### **getKey**

#### `[[nodiscard]] std::string_view getKey() const noexcept`

- **Returns:** `std::string_view`
- **Arguments:** none.

Borrow this viewport's registration key. Returns a view of the manager-owned key, or an empty view when detached.

The view is invalid after removal or manager/window destruction.

**Example:**

```cpp
const auto key = viewport.getKey();
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **hasValidSize**

#### `[[nodiscard]] bool hasValidSize() const noexcept`

- **Returns:** `bool`
- **Arguments:** none.

Test whether the current render target has nonzero width and height. Returns false when detached or unsized.

A newly created viewport normally starts at 1x1; later sizing follows UI image usage.

**Example:**

```cpp
const auto validSize = viewport.hasValidSize();
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getSize**

#### `[[nodiscard]] VkExtent2D getSize() const noexcept`

- **Returns:** `VkExtent2D`
- **Arguments:** none.

Read the current viewport render extent in framebuffer pixels. Returns VkExtent2D; detached/unsized state may contain zero dimensions.

The largest UI image area referencing this viewport drives resizing, with framebuffer scale and configured hysteresis.

**Example:**

```cpp
const auto size = viewport.getSize();
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **textureRef**

#### `[[nodiscard]] TextureRef textureRef() const noexcept`

- **Returns:** `TextureRef`
- **Arguments:** none.

Read this viewport's current frame-slot TextureRef. Returns an unavailable TextureRef when detached.

Reacquire per frame and after resizing; ViewPortManager::getTexture() is normally more convenient for keyed UI emission.

**Example:**

```cpp
const auto texture = viewport.textureRef();
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **setRenderCallback**

#### `[[nodiscard]] Status setRenderCallback(const RenderCallback& callback)`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `callback` retained callable

Prepare and publish an owned viewport render callback. Status reports detached/phase/configuration validation failures without replacing the previous callback. An empty RenderCallback disables rendering.

FlowUi begins/ends the provided secondary command buffer and owns the surrounding pass/transitions. Record commands only; do not begin/end/submit it or destroy App-owned handles. The generic overload constructs an owned std::function before publication. The shared_ptr overload retains its payload until the callback is replaced/cleared and rejects null payloads.

Exceptions: Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.

**Example:**

```cpp
const FlowUi::ViewPort::RenderCallback callback = [](const FlowUi::ViewPortRenderContext& context) {
    VkViewport dynamicViewport{};
    dynamicViewport.width = static_cast<float>(context.extent.width);
    dynamicViewport.height = static_cast<float>(context.extent.height);
    dynamicViewport.maxDepth = 1.0f;
    vkCmdSetViewport(context.commandBuffer, 0, 1, &dynamicViewport);
};
auto result = viewport.setRenderCallback(callback);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **setRenderCallback**

#### `template <typename Callback> requires (!std::is_same_v<std::remove_cvref_t<Callback>, RenderCallback>) [[nodiscard]] Status setRenderCallback(Callback&& callback)`

- **Returns:** `requires (construction)`
- **Arguments:** none.

Prepare and publish an owned viewport render callback. Status reports detached/phase/configuration validation failures without replacing the previous callback. An empty RenderCallback disables rendering.

FlowUi begins/ends the provided secondary command buffer and owns the surrounding pass/transitions. Record commands only; do not begin/end/submit it or destroy App-owned handles. The generic overload constructs an owned std::function before publication. The shared_ptr overload retains its payload until the callback is replaced/cleared and rejects null payloads.

Exceptions: Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.

**Example:**

```cpp
auto result = viewport.setRenderCallback([](const FlowUi::ViewPortRenderContext& context) {
    VkViewport dynamicViewport{};
    dynamicViewport.width = static_cast<float>(context.extent.width);
    dynamicViewport.height = static_cast<float>(context.extent.height);
    dynamicViewport.maxDepth = 1.0f;
    vkCmdSetViewport(context.commandBuffer, 0, 1, &dynamicViewport);
});
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **setRenderCallback**

#### `template <typename T, typename Fn> [[nodiscard]] Status setRenderCallback(std::shared_ptr<T> userData, Fn&& callback)`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `userData` borrowed visitor context, `callback` retained callable

Prepare and publish an owned viewport render callback. Status reports detached/phase/configuration validation failures without replacing the previous callback. An empty RenderCallback disables rendering.

FlowUi begins/ends the provided secondary command buffer and owns the surrounding pass/transitions. Record commands only; do not begin/end/submit it or destroy App-owned handles. The generic overload constructs an owned std::function before publication. The shared_ptr overload retains its payload until the callback is replaced/cleared and rejects null payloads.

Exceptions: Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.

**Example:**

```cpp
auto blendConstants = std::make_shared<std::array<float, 4>>(
    std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f});
auto result = viewport.setRenderCallback(std::move(blendConstants), [](const FlowUi::ViewPortRenderContext& context, std::array<float, 4>& constants) {
    vkCmdSetBlendConstants(context.commandBuffer, constants.data());
});
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **clearRenderCallback**

#### `[[nodiscard]] Status clearRenderCallback()`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** none.

Clear the installed viewport render callback. Status reports detached/phase validation failure without replacing the existing callback.

The texture remains managed and may still be cleared; no custom drawing callback is invoked.

Exceptions: Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.

**Example:**

```cpp
auto result = viewport.clearRenderCallback();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **hasRenderCallback**

#### `[[nodiscard]] bool hasRenderCallback() const noexcept`

- **Returns:** `bool`
- **Arguments:** none.

Test whether this viewport has an installed callback. Returns false when detached or no callback is installed.

This is a direct noexcept observation.

**Example:**

```cpp
const auto hasCallback = viewport.hasRenderCallback();
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **setClearColor**

#### `[[nodiscard]] Status setClearColor(float r, float g, float b, float a)`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `r`, `g`, `b`, `a`

Publish the RGBA viewport clear color. Status reports detached/phase validation failures without changing the color.

The four float channels are stored as provided.

Exceptions: Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.

**Example:**

```cpp
auto result = viewport.setClearColor(0.02f, 0.02f, 0.03f, 1.0f);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **clearColor**

#### `[[nodiscard]] std::array<float, 4> clearColor() const noexcept`

- **Returns:** `std::array<float, 4>`
- **Arguments:** none.

Read the viewport's RGBA clear color. Returns a copied `std::array<float, 4>`; detached state returns zeros.

This is a direct noexcept observation.

**Example:**

```cpp
const auto clearColor = viewport.clearColor();
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **setClearEveryFrame**

#### `[[nodiscard]] Status setClearEveryFrame(bool enabled)`

- **Returns:** `Status`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `enabled`

Choose whether every pass clears the viewport. Status reports detached/phase validation failures without changing the policy.

false preserves image contents where available after the image's required initialization clear.

Exceptions: Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.

**Example:**

```cpp
auto result = viewport.setClearEveryFrame(false);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **clearEveryFrame**

#### `[[nodiscard]] bool clearEveryFrame() const noexcept`

- **Returns:** `bool`
- **Arguments:** none.

Read the viewport clear policy. Returns the stored policy; detached state returns true.

false means previous initialized contents can be loaded.

**Example:**

```cpp
const auto clearsEveryFrame = viewport.clearEveryFrame();
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

## Public API

### **create**

#### `[[nodiscard]] Result<bool> create(std::string_view key, const ViewPortCreateInfo& createInfo = {}) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `createInfo` family or target configuration

Create a window-local viewport and its initial 1x1 render targets. Success currently contains true. A duplicate key returns ViewportKeyCollision, not a successful false value. Invalid configuration/manager/key and allocation/GPU resource failures are errors.

The viewport resizes from the largest referenced UI image area, scaled to framebuffer pixels. colorFormat == VK_FORMAT_UNDEFINED selects VK_FORMAT_R8G8B8A8_UNORM; createInfo defaults to {}.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.create("scene", FlowUi::ViewPortCreateInfo{});
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto created = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **create**

#### `[[nodiscard]] Result<bool> create(ResourceKey key, const ViewPortCreateInfo& createInfo = {}) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `createInfo` family or target configuration

Create a window-local viewport and its initial 1x1 render targets. Success currently contains true. A duplicate key returns ViewportKeyCollision, not a successful false value. Invalid configuration/manager/key and allocation/GPU resource failures are errors.

The viewport resizes from the largest referenced UI image area, scaled to framebuffer pixels. colorFormat == VK_FORMAT_UNDEFINED selects VK_FORMAT_R8G8B8A8_UNORM; createInfo defaults to {}. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "scene"};
auto result = manager.create(key, FlowUi::ViewPortCreateInfo{});
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto created = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **remove**

#### `[[nodiscard]] Result<bool> remove(std::string_view key) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Remove a viewport and retire its per-frame targets. Success contains true if removed or false if absent.

Borrowed ViewPort pointers and their key views become invalid immediately; GPU targets retire after their recorded texture uses complete.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.remove("scene");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto removed = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **remove**

#### `[[nodiscard]] Result<bool> remove(ResourceKey key) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Remove a viewport and retire its per-frame targets. Success contains true if removed or false if absent.

Borrowed ViewPort pointers and their key views become invalid immediately; GPU targets retire after their recorded texture uses complete. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "scene"};
auto result = manager.remove(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto removed = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **contains**

#### `[[nodiscard]] Result<bool> contains(std::string_view key) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Test whether a viewport is registered in this window. Success contains true if present or false if absent.

Key normalization/initialization errors are distinct from an absent key.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.contains("scene");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto registered = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **contains**

#### `[[nodiscard]] Result<bool> contains(ResourceKey key) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Test whether a viewport is registered in this window. Success contains true if present or false if absent.

Key normalization/initialization errors are distinct from an absent key. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "scene"};
auto result = manager.contains(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto registered = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getViewPort**

#### `[[nodiscard]] Result<ViewPort*> getViewPort(std::string_view key) noexcept`

- **Returns:** `Result<ViewPort*>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Borrow a viewport facade by key. Success contains a borrowed pointer, which is nullptr for a missing key. Check both the Result and its contained pointer before dereferencing.

The mutable and const manager overloads return ViewPort* and const ViewPort* respectively. The pointer lasts until that viewport is removed or the manager/window is destroyed; do not retain it across removal/recreation.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.getViewPort("scene");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ViewPort* viewport = *result;
if (viewport != nullptr) {
    const bool sized = viewport->hasValidSize();
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getViewPort**

#### `[[nodiscard]] Result<ViewPort*> getViewPort(ResourceKey key) noexcept`

- **Returns:** `Result<ViewPort*>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Borrow a viewport facade by key. Success contains a borrowed pointer, which is nullptr for a missing key. Check both the Result and its contained pointer before dereferencing.

The mutable and const manager overloads return ViewPort* and const ViewPort* respectively. The pointer lasts until that viewport is removed or the manager/window is destroyed; do not retain it across removal/recreation. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "scene"};
auto result = manager.getViewPort(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ViewPort* viewport = *result;
if (viewport != nullptr) {
    const bool sized = viewport->hasValidSize();
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getViewPort**

#### `[[nodiscard]] Result<const ViewPort*> getViewPort(std::string_view key) const noexcept`

- **Returns:** `Result<const ViewPort*>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Borrow a viewport facade by key. Success contains a borrowed pointer, which is nullptr for a missing key. Check both the Result and its contained pointer before dereferencing.

The mutable and const manager overloads return ViewPort* and const ViewPort* respectively. The pointer lasts until that viewport is removed or the manager/window is destroyed; do not retain it across removal/recreation.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.getViewPort("scene");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::ViewPort* viewport = *result;
if (viewport != nullptr) {
    const bool sized = viewport->hasValidSize();
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getViewPort**

#### `[[nodiscard]] Result<const ViewPort*> getViewPort(ResourceKey key) const noexcept`

- **Returns:** `Result<const ViewPort*>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Borrow a viewport facade by key. Success contains a borrowed pointer, which is nullptr for a missing key. Check both the Result and its contained pointer before dereferencing.

The mutable and const manager overloads return ViewPort* and const ViewPort* respectively. The pointer lasts until that viewport is removed or the manager/window is destroyed; do not retain it across removal/recreation. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "scene"};
auto result = manager.getViewPort(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::ViewPort* viewport = *result;
if (viewport != nullptr) {
    const bool sized = viewport->hasValidSize();
}
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getTexture**

#### `[[nodiscard]] Result<TextureRef> getTexture(std::string_view key) const noexcept`

- **Returns:** `Result<TextureRef>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Acquire the viewport texture reference for the current frame slot. Missing keys are successful unavailable references, with an invalid handle and a once-per-key diagnostic. SkipVisual omits the visual; the fallback policy uses the renderer fallback.

Reacquire each frame because frame-slot images change. The TextureRef conveys a generation-checked borrowed resource identity, not ownership.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.getTexture("scene");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto texture = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getTexture**

#### `[[nodiscard]] Result<TextureRef> getTexture(ResourceKey key) const noexcept`

- **Returns:** `Result<TextureRef>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Acquire the viewport texture reference for the current frame slot. Missing keys are successful unavailable references, with an invalid handle and a once-per-key diagnostic. SkipVisual omits the visual; the fallback policy uses the renderer fallback.

Reacquire each frame because frame-slot images change. The TextureRef conveys a generation-checked borrowed resource identity, not ownership. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "scene"};
auto result = manager.getTexture(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto texture = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **getVulkanInterop**

#### `[[nodiscard]] Result<std::reference_wrapper<const ViewPortVulkanInterop>> getVulkanInterop() const noexcept`

- **Returns:** `Result<std::reference_wrapper<const ViewPortVulkanInterop>>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** none.

Borrow the App-owned Vulkan interop bundle. Success contains `std::reference_wrapper<const ViewPortVulkanInterop>`; detached state returns ObjectNotInitialized.

Unwrap result->get() after checking. Keep the borrowed bundle within the App/window lifetime; never destroy its instance/device/allocator/queue handles. This API requires FLOWUI_PUBLIC_VULKAN_INTEROP.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.getVulkanInterop();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::ViewPortVulkanInterop& borrowed = result->get();
```

## Build-gated development hooks

`FLOW_UI_DEV_MODE` and the development memory-level macros expose additional inspection/recording hooks in the header. They are development infrastructure, not the application API described here. Their existing build gates and signatures are unchanged.

## Vulkan interop values

The value types are declared in [interop/vulkan/Viewport.hpp](include/FlowUi/Public/interop/vulkan/Viewport.hpp) under the same `FLOWUI_PUBLIC_VULKAN_INTEROP` gate.

| Type | Fields and contract |
| --- | --- |
| `ViewPortCreateInfo` | `colorFormat` defaults to `VK_FORMAT_R8G8B8A8_UNORM` (undefined also selects that format); transparent `clearColor`; `clearEveryFrame = true`; `resizeHysteresisPixels = 4`. Runtime hysteresis is at least its default. Size is driven by UI usage. |
| `ViewPortVulkanInterop` | Borrowed `instance`, `physicalDevice`, `device`, VMA `allocator`, and `graphicsQueue`; `graphicsQueueFamily` identifies the graphics family; `framesInFlight` is the viewport resource-slot count. Neither copying the bundle nor unwrapping its reference transfers ownership. |
| `ViewPortRenderContext` | Synchronously borrowed `commandBuffer`, current pixel `extent`, `colorFormat`, `frameIndex`, key view, and nullable `vulkan` pointer. Use only during that callback. The command buffer is already begun and is ended/submitted by FlowUi. |

The `shared_ptr` callback overload retains only its supplied payload. It does not make separately borrowed pointers/references in that payload safe; keep those resources alive for all uses. Validate `context.vulkan != nullptr` before reading the optional bundle pointer.

## Checked development inspection examples

These hooks require `FLOW_UI_DEV_MODE`; viewport hooks also require `FLOWUI_PUBLIC_VULKAN_INTEROP`. They are observation/tooling APIs. A visitor returns `true` to continue; a successful returned `false` is distinct from a failed `Result`. Callback arguments and their resource views are borrowed. The examples use the already acquired manager for the owning App/window.

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **visitDevViewports**

#### `[[nodiscard]] Result<bool> visitDevViewports(void* target, DevViewportVisitor visitor) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.

Observe development catalogue entries without invoking application operations. Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.

Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts sized viewport registrations; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure. This does not schedule rendering or capture; capture_texture is an observation-cache binding and must not be retained as ownership.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
std::size_t count = 0;
auto result = manager.visitDevViewports(&count,
    [](void* target, const FlowUi::ViewPortManager::DevViewportView& viewport) {
        auto& observedCount = *static_cast<std::size_t*>(target);
        if (viewport.width != 0 && viewport.height != 0) {
            ++observedCount;
        }
        return true;
    });
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool completed = *result;
#endif
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **request_dev_capture**

#### `[[nodiscard]] Status request_dev_capture(std::string_view key) noexcept`

- **Returns:** `Status`; check for failure before continuing.

Request one immutable development copy on a later natural viewport render. Success when requested; ObjectNotInitialized, ViewportNotFound, unsupported-format ViewportConfigurationInvalid, capacity, storage, or allocation failure otherwise.

The example expects scene to exist. Successful submission only schedules a request: it does not force rendering or mean the GPU copy has completed. Poll visitDevViewports() for capture_pending/capture_failed and a nonempty capture_texture. Capture is format checked and bounded to 16 active observation caches. No exception escapes.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
auto status = viewports.request_dev_capture("scene");
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).

### **retain_dev_capture**

#### `[[nodiscard]] Result<TextureHandle> retain_dev_capture(std::string_view key, TextureHandle expected_capture) noexcept`

- **Returns:** `Result<TextureHandle>`; check success before accessing the value; use `.error()` on failure.

Create an independent texture binding for an exact completed development capture. A retained TextureHandle, or initialization, missing-viewport, stale/incomplete-handle, storage, or allocation failure.

Pass the exact capture_texture observed for the same key by visitDevViewports(); incomplete or replaced captures are ResourceHandleInvalid. The image is shared without another copy. This retained logical binding must later be released through the owning App::release_dev_viewport_capture_texture(); release_dev_capture(key) only releases the observation cache. Keep the owning App alive. No exception escapes.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
auto result = viewports.retain_dev_capture("scene", observedCapture);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::TextureHandle retained = *result;
#endif
```

Release every successful retained binding with checked `App::release_dev_viewport_capture_texture(retained)` when finished; [the App capture API](docs/api/app.md) gives that release example. Releasing only the manager observation cache does not release an independent retained binding.

See: [Owning header](include/FlowUi/Public/managers/ViewPortManager.hpp).
