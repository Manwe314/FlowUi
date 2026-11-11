# Configuration API

See the [configuration glossary](configuration-glossary.md) for member defaults, units, build conditions, capacity keys, and examples.

Configuration headers live under [FlowUi/Public/configurationStructs](../../include/FlowUi/Public/configurationStructs/). Include [AppConfig.hpp](../../include/FlowUi/Public/configurationStructs/AppConfig.hpp) for startup configuration, or the focused owner header for a leaf type. Cursor/frame-input values live in [InputTypes.hpp](../../include/FlowUi/Public/managerStructs/InputTypes.hpp); texture references and shared styling live in [TextureRef.hpp](../../include/FlowUi/Public/TextureRef.hpp) and [FlowUiTheme.hpp](../../include/FlowUi/Public/FlowUiTheme.hpp). Development configuration stays under [devSystems](../../include/devSystems/DevConfig.hpp), and agentic configuration under [devAgenticWorkflow](../../include/devSystems/devAgenticWorkflow/AgenticTypes.hpp), with their existing namespaces and build gates.

## Enums

### **PresentMode**


#### `enum class PresentMode : std::uint8_t`

Requested Vulkan swapchain presentation mode. Use it to choose FIFO, mailbox, or immediate presentation behavior.

### **CursorMode**


#### `enum class CursorMode : uint8_t`

Native cursor visibility and capture mode. It controls whether the cursor is normal, hidden, or disabled/captured.

### **CursorType**


#### `enum class CursorType : uint8_t`

Cursor shape requested by UI code. UiManager cursor requests use these values before forwarding to the window backend.

### **DevShortcutTrigger**


#### `enum class DevShortcutTrigger : uint8_t`

Trigger mode used by the developer panel shortcut. It describes whether the chord fires on press, release, or while held.

### **TextureFitMode**


#### `enum class TextureFitMode : uint8_t`

Texture layout mode inside a target rectangle. It controls stretch, contain, cover, or source-size drawing behavior.

## Public Structs

### **WindowInputConfig**


#### `struct WindowInputConfig`

Configures low-level window input behavior. These settings are forwarded to the window backend during creation and runtime updates.

### **WindowConfig**


#### `struct WindowConfig`

Describes the native window created for the app. It owns initial size, title, resize/fullscreen behavior, high-DPI preference, and input defaults.

### **VulkanConfig**


#### `struct VulkanConfig`

Configures Vulkan device, swapchain, validation, and frame scheduling defaults. These values are consumed during renderer initialization. Rendering uses a fixed sample count of one (x1); MSAA is not configurable.

### **InputManagerConfig**


#### `struct InputManagerConfig`

Configures caret and selection rendering for text input fields. It affects editing visuals, not low-level window input.

### **UiConfig**


#### `struct UiConfig`

Configures layout, text scaling, Clay memory, default font resources, and primitive anti-aliasing. Temporary UI storage is configured through `AppConfig::memoryCapacityProfile.storage.transientBytesPerFramePerWindow`. These values are consumed during app and UI manager initialization.

`primitive_antialiasing` controls the existing linear analytic coverage of solid primitives, borders, and image silhouettes. Defaults preserve the existing appearance. Set a finite positive `coverage_width_scale` to soften or sharpen coverage; setting `enabled = false` produces hard shape edges. The renderer derives the raster fringe from that width in framebuffer pixels. Glyph reconstruction, texture filtering, layout, hit testing, and strict scissor clipping retain their existing behavior.

Set these startup values before `makeApplication()`; existing windows keep their copied settings:

```cpp
FlowUi::AppConfig config{};
config.ui.primitive_antialiasing.coverage_width_scale = 1.5f;
config.ui.primitive_antialiasing.enabled = true;
```

Invalid width values (zero, negative, infinity, or NaN) fail startup with `RendererConfigurationInvalid`, including when primitive AA is disabled.

### **IconManagerConfig**


#### `struct IconManagerConfig`

Configures SVG icon rasterization, atlas storage, and cache behavior. It controls atlas size, page count, raster-size reuse, and padding.

### **DevShortcutChord**


#### `struct DevShortcutChord`

Keyboard chord used by developer tooling. The default chord is intended to toggle the developer panel.

### **DevToolsConfig**


#### `struct DevToolsConfig`

Configures modern developer monitoring, schema/override/tree capacities, runtime activation, and the dedicated developer-window shortcut. These settings are used only when `FLOW_UI_DEV_MODE` is compiled in. Durable developer changes use `.flowui/changes/active.flowchanges` through the modern bake pipeline.

### **AppConfig**


#### `struct AppConfig`

Top-level app configuration. It groups all subsystem-specific configuration blocks passed through startup.

### **TextureRef**


#### `struct TextureRef`

Texture handle and render options returned by resource managers. Manager-owned fields identify the texture, while app code may tune fit and tint options. Filtering comes from the logical texture's sampler.

### **FrameInput**


#### `struct FrameInput`

Normalized per-frame input snapshot consumed by managers and custom elements. It stores timing, pointer, scroll, key, modifier, and text input state.

## Public API

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

See: [Owning header](../../include/FlowUi/Public/configurationStructs/AppConfig.hpp).
