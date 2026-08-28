# FlowUi Configuration Editor & In-Process Hot-Restart Architecture

## Executive Summary

This specification details the complete design and implementation plan for **Runtime `AppConfig` Editing**, **In-Process App Hot-Restarting**, and **C++ Source Code Baking** in FlowUi.

The system provides a two-phase workflow:
1. **Dynamic In-Process Hot-Restart**: Edits made in the Dev Panel are saved to persistent runtime storage (`.flowui/overrides.v1.json`). The running `App` object is cleanly torn down (releasing Vulkan swapchains, windows, and UI managers) and immediately re-instantiated via `makeApplication(newConfig)` within the same process execution context.
2. **Permanent Source Code Baking**: When requested, staged config changes are exported to `.flowui/dev_changes.json`. During the next build, the `flowui_devChange_updater` tool parses C++ source files (`main.cpp`) and updates C++20 designated initializers or assignment statements to permanently bake the new baseline defaults into code.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Runtime Developer Interface                        │
│                (App Config Panel: Edit Window, Vulkan, UI, Dev)             │
└──────────────────────┬───────────────────────────────┬──────────────────────┘
                       │                               │
       [Apply & Hot-Restart]               [Bake Changes to C++ Source]
                       │                               │
                       ▼                               ▼
┌──────────────────────────────┐              ┌──────────────────────────────┐
│  .flowui/overrides.v1.json   │              │  .flowui/dev_changes.json    │
└──────────────┬───────────────┘              └──────────────┬───────────────┘
               │                                             │
               ▼                                             ▼
┌──────────────────────────────┐              ┌──────────────────────────────┐
│ Clean App Shutdown           │              │ flowui_devChange_updater     │
│  - Destroy Vulkan Surface    │              │  - Patch C++ Initializers    │
│  - Close GLFW Window         │              │  - Preserve Indent & Comments│
│ Re-call makeApplication()    │              └──────────────┬───────────────┘
│  - Re-create Window & Swap   │                             │
│  - Same Process Instance!    │                             ▼
└──────────────────────────────┘              ┌──────────────────────────────┐
                                              │  main.cpp Source Code        │
                                              └──────────────────────────────┘
```

---

## 1. Concrete Reflection & Schema Layer

FlowUi uses `FLOWUI_DEV_REGISTER_STRUCT` to register field offset and type metadata for runtime inspection and modification.

### 1.1 Structural Reflection Registration (`include/FlowUi/AppConfigReflection.hpp`)

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"

#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

namespace FlowUi::dev {

// 1. DevShortcutChord Reflection
FLOWUI_DEV_REGISTER_STRUCT(FlowUi::DevShortcutChord,
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevShortcutChord, key),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevShortcutChord, ctrl),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevShortcutChord, shift),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevShortcutChord, alt),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevShortcutChord, super),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevShortcutChord, trigger));

// 2. WindowConfig Reflection
FLOWUI_DEV_REGISTER_STRUCT(FlowUi::WindowConfig,
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::WindowConfig, width),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::WindowConfig, height),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::WindowConfig, title),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::WindowConfig, resizable),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::WindowConfig, decorated),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::WindowConfig, maximized),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::WindowConfig, fullscreen),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::WindowConfig, highDPI));

// 3. DevToolsConfig Reflection
FLOWUI_DEV_REGISTER_STRUCT(FlowUi::DevToolsConfig,
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevToolsConfig, enabled),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevToolsConfig, useShortcutManagerForPanelToggle),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevToolsConfig, panelToggleChord),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevToolsConfig, excludeInternalDevElementsFromCapture),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::DevToolsConfig, overridesPath));

// 4. IconManagerConfig Reflection
FLOWUI_DEV_REGISTER_STRUCT(FlowUi::IconManagerConfig,
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::IconManagerConfig, atlasSize),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::IconManagerConfig, maxAtlasPages),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::IconManagerConfig, sizeBucketStep),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::IconManagerConfig, atlasPadding));

// 5. AppConfig Top-Level Reflection
FLOWUI_DEV_REGISTER_STRUCT(FlowUi::AppConfig,
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::AppConfig, window),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::AppConfig, vk),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::AppConfig, ui),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::AppConfig, iconManager),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::AppConfig, errors),
    FLOWUI_DEV_REFLECT_FIELD(FlowUi::AppConfig, dev));

} // namespace FlowUi::dev
#endif
```

### 1.2 Dot-Notation Property Accessor (`include/devSystems/devTooling/DevConfigAccessor.hpp`)

```cpp
#pragma once
#include <string_view>
#include <variant>
#include <string>
#include "FlowUi/PublicStructs.hpp"

namespace FlowUi::dev {

using ConfigValue = std::variant<bool, int32_t, uint32_t, float, std::string>;

class DevConfigAccessor {
public:
    static bool getValue(const AppConfig& config, std::string_view fieldPath, ConfigValue& outValue);
    static bool setValue(AppConfig& config, std::string_view fieldPath, const ConfigValue& newValue);
};

// Example implementation snippet:
inline bool DevConfigAccessor::setValue(AppConfig& config, std::string_view fieldPath, const ConfigValue& newValue) {
    if (fieldPath == "window.width") {
        if (auto* val = std::get_if<int32_t>(&newValue)) { config.window.width = *val; return true; }
    } else if (fieldPath == "window.height") {
        if (auto* val = std::get_if<int32_t>(&newValue)) { config.window.height = *val; return true; }
    } else if (fieldPath == "window.title") {
        if (auto* val = std::get_if<std::string>(&newValue)) { config.window.title = *val; return true; }
    } else if (fieldPath == "window.fullscreen") {
        if (auto* val = std::get_if<bool>(&newValue)) { config.window.fullscreen = *val; return true; }
    } else if (fieldPath == "dev.panelToggleChord.key") {
        if (auto* val = std::get_if<int32_t>(&newValue)) { config.dev.panelToggleChord.key = *val; return true; }
    }
    return false;
}

} // namespace FlowUi::dev
```

---

## 2. Dynamic In-Process Hot-Restart Architecture

To restart the application with new `AppConfig` settings without quitting the executable, we use an **Application Runner Loop** combined with clean `App` teardown semantics.

### 2.1 Lifecycle State & Return Action (`include/FlowUi/AppLifecycle.hpp`)

```cpp
#pragma once

#include "FlowUi/PublicStructs.hpp"

namespace FlowUi {

enum class AppRunAction : uint8_t {
    Exit = 0,
    Restart = 1,
};

struct AppRunResult {
    AppRunAction action = AppRunAction::Exit;
    int exitCode = 0;
    AppConfig nextConfig{};
};

} // namespace FlowUi
```

### 2.2 Re-Entrant Runner (`include/FlowUi/AppRunner.hpp`)

Instead of calling `makeApplication` once in `main()`, applications wrap startup in `FlowUi::runApplicationLoop()`:

```cpp
#pragma once

#include <functional>
#include "FlowUi/App.hpp"
#include "FlowUi/AppLifecycle.hpp"

namespace FlowUi {

using AppBuildCallback = std::function<void(App& app)>;

inline int runApplicationLoop(AppConfig initialConfig, AppBuildCallback userCallback) {
    AppConfig currentConfig = initialConfig;

    while (true) {
        // 1. Instantiate application from current configuration
        App app = makeApplication(currentConfig);

        // 2. Register user application UI & logic callbacks
        userCallback(app);

        // 3. Enter event loop until exit or hot-restart request
        AppRunResult result = app.exec();

        // 4. Clean scope exit destroys app (tears down Vulkan, GLFW, UI managers)
        if (result.action == AppRunAction::Restart) {
            currentConfig = result.nextConfig; // Pass updated config to next iteration
            continue; // Re-calls makeApplication(currentConfig) instantly in same process!
        }

        return result.exitCode; // Normal exit
    }
}

} // namespace FlowUi
```

### 2.3 User Main Entrypoint Example (`src/main.cpp`)

```cpp
#include "FlowUi/App.hpp"
#include "FlowUi/AppRunner.hpp"

int main(int argc, char** argv) {
    FlowUi::AppConfig config{
        .window = {
            .width = 1280,
            .height = 720,
            .title = "FlowUi Application"
        },
        .dev = {
            .enabled = true,
            .overridesPath = ".flowui/overrides.v1.json"
        }
    };

    return FlowUi::runApplicationLoop(config, [](FlowUi::App& app) {
        // Application frame setup
        app.setUiCallback([](FlowUi::UiManager& ui) {
            // UI elements...
        });
    });
}
```

### 2.4 Dev Panel Hot-Restart Trigger Logic

Inside `DevInterface` / `DevTools`, when the user clicks **"Apply & Hot-Restart App"**:

```cpp
void DevTools::requestInProcessRestart(const AppConfig& editedConfig) {
    // 1. Persist edits to runtime overrides file
    saveOverridesToFile(editedConfig, ".flowui/overrides.v1.json");

    // 2. Flag restart on active App instance
    app_.requestRestart(editedConfig); // Causes app.exec() to exit with AppRunAction::Restart
}
```

---

## 3. Runtime Dev Config Editor UI

The Dev Panel introduces an **"App Config"** tab implemented in FSEL (`include/devSystems/devInterface`).

```cpp
void renderAppConfigEditorTab(FlowUi::UiManager& ui, DevRuntimeState& devState) {
    ui.VStack([](auto& col) {
        col.padding(12);
        col.spacing(8);

        // Information Header
        col.Label("Application Configuration Editor");
        col.Label("Edits can be applied instantly via Hot-Restart or baked to main.cpp for the next build.");

        // Accordion Group 1: Native Window Configuration
        col.CollapsingHeader("Native Window (window)", [&](auto& winSec) {
            winSec.InputInt("Width", &devState.stagedConfig.window.width);
            winSec.InputInt("Height", &devState.stagedConfig.window.height);
            winSec.InputText("Title", &devState.stagedConfig.window.title);
            winSec.Checkbox("Resizable", &devState.stagedConfig.window.resizable);
            winSec.Checkbox("Decorated", &devState.stagedConfig.window.decorated);
            winSec.Checkbox("Fullscreen", &devState.stagedConfig.window.fullscreen);
            winSec.Checkbox("High DPI", &devState.stagedConfig.window.highDPI);
        });

        // Accordion Group 2: Developer Tooling Settings
        col.CollapsingHeader("Developer Tools (dev)", [&](auto& devSec) {
            devSec.Checkbox("Enable Dev Tools", &devState.stagedConfig.dev.enabled);
            devSec.Checkbox("Use Panel Toggle Shortcut", &devState.stagedConfig.dev.useShortcutManagerForPanelToggle);
            devSec.InputKeyChord("Panel Toggle Shortcut", &devState.stagedConfig.dev.panelToggleChord);
            devSec.InputText("Overrides Export Path", &devState.stagedConfig.dev.overridesPath);
        });

        // Action Buttons Row
        col.HStack([](auto& btnRow) {
            if (btnRow.Button("Revert Edits")) {
                devState.stagedConfig = devState.baselineConfig;
            }
            if (btnRow.Button("Apply & Hot-Restart App")) {
                devState.tools.requestInProcessRestart(devState.stagedConfig);
            }
            if (btnRow.Button("Bake Changes to C++ Source")) {
                devState.tools.exportDevChangesManifest(devState.stagedConfig);
            }
        });
    });
}
```

---

## 4. Change Manifest JSON Extension

The `.flowui/dev_changes.json` format is extended to include a `"config"` section alongside `"definitions"` and `"instances"`.

### `.flowui/dev_changes.json` Example

```json
{
  "$schema": "https://flowui.dev/schemas/dev_changes.v1.json",
  "version": 1,
  "config": {
    "sourceFile": "/home/user/project/src/main.cpp",
    "sourceLine": 8,
    "sourceColumn": 5,
    "targetSymbol": "config",
    "changes": [
      {
        "fieldPath": "window.width",
        "value": 1920,
        "type": "int"
      },
      {
        "fieldPath": "window.height",
        "value": 1080,
        "type": "int"
      },
      {
        "fieldPath": "window.title",
        "value": "FlowUi Baked Production App",
        "type": "string"
      },
      {
        "fieldPath": "dev.panelToggleChord.key",
        "value": 70,
        "type": "int"
      }
    ]
  },
  "definitions": [],
  "instances": []
}
```

---

## 5. Source Code Baking Engine (`flowui_devChange_updater`)

The build-time tool `tools/flowui_devChange_updater/flowui_devChange_updater.cpp` parses the C++ source file located at `sourceFile` and updates initializer blocks in place.

### 5.1 Updater Data Structures

```cpp
struct ConfigFieldChange {
    std::string fieldPath;
    JsonValue value;
    std::string typeName;
};

struct ParsedConfigTarget {
    std::string sourceFile;
    uint32_t sourceLine = 0;
    uint32_t sourceColumn = 0;
    std::string targetSymbol;
    std::vector<ConfigFieldChange> changes;
};
```

### 5.2 Designated Initializer Patching Engine

```cpp
bool patchConfigTargetInSource(
    std::string& sourceContent,
    const ParsedConfigTarget& target,
    std::string& outError)
{
    std::vector<std::string> lines = splitLines(sourceContent);
    if (target.sourceLine == 0 || target.sourceLine > lines.size()) {
        outError = "Invalid target source line number.";
        return false;
    }

    const std::size_t lineIdx = target.sourceLine - 1;
    
    // Scan down from sourceLine for designated initializers matching requested field paths
    for (const auto& change : target.changes) {
        // Build regex pattern for field e.g. \.width\s*=\s*[^,\n\}]+
        std::string leafName = getLeafFieldName(change.fieldPath); // "width"
        std::string pattern = "\\." + leafName + "\\s*=\\s*[^,\\n\\}]+";
        
        std::string replacement = "." + leafName + " = " + formatJsonValueToCpp(change.value);
        
        bool replaced = false;
        for (std::size_t i = lineIdx; i < std::min(lines.size(), lineIdx + 50); ++i) {
            if (regexReplaceFirst(lines[i], pattern, replacement)) {
                replaced = true;
                break;
            }
        }

        // If field initializer was not present, insert it into the designated initializer block
        if (!replaced) {
            insertDesignatedInitializerLine(lines, lineIdx, change.fieldPath, change.value);
        }
    }

    sourceContent = joinLines(lines);
    return true;
}
```

### 5.3 C++ Value Formatter

```cpp
std::string formatJsonValueToCpp(const JsonValue& val) {
    switch (val.kind) {
        case JsonValue::Kind::Bool:
            return val.boolValue ? "true" : "false";
        case JsonValue::Kind::Number:
            return val.text;
        case JsonValue::Kind::String:
            return "\"" + val.text + "\"";
        default:
            return "{}";
    }
}
```

---

## 6. Verification and Integration Matrix

| Subsystem | Feature / Mechanism | Implementation Target |
| :--- | :--- | :--- |
| **Reflection** | `AppConfig` struct field registration | [`include/FlowUi/AppConfigReflection.hpp`](file:///home/lkukhale/kodi/FlowUi/include/FlowUi/PublicStructs.hpp#L422) |
| **Dev UI** | Interactive App Config property editor tab | [`include/devSystems/devInterface/DevInterface.hpp`](file:///home/lkukhale/kodi/FlowUi/include/devSystems/devInterface/DevInterface.hpp) |
| **Runtime Restart** | In-process clean App teardown & re-creation | [`include/FlowUi/AppRunner.hpp`](file:///home/lkukhale/kodi/FlowUi/include/FlowUi/App.hpp#L722) |
| **Persistence** | Fast JSON serialization of edits | `.flowui/overrides.v1.json` |
| **Manifest Export** | `dev_changes.json` extension with `"config"` schema | [`include/devSystems/devTooling/DevTooling.hpp`](file:///home/lkukhale/kodi/FlowUi/include/devSystems/devTooling/DevTooling.hpp) |
| **Code Baking** | AST / Regex initializer patcher in C++ updater | [`tools/flowui_devChange_updater/flowui_devChange_updater.cpp`](file:///home/lkukhale/kodi/FlowUi/tools/flowui_devChange_updater/flowui_devChange_updater.cpp#L3594) |
