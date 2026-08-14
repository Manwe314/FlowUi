# Action Manager API

`ActionManager` unifies two deliberately different activation lifetimes behind the cheap `ActionCall` value used by elements.

- `appActions()` registers semantic, app-wide operations by `AppActionID`. Its callable and binding record are owned internally through `StorageSystem`; every resource supplied by user code is borrowed and must outlive the binding.
- `uiActions()` creates unregistered, inline `UiActionCall` values from stateless `UiAction` recipes and local resources. The call borrows referenced resources and must not be retained beyond their lifetime.

`ActionCall{}` is inert. In release builds it is trivially copyable and no larger than 32 bytes on 64-bit targets.

## App actions

```cpp
struct Document {
    void save();
};

constexpr auto Save = FlowUi::AppAction("editor.save");

Document document;
FlowUi::AppActionCall save = app.actions().appActions().bind(
    {.id = Save, .debugName = "Save document"},
    [](Document* value) { value->save(); },
    &document);
```

Use `select(Save)` at distant call sites. Selection stores only the semantic ID; it does not expose the storage record. Duplicate `bind` is rejected, `rebind` explicitly replaces an existing implementation, and `unbind` retires the binding safely even during its own invocation.

Ordinary UI invocation discards a return value:

```cpp
(void)app.actions().invoke(FlowUi::ActionCall{save});
```

When the result matters, author that intent at the invocation site:

```cpp
auto path = app.actions().appActions().invokeFor<std::string>(exportPath);
if (path) useExportPath(path.value());
```

`ActionResult<T>` reports `Empty`, `Unbound`, `Disabled`, or `ResultTypeMismatch` without requiring C++23 `std::expected` support from the standard library.

## Local UI actions

Recipes are stateless constexpr values. Their arguments describe the entire operation; `.make()` supplies the concrete local resources.

```cpp
inline constexpr auto SetBool = FlowUi::UiAction(
    "ui.set_bool",
    [](bool& target, bool value) { target = value; });

inline constexpr auto ToggleBool = FlowUi::UiAction(
    "ui.toggle_bool",
    [](bool& target) { target = !target; });

auto openMenu = app.actions().uiActions().make(SetBool, menuOpen, true);
auto togglePanel = app.actions().uiActions().make(ToggleBool, panelOpen);
```

UI calls allocate nothing and do not use `StorageSystem`. References require lvalues, payloads must fit the inline budget, and recipes must be empty, trivial, concrete, and return `void`.

## Reusable Button and shortcuts

The standard `kButton` has one `ActionCall onActivate` property, with no action-kind flag or owned callback:

```cpp
ui.createElement(FlowUi::kButton, "save")
  .setParameters({.label = "Save", .onActivate = save})
  .draw();

ui.createElement(FlowUi::kButton, "menu")
  .setParameters({.label = "Menu", .onActivate = openMenu})
  .draw();
```

The Button is stateless. A Dropdown owns its genuine `open` state and gives its internal Button a UI call targeting that state; no element polls another element every frame.

Shortcuts intentionally retain only `AppActionCall`:

```cpp
app.ui().shortcuts().registerShortcut(
    {.key = GLFW_KEY_S, .ctrl = true},
    FlowUi::ShortcutScope::Global,
    100,
    save,
    FlowUi::ShortcutHandling::Consume);
```

`Consume` stops lower-priority matches only after a successful invocation. Disabled and unbound actions fall through. Shortcut ownership remains window-scoped in this release.

For a unique local case, put the required callback type in that element's own parameters and invoke it directly. Arbitrary owning callables are intentionally not part of `ActionCall`.
