# Font Manager API

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


#### `FontFamilyId createFamily(const FontFamilyCreateInfo& createInfo)`

- **Returns:** `FontFamilyId`
- **Arguments:** `createInfo` logical family name and initial concrete faces.

Creates a logical font family and immediately loads its listed faces. Family names must be unique, and face paths load baked `.arfont` files unless runtime font baking is enabled for `.ttf`.

**Example:**

```cpp
FlowUi::FontFamilyId bodyFamily = app.fonts().createFamily({.name = "Body"});
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a2fd6b73942b1e8ffbccd4420de098b3f).

### **getFamilyId**


#### `FontFamilyId getFamilyId(std::string_view familyName) const`

- **Returns:** `FontFamilyId`
- **Arguments:** `familyName` logical family name.

Looks up a previously registered family id by name. Missing families return `UINT32_MAX`, making this a non-throwing cache-friendly lookup.

**Example:**

```cpp
FlowUi::FontFamilyId bodyFamily = app.fonts().getFamilyId("Body");
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a829f7e37438eef1eb8fa9981a8d511ec).

### **addFamilyFace** `1/2`


#### `FontId addFamilyFace(FontFamilyId familyId, const FontFaceCreateInfo& createInfo)`

- **Returns:** `FontId`
- **Arguments:** `familyId` existing family id, `createInfo` concrete face source and style data.

Adds a concrete face to an existing family by id. The new face becomes available to `resolveFont()` for its weight and style.

**Example:**

```cpp
FlowUi::FontId boldFace = app.fonts().addFamilyFace("Body", {.path = "assets/fonts/Inter-Bold.arfont", .weight = 700});
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#aaa09f54aa73849bba861457941bf1f19).

### **addFamilyFace** `2/2`


#### `FontId addFamilyFace(std::string_view familyName, const FontFaceCreateInfo& createInfo)`

- **Returns:** `FontId`
- **Arguments:** `familyName` existing family name, `createInfo` concrete face source and style data.

Adds a concrete face to an existing family by name. Use this when the caller has not cached the family id.

**Example:**

```cpp
FlowUi::FontId boldFace = app.fonts().addFamilyFace("Body", {.path = "assets/fonts/Inter-Bold.arfont", .weight = 700});
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a5e530f7fa3bcb6b0c4da4987fc804a62).

### **resolveFont** `1/2`


#### `FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyId` existing family id, `weight` requested weight, `style` requested style.

Resolves a logical font request to the best concrete face in a family. It prefers matching style and closest weight, with fallback behavior for missing variants.

**Example:**

```cpp
FlowUi::FontId bodyFont = app.fonts().resolveFont("Body", 400, FlowUi::FontStyle::Normal);
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a39c0eeaed6002691f954b1bcc6eadd83).

### **resolveFont** `2/2`


#### `FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const`

- **Returns:** `FontId`
- **Arguments:** `familyName` existing family name, `weight` requested weight, `style` requested style.

Named-family overload for resolving a concrete Clay font id. Returns `0` when the family is missing or empty.

**Example:**

```cpp
FlowUi::FontId bodyFont = app.fonts().resolveFont("Body", 400, FlowUi::FontStyle::Normal);
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a32e8291f5b08bc0d46d0884b8b4bad12).

### **getFontById**


#### `const FlowUi::Font::FontFaceData* getFontById(FontId fontId) const`

- **Returns:** `const FlowUi::Font::FontFaceData*`
- **Arguments:** `fontId` concrete font id.

Returns loaded font metrics, glyphs, kerning, and atlas placement for a concrete face. Normal UI code usually only needs `resolveFont()`, but renderer or advanced layout integrations may need this data.

**Example:**

```cpp
const FlowUi::Font::FontFaceData* face = app.fonts().getFontById(bodyFont);
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#ad67d18ee896d36b0cd0b4567cf79c311).

### **getAtlasResource**


#### `const FlowUi::Font::AtlasArrayResource& getAtlasResource() const`

- **Returns:** `const FlowUi::Font::AtlasArrayResource&`
- **Arguments:** none.

Returns the Vulkan atlas array resource used by FlowUi text rendering. Use `bindingRevision` to decide when external descriptors need refreshing.

**Example:**

```cpp
const FlowUi::Font::AtlasArrayResource& atlas = app.fonts().getAtlasResource();
```

See: [Full Doxygen reference](structFlowUi_1_1FontManager.html#a20dada10a04d85cfb7b46f95c952085e).

### **kerningKey**


#### `static uint64_t kerningKey(uint32_t leftCodepoint, uint32_t rightCodepoint)`

- **Returns:** `uint64_t`
- **Arguments:** `leftCodepoint` left glyph codepoint, `rightCodepoint` right glyph codepoint.

Packs two codepoints into the key used by the kerning lookup table. This is mainly useful when inspecting or extending font metric data.

**Example:**

```cpp
uint64_t key = FlowUi::Font::FontVariantData::kerningKey(U'A', U'V');
```

See: [Full Doxygen reference](structFlowUi_1_1Font_1_1FontVariantData.html#aac641adb1ef8d9b2cfbaa10652b38986).

### **kerningAdvance**


#### `float kerningAdvance(uint32_t leftCodepoint, uint32_t rightCodepoint) const`

- **Returns:** `float`
- **Arguments:** `leftCodepoint` left glyph codepoint, `rightCodepoint` right glyph codepoint.

Returns kerning advance for a codepoint pair. Missing pairs return `0.0f`.

**Example:**

```cpp
float advance = variant.kerningAdvance(U'A', U'V');
```

See: [Full Doxygen reference](structFlowUi_1_1Font_1_1FontVariantData.html#acb58bcb53965391de0525cb6850a9d29).

### **defaultVariant**


#### `const FontVariantData* defaultVariant() const`

- **Returns:** `const FontVariantData*`
- **Arguments:** none.

Returns the default baked variant for a loaded font face. Returns `nullptr` when the face has no variants or the default index is invalid.

**Example:**

```cpp
const FlowUi::Font::FontVariantData* variant = face->defaultVariant();
```

See: [Full Doxygen reference](structFlowUi_1_1Font_1_1FontFaceData.html#ad97f5e22e625cb9698ddd429fc1c63e1).
