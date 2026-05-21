# Images, Icons, and Texture References

This tutorial follows one app-level asset story from startup registration to `Clay_ImageElementConfig`. The focus is images, SVG icons, `TextureRef`, and the icon atlas cache that turns size-dependent icon requests into reusable texture variants.

## Contents

- [Chapter 1: The Example App](#chapter-1-the-example-app)
  - [What We Are Building](#what-we-are-building)
  - [Asset Discovery](#asset-discovery)
  - [TextureRef Model](#textureref-model)
- [Chapter 2: Configure Icon Caching](#chapter-2-configure-icon-caching)
  - [Atlas Pages](#atlas-pages)
  - [Size Reuse Tolerance](#size-reuse-tolerance)
  - [Atlas Padding](#atlas-padding)
- [Chapter 3: Register Images](#chapter-3-register-images)
  - [Register Static Image Files](#register-static-image-files)
  - [Check Existing Images](#check-existing-images)
  - [Replace and Remove Images](#replace-and-remove-images)
  - [Get Image Texture References](#get-image-texture-references)
- [Chapter 4: Register Icons](#chapter-4-register-icons)
  - [Register SVG Files](#register-svg-files)
  - [Register SVG Source Text](#register-svg-source-text)
  - [Check and Remove Icons](#check-and-remove-icons)
  - [Get Icon Texture Requests](#get-icon-texture-requests)
- [Chapter 5: Draw Textures in UI](#chapter-5-draw-textures-in-ui)
  - [Store Texture References for Clay](#store-texture-references-for-clay)
  - [Draw Image Cards](#draw-image-cards)
  - [Draw Toolbar Icons](#draw-toolbar-icons)
- [Chapter 6: How the Icon Cache Works](#chapter-6-how-the-icon-cache-works)
  - [Registration Is Not Rasterization](#registration-is-not-rasterization)
  - [Frame Preparation Resolves Requests](#frame-preparation-resolves-requests)
  - [Cache Lookup and Rasterization](#cache-lookup-and-rasterization)
  - [Atlas Allocation and Eviction](#atlas-allocation-and-eviction)
- [Final Shape](#final-shape)

## Chapter 1: The Example App

### What We Are Building

Assume the app is a small asset browser. It has a toolbar, a project thumbnail grid, user avatars, and status badges.

```text
[open] [save] [sync]       Asset Browser

+----------------+  +----------------+
| thumbnail      |  | thumbnail      |
| avatar  name   |  | avatar  name   |
| check status   |  | warning status |
+----------------+  +----------------+
```

This target naturally touches the full image and icon path:

- Large thumbnails and avatars are normal image files handled by `ImageManager`.
- Toolbar/status symbols are SVG icons handled by `IconManager`.
- UI drawing receives `TextureRef` values from those managers.
- `UiManager::storeTexture()` copies each `TextureRef` into frame-local storage for Clay image commands.

### Asset Discovery

Pretend the app has project asset discovery functions.

```cpp
struct ProjectImageAsset {
    std::string key;
    std::filesystem::path path;
};

struct ProjectIconAsset {
    std::string key;
    std::filesystem::path path;
};

std::vector<ProjectImageAsset> discoverProjectImages();
std::vector<ProjectIconAsset> discoverProjectIcons();
```

For this tutorial, assume the app found these assets:

```cpp
std::vector<ProjectImageAsset> images = {
    {"thumbnail/tree", "assets/images/tree.png"},
    {"thumbnail/house", "assets/images/house.png"},
    {"avatar/lee", "assets/images/avatars/lee.png"},
};

std::vector<ProjectIconAsset> icons = {
    {"toolbar/open", "assets/icons/open.svg"},
    {"toolbar/save", "assets/icons/save.svg"},
    {"status/warning", "assets/icons/warning.svg"},
};
```

We will also register one inline SVG source for a checkmark status icon.

### TextureRef Model

Images, icons, and viewports all eventually draw through `FlowUi::TextureRef`.

```cpp
FlowUi::TextureRef ref{};
```

The manager-owned fields identify the texture:

- `id`
- `uv0x`, `uv0y`, `uv1x`, `uv1y`
- `sourceWidth`, `sourceHeight`

Application code should normally leave those alone. The app-facing options are:

- `fitMode`: `Stretch`, `Contain`, `Cover`, or `None`.
- `samplingMode`: `Linear` or `Nearest`.
- `tintEnabled`: whether the image command color should multiply the texture.

```cpp
FlowUi::TextureRef thumbnail = app.images().getTexture("thumbnail/tree");
thumbnail.fitMode = FlowUi::TextureFitMode::Cover;
thumbnail.samplingMode = FlowUi::TextureSamplingMode::Linear;
thumbnail.tintEnabled = false;
```

For icons, tinting is often useful:

```cpp
FlowUi::TextureRef saveIcon = app.icons().textureRef("toolbar/save");
saveIcon.fitMode = FlowUi::TextureFitMode::Contain;
saveIcon.tintEnabled = true;
```

## Chapter 2: Configure Icon Caching

### Atlas Pages

Icons are SVG documents. FlowUi rasterizes them on demand and stores rasterized variants inside atlas pages. Configure that before `makeApplication()`.

```cpp
FlowUi::AppConfig config{};
config.iconManager.atlasSize = 2048;
config.iconManager.maxAtlasPages = 6;

FlowUi::App app = FlowUi::makeApplication(config);
```

`atlasSize` is the width and height of each icon atlas page. `maxAtlasPages` limits how many pages the cache can allocate before it has to evict old variants.

### Size Reuse Tolerance

`sizeBucketStep` controls how aggressively icon variants are reused across close sizes.

```cpp
config.iconManager.sizeBucketStep = 8;
```

If an icon was cached at `32x32` and later requested at `34x34`, a tolerance of `8` lets FlowUi reuse the existing cached raster. This avoids creating a new atlas entry for every tiny layout change.

The cache prefers an exact match first. If there is no exact match, it looks for a close-enough larger variant before a close-enough smaller variant, because using a slightly larger raster usually preserves quality better.

### Atlas Padding

`atlasPadding` reserves empty pixels around each cached icon variant.

```cpp
config.iconManager.atlasPadding = 1;
```

Padding helps avoid filtering bleed between neighboring icons in the same atlas page.

## Chapter 3: Register Images

### Register Static Image Files

Use `app.images()` to access the app-owned `FlowUi::ImageManager`.

```cpp
FlowUi::ImageManager& imageManager = app.images();
```

Register discovered image files with application-defined keys.

```cpp
for (const ProjectImageAsset& image : images) {
    const bool inserted = imageManager.registerImage(image.key, image.path.string());
    (void)inserted;
}
```

`registerImage()` decodes the file, uploads it as a Vulkan texture, and registers it under the key. It returns `true` for a new key and `false` when an existing key was replaced.

### Check Existing Images

Use `contains()` for non-throwing lookup.

```cpp
if (!imageManager.contains("avatar/lee")) {
    (void)imageManager.registerImage("avatar/lee", "assets/images/avatars/lee.png");
}
```

This does not perform file IO or GPU work. It only checks the current registration table.

### Replace and Remove Images

Registering an existing image key replaces the texture while keeping the key active.

```cpp
const bool inserted = imageManager.registerImage(
    "thumbnail/tree",
    "assets/images/tree_updated.png");

if (!inserted) {
    // Existing key was replaced.
}
```

Use `removeImage()` when an asset is no longer valid.

```cpp
const bool removed = imageManager.removeImage("thumbnail/house");
```

Removed GPU resources are retired safely through FlowUi's frame cleanup path. Any old `TextureRef` values for that image key should be discarded.

### Get Image Texture References

Use `getTexture()` during UI construction.

```cpp
FlowUi::TextureRef treeThumbnail = imageManager.getTexture("thumbnail/tree");
treeThumbnail.fitMode = FlowUi::TextureFitMode::Cover;
treeThumbnail.tintEnabled = false;
```

If the key is missing, FlowUi returns fallback texture id `0` and logs a warning once for that missing key.

## Chapter 4: Register Icons

### Register SVG Files

Use `app.icons()` to access the app-owned `FlowUi::IconManager`.

```cpp
FlowUi::IconManager& iconManager = app.icons();
```

Register discovered SVG files with application-defined keys.

```cpp
for (const ProjectIconAsset& icon : icons) {
    const bool inserted = iconManager.registerFromFile(icon.key, icon.path.string());
    (void)inserted;
}
```

`registerFromFile()` parses the SVG immediately and stores the SVG document. It does not rasterize the icon yet.

### Register SVG Source Text

Use `registerSvg()` when the SVG is generated or embedded in code.

```cpp
constexpr std::string_view kCheckSvg = R"(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
    <path d="M6.2 11.3 2.9 8l1.2-1.2 2.1 2.1 5.7-5.7L13.1 4z"/>
</svg>
)";

const bool inserted = iconManager.registerSvg("status/check", kCheckSvg);
(void)inserted;
```

Like file registration, this parses the document immediately and delays rasterization until the icon is actually drawn.

### Check and Remove Icons

Use `contains()` to test whether an SVG document key is registered.

```cpp
if (!iconManager.contains("toolbar/save")) {
    (void)iconManager.registerFromFile("toolbar/save", "assets/icons/save.svg");
}
```

Use `remove()` to remove the SVG document and its cached atlas variants.

```cpp
const bool removed = iconManager.remove("status/warning");
```

After removal, previously returned `TextureRef` values for that icon key should be discarded.

### Get Icon Texture Requests

Use `textureRef()` during UI construction.

```cpp
FlowUi::TextureRef saveIcon = iconManager.textureRef("toolbar/save");
saveIcon.fitMode = FlowUi::TextureFitMode::Contain;
saveIcon.tintEnabled = true;
```

This is a request handle, not necessarily the final atlas texture. FlowUi resolves it after layout, when the icon's rendered size is known.

## Chapter 5: Draw Textures in UI

### Store Texture References for Clay

Clay image commands carry opaque image data. In FlowUi, pass a frame-stored `TextureRef*`.

```cpp
Clay_ImageElementConfig image{};
image.imageData = app.ui().storeTexture(treeThumbnail);
```

Do not pass the address of a temporary or local `TextureRef`. `storeTexture()` copies the ref into `UiManager` frame storage and returns a pointer that stays valid until the frame ends.

### Draw Image Cards

The thumbnail card uses an image texture and an icon texture in the same Clay subtree.

```cpp
FlowUi::TextureRef thumbnail = app.images().getTexture("thumbnail/tree");
thumbnail.fitMode = FlowUi::TextureFitMode::Cover;

FlowUi::TextureRef status = app.icons().textureRef("status/check");
status.fitMode = FlowUi::TextureFitMode::Contain;
status.tintEnabled = true;

Clay_ImageElementConfig thumbnailImage{};
thumbnailImage.imageData = app.ui().storeTexture(thumbnail);

Clay_ImageElementConfig statusImage{};
statusImage.imageData = app.ui().storeTexture(status);
```

Then emit the image nodes:

```cpp
Clay_ElementDeclaration card{};
card.layout.sizing = {.width = CLAY_SIZING_FIXED(220.0f), .height = CLAY_SIZING_FIT(0)};
card.layout.padding = CLAY_PADDING_ALL(10);
card.layout.childGap = 8;
card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
card.backgroundColor = FlowUi::Flow_Color("#151922ff");
card.cornerRadius = CLAY_CORNER_RADIUS(8);

Clay_ElementDeclaration thumbnailBox{};
thumbnailBox.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(130.0f)};

Clay_ElementDeclaration statusBox{};
statusBox.layout.sizing = {.width = CLAY_SIZING_FIXED(18.0f), .height = CLAY_SIZING_FIXED(18.0f)};

CLAY(app.ui().toClayEID("asset-card/tree"), card) {
    CLAY(app.ui().toClayEID("asset-card/tree/thumbnail"), thumbnailBox) {
        CLAY_IMAGE(thumbnailImage);
    }
    CLAY(app.ui().toClayEID("asset-card/tree/status"), statusBox) {
        CLAY_IMAGE(statusImage);
    }
}
```

The image manager texture is already a concrete texture slot. The icon manager texture is a request that will be resolved after Clay computes `statusBox` bounds.

### Draw Toolbar Icons

Toolbar icons are just more image commands. The only difference is that tinting is usually enabled and the bounding boxes are smaller.

```cpp
FlowUi::TextureRef openIcon = app.icons().textureRef("toolbar/open");
openIcon.fitMode = FlowUi::TextureFitMode::Contain;
openIcon.tintEnabled = true;

Clay_ImageElementConfig openImage{};
openImage.imageData = app.ui().storeTexture(openIcon);

Clay_ElementDeclaration button{};
button.layout.sizing = {.width = CLAY_SIZING_FIXED(32.0f), .height = CLAY_SIZING_FIXED(32.0f)};
button.layout.padding = CLAY_PADDING_ALL(6);
button.backgroundColor = FlowUi::Flow_Color("#202633ff");
button.cornerRadius = CLAY_CORNER_RADIUS(6);

CLAY(app.ui().toClayEID("toolbar/open"), button) {
    CLAY_IMAGE(openImage);
}
```

If this icon is drawn at `20x20` inside the padded button, FlowUi requests a cached icon variant close to `20x20` after layout.

## Chapter 6: How the Icon Cache Works

### Registration Is Not Rasterization

`registerSvg()` and `registerFromFile()` parse and store SVG documents. They do not allocate atlas space and do not produce a raster image yet.

That is important because SVG icons are size-dependent. A save icon drawn at `16x16`, `24x24`, and `48x48` may need different raster variants for good quality.

### Frame Preparation Resolves Requests

During UI construction, `textureRef("toolbar/save")` returns a request id. The request is stored in the Clay image command through `storeTexture()`.

After `endFrame()`, Clay has final bounding boxes for image nodes. FlowUi's icon manager then scans image render commands. For each icon request, it:

- Reads the final UI image bounding box.
- Converts that box to framebuffer pixels using the UI-to-framebuffer scale.
- Rounds up to a requested raster width and height.
- Replaces the request texture ref with the final cached atlas texture slot and UVs.

This is why icon rasterization happens after layout instead of when `textureRef()` is called.

### Cache Lookup and Rasterization

For each icon key and requested size, the manager looks for a cached variant.

The lookup order is:

1. Exact size match.
2. Close-enough larger variant within `IconManagerConfig::sizeBucketStep`.
3. Close-enough smaller variant within `IconManagerConfig::sizeBucketStep`.
4. No reusable variant, so rasterize the SVG at the requested size.

Larger close variants are preferred before smaller close variants because downsampling is usually better than stretching a smaller raster.

When a cached variant is used, FlowUi marks it as used for the current frame. When a new variant is needed, FlowUi rasterizes the SVG, allocates an atlas rectangle, uploads pixels into that atlas page, and records the texture slot and UV coordinates.

### Atlas Allocation and Eviction

Atlas pages are allocated up to `maxAtlasPages`. Each page has a free-rectangle allocator and uses `atlasPadding` around content.

If a new variant does not fit:

- FlowUi first tries available free rectangles in existing pages.
- If allowed, it creates a new atlas page.
- If no page can fit and the page limit is reached, it evicts the least-recently-used cached variant that was not referenced this frame.
- The evicted variant releases its atlas rectangle back to the page, and allocation is tried again.

If every cached variant is currently referenced and no space can be freed, the manager throws because the atlas is full and there is no evictable entry.

Removing an icon with `remove()` also removes its request id and cached variants, which frees atlas regions associated with that icon.

## Final Shape

The complete resource setup looks like this:

```cpp
FlowUi::AppConfig config{};
config.iconManager.atlasSize = 2048;
config.iconManager.maxAtlasPages = 6;
config.iconManager.sizeBucketStep = 8;
config.iconManager.atlasPadding = 1;

FlowUi::App app = FlowUi::makeApplication(config);

FlowUi::ImageManager& images = app.images();
FlowUi::IconManager& icons = app.icons();

for (const ProjectImageAsset& image : discoverProjectImages()) {
    if (!images.contains(image.key)) {
        (void)images.registerImage(image.key, image.path.string());
    }
}

for (const ProjectIconAsset& icon : discoverProjectIcons()) {
    if (!icons.contains(icon.key)) {
        (void)icons.registerFromFile(icon.key, icon.path.string());
    }
}

(void)icons.registerSvg("status/check", kCheckSvg);
```

The frame loop draws with `TextureRef` and `storeTexture()`:

```cpp
while (!app.shouldClose()) {
    app.beginFrame();

    FlowUi::TextureRef thumbnail = app.images().getTexture("thumbnail/tree");
    thumbnail.fitMode = FlowUi::TextureFitMode::Cover;

    FlowUi::TextureRef icon = app.icons().textureRef("status/check");
    icon.fitMode = FlowUi::TextureFitMode::Contain;
    icon.tintEnabled = true;

    Clay_ImageElementConfig thumbnailImage{};
    thumbnailImage.imageData = app.ui().storeTexture(thumbnail);

    Clay_ImageElementConfig iconImage{};
    iconImage.imageData = app.ui().storeTexture(icon);

    CLAY(app.ui().toClayEID("asset/thumbnail"), {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(220.0f), .height = CLAY_SIZING_FIXED(130.0f)}}}) {
        CLAY_IMAGE(thumbnailImage);
    }

    CLAY(app.ui().toClayEID("asset/status"), {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(18.0f), .height = CLAY_SIZING_FIXED(18.0f)}}}) {
        CLAY_IMAGE(iconImage);
    }

    app.endFrame();
    app.drawFrame();
}
```

The key model is that image textures are uploaded as concrete texture slots when registered, while icon textures start as SVG request handles and become cached atlas variants after layout reveals their final drawn size.
