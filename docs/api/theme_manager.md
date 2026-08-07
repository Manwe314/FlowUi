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

#### `template <typename T> void registerTheme(std::string_view variantName, T themeData, bool makeActive = true)`
#### `template <typename T> void registerTheme(T themeData, bool makeActive = true)`

- **Returns:** `void`
- **Arguments:** `variantName` logical variant name (e.g. `"dark"`, `"light"`), `themeData` instance of custom theme struct `T`, `makeActive` whether to set this variant active immediately.

Registers a theme variant struct with `ThemeManager` in persistent storage. If `variantName` is omitted, variant name `"default"` is used.

**Example:**

```cpp
struct AppTheme {
    Clay_Color brandAccent = Flow_Color("#ff6b00ff");
    float cardRadius = 6.0f;
};

app.themes().registerTheme<AppTheme>("default", AppTheme{});
app.themes().registerTheme<AppTheme>("light", AppTheme{.brandAccent = Flow_Color("#e05d00ff")});
```

---

### **setActiveVariant**

#### `template <typename T> bool setActiveVariant(std::string_view variantName)`

- **Returns:** `bool` (`true` if variant was found and set active, `false` otherwise)
- **Arguments:** `variantName` name of the registered variant.

Activates a named variant for theme type `T`. Immediate-mode elements querying `ui.theme<T>()` will observe the new variant on subsequent frames.

**Example:**

```cpp
app.themes().setActiveVariant<AppTheme>("light");
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

#### `template <typename T> void updateTheme(std::string_view variantName, std::function<void(T&)> mutator)`

- **Returns:** `void`
- **Arguments:** `variantName` target variant name, `mutator` callback mutating the theme payload reference.

Queues a staged theme mutation function for a named variant. Mutations take effect atomically during `app.pollEvents()` / `beginFrame()` to prevent intra-frame tearing across rendering windows.

**Example:**

```cpp
app.themes().updateTheme<AppTheme>("default", [](AppTheme& theme) {
    theme.cardRadius = 12.0f;
});
```

---

### **updateActiveTheme**

#### `template <typename T> void updateActiveTheme(std::function<void(T&)> mutator)`

- **Returns:** `void`
- **Arguments:** `mutator` callback mutating the active theme payload reference.

Queues a staged theme mutation function for the currently active variant of type `T`.

**Example:**

```cpp
app.themes().updateActiveTheme<AppTheme>([](AppTheme& theme) {
    theme.brandAccent = Flow_Color("#007accff");
});
```
