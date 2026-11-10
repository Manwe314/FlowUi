# Architecture

FlowUi 0.10.0 is organized into distinct layer boundaries:

```text
+-------------------------------------------------------------------+
|                        Application Layer                          |
|                   FlowUi::App / Window Loops                      |
+-------------------------------------------------------------------+
|                         Manager Layer                             |
| ThemeManager | UiManager | PopupManager | Fonts | Images | Icons |
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

## Repository layout

Core headers and source files use the following layout:

```text
include/
├── FlowUi.hpp                         # Application umbrella
├── FlowUi/
│   ├── Public/
│   │   ├── configurationStructs/
│   │   ├── elementAuthoring/
│   │   ├── elements/
│   │   ├── errorManagement/
│   │   ├── identificationTypes/
│   │   ├── interop/vulkan/
│   │   ├── managerStructs/
│   │   ├── managers/
│   │   └── App.hpp, Flow.hpp, Color.hpp, Resources.hpp, ...
│   └── Private/
│       ├── actionService/
│       ├── elementSystem/
│       ├── inputService/
│       ├── layoutService/
│       ├── renderer/vulkan/
│       ├── resourceService/
│       ├── storageSystem/
│       ├── textLayoutService/
│       ├── windowService/
│       └── AppRuntime.hpp, shared support headers
├── FSEL/                              # Standard element library
└── devSystems/                        # Developer services and interface
src/
├── FlowUi/
│   ├── Public/                        # App, shared values, manager facades
│   └── Private/                       # Runtime and service implementations
└── devSystems/                        # Developer implementations
```

[Public headers](../../include/FlowUi/Public/) define the supported application API. [Private headers](../../include/FlowUi/Private/) hold runtime state, controllers, backends, and template support; the subset required by public templates is installed with the library. `Private` identifies implementation ownership, including headers that consumers need to compile supported APIs.

[Public sources](../../src/FlowUi/Public/) implement the application and manager facades. [Private sources](../../src/FlowUi/Private/) implement the runtime and services in their owning domains. Source directories exist where out-of-line implementations are needed; header-only concepts, values, and templates do not require matching `.cpp` files. FSEL stays under [include/FSEL](../../include/FSEL/), while developer headers and sources stay under [include/devSystems](../../include/devSystems/) and [src/devSystems](../../src/devSystems/).

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
   - **`PopupManager`**: Floating placement, measurement, stacking, overflow correction, and dismissal.
   - **`ViewPortManager`**: Offscreen render target management and custom Vulkan rendering callbacks.

4. **Vulkan Renderer (`Vk_UiRenderer`)**
   - Multi-window batching engine converting Clay output commands into Vulkan draw calls using pipeline variants for solid, textured, and MSDF font elements.
