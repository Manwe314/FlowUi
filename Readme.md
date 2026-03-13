# FlowUi

## Intro
FlowUi is a Vulkan-first UI runtime that combines:
- [Clay](https://github.com/nicbarker/clay) for layout + command generation
- A custom Vulkan renderer for solid, textured, and MSDF text passes
- A lightweight element system for reusable UI building blocks

The project is pre-1.0 and API details can still change. Current code is focused on a practical baseline that is already usable for real app loops.

## Table Of Contents
- [What Is FlowUi?](#what-is-flowui)
- [Quick Start Guide](#quick-start-guide)
- [Build System And How To Use It](#build-system-and-how-to-use-it)
- [Core Tech Stack](#core-tech-stack)
- [Flow Element System Intro](#flow-element-system-intro)
- [Artery Font Tool](#artery-font-tool)
- [Small API Documentation](#small-api-documentation)

## What Is FlowUi?
FlowUi is a C++23 UI runtime that owns:
- Window/input backend via GLFW
- Vulkan bootstrap (instance, device, swapchain, per-frame resources)
- Per-frame lifecycle: `beginFrame()` -> `endFrame()` -> `drawFrame()`
- Clay layout + render command generation
- GPU rendering paths:
`Solid`: rounded rectangles + borders
`MSDF`: text rendering from baked `.arfont` font atlases
`Textured`: image/textured elements with fit modes (`Stretch`, `Contain`, `Cover`, `None`)
- Runtime registries for images, fonts, and (optionally) viewport Vulkan interop

Current focus areas:
- Stable core frame/render path
- Extensible element definitions and callbacks
- Offline font pipeline around `.arfont`

## Quick Start Guide
### 1) Requirements
- CMake `>= 3.20`
- C++23 compiler
- Vulkan SDK (with `glslc` preferred, `glslangValidator` fallback)
- GLFW3
- Git submodule init (required for `external/msdf-atlas-gen` when font baker is enabled)

```bash
git submodule update --init --recursive
```

### 2) Build
```bash
cmake -S . -B build
cmake --build build --parallel
```

### 3) Minimal app loop
```cpp
#include <Flow.hpp>

int main() {
    FlowUi::AppConfig config{};
    config.window.title = "FlowUi Quick Start";

    FlowUi::App app = FlowUi::makeApplication(config);

    while (!app.shouldClose()) {
        app.beginFrame();

        CLAY({
            .id = CLAY_ID("root"),
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER }
            },
            .backgroundColor = Clay_Color{ 24, 24, 24, 255 }
        }) {
            CLAY_TEXT(
                app.ui().toClayString("Hello FlowUi"),
                CLAY_TEXT_CONFIG(
                    .fontId = 0,
                    .fontSize = 24,
                    .textColor = Clay_Color{ 255, 255, 255, 255 }));
        }

        app.endFrame();
        app.drawFrame();
    }
    return 0;
}
```

### 4) Default config values worth knowing
- Window: `1280x720`, title `"FlowUi App"`, high-DPI enabled
- Vulkan: validation/debug on, `PresentMode::Fifo`, `framesInFlight = 2`
- UI: `uiScale = 1.0`, `fontScale = 1.0`, `dpi = 96`
- Default font path: `assets/fonts/FacultyGlyphic-Regular.arfont`

## Build System And How To Use It
### Key CMake options
| Option | Default | What it does |
|---|---|---|
| `FLOWUI_INSTALL` | `ON` | Enables `cmake --install` rules and package export |
| `FLOWUI_BUILD_FONT_BAKER` | `ON` | Builds `flowui_font_baker` CLI |
| `FLOWUI_ENABLE_RUNTIME_FONT_BAKING` | `OFF` | Links runtime font baking dependencies into `flowui` |
| `FLOWUI_PUBLIC_VULKAN_INTEROP` | `ON` | Exposes viewport Vulkan interop API in public headers |
| `FLOWUI_GLFW_PROVIDER` | `auto` | GLFW source: `auto`, `system`, or `vendored` |

### Common build presets (manual)
Build only core library (skip offline baker):
```bash
cmake -S . -B build -DFLOWUI_BUILD_FONT_BAKER=OFF
cmake --build build --parallel
```

Build core + baker (default behavior):
```bash
cmake -S . -B build -DFLOWUI_BUILD_FONT_BAKER=ON
cmake --build build --parallel
```

Install package locally:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix ./install
```

### Linking FlowUi in your own CMake project
As a subdirectory:
```cmake
add_subdirectory(path/to/FlowUi FlowUi-build)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FlowUi::FlowUi)
```

From an installed package:
```cmake
find_package(FlowUi CONFIG REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FlowUi::FlowUi)
```

## Core Tech Stack
### Clay
Clay is the layout and command-generation layer. You author UI with Clay macros and FlowUi renders the generated commands through Vulkan.

### Vulkan Memory Allocator (VMA)
FlowUi uses VMA (`external/vk_mem_alloc.h`) for GPU memory management (images, buffers, staging uploads), keeping Vulkan allocation code practical.

### msdf-atlas-gen
`external/msdf-atlas-gen` powers the offline font baker. It converts `ttf` glyphs into MTSDF atlases and exports `.arfont` artifacts.

### artery-font-format
FlowUi loads baked `.arfont` data at runtime through artery-font-format structures/serialization.

### stb_image
FlowUi uses `stb_image` for image decoding in `ImageManager`.

## Flow Element System Intro
FlowUi’s element layer is a small registry + builder abstraction on top of Clay.

### Concepts
- `ElementRegistry`:
stores `ElementDefinition` by type name
- `ElementDefinition`:
declares callbacks:
`initializeDefaultParameters`
`onHovered`, `onPressed`, `onHeld`, `onReleased`
`runLogic`
`buildElement` (required)
- `ElementBuilder`:
created via `app.ui().createElement(type, instancePath)` then configured with:
`.set(...)` for per-instance parameter overrides
`.bind(...)` for external state references
`.draw(...)` to execute callbacks/build

### Callback order per draw
1. Event callbacks (unless skipped)
2. Logic callback (unless skipped)
3. Build callback (unless skipped)

### Quick example
Use [template.cpp](template.cpp) as the starting point for registering custom elements and drawing them.

## Artery Font Tool
FlowUi ships with an offline tool: `flowui_font_baker`.

### Build only the tool
```bash
cmake -S . -B build -DFLOWUI_BUILD_FONT_BAKER=ON
cmake --build build --target flowui_font_baker --parallel
```

### CLI usage
```bash
./build/flowui_font_baker \
  --input ./assets/fonts/FacultyGlyphic-Regular.ttf \
  --output ./assets/fonts/FacultyGlyphic-Regular.arfont \
  --pixel-size 48 \
  --charset "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
```

Useful options:
- `--charset-file <path>`: load charset from file (msdf-atlas syntax)
- `--px-range <value>`: control distance-field range (default: `2`)
- `--threads <n>`: worker count (`0` = auto)

### Runtime loading
```cpp
const int myFontId = app.fonts().registerBakedFont("assets/fonts/MyFont.arfont", "MyFont");
```

Notes:
- `.arfont` is the currently supported runtime input for `loadFont/registerBakedFont`
- Runtime `ttf/otf -> atlas` baking is not implemented yet in the runtime path

## Small API Documentation
### App lifecycle
- `FlowUi::App makeApplication(const AppConfig&)`
- `bool shouldClose() const`
- `void beginFrame()`
- `void endFrame()`
- `void drawFrame()`

### Core managers
- `FontManager& App::fonts()`
`loadFont`, `registerBakedFont`, `getFontId`
- `ImageManager& App::images()`
`registerImage`, `removeImage`, `contains`, `getTexture`
- `UiManager& App::ui()`
`toClayString`, `toClayEID`, `createElement`
- `ElementRegistry& App::elementRegistry()`
`registerElement`, `findElement`

### Optional Vulkan interop (when `FLOWUI_PUBLIC_VULKAN_INTEROP=1`)
- `ViewPortManager& App::viewPorts()`
`create`, `remove`, `getViewPort`, `getTexture`, `getVulkanInterop`

### Primary config structs
- `WindowConfig`
- `VulkanConfig`
- `UiConfig`
- `AppConfig`

See public headers for details:
- `include/FlowUi/App.hpp`
- `include/FlowUi/PublicStructs.hpp`
- `include/managers/FlowUiElementSystem.hpp`
