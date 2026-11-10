# Font Manager API

The facade is [FontManager.hpp](../../include/FlowUi/Public/managers/FontManager.hpp). CPU identities, descriptors, and glyph/face metadata are in [FontTypes.hpp](../../include/FlowUi/Public/managerStructs/FontTypes.hpp); native atlas data is in [interop/vulkan/FontAtlas.hpp](../../include/FlowUi/Public/interop/vulkan/FontAtlas.hpp). The native structure retains its existing allocation handle and all original fields as borrowed data; ownership remains with FlowUi.

## Aliases

### **FontId**


#### `using FontId = uint16_t`

Stable concrete font id consumed by Clay text configuration. It identifies one loaded font face.

### **FontFamilyId**


#### `using FontFamilyId = uint32_t`

Stable logical font family id returned by `FlowUi::FontManager`. It identifies a group of concrete faces used for style resolution.

## Enums

### **FontStyle**


#### `enum class FontStyle : uint8_t`

Font style requested during font resolution. Current public values distinguish normal and italic faces.

## Public Structs

### **FontFaceCreateInfo**


#### `struct FontFaceCreateInfo`

Describes one concrete font face to load into a family. It includes source path, pixel size, weight, style, and optional name.

### **FontFamilyCreateInfo**


#### `struct FontFamilyCreateInfo`

Describes a logical font family and its initial faces. The family name is used for lookup, while faces provide style and weight variants.

### **FlowUi::Font::GlyphData**


#### `struct FlowUi::Font::GlyphData`

Baked glyph metrics and atlas coordinates loaded from font resources. It is consumed by text layout and rendering code.

### **FlowUi::Font::FontVariantData**


#### `struct FlowUi::Font::FontVariantData`

One baked variant of a font face. It stores metrics, glyphs, Unicode lookup, and kerning data.

### **FlowUi::Font::FontFaceData**


#### `struct FlowUi::Font::FontFaceData`

Loaded font face and its baked variants. It exposes atlas placement, source metadata, and default variant access.

### **FlowUi::Font::AtlasArrayResource**


#### `struct FlowUi::Font::AtlasArrayResource`

Vulkan resources for the font atlas array. Renderer integrations can inspect image, view, sampler, dimensions, layer counts, and binding revision.

## Public API

### **createFamily**


#### `Result<FontFamilyId> createFamily(const FontFamilyCreateInfo& createInfo)`

- **Returns:** `Result<FontFamilyId>`; check success before reading the value, and use `.error()` on failure.
- **Arguments:** `createInfo` logical family name and initial concrete faces.

Creates a logical font family and immediately loads its listed faces. Family names must be unique, and face paths load baked `.arfont` files unless runtime font baking is enabled for `.ttf`.

**Example:**

```cpp
const auto family = app.fonts().createFamily({.name = "Body"});
if (!family) {
    throw FlowUi::FlowUiException(family.error());
}
const FlowUi::FontFamilyId bodyFamily = *family;
```

See: [Owning header](../../include/FlowUi/Public/managers/FontManager.hpp).

### **getFamilyId**


#### `FontFamilyId getFamilyId(std::string_view familyName) const`

- **Returns:** `FontFamilyId`
- **Arguments:** `familyName` logical family name.

Looks up a previously registered family id by name. Missing families return `UINT32_MAX`. Lookup metadata may allocate.

**Example:**

```cpp
FlowUi::FontFamilyId bodyFamily = app.fonts().getFamilyId("Body");
```

See: [Owning header](../../include/FlowUi/Public/managers/FontManager.hpp).

### **addFamilyFace** `1/2`


#### `Result<FontId> addFamilyFace(FontFamilyId familyId, const FontFaceCreateInfo& createInfo)`

- **Returns:** `Result<FontId>`; check success before reading the value, and use `.error()` on failure.
- **Arguments:** `familyId` existing family id, `createInfo` concrete face source and style data.

Adds a concrete face to an existing family by id. The new face becomes available to `resolveFont()` for its weight and style.

**Example:**

```cpp
const auto face = app.fonts().addFamilyFace("Body", {.path = "assets/fonts/Inter-Bold.arfont", .weight = 700});
if (!face) {
    throw FlowUi::FlowUiException(face.error());
}
const FlowUi::FontId boldFace = *face;
```

See: [Owning header](../../include/FlowUi/Public/managers/FontManager.hpp).

### **addFamilyFace** `2/2`


#### `Result<FontId> addFamilyFace(std::string_view familyName, const FontFaceCreateInfo& createInfo)`

- **Returns:** `Result<FontId>`; check success before reading the value, and use `.error()` on failure.
- **Arguments:** `familyName` existing family name, `createInfo` concrete face source and style data.

Adds a concrete face to an existing family by name. Use this when the caller has not cached the family id.

**Example:**

```cpp
const auto face = app.fonts().addFamilyFace("Body", {.path = "assets/fonts/Inter-Bold.arfont", .weight = 700});
if (!face) {
    throw FlowUi::FlowUiException(face.error());
}
const FlowUi::FontId boldFace = *face;
```

See: [Owning header](../../include/FlowUi/Public/managers/FontManager.hpp).

### **resolveFont** `1/2`


#### `FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyId` existing family id, `weight` requested weight, `style` requested style.

Resolves a logical font request to the best concrete face in a family. It prefers matching style and closest weight, with fallback behavior for missing variants.

**Example:**

```cpp
FlowUi::FontId bodyFont = app.fonts().resolveFont("Body", 400, FlowUi::FontStyle::Normal);
```

See: [Owning header](../../include/FlowUi/Public/managers/FontManager.hpp).

### **resolveFont** `2/2`


#### `FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyName` existing family name, `weight` requested weight, `style` requested style.

Named-family overload for resolving a concrete Clay font id. Returns `0` when the family is missing or empty.

**Example:**

```cpp
FlowUi::FontId bodyFont = app.fonts().resolveFont("Body", 400, FlowUi::FontStyle::Normal);
```

See: [Owning header](../../include/FlowUi/Public/managers/FontManager.hpp).

### **getFontById**


#### `const FlowUi::Font::FontFaceData* getFontById(FontId fontId) const`

- **Returns:** `const FlowUi::Font::FontFaceData*`
- **Arguments:** `fontId` concrete font id.

Returns loaded font metrics, glyphs, kerning, and atlas placement for a concrete face. Normal UI code usually only needs `resolveFont()`, but renderer or advanced layout integrations may need this data.

**Example:**

```cpp
const FlowUi::Font::FontFaceData* face = app.fonts().getFontById(bodyFont);
```

See: [Owning header](../../include/FlowUi/Public/managers/FontManager.hpp).

### **getAtlasResource**


#### `const FlowUi::Font::AtlasArrayResource& getAtlasResource() const`

- **Returns:** `const FlowUi::Font::AtlasArrayResource&`
- **Arguments:** none.

Returns the Vulkan atlas array resource used by FlowUi text rendering. Use `bindingRevision` to decide when external descriptors need refreshing.

**Example:**

```cpp
const FlowUi::Font::AtlasArrayResource& atlas = app.fonts().getAtlasResource();
```

See: [Owning header](../../include/FlowUi/Public/managers/FontManager.hpp).

### **kerningKey**


#### `static uint64_t kerningKey(uint32_t leftCodepoint, uint32_t rightCodepoint)`

- **Returns:** `uint64_t`
- **Arguments:** `leftCodepoint` left glyph codepoint, `rightCodepoint` right glyph codepoint.

Packs two codepoints into the key used by the kerning lookup table. This is mainly useful when inspecting or extending font metric data.

**Example:**

```cpp
uint64_t key = FlowUi::Font::FontVariantData::kerningKey(U'A', U'V');
```

See: [Owning header](../../include/FlowUi/Public/managerStructs/FontTypes.hpp).

### **kerningAdvance**


#### `float kerningAdvance(uint32_t leftCodepoint, uint32_t rightCodepoint) const`

- **Returns:** `float`
- **Arguments:** `leftCodepoint` left glyph codepoint, `rightCodepoint` right glyph codepoint.

Returns kerning advance for a codepoint pair. Missing pairs return `0.0f`.

**Example:**

```cpp
float advance = variant.kerningAdvance(U'A', U'V');
```

See: [Owning header](../../include/FlowUi/Public/managerStructs/FontTypes.hpp).

### **defaultVariant**


#### `const FontVariantData* defaultVariant() const`

- **Returns:** `const FontVariantData*`
- **Arguments:** none.

Returns the default baked variant for a loaded font face. Returns `nullptr` when the face has no variants or the default index is invalid.

**Example:**

```cpp
const FlowUi::Font::FontVariantData* variant = face->defaultVariant();
```

See: [Owning header](../../include/FlowUi/Public/managerStructs/FontTypes.hpp).
