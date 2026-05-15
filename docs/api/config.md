# Configuration API

## Enums

### **PresentMode**
---

#### `enum class PresentMode`

Requested Vulkan swapchain presentation mode. Use it to choose FIFO, mailbox, or immediate presentation behavior.

### **MSAA**
---

#### `enum class MSAA`

Requested multisample anti-aliasing sample count. The value is retained for renderer configuration even where current support is limited.

### **CursorMode**
---

#### `enum class CursorMode : uint8_t`

Native cursor visibility and capture mode. It controls whether the cursor is normal, hidden, or disabled/captured.

### **CursorType**
---

#### `enum class CursorType : uint8_t`

Cursor shape requested by UI code. UiManager cursor requests use these values before forwarding to the window backend.

### **DevShortcutTrigger**
---

#### `enum class DevShortcutTrigger : uint8_t`

Trigger mode used by the developer panel shortcut. It describes whether the chord fires on press, release, or while held.

### **TextureFitMode**
---

#### `enum class TextureFitMode : uint8_t`

Texture layout mode inside a target rectangle. It controls stretch, contain, cover, or source-size drawing behavior.

### **TextureSamplingMode**
---

#### `enum class TextureSamplingMode : uint8_t`

Texture filtering preference stored on TextureRef. It distinguishes smooth linear sampling from nearest-neighbor sampling.

## Public Structs

### **WindowInputConfig**
---

#### `struct WindowInputConfig`

Configures low-level window input behavior. These settings are forwarded to the window backend during creation and runtime updates.

### **WindowConfig**
---

#### `struct WindowConfig`

Describes the native window created for the app. It owns initial size, title, resize/fullscreen behavior, high-DPI preference, and input defaults.

### **VulkanConfig**
---

#### `struct VulkanConfig`

Configures Vulkan device, swapchain, validation, and frame scheduling defaults. These values are consumed during renderer initialization.

### **InputManagerConfig**
---

#### `struct InputManagerConfig`

Configures caret and selection rendering for text input fields. It affects editing visuals, not low-level window input.

### **UiConfig**
---

#### `struct UiConfig`

Configures layout, text scaling, transient UI storage, Clay memory, and default font resources. These values are consumed during app and UI manager initialization.

### **IconManagerConfig**
---

#### `struct IconManagerConfig`

Configures SVG icon rasterization, atlas storage, and cache behavior. It controls atlas size, page count, raster-size reuse, and padding.

### **DevShortcutChord**
---

#### `struct DevShortcutChord`

Keyboard chord used by developer tooling. The default chord is intended to toggle the developer panel.

### **DevToolsConfig**
---

#### `struct DevToolsConfig`

Configures developer tooling, capture behavior, panel defaults, and export paths. These settings are used only when developer mode support is compiled in.

### **AppConfig**
---

#### `struct AppConfig`

Top-level app configuration. It groups all subsystem-specific configuration blocks passed through startup.

### **TextureRef**
---

#### `struct TextureRef`

Texture handle and render options returned by resource managers. Manager-owned fields identify the texture, while app code may tune fit, sampling, and tint options.

### **FrameInput**
---

#### `struct FrameInput`

Normalized per-frame input snapshot consumed by managers and custom elements. It stores timing, pointer, scroll, key, modifier, and text input state.

## Public API

### **Flow_Color**
---

#### `Clay_Color Flow_Color(std::string_view hexRgba)`

- **Returns:** `Clay_Color`
- **Arguments:** `hexRgba` color string in `#RRGGBBAA` form.

Converts a hex RGBA string into a Clay color. The input must include the leading `#` and eight hex digits; invalid input throws `std::invalid_argument`.
