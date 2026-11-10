# Public API Index

This page maps the supported public surfaces to their maintained references and owning headers. The individual references describe signatures, result handling, and lifetimes; source Doxygen comments remain the authority for exact overloads.

## Application and managers

| Surface | Common operations | Reference | Owning header |
| --- | --- | --- | --- |
| `App` | `makeApplication`, window creation/destruction, `beginFrame`, `endFrame`, `drawFrame`, `pollEvents`, manager access | [Application](app.md) | [App.hpp](../../include/FlowUi/Public/App.hpp) |
| Configuration | `AppConfig`, `WindowConfig`, `VulkanConfig`, `UiConfig`, `DevToolsConfig` | [Configuration](config.md) | [AppConfig.hpp](../../include/FlowUi/Public/configurationStructs/AppConfig.hpp), [configuration leaves](../../include/FlowUi/Public/configurationStructs/) |
| Identities and handles | element/window/action IDs, resource keys, texture handles, capacity target IDs | [Elements](elements.md), [Configuration](config.md) | [identificationTypes](../../include/FlowUi/Public/identificationTypes/) |
| Shared values | `TextureRef`, `TextureFitMode`, `FlowUiTheme` | [Configuration](config.md) | [TextureRef.hpp](../../include/FlowUi/Public/TextureRef.hpp), [FlowUiTheme.hpp](../../include/FlowUi/Public/FlowUiTheme.hpp) |
| `UiManager` | Clay strings/IDs, `imageData`, builders, themes, input, popups, frame state | [UI Manager](ui_manager.md) | [UiManager.hpp](../../include/FlowUi/Public/managers/UiManager.hpp) |
| `ElementManager` and builders | typed definitions, invocation, window-scoped state, app-wide resources, semantic parts | [Elements](elements.md) | [ElementManager.hpp](../../include/FlowUi/Public/managers/ElementManager.hpp), [builder](../../include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp) |
| `ActionManager` | app action binding, local UI recipes, invocation and availability | [Actions](action_manager.md) | [ActionManager.hpp](../../include/FlowUi/Public/managers/ActionManager.hpp) |
| `ThemeManager` | registration, named variants, staged frame-boundary updates | [Themes](theme_manager.md) | [ThemeManager.hpp](../../include/FlowUi/Public/managers/ThemeManager.hpp) |
| `FontManager` | families, faces, font and atlas resolution | [Fonts](font_manager.md) | [FontManager.hpp](../../include/FlowUi/Public/managers/FontManager.hpp) |
| `ImageManager` | stable image registration and texture lookup | [Images](image_manager.md) | [ImageManager.hpp](../../include/FlowUi/Public/managers/ImageManager.hpp) |
| `IconManager` | SVG registration, raster/atlas variants, texture lookup | [Icons](icon_manager.md) | [IconManager.hpp](../../include/FlowUi/Public/managers/IconManager.hpp) |
| `InputFieldManager` | typed field requests, transactions, text views, focus/carets/selection | [Input fields](input_field_manager.md) | [InputFieldManager.hpp](../../include/FlowUi/Public/managers/InputFieldManager.hpp) |
| `ShortcutManager` | chord registration, scoped dispatch, retained app actions | [Shortcuts](shortcut_manager.md) | [ShortcutManager.hpp](../../include/FlowUi/Public/managers/ShortcutManager.hpp) |
| `PopupManager` | placement, measurement, overflow, dismissal, stacking | [Popups](popup_manager.md) | [PopupManager.hpp](../../include/FlowUi/Public/managers/PopupManager.hpp) |
| `ViewPortManager` and `ViewPort` | offscreen targets and Vulkan callbacks | [Viewports](viewport_manager.md) | [ViewPortManager.hpp](../../include/FlowUi/Public/managers/ViewPortManager.hpp) |

## Strong identities and shared values

`FlowDefinitionID`, `FlowElementID`, `GlobalFlowID`, and `FlowElementPartID` represent distinct element addresses. Use `DefinitionID`, local literal names, `RuntimeName`, `Indexed`, `Keyed`, `Global`, and semantic `Part` declarations according to the intended scope. See [the identity reference](elements.md#identity) and [ElementID.hpp](../../include/FlowUi/Public/identificationTypes/ElementID.hpp).

Resource names use `ResourceKey`; textures use generation-checked `TextureHandle`/`TextureRef`. `TextureRef::fromStable(domain, key, fit, tint)` describes a stable lookup. Fit and tint are per-reference settings; filtering belongs to the logical texture's sampler.

`Flow_Color("#RRGGBB")` and `Flow_Color("#RRGGBBAA")` decode literal colors at compile time. The runtime `std::string_view` overload validates its input and can throw for invalid color text.

`Result<T>` and `Status` describe recoverable failures. Check success before dereferencing a result, and inspect `.error()` on failure. Queries, factories, and result-returning operations use `[[nodiscard]]`; callers should handle the outcome or explicitly discard it when the behavior is intentional. Only operations whose implementation supports a nonthrowing contract carry `noexcept`.

## Frame and resource lifetimes

```cpp
if (const auto status = application.beginFrame(); !status) {
    application.reportError(status.error());
    return 1;
}
auto& ui = application.ui();
ui.createElement(FlowUi::kButton, "toolbar/save")
    .setParameters({.onActivate = save_action, .label = "Save"})
    .draw();
if (const auto status = application.endFrame(); !status) {
    application.reportError(status.error());
    return 1;
}
if (const auto status = application.drawFrame(); !status) {
    application.reportError(status.error());
    return 1;
}
```

`ui.toClayString()` and `ui.imageData()` return frame-owned payloads. GPU texture ownership and in-flight retirement remain with storage. `FieldTextView` borrows manager text and is invalidated by field mutation: choose `contiguous()` for a confirmed contiguous view, `copy()` for owned text, or `forEachChunk()` for traversal. Use `FieldConfig::mode` with `TextFieldMode::SingleLine`/`MultiLine` for line behavior.

See [frame lifecycle](../concepts/frame_lifecycle.md) and [manager ownership](../concepts/managers.md) for complete lifetime boundaries.

## Modern developer systems

Compile `FLOW_UI_DEV_MODE` and activate `AppConfig::dev.enabled` to use the dedicated developer interface. [Developer Systems](../tutorials/developer_mode.md) describes the supported workflow.

| Service | Access | Contract |
| --- | --- | --- |
| `DevTooling` | `application.devTooling()` | App-owned schema, catalogue, overrides, overlays, and baking services. |
| `DevSchemaRegistry` | `devTooling().schemas()` | Queue roots; publish immutable schema generations at safe points. |
| `DevCatalogues` | `devTooling().catalogues()` | Typed element/theme/action/image/icon/font/atlas metadata. |
| `DevOverrideEngine` | `devTooling().overrides()` | Typed changes, constraint validation, safe-point commit, effective-value capture. |
| `DevBakePipeline` | `devTooling().bakePipeline()` | Revision-checked review and durable `.flowchanges` manifests. |
| Monitoring | `application.devMonitoring()` | Timing, memory, error collection and retained reporting. |
| Window tree | `application.ui(window).devTreeSnapshot()` | Completed Flow/Clay tree and identity correlation. |

Schema descriptors, registry, catalogues, and override types live in `FlowUi::devSystems::tooling`. Current headers are under [devSystems](../../include/devSystems/); the obsolete developer subsystem has been removed completely. Optional agentic evidence APIs require the additional `AGENTIC_WORKFLOW` build option and explicit runtime configuration; see [agentic workflow](../agentic-workflow.md).
