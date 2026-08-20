# Managers

## Introduction

FlowUi managers are compact subsystem objects owned by the main `FlowUi::App`. They are "singleton-like" within one app instance: there is one action manager, one font manager, one image manager, one viewport manager, and so on for that running application. A `UiManager` remains window-specific and owns its input-field, shortcut, and popup services. Users do not create, initialize, destroy, or synchronize these managers manually. `App` owns them, wires them to the window, Vulkan context, renderer, texture registry, and frame lifecycle, then exposes references through accessors such as `app.ui()`, `app.actions()`, `app.fonts()`, `app.images()`, and `app.viewPorts()`.

## TL;DR

Managers exist because Clay is a layout library, not a full desktop application runtime. FlowUi adds subsystems for text, textures, icons, input fields, shortcuts, popup behavior, viewports, frame-local UI services, and rendering support so users can start writing application code sooner.

You normally access a manager by reference and pass that reference around:

```cpp
FlowUi::UiManager& ui = app.ui();
FlowUi::ActionManager& actions = app.actions();
FlowUi::FontManager& fonts = app.fonts();
FlowUi::ImageManager& images = app.images();
```

The manager object carries the relevant internal logic and lifetime rules for its subsystem. User code asks it to do useful work; `App` handles ownership and frame integration.

## Why Managers Exist

Clay gives FlowUi a fast immediate-mode layout engine. That solves layout, but a desktop UI runtime needs more than layout:

- text needs fonts, glyph metrics, atlas textures, and measurement
- images need file loading, GPU upload, texture slots, and retirement rules
- icons need SVG parsing, rasterization, atlas packing, and per-size caching
- input fields need text state, focus, caret, selection, and edit operations
- shortcuts need keyboard chord registration and dispatch
- popups need stable placement, measurement, stacking, overflow correction, and dismissal
- viewports need offscreen render targets and Vulkan callback integration
- UI construction needs frame-local string storage, texture storage, input snapshots, and Clay context ownership

Managers group those responsibilities into named subsystems. This avoids one giant global API and keeps each kind of lifetime in the place that understands it. Font resources live in the font manager. Editable text state lives in the input field manager. Shortcut registrations live in the shortcut manager. Viewport images and command resources live in the viewport manager.

The result is a runtime that stays explicit but does not force every user application to rebuild the same infrastructure before drawing a useful app.

## Ownership Model

Managers are owned by `FlowUi::App`. They are initialized during `makeApplication()` and destroyed during `App` cleanup. Most of them depend on internal runtime objects that user code should not own directly: the Vulkan device, allocator, renderer, texture registry, frame count, window clipboard bridge, cursor bridge, and font manager connection.

This is why the public API returns references:

```cpp
FlowUi::UiManager& ui = app.ui();
FlowUi::ImageManager& images = app.images();
```

Those references are convenient handles into app-owned subsystems. They can be passed into helper functions:

```cpp
void drawToolbar(FlowUi::UiManager& ui, FlowUi::ImageManager& images) {
    FlowUi::TextureRef logo = images.getTexture("toolbar/logo");
    (void)logo;
    ui.createElement(kToolbar, "toolbar").draw();
}
```

The references should not outlive the `App` that owns them. They are not independent services. They are parts of one running FlowUi application.

## ActionManager

`ActionManager` is app-owned and exposed through every `UiManager`. Its `appActions()` surface stores semantic bindings in `StorageSystem`; the application still owns every pointer or `reference_wrapper` supplied as a resource. Its `uiActions()` surface creates allocation-free transient calls from stateless recipes and local lvalues. See the [Action Manager API](../api/action_manager.md) for lifetime rules and examples.

## UiManager

`UiManager` is the main frame-authoring surface. Most user UI code touches it directly or indirectly.

It owns the Clay context, frame-local string arenas, frame-local texture reference storage, current and previous frame input snapshots, previous/current interaction snapshots, cursor requests, clipboard accessors, and the input-field, shortcut, and popup managers. It also bridges font resolution by forwarding requests to the connected `FlowUi::FontManager`.

Common use cases:

```cpp
FlowUi::UiManager& ui = app.ui();

ui.createElement(kButton, "toolbar/save")
    .setParameters(ButtonParams{.label = "Save"})
    .draw();
```

```cpp
Clay_String label = ui.toClayString(dynamicLabel);
Clay_ElementId rootId = ui.toClayEID("settings/root");
FlowUi::TextureRef* texture = ui.imageData(app.images().getTexture("avatar"));
```

`UiManager` is frame-sensitive. Functions like `toClayString()` and `imageData()` use frame-local arena memory, so their returned pointers are only valid for the current frame. Storage independently retains each resolved logical GPU texture through submission.

## FlowUi::FontManager

`FlowUi::FontManager` owns loaded font faces, logical font families, font resolution, and the Vulkan font atlas array used by text rendering.

The common workflow is:

```cpp
FlowUi::FontFamilyId body = app.fonts().createFamily({
    .name = "Body",
    .faces = {{.path = "assets/fonts/Inter.arfont", .pixelSize = 18.0f}},
});

FlowUi::FontId bodyFont = app.fonts().resolveFont(body, 400, FlowUi::FontStyle::Normal);
```

Users generally think in terms of families, weights, and styles. The renderer needs a concrete `FontId`. `FlowUi::FontManager` bridges that gap and keeps the loaded font data and atlas resources alive.

It also supports adding more faces to a family:

```cpp
app.fonts().addFamilyFace("Body", {.path = "assets/fonts/Inter-Bold.arfont", .weight = 700});
```

This lets UI code ask for "Body, weight 700" without caring which exact baked face was loaded internally.

## ImageManager

`ImageManager` loads image files and exposes them as `TextureRef` values for UI drawing.

Typical usage:

```cpp
app.images().registerImage("profile/avatar", "assets/avatar.png");
FlowUi::TextureRef avatar = app.images().getTexture("profile/avatar");
```

Internally, the image manager decodes the file, uploads it as a Vulkan texture, registers it with the UI texture registry, and gives user code a compact texture reference. The returned `TextureRef` can then be passed through an element params struct or stored into Clay image data with `UiManager::storeTexture()`.

The image manager also owns replacement and removal behavior:

```cpp
app.images().removeImage("profile/avatar");
```

That matters because GPU resources cannot always be destroyed immediately while frames are still in flight. The manager hides those frame-safe lifetime details from user code.

## IconManager

`IconManager` is the SVG icon subsystem. It is available when FlowUi is built with icon manager support enabled.

Typical usage:

```cpp
app.icons().registerFromFile("toolbar/save", "assets/icons/save.svg");
FlowUi::TextureRef saveIcon = app.icons().textureRef("toolbar/save");
```

SVG icons are different from normal images because the useful raster size depends on where the icon is drawn. FlowUi stores the SVG document under a key, then during frame preparation it rasterizes and caches atlas variants sized for the rendered UI area.

This means app code can work with stable icon keys and texture refs while the manager handles parsing, rasterization, atlas allocation, cache reuse, and texture remapping.

## InputFieldManager

`InputFieldManager` owns editable text field state. It is reached through `UiManager`:

```cpp
FlowUi::InputFieldManager& inputFields = app.ui().inputFields();
```

Clay can lay out text and boxes, but editable input needs persistent text, focus, carets, selections, pointer hit testing, keyboard edits, key repeat, copy/paste support, and caret/selection rendering. `InputFieldManager` owns that behavior so custom input elements can be written as reusable visual wrappers instead of full text editor implementations.

Typical custom element code requests a field every frame:

```cpp
FlowUi::FieldQueryResult field = context.uiManager.inputFields().requestField(context.id, {
    .initialText = context.params.initialText,
    .textElementId = textId,
    .contentElementId = contentId,
});
```

Interaction callbacks can request focus:

```cpp
context.uiManager.inputFields().requestCaret(context.id, FlowUi::CaretRequestKind::SetPrimary);
```

The field's text lives in the manager, not in the element's params. That lets the element be rebuilt every frame while the user's edited text persists.

## ShortcutManager

`ShortcutManager` owns keyboard shortcut registration and dispatch. It is also reached through `UiManager`:

```cpp
FlowUi::ShortcutManager& shortcuts = app.ui().shortcuts();
```

Shortcuts are registered as chords with a scope, priority, and callback:

```cpp
FlowUi::ShortcutId saveShortcut = shortcuts.registerShortcut(
    {.key = GLFW_KEY_S, .ctrl = true},
    FlowUi::ShortcutScope::Global,
    100,
    [](FlowUi::ShortcutContext&) {
        saveDocument();
        return true;
    });
```

Each frame, the shortcut manager compares current and previous `FrameInput`, detects press/release/down transitions, checks scope, orders callbacks by priority, and stops dispatch when a callback returns `true`.

Scopes let local behavior handle a chord before global fallback behavior. For example, a focused input field can handle copy/paste while the app still has global shortcuts elsewhere.

## PopupManager

`PopupManager` is the window-scoped behavior service for floating popup roots. It is reached through `UiManager`:

```cpp
FlowUi::PopupManager& popups = app.ui().popups();
```

Custom elements submit a stable popup identity and `PopupRequest` each visible frame. The manager resolves parent, element, pointer, position, or viewport anchors; translates the requested nine-point attachment into Clay floating configuration; corrects viewport overflow once a size is known; assigns a z-index from the chosen popup layer; and reports automatic outside-press or Escape dismissal. Outside presses can be ignored, can dismiss while blocking only the popup anchor, or can dismiss while consuming the complete pointer gesture.

The manager does not prescribe visual styling, content, modal backdrops, focus trapping, or application-owned open state. `FlowUi::FSEL::kPopupSurface` is one convenience element built on this general service. See the [Popup Manager API](../api/popup_manager.md) for the request contract, first-frame measurement behavior, and custom-element example.

## ViewPortManager

`ViewPortManager` owns offscreen render targets that can be displayed inside the UI as textures. It is available when public Vulkan interop is enabled.

Typical usage:

```cpp
app.viewPorts().create("preview", {.clearColor = {0.02f, 0.02f, 0.03f, 1.0f}});

if (FlowUi::ViewPort* preview = app.viewPorts().getViewPort("preview")) {
    preview->setRenderCallback([](const FlowUi::ViewPortRenderContext& context) {
        recordPreviewCommands(context);
    });
}
```

Then UI can display it through a texture ref:

```cpp
FlowUi::TextureRef previewTexture = app.viewPorts().getTexture("preview");
```

The manager handles per-frame render target images, size tracking from UI image bounds, command resources, texture registry slots, and invoking render callbacks. User code records custom Vulkan work into the callback context, but the manager owns the target and frame integration.

This is the subsystem to use for scene previews, graph views, custom renderers, editor canvases, and other content that needs to be rendered separately and then composed into the Clay UI.

## FlowUi::ThemeManager

`ThemeManager` owns registrable theme structs, theme variant mapping, active variant dispatch, and atomic staged theme mutations.

Typical usage:

```cpp
app.themes().registerTheme<AppTheme>("dark", darkTheme, true);
app.themes().registerTheme<AppTheme>("light", lightTheme, false);

// In UI element code:
const auto& theme = context.uiManager.theme<AppTheme>();
```

Staged theme mutations take effect at frame boundaries (`app.pollEvents()` / `beginFrame()`):

```cpp
app.themes().updateActiveTheme<AppTheme>([](AppTheme& t) {
    t.brandAccent = Flow_Color("#ff0055ff");
});
```

`ThemeManager` integrates persistent storage (`FlowStorageSystem`) with lock-free reading, allowing immediate-mode elements to query design tokens safely and efficiently.

## Logical Textures and Renderer-Side Management

Image, icon, and viewport managers all return the same logical `TextureRef` type.

Managers do not hand Clay raw Vulkan handles or renderer-local slots. A `TextureRef` carries a generational app-level handle, UV coordinates, source size, fit mode, sampling mode, and tint behavior. Before direct instance emission, storage resolves the handle to the current window/frame descriptor index.

This lets different resource systems share one rendering path:

- `ImageManager` registers standalone image textures.
- `IconManager` registers atlas-backed SVG raster variants.
- `ViewPortManager` registers per-frame viewport images.

User code only needs to pass `TextureRef` values through UI code.

## How Managers Work Together During a Frame

Managers are not isolated utilities; they are wired into the frame lifecycle.

During `beginFrame()`:

- texture slots retired on this frame bucket can be reclaimed
- image and viewport managers start frame-specific cleanup/tracking
- `UiManager` resets frame arenas and begins Clay layout
- input fields and shortcuts receive current and previous input

During UI construction:

- user code asks managers for ids, texture refs, font ids, field state, or shortcut behavior
- Flow elements pass those values through params and callbacks
- Clay nodes are emitted with frame-local strings and texture pointers

During `endFrame()`:

- Clay produces render commands
- input fields compute caret and selection overrides
- icons resolve requested SVG sizes into cached atlas textures
- viewports inspect UI usage to prepare render target sizes

During `drawFrame()`:

- viewports record offscreen passes
- the renderer consumes font atlas data, texture refs, and input-field overrides
- manager-owned resources are used without user code manually binding them

The point is that managers are not just containers. They are subsystems that know when their data should be updated, prepared, retired, or consumed.

## What to Read Next

- [Core Mental Model](mental_model.md)
- [Frame Lifecycle](frame_lifecycle.md)
- [Element System](element_system.md)
- [API: UI Manager](../api/ui_manager.md)
- [API: Font Manager](../api/font_manager.md)
- [API: Image Manager](../api/image_manager.md)
- [API: Icon Manager](../api/icon_manager.md)
- [API: Input Field Manager](../api/input_field_manager.md)
- [API: Shortcut Manager](../api/shortcut_manager.md)
- [API: Viewport Manager](../api/viewport_manager.md)
