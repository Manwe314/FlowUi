# Theme Manager API

[ThemeManager.hpp](include/FlowUi/Public/managers/ThemeManager.hpp) owns registered theme payloads and copied variant names. Acquire the App-owned manager by checking `App::themes()`:

```cpp
auto managerResult = app.themes();
if (!managerResult) {
    throw FlowUi::FlowUiException(managerResult.error());
}
FlowUi::ThemeManager& themes = managerResult->get();
auto status = themes.registerTheme(FlowUi::FlowUiTheme::dark());
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

`Status` mutation functions return known FlowUi validation failures but can also propagate arbitrary payload/callback construction or allocation exceptions. They are deliberately not `noexcept`. Staged changes run at an App safe point; queueing success does not mean the callback has run, and mutation batches do not guarantee rollback after arbitrary callback failure. See [result handling](docs/api/result_handling.md).

Read functions return `Result<std::reference_wrapper<const T>>` without throwing. Check the result and extract `const T& theme = result->get()`; the reference does not keep the manager or payload alive. Reacquire after replacing variants or applying registry changes. `UiManager::theme<T>()` additionally respects development local scopes, whereas a named variant query addresses that registered variant directly.

`FlowUiTheme::dark()` and `FlowUiTheme::light()` are `[[nodiscard]] static FlowUiTheme ... noexcept` factories declared in [FlowUiTheme.hpp](include/FlowUi/Public/FlowUiTheme.hpp). Tokens cover colors, radii, typography and spacing. The legacy default color tokens use the normalization documented by that type; copying them does not imply converting Clay channel conventions.

```cpp
auto dark = FlowUi::FlowUiTheme::dark();
auto light = FlowUi::FlowUiTheme::light();
light.fontSizeMedium = 16.0f;
```

Development preview/catalogue services are conditional tooling APIs. `init()` and `destroy()` below are public framework lifecycle wiring; ordinary applications use the App-owned initialized manager.

### **ThemeManager::ThemeManager**

#### `ThemeManager() noexcept;`

Construct a detached manager; application code normally obtains the initialized App-owned manager.

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::~ThemeManager**

#### `~ThemeManager() noexcept;`

Release owned service data. The App controls this manager lifetime.

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::ThemeManager**

#### `ThemeManager(const ThemeManager&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::operator =**

#### `ThemeManager& operator=(const ThemeManager&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::registerTheme**

#### `template <typename T> [[nodiscard]] Status registerTheme(std::string_view variantName, const T& themeData, bool makeActive = true);`

Register an owned copy of a named theme payload; the shorter overload uses variant "default". makeActive selects the registered variant immediately.

Hybrid contract: known validation failures are returned; arbitrary payload copy/assignment and allocation exceptions propagate. Failed replacement can partially affect a payload; abandon it instead of assuming rollback.

**Example:**

```cpp
auto result = themes.registerTheme<FlowUi::FlowUiTheme>("light", FlowUi::FlowUiTheme::light(), false);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::registerTheme**

#### `template <typename T> [[nodiscard]] Status registerTheme(const T& themeData, bool makeActive = true);`

Register an owned copy of a named theme payload; the shorter overload uses variant "default". makeActive selects the registered variant immediately.

Hybrid contract: known validation failures are returned; arbitrary payload copy/assignment and allocation exceptions propagate. Failed replacement can partially affect a payload; abandon it instead of assuming rollback.

**Example:**

```cpp
auto result = themes.registerTheme<FlowUi::FlowUiTheme>(FlowUi::FlowUiTheme::dark());
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::setActiveVariant**

#### `template <typename T> [[nodiscard]] Status setActiveVariant(std::string_view variantName);`

Select a previously registered variant for this theme type. Missing initialization/type/variant failures are returned in Status.

Hybrid contract: known FlowUi validation failures are returned; non-FlowUi name preparation/allocation exceptions propagate.

**Example:**

```cpp
auto result = themes.setActiveVariant<FlowUi::FlowUiTheme>("light");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::getActiveTheme**

#### `template <typename T> [[nodiscard]] Result<std::reference_wrapper<const T>> getActiveTheme() const noexcept;`

Borrow the active registered payload through Result<reference_wrapper<const T>>; missing initialization or active variant is an error.

Check the Result, then use const T& value = result->get(). This is a borrow: reacquire after replacing variants, registry mutations or manager destruction/reinitialization.

**Example:**

```cpp
auto result = themes.getActiveTheme<FlowUi::FlowUiTheme>();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowUiTheme& theme = result->get();
const float fontSize = theme.fontSizeMedium;
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::getTheme**

#### `template <typename T> [[nodiscard]] Result<std::reference_wrapper<const T>> getTheme(std::string_view variantName) const noexcept;`

Borrow one named registered payload through Result<reference_wrapper<const T>>; missing initialization or variant is an error.

Check the Result, then use const T& value = result->get(). This is a borrow: reacquire after replacing variants, registry mutations or manager destruction/reinitialization.

**Example:**

```cpp
auto result = themes.getTheme<FlowUi::FlowUiTheme>("light");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowUiTheme& theme = result->get();
const float fontSize = theme.fontSizeMedium;
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::updateTheme**

#### `template <typename T, typename Mutator> [[nodiscard]] Status updateTheme(std::string_view variantName, Mutator&& mutator);`

Queue a retained mutator for the named variant. Success means queued, not applied; captured references must survive application of queued work.

Hybrid contract: initialization/type/variant failures are returned; callback construction, name preparation and allocation exceptions propagate. Mutators run later and may throw when applied.

**Example:**

```cpp
auto result = themes.updateTheme<FlowUi::FlowUiTheme>("light", [](FlowUi::FlowUiTheme& theme) { theme.fontSizeMedium = 16.0f; });
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::updateActiveTheme**

#### `template <typename T, typename Mutator> [[nodiscard]] Status updateActiveTheme(Mutator&& mutator);`

Queue a retained mutator for the active variant of this type. Success means queued, not applied; borrowed captures must survive application.

Hybrid contract: initialization/type/variant failures are returned; callback construction, name preparation and allocation exceptions propagate. Mutators run later and may throw when applied.

**Example:**

```cpp
auto result = themes.updateActiveTheme<FlowUi::FlowUiTheme>([](FlowUi::FlowUiTheme& theme) { theme.fontSizeMedium = 16.0f; });
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::init**

#### `void init(detail::storage::IStorageSystem& storage);`

Framework lifecycle wiring to the App-owned storage system. Application code obtains this manager through App::themes().

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::destroy**

#### `void destroy() noexcept;`

Release this manager storage records and pending mutations. Framework lifecycle service; application code should retain the App-owned manager.

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::applyStagedMutations**

#### `void applyStagedMutations();`

Apply queued mutations at an application safe point. App normally calls this service; avoid changing themes while UI output is being built.

Arbitrary mutator exceptions propagate; a mutator may already have changed its target. Remaining detached callbacks are abandoned after failure, with no automatic retry. Rejection before detaching retains the queued batch.

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

## Built-in token fields

| Fields | Type and units |
| --- | --- |
| `primary`, `primaryHover`, `primaryActive`, `onPrimary` | `Clay_Color` primary action palette. |
| `background`, `surface`, `surfaceHeader`, `surfaceHover`, `surfaceSelected` | `Clay_Color` background/surface palette. |
| `textPrimary`, `textSecondary`, `textDisabled`, `textLink` | `Clay_Color` text palette. |
| `border`, `borderFocused`, `divider` | `Clay_Color` border/separator palette. |
| `success`, `warning`, `danger` | `Clay_Color` status palette. |
| `radiusSmall`, `radiusMedium`, `radiusLarge`, `radiusPill` | `Clay_CornerRadius`, logical pixels. |
| `defaultFontFamily` | `FontFamilyId`, default zero. |
| `fontSizeSmall`, `fontSizeMedium`, `fontSizeLarge`, `fontSizeHeader` | `float`, logical pixels; defaults 12, 14, 18, 24. |
| `spacingXs`, `spacingSm`, `spacingMd`, `spacingLg` | `uint16_t`, logical pixels; defaults 4, 8, 16, 24. |

The built-in default palettes use normalized 0..1 color components as documented in FlowUiTheme. The factory values own all tokens and can be customized before registration; this does not change the convention of separate `Flow_Color` or Clay examples.

### **FlowUiTheme::dark**

#### `[[nodiscard]] static FlowUiTheme dark() noexcept;`

Return an owned value containing the default dark palette.

See: [Owning header](include/FlowUi/Public/FlowUiTheme.hpp).

### **FlowUiTheme::light**

#### `[[nodiscard]] static FlowUiTheme light() noexcept;`

Return an owned value containing the default light palette.

See: [Owning header](include/FlowUi/Public/FlowUiTheme.hpp).

## Checked development catalogue and preview hooks

The following optional hooks exist only with `FLOW_UI_DEV_MODE`. The same checked Result/Status usage applies to supported development services. The preview examples use the complete `Counter` definition in [the element reference](docs/api/elements.md).

### **ThemeManager::registerDevPreview**

#### `template <typename T> [[nodiscard]] Status registerDevPreview(std::string_view debug_name, const std::function<void(App&, UiManager&)>& draw, bool uses_local_theme = false) noexcept;`

Register or replace a named retained example for every variant of T. Name/callback must be nonempty. Callback/name storage failures inside this call become Status errors; constructing the std::function argument happens before the call and may itself throw. Captures must remain valid for the manager lifetime. Set uses_local_theme only when nested theme reads all use UiManager's scoped resolver. Preview drawing must not register previews or trigger application operations.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const std::function<void(FlowUi::App&, FlowUi::UiManager&)> draw =
    [](FlowUi::App&, FlowUi::UiManager& ui) {
        auto themeResult = ui.flowTheme();
        if (!themeResult) throw FlowUi::FlowUiException(themeResult.error());
        const FlowUi::FlowUiTheme& theme = themeResult->get();
        ui.createElement(counter, "theme-preview")
            .setParameters({.width = theme.fontSizeMedium * 8.0f})
            .draw();
    };
auto status = themes.registerDevPreview<FlowUi::FlowUiTheme>("Counter", draw, true);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::registerDevPreview**

#### `template <typename T, typename Draw> requires (!std::is_same_v<std::remove_cvref_t<Draw>, std::function<void(App&, UiManager&)>>) [[nodiscard]] Status registerDevPreview(std::string_view debugName, Draw&& draw, bool usesLocalTheme = false) noexcept;`

Construct the retained std::function inside the recoverable error boundary, then register it using the same name/ownership/scoped-theme contract as the std::function overload. Callback construction and registration failures become Status errors. The drawing callback runs later; its own drawing exceptions follow that enclosing frame boundary.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
auto status = themes.registerDevPreview<FlowUi::FlowUiTheme>("Counter",
    [](FlowUi::App&, FlowUi::UiManager& ui) {
        auto themeResult = ui.flowTheme();
        if (!themeResult) throw FlowUi::FlowUiException(themeResult.error());
        const FlowUi::FlowUiTheme& theme = themeResult->get();
        ui.createElement(counter, "theme-preview")
            .setParameters({.width = theme.fontSizeMedium * 8.0f})
            .draw();
    }, true);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::visitDevPreviews**

#### `template <typename Visitor> [[nodiscard]] Status visitDevPreviews(devSystems::tooling::DevTypeId theme_type, Visitor&& visitor) const;`

Synchronously visit examples of one reflected theme type in registration order. The preview and its fields are borrowed for the callback. A completed visit returns successful Status; arbitrary visitor exceptions propagate. This function has no noexcept contract.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
std::size_t previewCount = 0;
auto status = themes.visitDevPreviews(inspectedTheme.type,
    [&previewCount](const FlowUi::ThemeManager::DevThemePreview& preview) {
        if (preview.draw && !preview.debug_name.empty()) ++previewCount;
    });
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::clone_dev_variant**

#### `[[nodiscard]] Status clone_dev_variant(devSystems::tooling::DevTypeId type, std::string_view source, std::string_view destination) noexcept;`

Clone the live reflected payload into a new named variant, rejecting existing destination names. Check initialization/type/source/destination failures through Status. Obtain the reflected identity from a real catalogue payload view; cloning does not change its source payload. Reacquire catalogue borrows after registry mutation.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const std::string_view destination = "custom";
auto status = themes.clone_dev_variant(
    inspectedTheme.type, inspectedTheme.variant, destination);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).

### **ThemeManager::set_dev_active_variant**

#### `[[nodiscard]] Status set_dev_active_variant(devSystems::tooling::DevTypeId type, std::string_view variant) noexcept;`

Select the named variant of a reflected theme type. Return initialization/missing-variant errors through Status. Obtain the type and variant from a real catalogue payload view and reacquire theme borrows after changing the registry selection.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
auto status = themes.set_dev_active_variant(
    inspectedTheme.type, inspectedTheme.variant);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/managers/ThemeManager.hpp).
