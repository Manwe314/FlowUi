# Developer Mode

This tutorial shows how to make FlowUi elements visible to developer mode, edit their registered fields at runtime, export those changes as JSON, and use the updater tool to fold those exported values back into source.

## Contents

- [Chapter 1: What We Are Building](#chapter-1-what-we-are-building)
  - [The Target](#the-target)
  - [The Runtime Flow](#the-runtime-flow)
- [Chapter 2: Enable Developer Mode](#chapter-2-enable-developer-mode)
  - [App Config](#app-config)
  - [Panel Toggle](#panel-toggle)
- [Chapter 3: Register Editable Types](#chapter-3-register-editable-types)
  - [Register Enums](#register-enums)
  - [Register Parameter Structs](#register-parameter-structs)
  - [Register State and Resource Structs](#register-state-and-resource-structs)
- [Chapter 4: Register Elements](#chapter-4-register-elements)
  - [Element Shape](#element-shape)
  - [Registration](#registration)
  - [What the Registry Stores](#what-the-registry-stores)
- [Chapter 5: Edit Instances and Definitions](#chapter-5-edit-instances-and-definitions)
  - [Instance Overrides](#instance-overrides)
  - [Definition Overrides](#definition-overrides)
  - [Why Source Location Matters](#why-source-location-matters)
- [Chapter 6: Export JSON](#chapter-6-export-json)
  - [Export Path](#export-path)
  - [Manual Export](#manual-export)
  - [What the JSON Means](#what-the-json-means)
- [Chapter 7: Apply Exported Changes](#chapter-7-apply-exported-changes)
  - [Build the Updater](#build-the-updater)
  - [Run the Updater](#run-the-updater)
  - [Review the Result](#review-the-result)
- [Final Shape](#final-shape)

## Chapter 1: What We Are Building

### The Target

Assume the app has reusable dashboard cards. A card has a tone enum, a title, a root Clay declaration, and a few resource defaults. During development, we want to open the dev panel, select a card instance, tweak colors, padding, and text, then export those changes.

```cpp
enum class CardTone : uint8_t {
    Neutral,
    Warning,
    Success,
};

struct DashboardCardParams {
    std::string title = "Untitled";
    CardTone tone = CardTone::Neutral;
    Clay_ElementDeclaration root{};
    Clay_TextElementConfig titleText{};
};

struct DashboardCardState {
    bool expanded = true;
};

struct DashboardCardResources {
    Clay_Color warningColor = FlowUi::Flow_Color("#f2b84bff");
    Clay_Color successColor = FlowUi::Flow_Color("#43b883ff");
};
```

This is intentionally close to normal element code. Developer mode does not replace your element system; it adds reflection metadata around the same parameter, state, resource, enum, and definition types you already use.

### The Runtime Flow

Developer mode works in a few stages:

1. Registration macros describe structs, fields, enums, and element definitions.
2. `ElementBuilder` captures Flow element instances while the UI is built.
3. The dev panel uses the registry and runtime capture data to show editable fields.
4. Runtime edits become overrides stored in `DevRuntime`.
5. Export writes those overrides to `.flowui/overrides.v1.json` or another configured path.
6. The updater tool reads that JSON and patches source defaults where it can resolve a source location.

## Chapter 2: Enable Developer Mode

### App Config

Developer tooling is configured through `FlowUi::DevToolsConfig` inside `AppConfig`.

```cpp
FlowUi::AppConfig config{};
config.dev.enabled = true;
config.dev.panelOpenByDefault = true;
config.dev.panelWidthPx = 460.0f;
config.dev.overridesPath = ".flowui/dashboard-card-overrides.json";

FlowUi::App app = FlowUi::makeApplication(config);
```

The runtime config is still available from `UiManager`.

```cpp
app.ui().devToolsConfig().panelOpenByDefault = true;
```

Most applications should keep `excludeInternalDevElementsFromCapture` enabled. The dev panel itself is made from Flow elements, and hiding those internal nodes keeps the hierarchy focused on user-authored UI.

`autoSave` is present on the config, but in the current V1 workflow exports are explicit. Treat the export button or `exportOverridesAsJson()` call as the point where runtime edits become a file on disk.

### Panel Toggle

By default, developer mode can register its panel toggle with `ShortcutManager`.

```cpp
config.dev.useShortcutManagerForPanelToggle = true;
config.dev.panelToggleChord.ctrl = true;
config.dev.panelToggleChord.shift = true;
config.dev.panelToggleChord.key = GLFW_KEY_D;
```

If the application owns all global shortcuts, disable this and toggle the panel through app code instead.

```cpp
config.dev.useShortcutManagerForPanelToggle = false;
```

## Chapter 3: Register Editable Types

### Register Enums

Enums must be registered when you want symbolic values in the editor and exported JSON. In the current registry, enum registration expects a `uint8_t` backed enum.

```cpp
#include "devMode/devApi.hpp"

FLOWUI_DEV_REGISTER_ENUM(
    CardTone,
    FLOWUI_DEV_ENUM_VALUE(CardTone::Neutral),
    FLOWUI_DEV_ENUM_VALUE(CardTone::Warning),
    FLOWUI_DEV_ENUM_VALUE(CardTone::Success));
```

The enum value names are used by the panel and by JSON export. This matters when the updater tries to patch source because symbolic enum names are much safer than only numeric values.

### Register Parameter Structs

Register the params struct and list every field you want developer mode to capture and edit.

```cpp
FLOWUI_DEV_REGISTER_STRUCT(
    DashboardCardParams,
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardParams, title),
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardParams, tone),
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardParams, root),
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardParams, titleText));
```

Each reflected field stores its name, type hash, owner type, member pointer, and capture/apply functions. At runtime, `ElementBuilder` can capture the current value and apply a matching override back into the params object before the element builds.

Use this for fields that are genuinely useful to tune. Registering every member of every struct is possible, but it makes the dev panel noisier and exported JSON harder to review.

### Register State and Resource Structs

State and resources can also be registered.

```cpp
FLOWUI_DEV_REGISTER_STRUCT(
    DashboardCardState,
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardState, expanded));

FLOWUI_DEV_REGISTER_STRUCT(
    DashboardCardResources,
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardResources, warningColor),
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardResources, successColor));
```

Params are the main export path today. Registering state and resources is still useful because the dev panel can display richer element metadata and because the runtime has separate override/snapshot storage for definition params, instance params, state, and resources.

When `FLOW_UI_DEV_MODE` is disabled, these macros compile away. That lets you leave registration beside the element code without adding release-mode work.

## Chapter 4: Register Elements

### Element Shape

The element definition uses the same shape covered in the custom elements tutorial.

```cpp
using DashboardCardDefinition = FlowUi::ElementDefinition<
    DashboardCardParams,
    DashboardCardState,
    DashboardCardResources,
    FLOW_DEF_ID("tutorial_dashboard_card")>;

inline const DashboardCardDefinition kDashboardCard = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    +[](DashboardCardDefinition::BuildContext& context) {
        Clay_ElementDeclaration root = context.params.root;
        root.id = context.uiManager.toClayEID(context.elementID);

        CLAY(root) {
            CLAY_TEXT(
                context.uiManager.toClayString(context.params.title),
                &context.params.titleText);
        }
    },
};
```

The key part is that the element consumes its params normally. Developer overrides are applied before the build callback, so the callback can stay ordinary element code.

### Registration

Register the definition type, not the `inline const` object.

```cpp
FLOWUI_DEV_REGISTER_ELEMENT(DashboardCardDefinition, "Dashboard Card");
```

The display name is optional.

```cpp
FLOWUI_DEV_REGISTER_ELEMENT(DashboardCardDefinition);
```

The registry records the definition type hash, definition id, params struct type, state struct type, and resources struct type. That is how the dev panel connects a captured instance back to the structs you registered.

### What the Registry Stores

The registration layer is deliberately metadata-only. It does not own your element, and it does not create element instances. It only describes enough about known types for runtime capture and editing:

- structs: field names, type hashes, member pointers, capture functions, apply functions
- enums: numeric values and symbolic names
- elements: definition id and the params/state/resources type hashes

Because this is static registration, put the macros in a translation unit or header that is actually linked into the app.

## Chapter 5: Edit Instances and Definitions

### Instance Overrides

An instance override targets one call site and one element id.

```cpp
app.ui()
    .createElement(kDashboardCard, "dashboard/cards/primary")
    .setParameters(DashboardCardParams{
        .title = "Revenue",
        .tone = CardTone::Success,
        .root = makeCardRoot(),
        .titleText = makeCardTitleText(app.fonts()),
    })
    .draw();
```

If you select this exact instance and change `titleText.textColor`, the override is keyed by definition id, Flow id, element id, and field hash. Other `DashboardCardDefinition` instances keep their own values.

### Definition Overrides

A definition override targets the element definition rather than one instance. This is useful when every card should get the same default padding, border, or text style.

The runtime applies definition params first, then instance params. That means an instance-specific edit can still specialize one element after a broader definition edit.

For source patching, definition exports currently map back to the registered params struct and update that struct's default member initializers. In this example, a definition-level edit to `title` would patch `DashboardCardParams::title = "Untitled"` rather than a specific `.createElement(...)` call.

### Why Source Location Matters

`UiManager::createElement()` captures source location in dev mode. The exported JSON includes that location for instance overrides, and struct registration includes source metadata for definition-level defaults.

Keep element creation calls direct and stable while doing visual iteration. The V1 updater expects to find `.createElement` on the captured source line, then it searches the chained call for `.setParameters(...)`, `.setParams(...)`, or `mergeParams(...)`.

```cpp
app.ui()
    .createElement(kDashboardCard, "dashboard/cards/primary")
    .setParameters(DashboardCardParams{.title = "Revenue"})
    .draw();
```

Heavily generated element ids or deeply hidden factory functions can still work at runtime, but they may make exported source patches harder to apply automatically.

The updater also expects source paths in the export to be absolute. If your compiler expands `__FILE__` to relative paths, runtime editing still works, but automatic patching may report unresolved entries until the build is configured to emit absolute file paths or the changes are applied manually.

## Chapter 6: Export JSON

### Export Path

Exports write to `DevToolsConfig::overridesPath`.

```cpp
config.dev.overridesPath = ".flowui/overrides.v1.json";
```

The default path is `.flowui/overrides.v1.json`.

### Manual Export

The dev panel exposes export through its header. App code can also call the same function directly.

```cpp
#include "devMode/devJson.hpp"

const bool exported = FlowUi::devMode::exportOverridesAsJson(app.ui());
```

The function returns `false` if developer mode is not compiled in, the path is empty, the directory cannot be created, or the file cannot be written.

### What the JSON Means

The JSON has two main target lists: `definitions` and `instances`.

```json
{
  "schema": "flowui.dev.export.v1",
  "kind": "params",
  "definitions": [
    {
      "definitionId": 11003379752432211637,
      "definitionName": "Dashboard Card",
      "paramsStructName": "DashboardCardParams",
      "hasSourceMetadata": true,
      "sourceFile": "D:/FlowUi/examples/dashboard/DashboardCard.hpp",
      "sourceLine": 42,
      "changes": [
        {
          "fieldName": "root",
          "value": {
            "kind": "composite_struct",
            "value": {
              "type": "Clay_ElementDeclaration",
              "declaration": {
                "backgroundColor": { "r": 22, "g": 25, "b": 33, "a": 255 }
              }
            }
          }
        }
      ]
    }
  ],
  "instances": [
    {
      "definitionName": "Dashboard Card",
      "elementId": "dashboard/cards/primary",
      "authoredInstanceKey": "dashboard/cards/primary",
      "sourceFile": "D:/FlowUi/examples/dashboard/main.cpp",
      "sourceLine": 118,
      "changes": [
        {
          "fieldName": "title",
          "value": { "kind": "string", "value": "Monthly Revenue" }
        }
      ]
    }
  ]
}
```

Definition targets are meant for changing reusable defaults. Instance targets are meant for changing one specific call site. Every change also carries hashes and type data in the real file, so tooling can verify that the exported value still matches the reflected field.

## Chapter 7: Apply Exported Changes

### Build the Updater

The source patching tool lives here:

```text
tools/flowui_devChange_updater/flowui_devChange_updater.cpp
```

If your build system does not already compile it, build it as a standalone C++ tool with your normal compiler. It only depends on the C++ standard library.

On Windows with MSVC:

```powershell
cl /std:c++20 /EHsc tools\flowui_devChange_updater\flowui_devChange_updater.cpp /Fe:flowui_devChange_updater.exe
```

On Linux with GCC or Clang:

```bash
c++ -std=c++20 tools/flowui_devChange_updater/flowui_devChange_updater.cpp -o flowui_devChange_updater
```

### Run the Updater

Run the tool with the exported JSON path. The optional second argument is a report path.

On Windows:

```powershell
.\flowui_devChange_updater.exe .flowui\overrides.v1.json .flowui\apply-report.json
```

On Linux:

```bash
./flowui_devChange_updater .flowui/overrides.v1.json .flowui/apply-report.json
```

The updater reads the JSON, groups targets by source file, patches the referenced files in place, and writes a report JSON. The console output summarizes how many definition targets, instance targets, and files were patched.

### Review the Result

Always review the patch before committing it.

```bash
git diff
```

Some entries can be unresolved. Common reasons are missing source metadata, relative source paths, source files that moved, values the V1 patcher cannot express, or source code that changed enough that the tool cannot safely find the initializer/call site.

The report file keeps unresolved entries with a reason. Treat that as a to-do list: either apply those values by hand or adjust the source shape so future exports can patch it.

## Final Shape

The complete workflow looks like this:

```cpp
FLOWUI_DEV_REGISTER_ENUM(
    CardTone,
    FLOWUI_DEV_ENUM_VALUE(CardTone::Neutral),
    FLOWUI_DEV_ENUM_VALUE(CardTone::Warning),
    FLOWUI_DEV_ENUM_VALUE(CardTone::Success));

FLOWUI_DEV_REGISTER_STRUCT(
    DashboardCardParams,
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardParams, title),
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardParams, tone),
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardParams, root),
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardParams, titleText));

FLOWUI_DEV_REGISTER_STRUCT(
    DashboardCardState,
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardState, expanded));

FLOWUI_DEV_REGISTER_STRUCT(
    DashboardCardResources,
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardResources, warningColor),
    FLOWUI_DEV_REFLECT_FIELD(DashboardCardResources, successColor));

FLOWUI_DEV_REGISTER_ELEMENT(DashboardCardDefinition, "Dashboard Card");
```

```cpp
FlowUi::AppConfig config{};
config.dev.enabled = true;
config.dev.panelOpenByDefault = true;
config.dev.overridesPath = ".flowui/overrides.v1.json";

FlowUi::App app = FlowUi::makeApplication(config);
```

```cpp
app.ui()
    .createElement(kDashboardCard, "dashboard/cards/primary")
    .setParameters(DashboardCardParams{
        .title = "Revenue",
        .tone = CardTone::Success,
        .root = makeCardRoot(),
        .titleText = makeCardTitleText(app.fonts()),
    })
    .draw();
```

Developer mode is most useful when element params are data-shaped and stable. Register the fields you want to tune, make changes visually, export the override JSON, run the updater, and review the resulting source edits like any other code change.
