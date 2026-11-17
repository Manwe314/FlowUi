# Icon Manager API

[Public declarations](include/FlowUi/Public/managers/IconManager.hpp) · [Icon configuration](include/FlowUi/Public/configurationStructs/IconManagerConfig.hpp)

With `FLOWUI_INCLUDE_ICON_MANAGER` enabled, SVG source is parsed at registration and rasterized lazily at the rendered size. Existing keys are preserved rather than replaced. With icon support disabled, all normal registration/removal/lookup overloads below return `UnsupportedBuildFeature`; successful `false` is not the disabled-build signal. `App::icons()` can also report unavailable support before a manager is acquired.

## Acquire the manager

`App` getters return `Result<std::reference_wrapper<T>>`. Check the wrapper, then name the borrowed object as a normal reference:

**Example:**

```cpp
auto result = app.icons();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::IconManager& manager = result->get();
```

The reference is borrowed; the App must outlive it, and window-owned managers must also stay within their owning window lifetime. The examples use an already acquired manager reference and existing field/window identities where needed. They choose an application policy of throwing `FlowUiException` after a failed Result; returning/reporting the error is equally possible. Include the relevant manager/value headers and `FlowUi/Public/errorManagement/Exception.hpp`.

ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

For `Result<bool>`, `if (!result)` tests operational failure; `*result` is the separate operation outcome. Never treat a successful `false` as an error.

## Public API

### **registerSvg**

#### `[[nodiscard]] Result<bool> registerSvg(std::string_view key, std::string_view svgSource) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `svgSource`

Parse and register SVG source for lazy rasterization. Success contains true for insertion or false when the key already exists; a duplicate preserves the original document. Invalid SVG is an error.

Source bytes are consumed/copied during registration; rasterization occurs later at rendered size and cached variants are reused.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
constexpr std::string_view svgSource = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M2 8l4 4 8-8" fill="none" stroke="#fff"/></svg>)svg";
auto result = manager.registerSvg("status/check", svgSource);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **registerSvg**

#### `[[nodiscard]] Result<bool> registerSvg(ResourceKey key, std::string_view svgSource) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `svgSource`

Parse and register SVG source for lazy rasterization. Success contains true for insertion or false when the key already exists; a duplicate preserves the original document. Invalid SVG is an error.

Source bytes are consumed/copied during registration; rasterization occurs later at rendered size and cached variants are reused. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "status/check"};
constexpr std::string_view svgSource = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M2 8l4 4 8-8" fill="none" stroke="#fff"/></svg>)svg";
auto result = manager.registerSvg(key, svgSource);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **registerFromFile**

#### `[[nodiscard]] Result<bool> registerFromFile(std::string_view key, std::string_view filePath) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `filePath` UTF-8 file path

Read, parse, and register an SVG file from a UTF-8 path. Success contains true for insertion or false for an existing unchanged key. File/path/read/parse failures are errors.

The file path and bytes are consumed synchronously. Registration does not rasterize the SVG.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.registerFromFile("status/check", "assets/icons/check.svg");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **registerFromFile**

#### `[[nodiscard]] Result<bool> registerFromFile(ResourceKey key, std::string_view filePath) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `filePath` UTF-8 file path

Read, parse, and register an SVG file from a UTF-8 path. Success contains true for insertion or false for an existing unchanged key. File/path/read/parse failures are errors.

The file path and bytes are consumed synchronously. Registration does not rasterize the SVG. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "status/check"};
auto result = manager.registerFromFile(key, "assets/icons/check.svg");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **register_file**

#### `[[nodiscard]] Result<bool> register_file(ResourceKey key, const std::filesystem::path& file_path) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `file_path` native filesystem path

Read and register an SVG from a native filesystem path. Success contains true for insertion or false for an existing unchanged key. File/path/read/parse failures are errors.

Use native filesystem paths where platform path encoding matters; rasterization remains lazy. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "status/check"};
const std::filesystem::path assetPath{"assets/icons/check.svg"};
auto result = manager.register_file(key, assetPath);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **register_file**

#### `[[nodiscard]] Result<bool> register_file(std::string_view key, const std::filesystem::path& file_path) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `file_path` native filesystem path

Read and register an SVG from a native filesystem path. Success contains true for insertion or false for an existing unchanged key. File/path/read/parse failures are errors.

Use native filesystem paths where platform path encoding matters; rasterization remains lazy.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const std::filesystem::path assetPath{"assets/icons/check.svg"};
auto result = manager.register_file("status/check", assetPath);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **remove**

#### `[[nodiscard]] Result<bool> remove(std::string_view key) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Remove an SVG document and retire its cached raster generations. Success contains true if removed and false if absent.

Previously returned request/cached texture generations are invalid after removal. Reacquire after registration; actual GPU allocations retire when recorded uses finish.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.remove("status/check");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto removed = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **remove**

#### `[[nodiscard]] Result<bool> remove(ResourceKey key) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Remove an SVG document and retire its cached raster generations. Success contains true if removed and false if absent.

Previously returned request/cached texture generations are invalid after removal. Reacquire after registration; actual GPU allocations retire when recorded uses finish. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "status/check"};
auto result = manager.remove(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto removed = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **contains**

#### `[[nodiscard]] Result<bool> contains(std::string_view key) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Test whether an SVG key is registered. Success contains true for a registered document or false for absence.

This does not trigger rasterization.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.contains("status/check");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto registered = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **contains**

#### `[[nodiscard]] Result<bool> contains(ResourceKey key) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Test whether an SVG key is registered. Success contains true for a registered document or false for absence.

This does not trigger rasterization. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "status/check"};
auto result = manager.contains(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto registered = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **textureRef**

#### `[[nodiscard]] Result<TextureRef> textureRef(std::string_view key) noexcept`

- **Returns:** `Result<TextureRef>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Resolve an SVG key to a request TextureRef for UI rendering. Missing keys return IconNotFound under IconGenerationFailurePolicy::FailRequest. Other policies successfully return an unavailable reference requesting a fallback or omission, with a once-per-key diagnostic.

This non-const query may publish a request texture; final size-dependent rasterization occurs during frame preparation. sourceKey is borrowed App-owned metadata. Do not overwrite manager-owned handle/UV fields. Reacquire after removal and keep borrowed metadata within the App lifetime.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.textureRef("status/check");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto texture = *result;
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **textureRef**

#### `[[nodiscard]] Result<TextureRef> textureRef(ResourceKey key) noexcept`

- **Returns:** `Result<TextureRef>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Resolve an SVG key to a request TextureRef for UI rendering. Missing keys return IconNotFound under IconGenerationFailurePolicy::FailRequest. Other policies successfully return an unavailable reference requesting a fallback or omission, with a once-per-key diagnostic.

This non-const query may publish a request texture; final size-dependent rasterization occurs during frame preparation. sourceKey is borrowed App-owned metadata. Do not overwrite manager-owned handle/UV fields. Reacquire after removal and keep borrowed metadata within the App lifetime. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "status/check"};
auto result = manager.textureRef(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto texture = *result;
```

## Build-gated development hooks

`FLOW_UI_DEV_MODE` and the development memory-level macros expose additional inspection/recording hooks in the header. They are development infrastructure, not the application API described here. Their existing build gates and signatures are unchanged.

## Checked development inspection examples

These hooks require `FLOW_UI_DEV_MODE`; viewport hooks also require `FLOWUI_PUBLIC_VULKAN_INTEROP`. They are observation/tooling APIs. A visitor returns `true` to continue; a successful returned `false` is distinct from a failed `Result`. Callback arguments and their resource views are borrowed. The examples use the already acquired manager for the owning App/window.

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **visitDevIcons**

#### `Result<bool> visitDevIcons(void* userData, DevIconVisitor visitor) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.

Observe development catalogue entries without invoking application operations. Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.

Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts materialized raster rows (unrasterized SVG rows are excluded); completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure. With icon support disabled, this hook succeeds with true and makes no callbacks.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
std::size_t count = 0;
auto result = manager.visitDevIcons(&count,
    [](void* target, const FlowUi::IconManager::DevIconView& icon) {
        auto& observedCount = *static_cast<std::size_t*>(target);
        if (static_cast<bool>(icon.texture)) {
            ++observedCount;
        }
        return true;
    });
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool completed = *result;
#endif
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **visitDevAtlases**

#### `Result<bool> visitDevAtlases(void* userData, DevAtlasVisitor visitor) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.

Observe development catalogue entries without invoking application operations. Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.

Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts allocated nonempty atlas pages; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure. With icon support disabled, this hook succeeds with true and makes no callbacks.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
std::size_t count = 0;
auto result = manager.visitDevAtlases(&count,
    [](void* target, const FlowUi::IconManager::DevAtlasView& page) {
        auto& observedCount = *static_cast<std::size_t*>(target);
        if (page.width != 0 && page.height != 0) {
            ++observedCount;
        }
        return true;
    });
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool completed = *result;
#endif
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **devRasterize**

#### `[[nodiscard]] Result<DevIconView> devRasterize(std::string_view key, uint32_t width, uint32_t height) noexcept`

- **Returns:** `Result<DevIconView>`; check success before accessing the value; use `.error()` on failure.

Materialize or reuse one exact development SVG raster. A checked raster view, or initialization, IconRasterInvalid, IconNotFound, publication, allocation, or storage failure.

The key must name a registered SVG; width and height must be in [1, 512]. The example expects status/check to be registered. The returned DevIconView and texture identity are borrowed and invalidated by raster/document removal or manager destruction. newlyRasterized distinguishes a new cached raster from a reused one. No exception escapes. With icon support disabled, return UnsupportedBuildFeature.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
auto result = icons.devRasterize("status/check", 32, 32);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool newlyRasterized = result->newlyRasterized;
const auto retained = result->texture;
#endif
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).

### **devRemoveRaster**

#### `[[nodiscard]] Result<bool> devRemoveRaster(TextureHandle texture) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.

Retire only the exact development raster texture generation. Success contains true when a cached raster was removed or false when the texture did not match any cached raster; initialization/storage/allocation failures are errors.

The SVG registration remains. A removed raster can be regenerated by later UI usage or devRasterize(). Its previous borrowed texture identity must be discarded; backing allocations retire after their recorded uses. No exception escapes. With icon support disabled, return UnsupportedBuildFeature.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
auto result = icons.devRemoveRaster(raster);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto removed = *result;
#endif
```

See: [Owning header](include/FlowUi/Public/managers/IconManager.hpp).
