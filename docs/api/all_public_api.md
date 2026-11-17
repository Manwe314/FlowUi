# Public API index

These references describe the current application-facing API, including overloads, constness, build gates, error contracts and lifetimes. Result-returning operations include concrete checked examples in their references and public Doxygen comments. Start with [result handling](docs/api/result_handling.md) for extracting `Result<std::reference_wrapper<T>>` into `T&` or `const T&`.

| Surface | Reference | Owning public header |
| --- | --- | --- |
| App ownership, windows, frames, manager access and optional developer facades | [App](docs/api/app.md) | [App.hpp](include/FlowUi/Public/App.hpp), [element windows](include/FlowUi/Public/AppElementWindows.hpp) |
| Startup and operation configuration, capacities and build settings | [Configuration](docs/api/config.md), [field glossary](docs/api/configuration-glossary.md) | [configurationStructs](include/FlowUi/Public/configurationStructs/) |
| Results, errors, identities, hashing and texture references | [Values and errors](docs/api/values.md), [handling results](docs/api/result_handling.md) | [errorManagement](include/FlowUi/Public/errorManagement/), [identificationTypes](include/FlowUi/Public/identificationTypes/), [TextureRef.hpp](include/FlowUi/Public/TextureRef.hpp) |
| Filesystem resources and default helpers | [Resources](docs/api/resources.md) | [Resources.hpp](include/FlowUi/Public/Resources.hpp), [defaults](include/FlowUi/Public/defaults/) |
| Color parsing, ValueOrThrow and shared configuration values | [Configuration](docs/api/config.md), [result propagation](docs/api/result_handling.md#propagate-a-checked-result-with-valueorthrow) | [Color.hpp](include/FlowUi/Public/Color.hpp), [FlowUiTheme.hpp](include/FlowUi/Public/FlowUiTheme.hpp) |
| InputSink, translators, semantic events and key conversions | [Input translation](docs/api/input.md) | [InputTranslation.hpp](include/FlowUi/Public/input/InputTranslation.hpp) |
| Clay data, IDs, frame data and interaction snapshots | [UI manager](docs/api/ui_manager.md) | [UiManager.hpp](include/FlowUi/Public/managers/UiManager.hpp) |
| ElementManager, typed definitions, builders, contexts, state/resources and semantic parts | [Elements](docs/api/elements.md) | [ElementManager.hpp](include/FlowUi/Public/managers/ElementManager.hpp), [elementAuthoring](include/FlowUi/Public/elementAuthoring/) |
| Actions, bindings, UI recipes, invocation, availability and typed results | [Actions](docs/api/action_manager.md) | [ActionManager.hpp](include/FlowUi/Public/managers/ActionManager.hpp), [ActionTypes.hpp](include/FlowUi/Public/managerStructs/ActionTypes.hpp) |
| Theme registration, variants, staged changes and FlowUiTheme factories | [Themes](docs/api/theme_manager.md) | [ThemeManager.hpp](include/FlowUi/Public/managers/ThemeManager.hpp) |
| Standard element resource constructors and slider visibility helpers | [Standard elements](docs/api/standard_elements.md) | [FSEL.hpp](include/FSEL.hpp) |
| Fonts, faces and Vulkan atlas views | [Fonts](docs/api/font_manager.md) | [FontManager.hpp](include/FlowUi/Public/managers/FontManager.hpp) |
| Images and texture lookup | [Images](docs/api/image_manager.md) | [ImageManager.hpp](include/FlowUi/Public/managers/ImageManager.hpp) |
| SVG resources and icon atlas variants | [Icons](docs/api/icon_manager.md) | [IconManager.hpp](include/FlowUi/Public/managers/IconManager.hpp) |
| Text fields, transactions, views, focus, carets and selection | [Input fields](docs/api/input_field_manager.md) | [InputFieldManager.hpp](include/FlowUi/Public/managers/InputFieldManager.hpp) |
| Shortcuts, scopes, dispatch and retained app actions | [Shortcuts](docs/api/shortcut_manager.md) | [ShortcutManager.hpp](include/FlowUi/Public/managers/ShortcutManager.hpp) |
| Popup measurement, placement, dismissal and stacking | [Popups](docs/api/popup_manager.md) | [PopupManager.hpp](include/FlowUi/Public/managers/PopupManager.hpp) |
| Offscreen targets, Vulkan callbacks and capture/target values | [Viewports](docs/api/viewport_manager.md) | [ViewPortManager.hpp](include/FlowUi/Public/managers/ViewPortManager.hpp) |

## Borrowing and frame boundaries

App manager getters return checked results and borrowed references. `UiManager::actions()` and `elements()` are direct borrowed references; `ActionManager::uiActions()` also returns a direct borrowed reference. Builders and fluent methods are direct authoring operations and can throw; `draw()` returns `void`. Use a frame boundary to contain UI build failures:

```cpp
const auto status = application.runFrame([](FlowUi::UiManager& ui, FlowUi::WindowId) {
    ui.createElement(FlowUi::kButton, "toolbar/save")
        .setParameters({.label = "Save"})
        .draw();
});
if (!status)
    application.reportError(status.error());
```

`UiManager::toClayString()` and `imageData()` return direct frame-owned values and throw on operational failure. Root context `clayID()` is direct; named identity helpers remain checked. Use public `FlowUi::ValueOrThrow` for retained Results when the enclosing operation deliberately propagates failure. `FieldTextView` borrows field storage and is invalidated by mutation: `copy()` creates owned text, `contiguous()` can return no contiguous view, and `forEachChunk()` propagates visitor exceptions. Copying a texture reference or UI action never acquires its GPU resource or borrowed application data.

## Optional developer access

Developer App accessors are documented with their checked types in [App](docs/api/app.md). They require `FLOW_UI_DEV_MODE`; runtime activation is controlled by `AppConfig::dev.enabled`. Agentic support also requires `AGENTIC_WORKFLOW`/`FLOW_UI_AGENTIC_WORKFLOW` and its explicit configuration. The extracted facade remains App-owned. Internal storage, platform providers and backend helpers are implementation APIs.

See [developer workflow](docs/tutorials/developer_mode.md), [agentic workflow](docs/agentic-workflow.md), [frame lifecycle](docs/concepts/frame_lifecycle.md) and [manager ownership](docs/concepts/managers.md) for broader workflows.
