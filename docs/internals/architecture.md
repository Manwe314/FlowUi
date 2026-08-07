# Architecture

FlowUi 0.10.0 is organized into distinct layer boundaries:

```text
+-------------------------------------------------------------------+
|                        Application Layer                          |
|                   FlowUi::App / Window Loops                      |
+-------------------------------------------------------------------+
|                         Manager Layer                             |
|  ThemeManager | UiManager | FontManager | ImageManager | Icons    |
+-------------------------------------------------------------------+
|                      Clay Layout Engine                           |
|         Immediate-Mode Element System & Command Generation        |
+-------------------------------------------------------------------+
|                   Storage System (FlowStorageSystem)              |
| Generational Handles | Manager Records | Multi-Window Arenas      |
+-------------------------------------------------------------------+
|                     Vulkan Render Backend                         |
|   Vk_Context | Vk_Frames | Vk_Swapchain | Vk_UiRenderer           |
+-------------------------------------------------------------------+
```

## Subsystems

1. **Application Lifecycle & Window Backend**
   - Managed by `FlowUi::App` and GLFW native window wrapper `AppWindow`.
   - Supports multi-window creation (`createWindow`), frame iteration (`beginFrame`, `endFrame`, `drawFrame`), and global event polling (`pollEvents()`).

2. **Storage System (`IStorageSystem` / `FlowStorageSystem`)**
   - Centralized memory subsystem managing persistent CPU/GPU memory pools, frame-scoped transient arenas, generational handles (`TextureHandle`, `BufferHandle`, `BlobHandle`), descriptor set revisions, and manager record storage (`ResourceKind::UiTheme`, `ResourceKind::UiFont`, `ResourceKind::UiImage`, `ResourceKind::UiIcon`).
   - Supports deferred retirement linked to GPU submission serials (`SubmissionSerial`).

3. **Manager Layer**
   - **`ThemeManager`**: Manages registrable C++ theme structs, multi-variant mappings, active variant dispatch, and atomic frame-boundary staged mutations.
   - **`UiManager`**: Frame authoring surface owning Clay context, string/texture arenas, interaction/input snapshots, font resolution, caret management, and element builders.
   - **`FontManager`**: Manages font families, MSDF font atlas baking, and `FontId` resolution.
   - **`ImageManager` / `IconManager`**: Asset texture loading, SVG rasterization, and `TextureRef` lookup.
   - **`ShortcutManager` / `InputFieldManager`**: Input chord matching and text field state tracking.
   - **`ViewPortManager`**: Offscreen render target management and custom Vulkan rendering callbacks.

4. **Vulkan Renderer (`Vk_UiRenderer`)**
   - Multi-window batching engine converting Clay output commands into Vulkan draw calls using pipeline variants for solid, textured, and MSDF font elements.
