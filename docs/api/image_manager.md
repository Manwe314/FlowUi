# Image Manager API

## Enums

### **TextureFitMode**


#### `enum class TextureFitMode : uint8_t`

Texture layout mode inside a target rectangle. Image texture refs use it to control stretch, contain, cover, or source-size behavior.

## Public Structs

### **TextureRef**


#### `struct TextureRef`

Renderer texture handle and draw options returned by ImageManager. App code may adjust fit mode and tint behavior before drawing.

## Public API

### **registerImage**


#### `Result<bool> registerImage(std::string_view key, std::string_view filePath)`

- **Returns:** `Result<bool>`; check success before reading the value, and use `.error()` on failure.
- **Arguments:** `key` application image key, `filePath` image file path.

Loads an image file, uploads it, and registers it under the provided key. The successful result contains `true` for a new key or `false` when replacing an existing key; failures carry a structured error.

**Example:**

```cpp
const auto registered = app.images().registerImage("avatar", "assets/avatar.png");
if (!registered) {
    throw FlowUi::FlowUiException(registered.error());
}
```

See: [Owning header](../../include/FlowUi/Public/managers/ImageManager.hpp).

### **removeImage**


#### `Result<bool> removeImage(std::string_view key)`

- **Returns:** `Result<bool>`; check success before reading the value, and use `.error()` on failure.
- **Arguments:** `key` registered image key.

Removes an image registration and retires the GPU resource safely. Existing `TextureRef` values for that key should be treated as invalid after removal.

**Example:**

```cpp
const auto removal = app.images().removeImage("avatar");
if (!removal) {
    throw FlowUi::FlowUiException(removal.error());
}
const bool removed = *removal;
```

See: [Owning header](../../include/FlowUi/Public/managers/ImageManager.hpp).

### **contains**


#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` image key to test.

Checks whether an image key is currently registered. This performs no file IO or GPU work.

**Example:**

```cpp
if (!app.images().contains("avatar")) {
    const auto registered = app.images().registerImage("avatar", "assets/avatar.png");
    if (!registered) {
        throw FlowUi::FlowUiException(registered.error());
    }
}
```

See: [Owning header](../../include/FlowUi/Public/managers/ImageManager.hpp).

### **getTexture**


#### `TextureRef getTexture(std::string_view key) const`

- **Returns:** `TextureRef`
- **Arguments:** `key` registered image key.

Returns a generation-checked texture reference for a registered image. Missing keys return an invalid handle and report the configured fallback or skip resolution once for that key.

**Example:**

```cpp
FlowUi::TextureRef avatar = app.images().getTexture("avatar");
```

See: [Owning header](../../include/FlowUi/Public/managers/ImageManager.hpp).
