# FlowUi Quick Start

Fast-track a new C++ app from an empty folder to a running FlowUi window with a small custom element.

## Contents

- [Requirements](#requirements)
- [Chapter 1: Set Up a FlowUi Project](#chapter-1-set-up-a-flowui-project)
  - [Create the Project Layout](#create-the-project-layout)
  - [Add FlowUi as an External Library](#add-flowui-as-an-external-library)
  - [Write the Root CMake File](#write-the-root-cmake-file)
  - [Add the First Source Files](#add-the-first-source-files)
  - [Configure and Build](#configure-and-build)
- [Chapter 2: Build the First FlowUi App](#chapter-2-build-the-first-flowui-app)
  - [Create the App Config](#create-the-app-config)
  - [Write the App Lifecycle](#write-the-app-lifecycle)
  - [Add a Custom Rainbow Button Element](#add-a-custom-rainbow-button-element)
  - [Initialize Element Resources](#initialize-element-resources)
  - [Draw the Element](#draw-the-element)
- [Complete Example Files](#complete-example-files)

## Requirements

FlowUi is a C++23 Vulkan-first UI library. Before creating an app, make sure these are installed:

- **CMake 3.20 or newer**
  - Download: <https://cmake.org/download/>
  - Check:

    ```bash
    cmake --version
    ```

- **A C++23-capable compiler**
  - Linux: recent Clang or GCC.
  - Windows: recent MSVC from Visual Studio 2022.
  - macOS: recent Apple Clang may work, but Vulkan setup usually needs extra care through MoltenVK.

- **Vulkan SDK**
  - Download: <https://vulkan.lunarg.com/sdk/home>
  - FlowUi can use `glslc` when available and falls back to `glslangValidator`.
  - Check:

    ```bash
    vulkaninfo --summary
    glslc --version
    ```

- **Git**
  - Download: <https://git-scm.com/downloads>
  - Used here to pull FlowUi into the example application's `external/` folder.

- **GLFW**
  - FlowUi can use its vendored GLFW submodule when present.
  - If you do not use the vendored GLFW submodule, install system `glfw3` so CMake can find it.

## Chapter 1: Set Up a FlowUi Project

This chapter creates a small app project that vendors FlowUi as an external dependency and proves the project compiles.

The app will use this layout:

```text
RainbowApp/
├── CMakeLists.txt
├── external/
│   └── FlowUi/
├── include/
│   └── application.hpp
└── src/
    └── main.cpp
```

### Create the Project Layout

```bash
mkdir RainbowApp
cd RainbowApp

mkdir src include external
```

### Add FlowUi as an External Library

From the root of `RainbowApp`, clone FlowUi into `external/FlowUi`.
Replace `<flowui-repository-url>` with the Git URL for the FlowUi repository you want to use:

```bash
git clone --recursive <flowui-repository-url> external/FlowUi
```

If you already cloned without `--recursive`, initialize FlowUi's submodules afterward:

```bash
git -C external/FlowUi submodule update --init --recursive
```

For a real project, prefer pinning FlowUi to a known commit or release tag:

```bash
git -C external/FlowUi fetch --tags
git -C external/FlowUi checkout <version-tag-or-commit>
git -C external/FlowUi submodule update --init --recursive
```

### Write the Root CMake File

Create `CMakeLists.txt` in the `RainbowApp` root:

```cmake
cmake_minimum_required(VERSION 3.20)

project(RainbowApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable FlowUi's developer-mode APIs and debug tooling for this tutorial.
set(FLOW_UI_DEV_MODE ON CACHE BOOL "" FORCE)

add_subdirectory(external/FlowUi)

add_executable(rainbow_app
    src/main.cpp
)

target_include_directories(rainbow_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rainbow_app
    PRIVATE
        FlowUi::FlowUi
)
```

This uses FlowUi's default build options and only turns developer mode on.

### Add the First Source Files

Create `include/application.hpp`:

```cpp
#pragma once

#include <FlowUi/Flow.hpp>
```

Create `src/main.cpp`:

```cpp
#include "application.hpp"

#include <iostream>

int main() {
    std::cout << "Hello from RainbowApp.\n";
    return 0;
}
```

This first program does not open a FlowUi window yet. It is only a fast compile/link check that your CMake project can see FlowUi.

### Configure and Build

From the `RainbowApp` root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Run the app:

```bash
./build/rainbow_app
```

On Windows with a multi-config generator, use:

```powershell
cmake -S . -B build
cmake --build build --config Debug --parallel
.\build\Debug\rainbow_app.exe
```

If this prints `Hello from RainbowApp.`, Chapter 1 is done.

## Chapter 2: Build the First FlowUi App

This chapter turns the compile check into a real FlowUi app:

- Create an `AppConfig`.
- Start FlowUi with `FlowUi::makeApplication`.
- Run the `while (!app.shouldClose())` lifecycle loop.
- Add one custom element that cycles through rainbow colors when pressed.

### Create the App Config

Replace `src/main.cpp` with a first FlowUi window:

```cpp
#include "application.hpp"

int main() {
    FlowUi::AppConfig config{};
    config.window.title = "RainbowApp";
    config.window.width = 960;
    config.window.height = 540;
    config.dev.enabled = true;

    FlowUi::App app = FlowUi::makeApplication(config);

    while (!app.shouldClose()) {
        app.beginFrame();
        app.endFrame();
        app.drawFrame();
    }

    return 0;
}
```

Build and run again:

```bash
cmake --build build --parallel
./build/rainbow_app
```

You should now get a FlowUi window. It is empty because no UI has been drawn yet.

### Write the App Lifecycle

The core FlowUi lifecycle is:

```cpp
while (!app.shouldClose()) {
    app.beginFrame();

    // Build UI here.

    app.endFrame();
    app.drawFrame();
}
```

The order matters:

- `beginFrame()` polls input and starts the UI frame.
- UI construction happens between `beginFrame()` and `endFrame()`.
- `endFrame()` finalizes Clay render commands and frame-dependent resources.
- `drawFrame()` records and presents the Vulkan frame.

### Add a Custom Rainbow Button Element

Now update `include/application.hpp` with a real element. We can follow the shape of `template.hpp`.

`FlowUi::ElementDefinition` has three template struct types: parameters, state, and resources. Flow elements also have seven function pointers that can be set for interaction, logic, construction, and drawing.

`template.hpp` already has those pieces written with placeholders, so we can copy that structure and replace the placeholder names with our own. Start with the structs, then fill in the function pointers.

#### Define Parameters Struct

Parameters are per-instance data that is created and passed into the element each frame. Store values here when you might want different element instances to draw with different settings.

In this example, the parameters store a `Clay_Sizing` value and a `cornerRadius` float.

```cpp
struct RainbowButtonParams {
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIXED(240.0f),
        .height = CLAY_SIZING_FIXED(120.0f),
    };
    float cornerRadius = 12.0f;
};
```

#### Define State Struct

State is persistent data storage that also exists **per instance**. Unlike the parameters struct, state is not recreated each frame.

Use state to keep track of element-specific data that should survive across frames. In this example, a single `int` tracks which background color to use.

```cpp
struct RainbowButtonState {
    int colorIndex = 0;
};
```

#### Define Resources Struct

Resources are also persistent data storage, but unlike state, resources are stored **per element definition**, not per instance.

Use resources for data or objects that are shared by every instance of the element. Resource structs also need a constructor that can receive a `FlowUi::App&`.

In this example, the button only cycles through rainbow colors, so we create the color array once and store one `std::array` for all future `RainbowButton` elements to use.

```cpp
struct RainbowButtonResources {
    std::array<Clay_Color, 7> rainbowColors{
        FlowUi::Flow_Color("#ff3b30ff"), // red
        FlowUi::Flow_Color("#ff9500ff"), // orange
        FlowUi::Flow_Color("#ffcc00ff"), // yellow
        FlowUi::Flow_Color("#34c759ff"), // green
        FlowUi::Flow_Color("#007affff"), // blue
        FlowUi::Flow_Color("#5856d6ff"), // indigo
        FlowUi::Flow_Color("#af52deff"), // violet
    };

    RainbowButtonResources() = default;
    explicit RainbowButtonResources(FlowUi::App& app) {
        (void)app;
    }
};
```

#### Write the Element Definition

To use Flow elements, we pass element definitions to the UI manager. Because of this, it is useful to store the element definition in a named `const` variable.

Using a type alias keeps the later callback code shorter:

```cpp
using RainbowButtonDefinition = FlowUi::ElementDefinition<
    RainbowButtonParams,
    RainbowButtonState,
    RainbowButtonResources,
    FLOW_DEF_ID("rainbow_button")>;

inline const RainbowButtonDefinition kRainbowButton = { /* Function pointers set here. */ };
```

#### Write the Necessary Function Pointers

Flow elements have seven function pointers that can be set. Each one is triggered during the app lifecycle by events or explicit draw calls.

Each callback receives either an `InteractionContext` or a `BuildContext`, matching the placeholders shown in `template.hpp`.

Without going over every function pointer in detail, focus on the two callbacks this example needs: `onPressed` and `buildElement`.

The `onPressed` callback runs when the user presses mouse button 1 on this element. The `buildElement` callback runs when this element is drawn between `beginFrame()` and `endFrame()`.

Therefore, `onPressed` will look like this:

```cpp
// onPressed
+[](RainbowButtonDefinition::InteractionContext& context) {
    RainbowButtonState& state =
        RainbowButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
    state.colorIndex = (state.colorIndex + 1) % 7;
}
```

And `buildElement` will look like this:

```cpp
// buildElement
+[](RainbowButtonDefinition::BuildContext& context) {
    RainbowButtonState& state =
        RainbowButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
    RainbowButtonResources& resources = RainbowButtonDefinition::resources.value();

    const Clay_Color color = resources.rainbowColors[
        static_cast<std::size_t>(state.colorIndex) % resources.rainbowColors.size()];

    Clay_ElementDeclaration root{};
    root.layout.sizing = context.params.sizing;
    root.backgroundColor = color;
    root.cornerRadius = CLAY_CORNER_RADIUS(context.params.cornerRadius);

    CLAY(context.uiManager.toClayEID(context.elementID), root) {}
}
```

#### Write the Final Element Definition Variable

Now that we know which callbacks to use, we can finish the `const` variable by setting every unused function pointer to `nullptr`:

```cpp
inline const RainbowButtonDefinition kRainbowButton = {
    // onHovered
    nullptr,

    // onPressed
    +[](RainbowButtonDefinition::InteractionContext& context) {
        RainbowButtonState& state =
            RainbowButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        state.colorIndex = (state.colorIndex + 1) % 7;
    },

    // onHeld
    nullptr,

    // onReleased
    nullptr,

    // runLogic
    nullptr,

    // constructElement
    nullptr,

    // buildElement
    +[](RainbowButtonDefinition::BuildContext& context) {
        RainbowButtonState& state =
            RainbowButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        RainbowButtonResources& resources = RainbowButtonDefinition::resources.value();

        const Clay_Color color = resources.rainbowColors[
            static_cast<std::size_t>(state.colorIndex) % resources.rainbowColors.size()];

        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.backgroundColor = color;
        root.cornerRadius = CLAY_CORNER_RADIUS(context.params.cornerRadius);

        CLAY(context.uiManager.toClayEID(context.elementID), root) {}
    },
};
```

#### Write a Small Helper and Register to Developer Mode

Element resources need a reference to `FlowUi::App` for initialization. Since they should be created once, ideally before the main lifecycle loop, we can write a small inline helper and call it from `main.cpp`.

```cpp
inline void initializeApplicationResources(FlowUi::App& app) {
    (void)RainbowButtonDefinition::getResources(app);
}
```

The project is also set up in developer mode, meaning we can use dev tools. To make the element definition and its data editable in dev tools, register the element and its structs with the provided macros.

For definition editing, make sure the parameter struct fields are registered under the parameter struct definition:

```cpp
FLOWUI_DEV_REGISTER_STRUCT(
    RainbowButtonParams,
    FLOWUI_DEV_REFLECT_FIELD(RainbowButtonParams, sizing),
    FLOWUI_DEV_REFLECT_FIELD(RainbowButtonParams, cornerRadius));

FLOWUI_DEV_REGISTER_STRUCT(
    RainbowButtonState,
    FLOWUI_DEV_REFLECT_FIELD(RainbowButtonState, colorIndex));

FLOWUI_DEV_REGISTER_STRUCT(RainbowButtonResources);

FLOWUI_DEV_REGISTER_ELEMENT(RainbowButtonDefinition, "Rainbow Button");
```

#### Final look at `application.hpp`

At the end, `include/application.hpp` should look like this:

```cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <FlowUi/Flow.hpp>
#include <devMode/devApi.hpp>

struct RainbowButtonParams {
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIXED(240.0f),
        .height = CLAY_SIZING_FIXED(120.0f),
    };
    float cornerRadius = 12.0f;
};

struct RainbowButtonState {
    int colorIndex = 0;
};

struct RainbowButtonResources {
    std::array<Clay_Color, 7> rainbowColors{
        FlowUi::Flow_Color("#ff3b30ff"), // red
        FlowUi::Flow_Color("#ff9500ff"), // orange
        FlowUi::Flow_Color("#ffcc00ff"), // yellow
        FlowUi::Flow_Color("#34c759ff"), // green
        FlowUi::Flow_Color("#007affff"), // blue
        FlowUi::Flow_Color("#5856d6ff"), // indigo
        FlowUi::Flow_Color("#af52deff"), // violet
    };

    RainbowButtonResources() = default;
    explicit RainbowButtonResources(FlowUi::App& app) {
        (void)app;
    }
};

using RainbowButtonDefinition = FlowUi::ElementDefinition<
    RainbowButtonParams,
    RainbowButtonState,
    RainbowButtonResources,
    FLOW_DEF_ID("rainbow_button")>;

inline const RainbowButtonDefinition kRainbowButton = {
    // onHovered
    nullptr,

    // onPressed
    +[](RainbowButtonDefinition::InteractionContext& context) {
        RainbowButtonState& state =
            RainbowButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        state.colorIndex = (state.colorIndex + 1) % 7;
    },

    // onHeld
    nullptr,

    // onReleased
    nullptr,

    // runLogic
    nullptr,

    // constructElement
    nullptr,

    // buildElement
    +[](RainbowButtonDefinition::BuildContext& context) {
        RainbowButtonState& state =
            RainbowButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        RainbowButtonResources& resources = RainbowButtonDefinition::resources.value();

        const Clay_Color color = resources.rainbowColors[
            static_cast<std::size_t>(state.colorIndex) % resources.rainbowColors.size()];

        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.backgroundColor = color;
        root.cornerRadius = CLAY_CORNER_RADIUS(context.params.cornerRadius);

        CLAY(context.uiManager.toClayEID(context.elementID), root) {}
    },
};

inline void initializeApplicationResources(FlowUi::App& app) {
    (void)RainbowButtonDefinition::getResources(app);
}

FLOWUI_DEV_REGISTER_STRUCT(
    RainbowButtonParams,
    FLOWUI_DEV_REFLECT_FIELD(RainbowButtonParams, sizing),
    FLOWUI_DEV_REFLECT_FIELD(RainbowButtonParams, cornerRadius));

FLOWUI_DEV_REGISTER_STRUCT(
    RainbowButtonState,
    FLOWUI_DEV_REFLECT_FIELD(RainbowButtonState, colorIndex));

FLOWUI_DEV_REGISTER_STRUCT(RainbowButtonResources);

FLOWUI_DEV_REGISTER_ELEMENT(RainbowButtonDefinition, "Rainbow Button");
```

What this element does:

- `RainbowButtonParams` controls the element size and corner radius.
- `RainbowButtonState` stores the active color index per element instance.
- `RainbowButtonResources` stores the constant rainbow color palette.
- `onPressed` advances the color index.
- `buildElement` emits one Clay node using the current color.

### Initialize Element Resources

`RainbowButtonDefinition::resources` is lazy, but this element expects resources to exist before drawing. Call `initializeApplicationResources(app)` after `makeApplication`.

Update `src/main.cpp`:

```cpp
#include "application.hpp"

int main() {
    FlowUi::AppConfig config{};
    config.window.title = "RainbowApp";
    config.window.width = 960;
    config.window.height = 540;
    config.dev.enabled = true;

    FlowUi::App app = FlowUi::makeApplication(config);
    initializeApplicationResources(app);

    while (!app.shouldClose()) {
        app.beginFrame();

        app.endFrame();
        app.drawFrame();
    }

    return 0;
}
```

### Draw the Element

Now draw the button between `beginFrame()` and `endFrame()`:

```cpp
while (!app.shouldClose()) {
    app.beginFrame();

    app.ui()
        .createElement(kRainbowButton, "main/rainbow-button")
        .setParameters(RainbowButtonParams{
            .cornerRadius = 24.0f,
        })
        .draw();

    app.endFrame();
    app.drawFrame();
}
```

The default size still comes from `RainbowButtonParams`, but this call overrides `cornerRadius` to demonstrate per-instance parameters.

Build and run:

```bash
cmake --build build --parallel
./build/rainbow_app
```

Click the colored rectangle. Each press should advance to the next rainbow color.

## Complete Example Files

At this point, the important files should look like this.

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)

project(RainbowApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FLOW_UI_DEV_MODE ON CACHE BOOL "" FORCE)

add_subdirectory(external/FlowUi)

add_executable(rainbow_app
    src/main.cpp
)

target_include_directories(rainbow_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rainbow_app
    PRIVATE
        FlowUi::FlowUi
)
```

`include/application.hpp`:

```cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <FlowUi/Flow.hpp>
#include <devMode/devApi.hpp>

struct RainbowButtonParams {
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIXED(240.0f),
        .height = CLAY_SIZING_FIXED(120.0f),
    };
    float cornerRadius = 12.0f;
};

struct RainbowButtonState {
    int colorIndex = 0;
};

struct RainbowButtonResources {
    std::array<Clay_Color, 7> rainbowColors{
        FlowUi::Flow_Color("#ff3b30ff"),
        FlowUi::Flow_Color("#ff9500ff"),
        FlowUi::Flow_Color("#ffcc00ff"),
        FlowUi::Flow_Color("#34c759ff"),
        FlowUi::Flow_Color("#007affff"),
        FlowUi::Flow_Color("#5856d6ff"),
        FlowUi::Flow_Color("#af52deff"),
    };

    RainbowButtonResources() = default;
    explicit RainbowButtonResources(FlowUi::App& app) {
        (void)app;
    }
};

using RainbowButtonDefinition = FlowUi::ElementDefinition<
    RainbowButtonParams,
    RainbowButtonState,
    RainbowButtonResources,
    FLOW_DEF_ID("rainbow_button")>;

inline const RainbowButtonDefinition kRainbowButton = {
    nullptr,
    +[](RainbowButtonDefinition::InteractionContext& context) {
        RainbowButtonState& state =
            RainbowButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        state.colorIndex = (state.colorIndex + 1) % 7;
    },
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    +[](RainbowButtonDefinition::BuildContext& context) {
        RainbowButtonState& state =
            RainbowButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        RainbowButtonResources& resources = RainbowButtonDefinition::resources.value();

        const Clay_Color color = resources.rainbowColors[
            static_cast<std::size_t>(state.colorIndex) % resources.rainbowColors.size()];

        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.backgroundColor = color;
        root.cornerRadius = CLAY_CORNER_RADIUS(context.params.cornerRadius);

        CLAY(context.uiManager.toClayEID(context.elementID), root) {}
    },
};

inline void initializeApplicationResources(FlowUi::App& app) {
    (void)RainbowButtonDefinition::getResources(app);
}

FLOWUI_DEV_REGISTER_STRUCT(
    RainbowButtonParams,
    FLOWUI_DEV_REFLECT_FIELD(RainbowButtonParams, sizing),
    FLOWUI_DEV_REFLECT_FIELD(RainbowButtonParams, cornerRadius));

FLOWUI_DEV_REGISTER_STRUCT(
    RainbowButtonState,
    FLOWUI_DEV_REFLECT_FIELD(RainbowButtonState, colorIndex));

FLOWUI_DEV_REGISTER_STRUCT(RainbowButtonResources);

FLOWUI_DEV_REGISTER_ELEMENT(RainbowButtonDefinition, "Rainbow Button");
```

`src/main.cpp`:

```cpp
#include "application.hpp"

int main() {
    FlowUi::AppConfig config{};
    config.window.title = "RainbowApp";
    config.window.width = 960;
    config.window.height = 540;
    config.dev.enabled = true;

    FlowUi::App app = FlowUi::makeApplication(config);
    initializeApplicationResources(app);

    while (!app.shouldClose()) {
        app.beginFrame();

        app.ui()
            .createElement(kRainbowButton, "main/rainbow-button")
            .setParameters(RainbowButtonParams{
                .cornerRadius = 24.0f,
            })
            .draw();

        app.endFrame();
        app.drawFrame();
    }

    return 0;
}
```
