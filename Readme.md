# FlowUi

FlowUi is an early-stage UI runtime built on top of Clay + Vulkan.

This is not v1.0.
This is a first-working baseline and the API can change.

## What Works Right Now

- Window creation through GLFW backend.
- Vulkan init path (instance, device, swapchain, frame resources).
- Per-frame UI flow: `beginFrame()`, `endFrame()`, `drawFrame()`.
- Clay initialization and layout lifecycle.
- Element registry + element builder system.
- Solid UI rendering (rounded rect + border shader path).
- Shader compilation during build (`glslc` preferred, `glslangValidator` fallback).

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Minimal App Loop

```cpp
#include <FlowUi.hpp>

int main() {
    FlowUi::AppConfig config{};
    FlowUi::App app = FlowUi::makeApplication(config);

    while (!app.shouldClose()) {
        app.beginFrame();

        // Build your UI here.
        // Example:
        // app.ui().createElement("SomeElement", "root/some_id").draw();

        app.endFrame();
        app.drawFrame();
    }

    return 0;
}
```

## Custom Elements

Use [`template.cpp`](template.cpp) as the copy-paste starting point for registering your own elements.

Check Out [`example.cpp`](example.cpp) for an example application that has 3 *"Check Boxes"* in the middle where each swaps the color of the background to the color of their border.

[The App](assets/pictures/Screenshot%20from%202026-03-04%2019-16-52.png)

## Not Done Yet

- Real MSDF text rendering path.
- Real textured UI path.
- Mature widget set and stable public API.
