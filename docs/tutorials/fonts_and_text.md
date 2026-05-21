# Fonts and Text

This tutorial follows one app-level font loading story from `AppConfig` to `CLAY_TEXT`. The focus is only fonts and text: how font files become families, how variants are added, how resolution chooses a concrete `FontId`, and where the lower-level font resources can be inspected.

## Contents

- [Chapter 1: The Example App](#chapter-1-the-example-app)
  - [Starting With the Default Family](#starting-with-the-default-family)
  - [Discovered Font Files](#discovered-font-files)
  - [The Loading Plan](#the-loading-plan)
- [Chapter 2: Create Families and Add Faces](#chapter-2-create-families-and-add-faces)
  - [Create Empty Families](#create-empty-families)
  - [Add Faces by Family Id](#add-faces-by-family-id)
  - [Add Faces by Family Name](#add-faces-by-family-name)
  - [Look Up Family Ids Later](#look-up-family-ids-later)
- [Chapter 3: Resolve Fonts for Text](#chapter-3-resolve-fonts-for-text)
  - [Resolve Existing Variants](#resolve-existing-variants)
  - [Resolve Missing Weights](#resolve-missing-weights)
  - [Resolve Missing Styles](#resolve-missing-styles)
  - [Use the Resolved FontIds](#use-the-resolved-fontids)
- [Chapter 4: Inspect Font Resources](#chapter-4-inspect-font-resources)
  - [Get Font Data by Id](#get-font-data-by-id)
  - [Get the Atlas Resource](#get-the-atlas-resource)
- [Final Shape](#final-shape)

## Chapter 1: The Example App

### Starting With the Default Family

Assume the app starts with a simple default family named `"Default"` in `AppConfig`. This is the family FlowUi attempts to load during `makeApplication()`, before the first frame.

```cpp
FlowUi::AppConfig config{};
config.window.title = "Font Tutorial";
config.ui.defaultFontFamily = FlowUi::FontFamilyCreateInfo{
    .name = "Default",
    .faces = {
        FlowUi::FontFaceCreateInfo{
            .path = "assets/fonts/Inter-Regular.arfont",
            .pixelSize = 18.0f,
            .weight = 400,
            .style = FlowUi::FontStyle::Normal,
            .name = "Inter Regular",
        },
    },
};

FlowUi::App app = FlowUi::makeApplication(config);
```

The default family gives the app a safe baseline. Later families can be richer, but it is still useful to have `"Default"` as the fallback you can resolve when a custom family is unavailable.

### Discovered Font Files

Now imagine the app has a project font scanner. It returns font files and metadata collected from a theme folder. The list contains both baked `.arfont` files and `.ttf` files.

```cpp
struct DiscoveredFontFace {
    std::filesystem::path path;
    std::string family;
    uint32_t weight = 400;
    FlowUi::FontStyle style = FlowUi::FontStyle::Normal;
    float pixelSize = 18.0f;
    std::string name;
};

std::vector<DiscoveredFontFace> discoverProjectFonts();
```

For this tutorial, assume it returned three variants for `"Interface"` and two variants for `"Mono"`.

```cpp
std::vector<DiscoveredFontFace> discovered = {
    {"assets/fonts/Inter-Regular.arfont", "Interface", 400, FlowUi::FontStyle::Normal, 18.0f, "Interface Regular"},
    {"assets/fonts/Inter-SemiBold.arfont", "Interface", 600, FlowUi::FontStyle::Normal, 18.0f, "Interface Semibold"},
    {"assets/fonts/Inter-Italic.ttf", "Interface", 400, FlowUi::FontStyle::Italic, 18.0f, "Interface Italic"},
    {"assets/fonts/JetBrainsMono-Regular.arfont", "Mono", 400, FlowUi::FontStyle::Normal, 16.0f, "Mono Regular"},
    {"assets/fonts/JetBrainsMono-Bold.ttf", "Mono", 700, FlowUi::FontStyle::Normal, 16.0f, "Mono Bold"},
};
```

`.arfont` files are baked assets and are the normal path. `.ttf` files only work when FlowUi is built with runtime font baking enabled; otherwise they are not the recommended runtime dependency.

### The Loading Plan

We will load the discovered fonts like this:

- Keep `"Default"` from config as the basic fallback family.
- Create `"Interface"` and `"Mono"` as extra families.
- For `"Interface"`, add faces using the `FontFamilyId` overload.
- For `"Mono"`, add faces using the `std::string_view` family-name overload.
- Resolve fonts into `Clay_TextElementConfig` values for actual text.
- Briefly inspect `getFontById()` and `getAtlasResource()`.

## Chapter 2: Create Families and Add Faces

### Create Empty Families

After creating the app, grab the app-owned `FlowUi::FontManager`.

```cpp
FlowUi::FontManager& fonts = app.fonts();
```

The default family already exists because it came from `AppConfig`. The two project families are new, so create them with `createFamily()`.

```cpp
FlowUi::FontFamilyId interfaceFamily = fonts.createFamily({
    .name = "Interface",
    .faces = {},
});

FlowUi::FontFamilyId monoFamily = fonts.createFamily({
    .name = "Mono",
    .faces = {},
});
```

`createFamily()` can also load initial faces through `.faces`, but creating the family first makes it easier to demonstrate both `addFamilyFace()` overloads.

### Add Faces by Family Id

For `"Interface"`, use the id returned by `createFamily()`.

```cpp
for (const DiscoveredFontFace& face : discovered) {
    if (face.family != "Interface") {
        continue;
    }

    fonts.addFamilyFace(
        interfaceFamily,
        FlowUi::FontFaceCreateInfo{
            .path = face.path,
            .pixelSize = face.pixelSize,
            .weight = face.weight,
            .style = face.style,
            .name = face.name,
        });
}
```

The `FontFamilyId` overload is useful when setup code has already cached the family id. It avoids another lookup by name.

### Add Faces by Family Name

For `"Mono"`, use the family-name overload.

```cpp
for (const DiscoveredFontFace& face : discovered) {
    if (face.family != "Mono") {
        continue;
    }

    fonts.addFamilyFace(
        "Mono",
        FlowUi::FontFaceCreateInfo{
            .path = face.path,
            .pixelSize = face.pixelSize,
            .weight = face.weight,
            .style = face.style,
            .name = face.name,
        });
}
```

The `std::string_view` overload is convenient when the call site naturally has a family name and does not need to store the id.

### Look Up Family Ids Later

If another part of the app needs a family id later, use `getFamilyId()`.

```cpp
FlowUi::FontFamilyId defaultFamily = fonts.getFamilyId("Default");
FlowUi::FontFamilyId cachedMonoFamily = fonts.getFamilyId("Mono");
```

Missing families return `std::numeric_limits<FlowUi::FontFamilyId>::max()`.

```cpp
if (cachedMonoFamily == std::numeric_limits<FlowUi::FontFamilyId>::max()) {
    cachedMonoFamily = defaultFamily;
}
```

That makes `getFamilyId()` useful for optional theme fonts: check whether the family exists, then fall back to `"Default"` if it does not.

## Chapter 3: Resolve Fonts for Text

### Resolve Existing Variants

`resolveFont()` turns a logical family, weight, and style into a concrete `FlowUi::FontId` for Clay.

```cpp
FlowUi::FontId interfaceRegular = fonts.resolveFont(
    interfaceFamily,
    400,
    FlowUi::FontStyle::Normal);

FlowUi::FontId interfaceItalic = fonts.resolveFont(
    interfaceFamily,
    400,
    FlowUi::FontStyle::Italic);

FlowUi::FontId monoBold = fonts.resolveFont(
    "Mono",
    700,
    FlowUi::FontStyle::Normal);
```

The first two calls use the `FontFamilyId` overload. The last call uses the family-name overload.

### Resolve Missing Weights

Resolution first filters faces by the requested style, then chooses the closest weight inside that style.

Our `"Interface"` family has normal `400` and normal `600`, but not normal `700`.

```cpp
FlowUi::FontId interfaceRequested700 = fonts.resolveFont(
    "Interface",
    700,
    FlowUi::FontStyle::Normal);
```

Because a normal `700` face does not exist, FlowUi picks the closest normal weight. In this example that is `600`, not `400`.

This matters when designing font families: if you want missing bold requests to feel close to the target, load a semibold or bold face. If you only load `400 Normal`, every normal-weight request resolves to that face.

### Resolve Missing Styles

If no face matches the requested style, FlowUi falls back to the first face stored in the family.

For predictable fallback behavior, add the family default first: `400 Normal`.

```cpp
FlowUi::FontId monoItalicRequest = fonts.resolveFont(
    "Mono",
    400,
    FlowUi::FontStyle::Italic);
```

Our `"Mono"` family has only normal faces. Because no italic face exists, resolution returns the first face in `"Mono"`. Since we added `400 Normal` first, this behaves like a default `400 Normal` fallback.

If the family is missing or empty, resolution returns `0`.

```cpp
FlowUi::FontId missing = fonts.resolveFont("NotRegistered", 400, FlowUi::FontStyle::Normal);
```

### Use the Resolved FontIds

Resolved ids are assigned to `Clay_TextElementConfig::fontId`.

```cpp
Clay_TextElementConfig titleText{};
titleText.fontId = interfaceRequested700;
titleText.fontSize = 28;
titleText.textColor = FlowUi::Flow_Color("#f5f7fbff");

Clay_TextElementConfig bodyText{};
bodyText.fontId = interfaceRegular;
bodyText.fontSize = 18;
bodyText.textColor = FlowUi::Flow_Color("#c8d0dcff");

Clay_TextElementConfig noteText{};
noteText.fontId = interfaceItalic;
noteText.fontSize = 16;
noteText.textColor = FlowUi::Flow_Color("#8ea0b8ff");

Clay_TextElementConfig codeText{};
codeText.fontId = monoBold;
codeText.fontSize = 16;
codeText.textColor = FlowUi::Flow_Color("#d9e6ffff");
```

Then use those configs in your UI construction code.

```cpp
Clay_ElementDeclaration panel{};
panel.layout.sizing = {.width = CLAY_SIZING_FIXED(560.0f), .height = CLAY_SIZING_FIT(0)};
panel.layout.padding = CLAY_PADDING_ALL(18);
panel.layout.childGap = 10;
panel.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
panel.backgroundColor = FlowUi::Flow_Color("#151922ff");
panel.cornerRadius = CLAY_CORNER_RADIUS(8);

CLAY(app.ui().toClayEID("typography/panel"), panel) {
    CLAY_TEXT(app.ui().toClayString("Typography"), CLAY_TEXT_CONFIG(titleText));
    CLAY_TEXT(app.ui().toClayString("Normal interface copy"), CLAY_TEXT_CONFIG(bodyText));
    CLAY_TEXT(app.ui().toClayString("Italic note from the same family"), CLAY_TEXT_CONFIG(noteText));
    CLAY_TEXT(app.ui().toClayString("Mono bold code label"), CLAY_TEXT_CONFIG(codeText));
}
```

At this point, the normal app path is complete: load families, resolve font ids, draw text.

## Chapter 4: Inspect Font Resources

### Get Font Data by Id

`getFontById()` exposes the loaded face data for diagnostics, custom layout, or renderer interop.

```cpp
const FlowUi::Font::FontFaceData* face = fonts.getFontById(interfaceRegular);
if (face) {
    const std::string faceName = face->name;
    const uint32_t atlasLayer = face->atlasLayer;
    (void)faceName;
    (void)atlasLayer;
}
```

If you need the default baked variant, call `defaultVariant()`.

```cpp
const FlowUi::Font::FontVariantData* variant = face ? face->defaultVariant() : nullptr;
```

The variant contains metrics, glyph data, Unicode lookup, and kerning pairs. For example:

```cpp
const float avKerning = variant ? variant->kerningAdvance(U'A', U'V') : 0.0f;
const uint64_t avKey = FlowUi::Font::FontVariantData::kerningKey(U'A', U'V');
```

Most application UI code does not need this data. It is there when you need to inspect or extend text behavior.

### Get the Atlas Resource

`getAtlasResource()` returns the Vulkan atlas array used by FlowUi text rendering.

```cpp
const FlowUi::Font::AtlasArrayResource& atlas = fonts.getAtlasResource();
```

For a simple diagnostics line:

```cpp
const std::string atlasLine =
    "Atlas: " +
    std::to_string(atlas.layersUsed) + "/" +
    std::to_string(atlas.layersCapacity) +
    " layers, revision " +
    std::to_string(atlas.bindingRevision);
```

External renderer integration can inspect the image view and sampler:

```cpp
if (atlas.view != VK_NULL_HANDLE && atlas.sampler != VK_NULL_HANDLE) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = atlas.view;
    imageInfo.sampler = atlas.sampler;
}
```

FlowUi's own UI renderer already uses this atlas internally, so this is only for diagnostics and interop.

## Final Shape

The complete font story looks like this:

```cpp
FlowUi::AppConfig config{};
config.ui.defaultFontFamily = FlowUi::FontFamilyCreateInfo{
    .name = "Default",
    .faces = {FlowUi::FontFaceCreateInfo{.path = "assets/fonts/Inter-Regular.arfont", .pixelSize = 18.0f, .weight = 400}},
};

FlowUi::App app = FlowUi::makeApplication(config);
FlowUi::FontManager& fonts = app.fonts();

FlowUi::FontFamilyId interfaceFamily = fonts.createFamily({.name = "Interface", .faces = {}});
FlowUi::FontFamilyId monoFamily = fonts.createFamily({.name = "Mono", .faces = {}});
(void)monoFamily;

fonts.addFamilyFace(interfaceFamily, FlowUi::FontFaceCreateInfo{.path = "assets/fonts/Inter-Regular.arfont", .pixelSize = 18.0f, .weight = 400, .style = FlowUi::FontStyle::Normal});
fonts.addFamilyFace(interfaceFamily, FlowUi::FontFaceCreateInfo{.path = "assets/fonts/Inter-SemiBold.arfont", .pixelSize = 18.0f, .weight = 600, .style = FlowUi::FontStyle::Normal});
fonts.addFamilyFace(interfaceFamily, FlowUi::FontFaceCreateInfo{.path = "assets/fonts/Inter-Italic.ttf", .pixelSize = 18.0f, .weight = 400, .style = FlowUi::FontStyle::Italic});

fonts.addFamilyFace("Mono", FlowUi::FontFaceCreateInfo{.path = "assets/fonts/JetBrainsMono-Regular.arfont", .pixelSize = 16.0f, .weight = 400});
fonts.addFamilyFace("Mono", FlowUi::FontFaceCreateInfo{.path = "assets/fonts/JetBrainsMono-Bold.ttf", .pixelSize = 16.0f, .weight = 700});

FlowUi::FontId titleFont = fonts.resolveFont("Interface", 700, FlowUi::FontStyle::Normal);
FlowUi::FontId bodyFont = fonts.resolveFont(interfaceFamily, 400, FlowUi::FontStyle::Normal);
FlowUi::FontId noteFont = fonts.resolveFont(interfaceFamily, 400, FlowUi::FontStyle::Italic);
FlowUi::FontId codeFont = fonts.resolveFont("Mono", 700, FlowUi::FontStyle::Normal);

const FlowUi::Font::FontFaceData* face = fonts.getFontById(bodyFont);
const FlowUi::Font::AtlasArrayResource& atlas = fonts.getAtlasResource();
(void)face;
(void)atlas;
```

The key mental model is simple: `FlowUi::FontManager` owns the loaded faces and atlas. Your app asks for fonts in family/weight/style terms, and `resolveFont()` returns the concrete `FontId` that Clay text rendering needs.
