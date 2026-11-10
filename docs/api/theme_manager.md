# Theme Manager API

## Public Structs

### **FlowUiTheme**

#### `struct FlowUiTheme`

Standard design system tokens for built-in FlowUi elements and application components. It contains tokens for primary accent colors, background/surface colors, text hierarchy, borders, status colors, corner radii, typography sizing, and layout spacing.

**Factories:**
- `static FlowUiTheme dark()` — Default dark mode design tokens.
- `static FlowUiTheme light()` — Default light mode design tokens.

---

## Public API

### **registerTheme**

#### `template <typename T> Status registerTheme(std::string_view variantName, T themeData, bool makeActive = true)`
#### `template <typename T> Status registerTheme(T themeData, bool makeActive = true)`

- **Returns:** `Status`; check success and use `.error()` on failure.
- **Arguments:** `variantName` logical variant name (e.g. `"dark"`, `"light"`), `themeData` instance of custom theme struct `T`, `makeActive` whether to set this variant active immediately.

Registers a theme variant struct with `ThemeManager` in persistent storage. If `variantName` is omitted, variant name `"default"` is used.

**Example:**

```cpp
struct AppTheme {
    Clay_Color brandAccent = Flow_Color("#ff6b00ff");
    float cardRadius = 6.0f;
};

if (const auto status = app.themes().registerTheme<AppTheme>("default", AppTheme{}); !status) {
    throw FlowUi::FlowUiException(status.error());
}
if (const auto status = app.themes().registerTheme<AppTheme>("light", AppTheme{.brandAccent = Flow_Color("#e05d00ff")}); !status) {
    throw FlowUi::FlowUiException(status.error());
}
```

---

### **setActiveVariant**

#### `template <typename T> Status setActiveVariant(std::string_view variantName)`

- **Returns:** `Status`; check success and use `.error()` on failure.
- **Arguments:** `variantName` name of the registered variant.

Activates a named variant for theme type `T`. Immediate-mode elements querying `ui.theme<T>()` will observe the new variant on subsequent frames.

**Example:**

```cpp
if (const auto status = app.themes().setActiveVariant<AppTheme>("light"); !status) {
    throw FlowUi::FlowUiException(status.error());
}
```

---

### **getActiveTheme**

#### `template <typename T> const T& getActiveTheme() const`

- **Returns:** `const T&`
- **Arguments:** none

Returns a const reference to the currently active variant payload for theme struct type `T`.

**Example:**

```cpp
const auto& currentTheme = app.themes().getActiveTheme<AppTheme>();
```

---

### **getTheme**

#### `template <typename T> const T& getTheme(std::string_view variantName) const`

- **Returns:** `const T&`
- **Arguments:** `variantName` registered variant name.

Returns a const reference to a specific named variant payload for theme type `T`.

**Example:**

```cpp
const auto& lightTheme = app.themes().getTheme<AppTheme>("light");
```

---

### **updateTheme**

#### `template <typename T> Status updateTheme(std::string_view variantName, std::function<void(T&)> mutator)`

- **Returns:** `Status`; check success and use `.error()` on failure.
- **Arguments:** `variantName` target variant name, `mutator` callback mutating the theme payload reference.

Queues a staged theme mutation function for a named variant. Mutations take effect atomically during `app.pollEvents()` / `beginFrame()` to prevent intra-frame tearing across rendering windows.

**Example:**

```cpp
if (const auto status = app.themes().updateTheme<AppTheme>("default", [](AppTheme& theme) {
    theme.cardRadius = 12.0f;
}); !status) {
    throw FlowUi::FlowUiException(status.error());
}
```

---

### **updateActiveTheme**

#### `template <typename T> Status updateActiveTheme(std::function<void(T&)> mutator)`

- **Returns:** `Status`; check success and use `.error()` on failure.
- **Arguments:** `mutator` callback mutating the active theme payload reference.

Queues a staged theme mutation function for the currently active variant of type `T`.

**Example:**

```cpp
if (const auto status = app.themes().updateActiveTheme<AppTheme>([](AppTheme& theme) {
    theme.brandAccent = Flow_Color("#007accff");
}); !status) {
    throw FlowUi::FlowUiException(status.error());
}
```


## Development previews

Register example scenes once per theme type, alongside your theme values. The
Workbench offers vertical, horizontal, and individual-scene layouts. Each scene's
name appears in the dropdown. Re-registering the same name replaces its callback.
Normal theme registration and reflection remain sufficient for inspecting tokens.

```cpp
#if FLOW_UI_DEV_MODE
const auto preview_status = app.themes().registerDevPreview<AppTheme>(
    "Brand button", [](FlowUi::App&, FlowUi::UiManager& ui) {
        const auto& theme = ui.theme<AppTheme>();
        FlowUi::FSEL::ButtonParameters example{};
        example.text = "Example action";
        example.contentMode = FlowUi::FSEL::ButtonContentMode::TextOnly;
        example.idleOverrides.backgroundColor = theme.brandAccent;
        example.cornerRadius = CLAY_CORNER_RADIUS(theme.cardRadius);
        ui.createElement(FlowUi::FSEL::kButton, "brand-button-example")
            .setParameters(example).setDevInternalCapture(true).draw();
    });
if (!preview_status) return preview_status;
#endif
```

Callbacks receive the current app and window UI manager and use the application's
active theme. Selecting an inactive variant shows a centered activation message;
All Variants shows the active variant's scene and labels it accordingly. Preview
rendering never temporarily switches the application's theme. Without a scene,
custom themes display registration guidance; reflected values remain available
in Token Overview. FSEL has a built-in gallery.

Capture owned example data/state by value, or keep borrowed resources alive until
ThemeManager destruction. Use distinct element names for each example. Keep
preview actions local to the example, and register scenes before rendering them.
Callbacks must keep Clay scopes balanced and must not mutate the preview registry
while drawing. Preview contents are clipped to the bounded card; select an
individual scene when a group is too large. Registration is development-only and
returns `Status` for invalid names/callbacks or allocation failures.
