# FlowUi Quick Start

Create a C++23 application with a custom, stateful Flow element and a Vulkan-backed window. This example draws a rectangle that cycles through a palette when pressed, so no font asset is required.

## Requirements and project layout

Use CMake 3.25 or newer, Ninja, a C++23 compiler, Vulkan development libraries, a supported window system, and a shader compiler (`glslc` or `glslangValidator`). FlowUi uses its vendored GLFW submodule when present; otherwise install a system `glfw3` package. See [portability](docs/portability.md) for supported dependency configurations.

```text
RainbowApp/
├── CMakeLists.txt
├── external/FlowUi/
├── include/application.hpp
└── src/main.cpp
```

Clone FlowUi with its submodules into `external/FlowUi` using your repository URL. Pin the dependency to the revision you intend to build. For an existing clone:

```sh
git -C external/FlowUi submodule update --init --recursive
```

## CMake configuration

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(RainbowApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional: compile modern developer systems for inspection and editing.
set(FLOW_UI_DEV_MODE ON CACHE BOOL "" FORCE)
add_subdirectory(external/FlowUi)

add_executable(rainbow_app src/main.cpp)
target_include_directories(rainbow_app PRIVATE include)
target_link_libraries(rainbow_app PRIVATE FlowUi::FlowUi)
```

## Define a reusable element

Create `include/application.hpp`:

```cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <FlowUi/Public/Flow.hpp>
#include <devSystems/devTooling/schema/DevSchemaDescriptor.hpp>

namespace rainbow {

struct ButtonParameters {
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIXED(240.0f),
        .height = CLAY_SIZING_FIXED(120.0f),
    };
    float corner_radius = 12.0f;
};

struct ButtonState {
    std::uint8_t color_index = 0;
};

struct ButtonResources {
    std::array<Clay_Color, 7> colors{
        FlowUi::Flow_Color("#ff3b30ff"),
        FlowUi::Flow_Color("#ff9500ff"),
        FlowUi::Flow_Color("#ffcc00ff"),
        FlowUi::Flow_Color("#34c759ff"),
        FlowUi::Flow_Color("#007affff"),
        FlowUi::Flow_Color("#5856d6ff"),
        FlowUi::Flow_Color("#af52deff"),
    };
};

struct RainbowButton {
    using Parameters = ButtonParameters;
    using State = ButtonState;
    using Resources = ButtonResources;
    using BuildContext = FlowUi::ElementBuildContext<RainbowButton>;
    using InteractionContext = FlowUi::ElementInteractionContext<RainbowButton>;

    static constexpr auto definitionId = FlowUi::DefinitionID("rainbow.button");
    static constexpr std::string_view debugName = "Rainbow Button";

    static void onHovered(InteractionContext& context) {
        context.uiManager.requestCursor(FlowUi::CursorType::PointingHand);
    }

    static void onPressed(InteractionContext& context) {
        auto& state = context.state();
        state.color_index = static_cast<std::uint8_t>((state.color_index + 1u) % 7u);
    }

    static void buildElement(BuildContext& context) {
        const auto& resources = context.resources();
        const auto& state = context.state();
        const auto rootId = context.clayID();
        const auto& colors = resources.colors;
        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.backgroundColor = colors[state.color_index % colors.size()];
        root.cornerRadius = CLAY_CORNER_RADIUS(context.params.corner_radius);
        CLAY(rootId, root) {}
    }
};

inline constexpr RainbowButton kRainbowButton{};
static_assert(FlowUi::FlowElement<RainbowButton>);

FLOWUI_DEV_SCHEMA(
    ButtonParameters,
    FLOWUI_DEV_FIELD(ButtonParameters, sizing),
    FLOWUI_DEV_FIELD(ButtonParameters, corner_radius))

FLOWUI_DEV_SCHEMA(ButtonState, FLOWUI_DEV_FIELD(ButtonState, color_index))
FLOWUI_DEV_SCHEMA(ButtonResources, FLOWUI_DEV_FIELD(ButtonResources, colors))

} // namespace rainbow
```

A Flow element is an empty tag struct with a strong `definitionId` and static callbacks. Declare only the capabilities it needs. Parameters belong to one invocation, state belongs to the resolved window/instance, and resources are initialized lazily once per definition and application. `context.state()` and `context.resources()` access the app-owned runtime data. The tag itself stores neither state nor resources.

The builder invokes event callbacks using the previous completed interaction snapshot, then logic and drawing. `context.clayID()` bridges the instance's strong Flow identity to its Clay root. The root must use that identity for normal element interaction.

The schema macros describe values for the modern developer systems. They expand away when developer support is disabled; normal element behavior has no dependency on mutable registration or a separate developer runtime. See [Developer Systems](docs/tutorials/developer_mode.md) for constraints, explicit catalogues, overrides, and baking.

## Run the application

Create `src/main.cpp`:

```cpp
#include "application.hpp"

int main() {
    FlowUi::AppConfig configuration{};
    configuration.window.title = "RainbowApp";
    configuration.window.width = 800;
    configuration.window.height = 600;
#if FLOW_UI_DEV_MODE
    configuration.dev.enabled = true;
#endif
    auto application = FlowUi::makeApplication(configuration);
    if (!application)
        return 1;

    while (true) {
        const auto closed = application->shouldClose();
        if (!closed) {
            application->reportError(closed.error());
            return 1;
        }
        if (*closed)
            break;
        const auto frame = application->runFrame([](FlowUi::UiManager& ui, FlowUi::WindowId) {
            ui.createElement(rainbow::kRainbowButton, "main/rainbow")
                .setParameters({.corner_radius = 18.0f})
                .draw();
        });
        if (!frame) {
            application->reportError(frame.error());
            return 1;
        }
    }
    return 0;
}
```

`makeApplication()` returns `Result<App>`. Check it before using the owned application. `runFrame()` polls events, begins the main-window frame, executes the callback, ends the frame, and presents it. Check its `Status` before proceeding. UI helper failures are retained on the frame, so returning early from the callback lets `runFrame()` cancel the frame and return the original `FlowUiError`. Exceptions from user element or build callbacks are also contained at this library-owned boundary. For manual window sequencing, check the separate `pollEvents()`, `beginFrame()`, `endFrame()`, and `drawFrame()` results.

## Handle public API results

Operations with a local recovery decision return `Result<T>` or `Status`. Check those results before dereferencing them. Builder operations, invocation-local state/resources, and ordinary action invocation propagate exceptions to the enclosing build/frame or caller boundary; they do not manufacture successful-looking empty values. Hybrid operations retain documented returned errors while allowing application or preparation exceptions to propagate. Existing `FlowUiError` values retain their code, detecting site, subject, auxiliary value, and native status. Allocation failures become `AllocationFailed`; otherwise unclassified callback or third-party exceptions become `ForeignExceptionObserved`.

The application-facing contracts are:

| Operation | Checked result |
| --- | --- |
| `makeApplication(config)` | `Result<App>`; move or access the app after checking success. |
| `app.ui()`, `app.fonts()`, and other manager accessors | `Result<std::reference_wrapper<T>>`; use `result->get()` after checking success. |
| Window queries such as `shouldClose()` and `windowSize()` | `Result<T>`; test success before inspecting the returned value. |
| Window commands such as `setWindowTitle()` | `Status`; handle its error before proceeding. |
| `ui.createElement(...)` | `ElementBuilder<...>`; construction/identity failures propagate to the enclosing frame. |
| Builder setters such as `setParameters(...)` | `ElementBuilder&`; chain directly and abandon the builder/frame if an assignment or merge throws. |
| `builder.draw()`, `builder.construct()`, `ui.drawConstructed()` | `void`; failures propagate and mark the frame failed. |

For example, manager access keeps ownership in the app:

```cpp
auto manager = application->fonts();
if (!manager)
    return 1;
FlowUi::FontManager& fonts = manager->get();
```

`runFrame()` accepts a callback returning either `void` or `Status`. A failing callback `Status` becomes the frame result with the same error. UI helpers also retain the first failure on their frame, so an ignored helper failure still cancels that frame at its completion boundary. Manual UI builds must inspect helper results and the results of `endFrame()` and `drawFrame()`; `beginFrame()` has already returned before manually submitted build code runs.

Configure and build:

```sh
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target rainbow_app -j
```

Run the resulting executable from the build directory. If developer systems are enabled, Ctrl+Shift+D toggles the dedicated developer window. A production build can set `FLOW_UI_DEV_MODE=OFF` without changing the element code.

Continue with [custom elements](docs/tutorials/custom_elements.md), [element identity and lifetime](docs/concepts/element_system.md), or [input fields and shortcuts](docs/tutorials/input_fields_and_shortcuts.md).
