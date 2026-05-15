# FlowUi {#mainpage}

FlowUi is a C++23 Vulkan-first UI runtime built around:

- [Clay](https://github.com/nicbarker/clay) for layout and command generation
- A custom Vulkan renderer
- A typed element system for reusable UI components

## What is FlowUi?

FlowUi is a runtime library for cross-platform desktop applications. At its core, FlowUi acts as a C++ wrapper around [Clay](https://github.com/nicbarker/clay), paired with a built-in custom [Vulkan](https://www.vulkan.org/) renderer.

FlowUi also offers more than that minimal layer. With multiple managers, a custom element system, and developer tooling built around those systems, FlowUi aims to make low-level desktop application development faster to start and easier to iterate on.

## How to Start Using FlowUi

For a full guided setup, start with the [Quick Start tutorial](tutorials/quick_start.md). For a shorter bootstrap, the usual project shape is:

```text
MyApp/
├── CMakeLists.txt
├── external/
│   └── FlowUi/
├── include/
└── src/
```

Clone FlowUi into `external/FlowUi` with its submodules:

```bash
git clone --recursive <flowui-repository-url> external/FlowUi
```

Add FlowUi to your root `CMakeLists.txt` and link the app target:

```cmake
cmake_minimum_required(VERSION 3.20)

project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional, but useful while building custom elements and UI.
set(FLOW_UI_DEV_MODE ON CACHE BOOL "" FORCE)

add_subdirectory(external/FlowUi)

add_executable(my_app
    src/main.cpp
)

target_include_directories(my_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(my_app
    PRIVATE
        FlowUi::FlowUi
)
```

Then configure and build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Your first source file can include the combined FlowUi header and create the application:

```cpp
#include <FlowUi/Flow.hpp>

int main() {
    FlowUi::AppConfig config{};
    config.window.title = "My FlowUi App";
    config.window.width = 1280;
    config.window.height = 720;

    FlowUi::App app = FlowUi::makeApplication(config);

    while (!app.shouldClose()) {
        app.beginFrame();

        // Build UI here.

        app.endFrame();
        app.drawFrame();
    }

    return 0;
}
```

If this is your first FlowUi project, the [Quick Start tutorial](tutorials/quick_start.md) continues from this loop into a custom element example.

## Build Options

These options are set before `add_subdirectory(external/FlowUi)`.

| Option | Default | Description |
|---|---|---|
| `FLOWUI_INSTALL` | `ON` | Enable install rules and CMake package export files. |
| `FLOWUI_BUILD_FONT_BAKER` | `ON` | Build the offline `flowui_font_baker` tool. If install rules are enabled, the tool is installed with FlowUi. |
| `FLOWUI_ENABLE_RUNTIME_FONT_BAKING` | `OFF` | Link the runtime TTF-to-MSDF atlas generation dependencies and define `FLOWUI_RUNTIME_FONT_BAKING=1`. |
| `FLOWUI_PUBLIC_VULKAN_INTEROP` | `ON` | Expose viewport Vulkan interop APIs in public headers. |
| `FLOWUI_INCLUDE_ICON_MANAGER` | `ON` | Build IconManager support and link PlutoSVG. |
| `FLOW_UI_DEV_MODE` | `OFF` | Compile developer-mode APIs and tooling, including dev registration macros, debug UI, runtime capture, overrides, and JSON export support. |
| `FLOWUI_GLFW_PROVIDER` | `auto` | Select the GLFW provider: `auto`, `system`, or `vendored`. |

`FLOWUI_GLFW_PROVIDER=auto` uses the vendored GLFW submodule when it is present, otherwise it falls back to a system `glfw3` package.

FlowUi requires Vulkan. During configuration it looks for `glslc` first and falls back to `glslangValidator` for shader compilation.

## Minimal App Loop Example

```cpp
#include <FlowUi/Flow.hpp>
#include "backgroundElement.hpp"

int main() {
    FlowUi::AppConfig config{};
    FlowUi::App app = FlowUi::makeApplication(config);

    while (!app.shouldClose()) {
        app.beginFrame();

        app.ui().createElement(kBackground, "background")
            .setParameters({.color = FlowUi::Flow_Color("#09f1deff")})
            .draw();

        app.endFrame();
        app.drawFrame();
    }
}
```

## Core FlowUi Concepts

FlowUi is intended to keep the core application loop explicit while making UI code reusable and inspectable.

- **Clay owns layout.** FlowUi builds on Clay's immediate-mode layout model and converts Clay output into Vulkan draw work.
- **`App` owns the runtime.** A `FlowUi::App` is created from `FlowUi::AppConfig` and gives access to the window, UI manager, asset managers, input systems, and render lifecycle.
- **Frames are explicit.** Application code builds UI between `beginFrame()` and `endFrame()`, then presents with `drawFrame()`.
- **Elements are typed.** Reusable UI is expressed through `ElementDefinition<Params, State, Resources, Id>`, separating per-frame parameters, persistent per-instance state, and shared per-definition resources.
- **Managers own specialized systems.** Fonts, images, icons, input fields, shortcuts, viewports, and UI rendering have dedicated managers instead of one large global API.
- **Developer mode is opt-in.** `FLOW_UI_DEV_MODE` enables the dev registration and capture systems used to inspect and edit Flow elements while building an app.
- **Vulkan interop is a first-class path.** Viewport APIs can expose Vulkan render context data when `FLOWUI_PUBLIC_VULKAN_INTEROP` is enabled, so apps can integrate custom rendering where needed.

## Documentation Map

### Tutorials

- [Quick Start](tutorials/quick_start.md)
- [Custom Elements](tutorials/custom_elements.md)
- [Fonts and Text](tutorials/fonts_and_text.md)
- [Images, Icons, and Texture References](tutorials/images_icons_textures.md)
- [Input Fields and Shortcuts](tutorials/input_fields_and_shortcuts.md)
- [Viewports and Vulkan Interop](tutorials/viewports_vulkan_interop.md)
- [Developer Mode](tutorials/developer_mode.md)
- [Complete Application Tutorial](tutorials/complete_app.md)

### Concepts

- [Core Mental Model](concepts/mental_model.md)
- [Frame Lifecycle](concepts/frame_lifecycle.md)
- [Element System](concepts/element_system.md)
- [Managers](concepts/managers.md)
- [IDs and State](concepts/ids_and_state.md)

### API Markdown Index

- [All Public API](api/all_public_api.md)
- [App API](api/app.md)
- [Configuration API](api/config.md)
- [Element API](api/elements.md)
- [UI Manager API](api/ui_manager.md)
- [Font Manager API](api/font_manager.md)
- [Image Manager API](api/image_manager.md)
- [Icon Manager API](api/icon_manager.md)
- [Input Field Manager API](api/input_field_manager.md)
- [Shortcut Manager API](api/shortcut_manager.md)
- [Viewport Manager API](api/viewport_manager.md)

### Public API

- @ref flowui_app "App and lifecycle"
- @ref flowui_config "Configuration structs and public value types"
- @ref flowui_element_system "Typed element system"
- @ref flowui_ui_manager "UI manager"
- @ref flowui_font_manager "Font manager"
- @ref flowui_image_manager "Image manager"
- @ref flowui_icon_manager "Icon manager"
- @ref flowui_input_field_manager "Input field manager"
- @ref flowui_shortcut_manager "Shortcut manager"
- @ref flowui_viewport_manager "Viewport manager"

### Internal and Backend Notes

- [Architecture](internals/architecture.md)
- [Window Backend](internals/window_backend.md)
- [Vulkan Context, Frames, and Swapchain](internals/vulkan_context_frames_swapchain.md)
- [UI Renderer](internals/ui_renderer.md)
- [Texture Registry](internals/texture_registry.md)
- [Text Layout Engine](internals/text_layout_engine.md)
- [Developer Runtime](internals/dev_runtime.md)

### Troubleshooting

- [Troubleshooting](troubleshooting.md)
