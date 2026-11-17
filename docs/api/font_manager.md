# Font Manager API

[Public declarations](include/FlowUi/Public/managers/FontManager.hpp) · [Font values](include/FlowUi/Public/managerStructs/FontTypes.hpp)

Fonts are App-shared. Register families/faces during setup, then resolve `FontId` values for Clay text. The startup family is configured by `UiConfig::defaultFontFamily`. Additional source fonts require the configured runtime-baking build support; baked `.arfont` assets work without runtime baking. Family names and face IDs are separate identities.

## Acquire the manager

`App` getters return `Result<std::reference_wrapper<T>>`. Check the wrapper, then name the borrowed object as a normal reference:

**Example:**

```cpp
auto result = app.fonts();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::FontManager& manager = result->get();
```

The reference is borrowed; the App must outlive it, and window-owned managers must also stay within their owning window lifetime. The examples use an already acquired manager reference and existing field/window identities where needed. They choose an application policy of throwing `FlowUiException` after a failed Result; returning/reporting the error is equally possible. Include the relevant manager/value headers and `FlowUi/Public/errorManagement/Exception.hpp`.

ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

For `Result<bool>`, `if (!result)` tests operational failure; `*result` is the separate operation outcome. Never treat a successful `false` as an error.

## Value helpers: FontVariantData

### **kerningKey**

#### `[[nodiscard]] static constexpr uint64_t kerningKey(uint32_t leftCodepoint, uint32_t rightCodepoint) noexcept`

- **Returns:** `uint64_t`
- **Arguments:** `leftCodepoint` left Unicode codepoint, `rightCodepoint` right Unicode codepoint

Pack two codepoints into the kerning-table key. The left codepoint occupies the high 32 bits and the right occupies the low 32 bits.

This constexpr/noexcept helper does not validate Unicode scalar values.

**Example:**

```cpp
const auto kerningKey = FlowUi::Font::FontVariantData::kerningKey(static_cast<uint32_t>(U'A'), static_cast<uint32_t>(U'V'));
```

See: [Owning header](include/FlowUi/Public/managerStructs/FontTypes.hpp).

### **kerningAdvance**

#### `[[nodiscard]] float kerningAdvance(uint32_t leftCodepoint, uint32_t rightCodepoint) const noexcept`

- **Returns:** `float`
- **Arguments:** `leftCodepoint` left Unicode codepoint, `rightCodepoint` right Unicode codepoint

Read the stored horizontal kerning adjustment. Returns zero when the codepoint pair is absent.

The adjustment is in this variant's metric units; this lookup neither scales nor synthesizes kerning.

**Example:**

```cpp
const auto advance = value.kerningAdvance(static_cast<uint32_t>(U'A'), static_cast<uint32_t>(U'V'));
```

See: [Owning header](include/FlowUi/Public/managerStructs/FontTypes.hpp).

## Value helpers: FontFaceData

### **defaultVariant**

#### `[[nodiscard]] const FontVariantData* defaultVariant() const noexcept`

- **Returns:** `const FontVariantData*`
- **Arguments:** none.

Borrow the selected variant of a font face. Returns nullptr when the variant list is empty or defaultVariantIndex is out of bounds.

The pointer is invalidated by mutation/destruction of the variants or by invalidation of the containing borrowed face.

**Example:**

```cpp
const auto variant = value.defaultVariant();
```

See: [Owning header](include/FlowUi/Public/managerStructs/FontTypes.hpp).

## Public API

### **createFamily**

#### `[[nodiscard]] Result<FontFamilyId> createFamily(const FontFamilyCreateInfo& createInfo) noexcept`

- **Returns:** `Result<FontFamilyId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `createInfo` family or target configuration

Create a font family and synchronously load its initial faces. The value is the stable family ID. Duplicate names are FontFamilyAlreadyExists errors. An empty createInfo name uses Default, which startup normally already registered. The ResourceKey overload uses its normalized key name instead of createInfo.name. A failed family registration rolls back the candidate family and faces.

Faces accept baked .arfont assets or borrowed baked_data; source-font baking depends on FLOWUI_RUNTIME_FONT_BAKING. Borrowed bytes are consumed during this call. Family and face IDs remain valid for this manager's lifetime.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
FlowUi::FontFamilyCreateInfo createInfo{};
createInfo.name = "Body";
auto result = manager.createFamily(createInfo);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto familyId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **createFamily**

#### `[[nodiscard]] Result<FontFamilyId> createFamily(ResourceKey key, const FontFamilyCreateInfo& createInfo) noexcept`

- **Returns:** `Result<FontFamilyId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `createInfo` family or target configuration

Create a font family and synchronously load its initial faces. The value is the stable family ID. Duplicate names are FontFamilyAlreadyExists errors. An empty createInfo name uses Default, which startup normally already registered. The ResourceKey overload uses its normalized key name instead of createInfo.name. A failed family registration rolls back the candidate family and faces.

Faces accept baked .arfont assets or borrowed baked_data; source-font baking depends on FLOWUI_RUNTIME_FONT_BAKING. Borrowed bytes are consumed during this call. Family and face IDs remain valid for this manager's lifetime. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "Body"};
FlowUi::FontFamilyCreateInfo createInfo{};
createInfo.name = "Body";
auto result = manager.createFamily(key, createInfo);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto familyId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **getFamilyId**

#### `[[nodiscard]] Result<FontFamilyId> getFamilyId(std::string_view familyName) const noexcept`

- **Returns:** `Result<FontFamilyId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `familyName` registered font-family name

Look up a family ID without loading a font. A missing family is a successful std::numeric_limits<FontFamilyId>::max() value, not an error.

String family lookup is exact; ResourceKey lookup additionally validates domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.getFamilyId("Body");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto familyId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **getFamilyId**

#### `[[nodiscard]] Result<FontFamilyId> getFamilyId(ResourceKey key) const noexcept`

- **Returns:** `Result<FontFamilyId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity

Look up a family ID without loading a font. A missing family is a successful std::numeric_limits<FontFamilyId>::max() value, not an error.

String family lookup is exact; ResourceKey lookup additionally validates domain and scope. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "Body"};
auto result = manager.getFamilyId(key);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto familyId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **addFamilyFace**

#### `[[nodiscard]] Result<FontId> addFamilyFace(FontFamilyId familyId, const FontFaceCreateInfo& createInfo) noexcept`

- **Returns:** `Result<FontId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `familyId` registered font-family identity, `createInfo` family or target configuration

Synchronously load and append a face to an existing family. The value is a concrete FontId; a missing family is FontFamilyNotFound.

The manager owns loaded bytes. FontFaceCreateInfo::baked_data is borrowed only for this call; adding faces can invalidate previously borrowed FontFaceData pointers.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
FlowUi::FontFaceCreateInfo face{};
face.path = "assets/fonts/Inter.arfont";
auto result = manager.addFamilyFace(familyId, face);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto fontId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **addFamilyFace**

#### `[[nodiscard]] Result<FontId> addFamilyFace(std::string_view familyName, const FontFaceCreateInfo& createInfo) noexcept`

- **Returns:** `Result<FontId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `familyName` registered font-family name, `createInfo` family or target configuration

Synchronously load and append a face to an existing family. The value is a concrete FontId; a missing family is FontFamilyNotFound.

The manager owns loaded bytes. FontFaceCreateInfo::baked_data is borrowed only for this call; adding faces can invalidate previously borrowed FontFaceData pointers.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
FlowUi::FontFaceCreateInfo face{};
face.path = "assets/fonts/Inter.arfont";
auto result = manager.addFamilyFace("Body", face);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto fontId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **addFamilyFace**

#### `[[nodiscard]] Result<FontId> addFamilyFace(ResourceKey key, const FontFaceCreateInfo& createInfo) noexcept`

- **Returns:** `Result<FontId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `createInfo` family or target configuration

Synchronously load and append a face to an existing family. The value is a concrete FontId; a missing family is FontFamilyNotFound.

The manager owns loaded bytes. FontFaceCreateInfo::baked_data is borrowed only for this call; adding faces can invalidate previously borrowed FontFaceData pointers. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "Body"};
FlowUi::FontFaceCreateInfo face{};
face.path = "assets/fonts/Inter.arfont";
auto result = manager.addFamilyFace(key, face);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto fontId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **resolveFont**

#### `[[nodiscard]] Result<FontId> resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const noexcept`

- **Returns:** `Result<FontId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `familyId` registered font-family identity, `weight` requested font weight, `style` requested face style

Select a registered face for a family, weight, and style. Success contains the nearest weight within the requested style (first registered face wins ties); if that style is absent, the first family face is used. A missing or empty family successfully resolves to fallback FontId 0.

Defaults are weight 400 and FontStyle::Normal. This query does not load or bake fonts.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.resolveFont(familyId, 400, FlowUi::FontStyle::Normal);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto fontId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **resolveFont**

#### `[[nodiscard]] Result<FontId> resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const noexcept`

- **Returns:** `Result<FontId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `familyName` registered font-family name, `weight` requested font weight, `style` requested face style

Select a registered face for a family, weight, and style. Success contains the nearest weight within the requested style (first registered face wins ties); if that style is absent, the first family face is used. A missing or empty family successfully resolves to fallback FontId 0.

Defaults are weight 400 and FontStyle::Normal. This query does not load or bake fonts.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.resolveFont("Body", 400, FlowUi::FontStyle::Normal);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto fontId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **resolveFont**

#### `[[nodiscard]] Result<FontId> resolveFont(ResourceKey key, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const noexcept`

- **Returns:** `Result<FontId>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** `key` logical resource identity, `weight` requested font weight, `style` requested face style

Select a registered face for a family, weight, and style. Success contains the nearest weight within the requested style (first registered face wins ties); if that style is absent, the first family face is used. A missing or empty family successfully resolves to fallback FontId 0.

Defaults are weight 400 and FontStyle::Normal. This query does not load or bake fonts. ResourceKey overloads infer the manager's resource domain when unspecified and require app-shared scope; incompatible explicit domains/scopes are errors. String keys use that default domain and scope.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
const FlowUi::ResourceKey key{.name = "Body"};
auto result = manager.resolveFont(key, 400, FlowUi::FontStyle::Normal);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto fontId = *result;
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **getFontById**

#### `[[nodiscard]] const FlowUi::Font::FontFaceData* getFontById(FontId fontId) const noexcept`

- **Returns:** `const FlowUi::Font::FontFaceData*`
- **Arguments:** `fontId` registered font-face identity

Borrow a loaded face and its metrics. Returns nullptr when the concrete ID is unknown, including on a detached manager.

Additional font registrations can reallocate face storage and invalidate the pointer. Reacquire it after loading fonts; do not retain it beyond the manager.

**Example:**

```cpp
const auto face = manager.getFontById(fontId);
```

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **getAtlasResource**

#### `[[nodiscard]] Result<std::reference_wrapper<const FlowUi::Font::AtlasArrayResource>> getAtlasResource() const noexcept`

- **Returns:** `Result<std::reference_wrapper<const FlowUi::Font::AtlasArrayResource>>`; check success before accessing the value; use `.error()` on failure.
- **Arguments:** none.

Borrow the current Vulkan font atlas resource. Success contains `std::reference_wrapper<const Font::AtlasArrayResource>`; a detached manager returns ObjectNotInitialized.

Unwrap with result->get() after checking the Result. The manager owns the atlas and Vulkan handles; never destroy them. The resource reference lasts until manager destruction/reinitialization, while bindingRevision changes when descriptor bindings need refreshing.

Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.

**Example:**

```cpp
auto result = manager.getAtlasResource();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::Font::AtlasArrayResource& borrowed = result->get();
```

## Build-gated development hooks

`FLOW_UI_DEV_MODE` and the development memory-level macros expose additional inspection/recording hooks in the header. They are development infrastructure, not the application API described here. Their existing build gates and signatures are unchanged.

## Font resource values

`FontFaceCreateInfo` owns its path/name and borrows optional `baked_data` only until registration returns (or until initialization completes when supplied through `UiConfig`). Nonempty baked bytes take precedence over the path. Defaults are 18 px, weight 400, normal style. `FontFamilyCreateInfo` defaults to name `Default` and the `assets/fonts/Inter.arfont` face; set the name/faces explicitly when registering an additional family.

`Font::FontFaceData` holds face metadata, variants, and atlas placement; `Font::FontVariantData` holds metrics, glyphs, Unicode lookup, and kerning. Access through a manager-returned pointer is read-only and borrowed. `Font::GlyphData` stores codepoint, plane/image bounds, source image index, and advances.

[AtlasArrayResource declaration](include/FlowUi/Public/interop/vulkan/FontAtlas.hpp) must be included when reading atlas fields; `FontManager.hpp` only forward-declares this Vulkan interop type.

| Atlas field | Meaning |
| --- | --- |
| `image`, `allocation`, `view`, `sampler` | Borrowed Vulkan/VMA resources; never destroy them. |
| `width`, `height` | Atlas extent in pixels. |
| `layersUsed`, `layersCapacity` | Published and allocated array-layer counts. |
| `bindingRevision` | Re-read binding handles and refresh descriptors when this revision changes. |

**Example:**

```cpp
#include "FlowUi/Public/interop/vulkan/FontAtlas.hpp"
auto result = fonts.getAtlasResource();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::Font::AtlasArrayResource& atlas = result->get();
VkDescriptorImageInfo descriptor{};
descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
descriptor.imageView = atlas.view;
descriptor.sampler = atlas.sampler;
```

The descriptor also borrows these handles. Rebuild it after `bindingRevision` changes and finish using it before the owning App is destroyed.

## Checked development inspection examples

These hooks require `FLOW_UI_DEV_MODE`; viewport hooks also require `FLOWUI_PUBLIC_VULKAN_INTEROP`. They are observation/tooling APIs. A visitor returns `true` to continue; a successful returned `false` is distinct from a failed `Result`. Callback arguments and their resource views are borrowed. The examples use the already acquired manager for the owning App/window.

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).

### **visitDevFonts**

#### `Result<bool> visitDevFonts(void* userData, DevFontVisitor visitor) const noexcept`

- **Returns:** `Result<bool>`; check success before accessing the value; use `.error()` on failure.

Observe development catalogue entries without invoking application operations. Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.

Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts loaded face rows; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
std::size_t count = 0;
auto result = manager.visitDevFonts(&count,
    [](void* target, const FlowUi::FontManager::DevFontView& face) {
        auto& observedCount = *static_cast<std::size_t*>(target);
        if (face.face != nullptr) {
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

See: [Owning header](include/FlowUi/Public/managers/FontManager.hpp).
