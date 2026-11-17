# Image Manager API

[Public declarations](include/FlowUi/Public/managers/ImageManager.hpp) · [TextureRef](include/FlowUi/Public/TextureRef.hpp)

Images are App-shared sampled resources. Registration decodes supported stb_image formats to RGBA8 sRGB and prepares publication; image replacement is supported. A successful registration does not mean the GPU upload has already completed: FlowUi orders the upload before recorded sampling.

## Acquire the manager

`App` getters return `Result<std::reference_wrapper<T>>`. Check the wrapper, then name the borrowed object as a normal reference:

**Example:**

```cpp
auto result = app.images();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ImageManager& manager = result->get();
```

The reference is borrowed; the App must outlive it, and window-owned managers must also stay within their owning window lifetime. The examples use an already acquired manager reference and existing field/window identities where needed. They choose an application policy of throwing `FlowUiException` after a failed Result; returning/reporting the error is equally possible. Include the relevant manager/value headers and `FlowUi/Public/errorManagement/Exception.hpp`.

ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

For `Result<bool>`, `if (!result)` tests operational failure; `*result` is the separate operation outcome. Never treat a successful `false` as an error.

## Public API

### **registerImage**

#### `[[nodiscard]] Result<bool> registerImage(ResourceKey key, std::string_view filePath) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `filePath` UTF-8 file path

Load an encoded image and publish its sampled texture. Success contains true for a new key or false when an existing image was replaced; false is a completed replacement, not an error.

The UTF-8 file path is consumed synchronously. Supported stb_image formats are decoded as RGBA8 sRGB. The replacement is prepared before publication; replaced generations retire after their recorded GPU uses. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "brand/logo"};
auto result = manager.registerImage(key, "assets/logo.png");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **register_image_file**

#### `[[nodiscard]] Result<bool> register_image_file(ResourceKey key, const std::filesystem::path& file_path) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `file_path` native filesystem path

Load an encoded image from a native filesystem path. Success contains true for insertion or false for a completed replacement; inspect the Result separately from that bool.

Use this overload for native platform paths. Decoding/publication and lifetime rules match registerImage(). ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "brand/logo"};
const std::filesystem::path assetPath{"assets/logo.png"};
auto result = manager.register_image_file(key, assetPath);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **register_image_file**

#### `[[nodiscard]] Result<bool> register_image_file(std::string_view key, const std::filesystem::path& file_path) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `file_path` native filesystem path

Load an encoded image from a native filesystem path. Success contains true for insertion or false for a completed replacement; inspect the Result separately from that bool.

Use this overload for native platform paths. Decoding/publication and lifetime rules match registerImage().

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const std::filesystem::path assetPath{"assets/logo.png"};
auto result = manager.register_image_file("brand/logo", assetPath);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **registerImage**

#### `[[nodiscard]] Result<bool> registerImage(std::string_view key, std::string_view filePath) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `filePath` UTF-8 file path

Load an encoded image and publish its sampled texture. Success contains true for a new key or false when an existing image was replaced; false is a completed replacement, not an error.

The UTF-8 file path is consumed synchronously. Supported stb_image formats are decoded as RGBA8 sRGB. The replacement is prepared before publication; replaced generations retire after their recorded GPU uses.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.registerImage("brand/logo", "assets/logo.png");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto inserted = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **removeImage**

#### `[[nodiscard]] Result<bool> removeImage(ResourceKey key) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Unregister an image and retire its texture generation. Success contains true if removed and false if no image was registered.

Previously acquired texture handles no longer identify a published generation; reacquire a texture after registering the key again. GPU resources retire after recorded uses finish. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "brand/logo"};
auto result = manager.removeImage(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto removed = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **removeImage**

#### `[[nodiscard]] Result<bool> removeImage(std::string_view key) noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Unregister an image and retire its texture generation. Success contains true if removed and false if no image was registered.

Previously acquired texture handles no longer identify a published generation; reacquire a texture after registering the key again. GPU resources retire after recorded uses finish.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.removeImage("brand/logo");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto removed = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **contains**

#### `[[nodiscard]] Result<bool> contains(ResourceKey key) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Test whether an image key is registered. Success contains true when registered or false when absent; an invalid key or detached manager is an error.

No file loading or GPU work is performed, but key normalization can allocate or fail. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "brand/logo"};
auto result = manager.contains(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto registered = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **contains**

#### `[[nodiscard]] Result<bool> contains(std::string_view key) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Test whether an image key is registered. Success contains true when registered or false when absent; an invalid key or detached manager is an error.

No file loading or GPU work is performed, but key normalization can allocate or fail.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.contains("brand/logo");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto registered = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **getTexture**

#### `[[nodiscard]] Result<TextureRef> getTexture(ResourceKey key) const noexcept`

- **Returns:** `Result<TextureRef>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Resolve an image key to a TextureRef value. A missing key is a successful unavailable reference with an invalid handle. MissingVisualPolicy::SkipVisual requests omission; the fallback policy requests the renderer fallback, and the missing-key diagnostic is reported once.

TextureRef is a value with borrowed source-key metadata and generation-checked resource identity, not ownership of a texture. Reacquire after replacement/removal and keep borrowed metadata within the App lifetime. fit and tint are caller presentation settings. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "brand/logo"};
auto result = manager.getTexture(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto texture = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **getTexture**

#### `[[nodiscard]] Result<TextureRef> getTexture(std::string_view key) const noexcept`

- **Returns:** `Result<TextureRef>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Resolve an image key to a TextureRef value. A missing key is a successful unavailable reference with an invalid handle. MissingVisualPolicy::SkipVisual requests omission; the fallback policy requests the renderer fallback, and the missing-key diagnostic is reported once.

TextureRef is a value with borrowed source-key metadata and generation-checked resource identity, not ownership of a texture. Reacquire after replacement/removal and keep borrowed metadata within the App lifetime. fit and tint are caller presentation settings.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.getTexture("brand/logo");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto texture = *result;
```

## Build-gated development hooks

`FLOW_UI_DEV_MODE` and the development memory-level macros expose additional inspection/recording hooks in the header. They are development infrastructure, not the application API described here. Their existing build gates and signatures are unchanged.

## Checked development inspection examples

These hooks require `FLOW_UI_DEV_MODE`; viewport hooks also require `FLOWUI_PUBLIC_VULKAN_INTEROP`. They are observation/tooling APIs. A visitor returns `true` to continue; a successful returned `false` is distinct from a failed `Result`. Callback arguments and their resource views are borrowed. The examples use the already acquired manager for the owning App/window.

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).

### **visitDevImages**

#### `Result<bool> visitDevImages(void* userData, DevImageVisitor visitor) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.

Observe development catalogue entries without invoking application operations. Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.

Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts named image registrations; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
std::size_t count = 0;
auto result = manager.visitDevImages(&count,
    [](void* target, const FlowUi::ImageManager::DevImageView& image) {
        auto& observedCount = *static_cast<std::size_t*>(target);
        if (!image.key.empty()) {
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

See: [Owning header](include/FlowUi/Public/managers/ImageManager.hpp).
