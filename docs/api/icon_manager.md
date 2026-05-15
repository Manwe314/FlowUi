# Icon Manager API

## Enums

### **TextureFitMode**
---

#### `enum class TextureFitMode : uint8_t`

Texture layout mode inside a target rectangle. Icon texture refs typically use contain sizing.

### **TextureSamplingMode**
---

#### `enum class TextureSamplingMode : uint8_t`

Texture filtering preference stored on TextureRef. It records intended filtering behavior for rendered icons.

## Public Structs

### **IconManagerConfig**
---

#### `struct IconManagerConfig`

Configures SVG icon rasterization and atlas caching. It controls atlas size, maximum pages, size reuse tolerance, and padding.

### **TextureRef**
---

#### `struct TextureRef`

Texture request handle returned by IconManager. FlowUi resolves it to a cached atlas variant sized to the rendered UI area.

## Public API


### **registerSvg**
---

#### `bool registerSvg(std::string_view key, std::string_view svgSource)`

- **Returns:** `bool`
- **Arguments:** `key` icon key, `svgSource` complete SVG source text.

Parses and registers an SVG document from memory. Raster variants are created lazily later when the icon is drawn at a particular size.

### **registerFromFile**
---

#### `bool registerFromFile(std::string_view key, std::string_view filePath)`

- **Returns:** `bool`
- **Arguments:** `key` icon key, `filePath` SVG file path.

Parses and registers an SVG document from disk. Returns `false` if the key already exists and the current icon is left unchanged.

### **remove**
---

#### `bool remove(std::string_view key)`

- **Returns:** `bool`
- **Arguments:** `key` registered icon key.

Removes a registered SVG document and its cached atlas variants. Previously returned texture references for the key should be discarded.

### **contains**
---

#### `bool contains(std::string_view key) const`

- **Returns:** `bool`
- **Arguments:** `key` icon key to test.

Checks whether an SVG document key is registered. This does not force rasterization or atlas allocation.

### **textureRef**
---

#### `TextureRef textureRef(std::string_view key)`

- **Returns:** `TextureRef`
- **Arguments:** `key` registered icon key.

Returns a texture request reference for a registered icon. FlowUi later resolves the request into a cached atlas variant sized to the rendered UI image area.
