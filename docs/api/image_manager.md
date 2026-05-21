# Image Manager API

## Enums

### **TextureFitMode**


#### `enum class TextureFitMode : uint8_t`

Texture layout mode inside a target rectangle. Image texture refs use it to control stretch, contain, cover, or source-size behavior.

### **TextureSamplingMode**


#### `enum class TextureSamplingMode : uint8_t`

Texture filtering preference stored on TextureRef. It distinguishes linear and nearest sampling intent.

## Public Structs

### **TextureRef**


#### `struct TextureRef`

Renderer texture handle and draw options returned by ImageManager. App code may adjust fit mode, sampling mode, and tint behavior before drawing.

## Public API

### **registerImage**


#### `bool registerImage(std::string_view key, std::string_view filePath)`

- **Returns:** `bool`
- **Arguments:** `key` application image key, `filePath` image file path.

Loads an image file, uploads it, and registers it under the provided key. Returns `true` for a new key and `false` when replacing an existing key.

**Example:**

```cpp
app.images().registerImage("avatar", "assets/avatar.png");
```

See: [Full Doxygen reference](classFlowUi_1_1ImageManager.html#a87c55cc08f33bb3fd8f83108aa15ea23).

### **removeImage**


#### `bool removeImage(std::string_view key)`

- **Returns:** `bool`
- **Arguments:** `key` registered image key.

Removes an image registration and retires the GPU resource safely. Existing `TextureRef` values for that key should be treated as invalid after removal.

**Example:**

```cpp
const bool removed = app.images().removeImage("avatar");
```

See: [Full Doxygen reference](classFlowUi_1_1ImageManager.html#ab1bb3be63b631b3947dc8d8a1950b7b5).

### **contains**


#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` image key to test.

Checks whether an image key is currently registered. This performs no file IO or GPU work.

**Example:**

```cpp
if (!app.images().contains("avatar")) { app.images().registerImage("avatar", "assets/avatar.png"); }
```

See: [Full Doxygen reference](classFlowUi_1_1ImageManager.html#a3ce30605daebf796adc6e757a93848c1).

### **getTexture**


#### `TextureRef getTexture(std::string_view key) const`

- **Returns:** `TextureRef`
- **Arguments:** `key` registered image key.

Returns the texture reference for a registered image. Missing keys return fallback texture id `0` and log a warning once for that key.

**Example:**

```cpp
FlowUi::TextureRef avatar = app.images().getTexture("avatar");
```

See: [Full Doxygen reference](classFlowUi_1_1ImageManager.html#a8db367968a237f11a7157d194a3720bc).
