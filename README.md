# FlowUi

FlowUi is a C++23 Vulkan-first UI runtime built around:
- [Clay](https://github.com/nicbarker/clay) for layout and command generation
- A custom Vulkan renderer
- A typed element system for reusable UI components

## Overview

### What is FlowUi?

FlowUi is a runtime library for cross-platform desktop applications. At its core, FlowUi acts as a C++ wrapper around [Clay](https://github.com/nicbarker/clay), paired with a built-in custom [Vulkan](https://www.vulkan.org/) renderer.

FlowUi also offers more than that minimal layer. With multiple managers, a custom element system, and developer tooling built around those systems, FlowUi aims to make low-level desktop application development faster to start and easier to iterate on.

### Why Use FlowUi?

For application runtimes, there are more mature, pluggable C++ libraries such as [Dear ImGui](https://github.com/ocornut/imgui).

There are also higher-abstraction libraries and tools for building desktop applications with less systems knowledge.

FlowUi is for the niche case where you want to grab a library, build and link it into an application, and immediately start writing app code while keeping low-level access, *crucially* with an [immediate-mode UI](https://en.wikipedia.org/wiki/Immediate_mode_(computer_graphics)).

But The biggest reason to use FlowUi is if you already want Clay as your layout library. Thinking about UI the way Clay encourages can be a powerful way to approach interface work. FlowUi is trying to support that workflow, not change it.

### Overview of FlowUi's functionality

- Custom element system built around Clay elements for fast, reusable functional blocks.
- Font manager for font registration and usage, with tooling to generate `.arfont` files.
- Icon manager for dynamically sized SVG icons throughout an app.
- Image manager for loading and reusing `png` images.
- Input field manager for user text input, caret placement, and text state.
- Shortcut manager for setting up key chords for app-specific functionality.
- ViewPort manager for custom rendering tasks, such as rendering a 3D scene.
- UI manager for Clay/FlowUi integration and general UI needs.
- Dev and release modes. Dev mode includes tooling that makes design iteration simpler, while release mode stays minimal.

### External tools used by FlowUi

FlowUi uses:

- [msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen) to make font atlases and then render text.
- [plutoSVG](https://github.com/sammycage/plutosvg) to manage SVG data and create textures.
- [stb_image.h](https://github.com/nothings/stb) for loading `png` files.
- [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) for Vulkan memory allocation.
- And, of course, [Clay](https://github.com/nicbarker/clay) for UI layout.

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

### Build Options

| Option | Default | Description |
|---|---|---|
| `FLOWUI_INSTALL` | `ON` | Enable install rules and CMake package export files. |
| `FLOWUI_BUILD_FONT_BAKER` | `ON` | Build and optionally install the offline `flowui_font_baker` tool. |
| `FLOWUI_ENABLE_RUNTIME_FONT_BAKING` | `OFF` | Link the runtime TTF-to-MSDF atlas generation dependencies and define `FLOWUI_RUNTIME_FONT_BAKING=1`. |
| `FLOWUI_PUBLIC_VULKAN_INTEROP` | `ON` | Expose viewport Vulkan interop APIs in public headers. |
| `FLOWUI_INCLUDE_ICON_MANAGER` | `ON` | Build IconManager support and link PlutoSVG. |
| `FLOW_UI_DEV_MODE` | `OFF` | Compile FlowUi developer-mode APIs and tooling, including `debugView`, runtime capture, overrides, and JSON export support. |
| `FLOWUI_GLFW_PROVIDER` | `auto` | Select the GLFW provider: `auto`, `system`, or `vendored`. |

`FLOWUI_GLFW_PROVIDER=auto` uses the vendored GLFW submodule when it is present, otherwise it falls back to a system `glfw3` package.
