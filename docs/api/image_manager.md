# Image Manager API

## Enums

### **TextureFitMode**
---

#### `enum class TextureFitMode : uint8_t`

Texture layout mode inside a target rectangle. Image texture refs use it to control stretch, contain, cover, or source-size behavior.

### **TextureSamplingMode**
---

#### `enum class TextureSamplingMode : uint8_t`

Texture filtering preference stored on TextureRef. It distinguishes linear and nearest sampling intent.

## Public Structs

### **TextureRef**
---

#### `struct TextureRef`

Renderer texture handle and draw options returned by ImageManager. App code may adjust fit mode, sampling mode, and tint behavior before drawing.

## Public API


### **registerImage**
---

#### `bool registerImage(std::string_view key, std::string_view filePath)`

- **Returns:** `bool`
- **Arguments:** `key` application image key, `filePath` image file path.

Loads an image file, uploads it, and registers it under the provided key. Returns `true` for a new key and `false` when replacing an existing key.

### **removeImage**
---

#### `bool removeImage(std::string_view key)`

- **Returns:** `bool`
- **Arguments:** `key` registered image key.

Removes an image registration and retires the GPU resource safely. Existing `TextureRef` values for that key should be treated as invalid after removal.

### **contains**
---

#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` image key to test.

Checks whether an image key is currently registered. This performs no file IO or GPU work.

### **getTexture**
---

#### `TextureRef getTexture(std::string_view key) const`

- **Returns:** `TextureRef`
- **Arguments:** `key` registered image key.

Returns the texture reference for a registered image. Missing keys return fallback texture id `0` and log a warning once for that key.
