# FlowUi

FlowUi is a C++23 Vulkan-first UI runtime built around:
- [Clay](https://github.com/nicbarker/clay) for layout and command generation
- A custom Vulkan renderer (solid shapes, text, textures)
- A typed element system for reusable UI components

## Quick Start

### Requirements
- CMake `>= 3.20`
- C++23 compiler
- Vulkan SDK (`glslc` preferred)
- GLFW3
- Git submodules initialized

```bash
git submodule update --init --recursive
```

### Build

```bash
cmake -S . -B build
cmake --build build --parallel
```

### Minimal App Loop

```cpp
#include <FlowUi/Flow.hpp>

int main() {
    FlowUi::AppConfig config{};
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
                CLAY_TEXT_CONFIG({
                    .fontId = 0,
                    .fontSize = 24,
                    .textColor = Clay_Color{255, 255, 255, 255},
                    .wrapMode = CLAY_TEXT_WRAP_NONE,
                }));
        }

        app.endFrame();
        app.drawFrame();
    }
}
```

## Build Options

| Option | Default | Description |
|---|---|---|
| `FLOWUI_INSTALL` | `ON` | Enable install/package export rules |
| `FLOWUI_BUILD_FONT_BAKER` | `ON` | Build `flowui_font_baker` tool |
| `FLOWUI_ENABLE_RUNTIME_FONT_BAKING` | `OFF` | Link runtime TTF->atlas dependencies |
| `FLOWUI_PUBLIC_VULKAN_INTEROP` | `ON` | Expose viewport Vulkan interop in public headers |
| `FLOWUI_INCLUDE_SVG_MANAGER` | `ON` | Enable SVG manager support |
| `FLOW_UI_DEV_MODE` | `OFF` | Compile FlowUi developer-mode tooling (`debugView`, dev runtime capture/overrides) |
| `FLOWUI_GLFW_PROVIDER` | `auto` | `auto`, `system`, `vendored` |

## Dev Mode (Vertical Slice)

Dev mode is opt-in at compile time and runtime:

- Compile time: set `-DFLOW_UI_DEV_MODE=ON` (or define `FLOW_UI_DEV_MODE=1` before including FlowUi headers).
- Runtime: set `AppConfig::dev.enabled = true`.

Current vertical-slice behavior:

- `UiManager::beginFrame()` auto-wraps user UI with an internal root (`"_Flow_Dev_root_"`) when the dev panel is visible.
- `UiManager::endFrame()` appends the built-in `debugView` element before `Clay_EndLayout()`.
- Layout is left-to-right inside the injected root, so user UI and debug tooling are siblings in one Clay tree.
- Toggle shortcut defaults to `Ctrl + Shift + D` and is configured via `AppConfig::dev.panelToggleChord`.
- Definition-level param overrides from the debug panel propagate to all instances of the selected definition.

## Modern Element System

FlowUi no longer uses a public element registry.
Users define element types directly as typed constants and pass them to `createElement(...)`.

### 1) Define params/state/resources

```cpp
struct ButtonParams {
    std::string_view label = "Button";
    float width = 220.0f;
    float height = 44.0f;
};

struct ButtonState {
    bool enabled = true;
};

struct ButtonResources {
    explicit ButtonResources(FlowUi::UiManager& ui) {
        (void)ui;
    }
};
```

### 2) Define the typed element definition

```cpp
using ButtonDefinition = FlowUi::ElementDefinition<
    ButtonParams,
    ButtonState,
    ButtonResources,
    FLOW_DEF_ID("button")
>;

inline const ButtonDefinition kButton = {
    nullptr, // onHovered
    +[](ButtonDefinition::InteractionContext& context) { // onPressed
        const uint64_t flowId = FlowUi::toFlowId(context.elementID);
        ButtonDefinition::getOrCreateState(flowId).enabled =
            !ButtonDefinition::getOrCreateState(flowId).enabled;
    },
    nullptr, // onHeld
    nullptr, // onReleased

    nullptr, // runLogic

    nullptr, // constructElment (optional for .construct() flows)
    +[](ButtonDefinition::BuildContext& context) { // buildElement (.draw())
        const uint64_t flowId = FlowUi::toFlowId(context.elementID);
        const ButtonState& state = ButtonDefinition::getOrCreateState(flowId);
        ButtonResources& resources = ButtonDefinition::resources.value();
        (void)resources;

        Clay_ElementDeclaration root{};
        root.id = context.uiManager.toClayEID(context.elementID);
        root.layout.sizing.width = CLAY_SIZING_FIXED(context.params.width);
        root.layout.sizing.height = CLAY_SIZING_FIXED(context.params.height);
        root.backgroundColor = state.enabled
            ? Clay_Color{52, 94, 239, 255}
            : Clay_Color{90, 90, 90, 255};
        root.layout.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
        CLAY(root) {
            CLAY_TEXT(
                context.uiManager.toClayString(context.params.label),
                CLAY_TEXT_CONFIG({ .fontId = 0, .fontSize = 18, .textColor = Clay_Color{255,255,255,255} }));
        }
    },
};
```

Initialize resources once (typically during startup, before the main loop):

```cpp
(void)ButtonDefinition::getResources(app);
```

### 3) Use in frame code

```cpp
app.ui().createElement(kButton, "main_menu/play")
    .setParameters(ButtonParams{
        .label = "Play",
        .width = 260.0f,
        .height = 52.0f,
    })
    .draw();
```

## Flow IDs

FlowUi provides 64-bit ID helpers:

```cpp
FlowUi::FlowElementId id0 = FLOW_ID("main_menu/play");
FlowUi::FlowElementId id1 = FlowUi::toFlowId(dynamicStringView);
FlowUi::FlowElementId id2 = FlowUi::createIndexedFlowId("items/row", i);
FlowUi::FlowDefinitionId defId = FLOW_DEF_ID("button");
```

## State And Resources Model

For each `ElementDefinition<Params, State, Resources, DefId>` specialization:
- `resources` is static-lazy:
  - `Definition::getResources(app)`
- `statePool` is static and keyed by Flow element ID:
  - `Definition::getOrCreateState(flowId)`
  - `Definition::tryGetState(flowId)`
  - `Definition::tryGetStateConst(flowId)`
  - `Definition::eraseState(flowId)`

This means state/resources are shared by all instances of that exact definition specialization.

## Core Runtime Lifecycle

- `app.beginFrame()`
- build UI (`CLAY(...)` + `ui.createElement(...).draw()` / `.construct()`)
- `app.endFrame()`
- `app.drawFrame()`

## Notes

- `template.hpp`, `template.cpp`, and `example.cpp` are updated to this typed system.
- `createElement(...)` still takes string element IDs for Clay interop; Flow IDs are opt-in where needed.
- API is pre-1.0 and still evolving.
