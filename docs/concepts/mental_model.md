# Core Mental Model

## Introduction

FlowUi is a C++23 desktop UI runtime that combines [Clay](https://github.com/nicbarker/clay), a Vulkan renderer, a typed reusable element system, and a set of app-oriented managers for fonts, images, icons, input fields, shortcuts, viewports, and developer tooling. Clay is the layout foundation: a performant, low-level, immediate-mode UI layout library. These docs will explain how FlowUi uses Clay, but they are not a full Clay tutorial; Clay's own documentation and videos from its creator are the right place to learn Clay itself. What matters most for FlowUi is that Clay makes layout explicit and frame-local: the UI is described again each frame, which is often easier to reason about than a long-lived retained widget tree, while still giving FlowUi a fast low-level layout engine to build on instead of inventing one.

## TL;DR

FlowUi code is shaped around one explicit frame loop: poll the window, build UI, finalize Clay output, then render through Vulkan. The app is rebuilt each frame, but stable ids, typed element state, shared resources, and subsystem managers preserve the data that should live longer than one frame.

Most of FlowUi exists to bridge the gap between Clay as a layout library and a usable desktop application runtime. Clay decides layout, FlowUi owns runtime systems around that layout, and the renderer turns the resulting commands into pixels.

## Core Lifecycle

A FlowUi application starts with one `FlowUi::App`. That object owns the native window backend, the Vulkan context, frame resources, the UI manager, asset managers, input systems, viewport rendering, and optional developer tooling. User code usually sees that ownership as manager accessors on `App`, but internally the important point is that the app coordinates resource lifetime and frame order.

The frame loop is the smallest useful unit of FlowUi work:

```cpp
FlowUi::AppConfig config{};
config.window.title = "Studio";
config.dev.enabled = true;

FlowUi::App app = FlowUi::makeApplication(config);

FlowUi::FontFamilyId bodyFamily = app.fonts().createFamily({
    .name = "Body",
    .faces = {{.path = "assets/fonts/Inter.arfont", .pixelSize = 18.0f}},
});

app.images().registerImage("avatar", "assets/avatar.png");
app.icons().registerFromFile("save", "assets/icons/save.svg");
app.viewPorts().create("preview", {.clearColor = {0.02f, 0.02f, 0.03f, 1.0f}});

if (FlowUi::ViewPort* preview = app.viewPorts().getViewPort("preview")) {
    preview->setRenderCallback([](const FlowUi::ViewPortRenderContext& context) {
        recordPreviewCommands(context);
    });
}

FlowUi::ShortcutId saveShortcut = app.ui().shortcuts().registerShortcut(
    {.key = GLFW_KEY_S, .ctrl = true},
    FlowUi::ShortcutScope::Global,
    100,
    [](FlowUi::ShortcutContext& context) {
        saveDocument();
        return true;
    });

while (!app.shouldClose()) {
    app.beginFrame();

    FlowUi::UiManager& ui = app.ui();
    const FlowUi::FontId bodyFont = ui.resolveFont(bodyFamily, 400, FlowUi::FontStyle::Normal);
    FlowUi::TextureRef avatar = app.images().getTexture("avatar");
    FlowUi::TextureRef saveIcon = app.icons().textureRef("save");
    FlowUi::TextureRef previewTexture = app.viewPorts().getTexture("preview");

    ui.createElement(kToolbarButton, "toolbar/save")
        .setParameters(ToolbarButtonParams{.icon = saveIcon, .label = "Save", .fontId = bodyFont})
        .draw();

    ui.createElement(kProfileCard, "sidebar/profile")
        .setParameters(ProfileCardParams{.avatar = avatar, .fontId = bodyFont})
        .draw();

    ui.createElement(kSearchField, "content/search")
        .setParameters(SearchFieldParams{.initialText = "Search", .fontId = bodyFont})
        .draw();

    ui.createElement(kPreviewPanel, "content/preview")
        .setParameters(PreviewPanelParams{.texture = previewTexture})
        .draw();

    app.endFrame();
    app.drawFrame();
}
```

The loop starts with `app.shouldClose()`. That call is backed by the window backend. In the default runtime, the native window system owns the close request, input event collection, framebuffer size, cursor behavior, clipboard integration, and platform-specific window handle. FlowUi keeps that backend behind `App` so application code can ask simple questions like "should the app close?" without directly owning the platform window.

`app.beginFrame()` moves the runtime from the previous presented frame into a new UI construction frame. Internally, this is where window input is polled and normalized into `FrameInput`, the active frame index is advanced, UI string arenas are reset, cursor requests are cleared, shortcut dispatch is prepared, input field editing state receives the current and previous input snapshots, image resources retire pending frame-safe deletions, and the Clay context is initialized for the current window size and UI scale. Conceptually, `beginFrame()` makes the app ready for user code to describe the current frame.

The code between `beginFrame()` and `endFrame()` is the UI construction phase. This is immediate-mode UI: the app describes the toolbar, profile card, search field, and preview panel every frame. The fact that those calls run every frame does not mean every piece of data is temporary. Text inside the search field is owned by `InputFieldManager`, font ids and atlas data are owned by `FlowUi::FontManager`, uploaded image textures are owned by `ImageManager`, parsed SVGs and icon atlas entries are owned by `IconManager`, the preview render target is owned by `ViewPortManager`, and custom element state is keyed by stable Flow ids.

During a `createElement(...).draw()` call, the element builder has a clear sequence. It maps the Flow element id string to a Clay root id, checks previous-frame interaction for hover, press, hold, and release callbacks, optionally runs logic, applies developer-mode parameter overrides when enabled, and then calls the element's build callback. That build callback emits Clay nodes, but the element system gives the emitted nodes a typed wrapper: params describe this frame's configuration, state remembers per-instance data, resources store shared per-definition data, and callbacks define behavior.

The managers are usually touched during this same construction phase. `FlowUi::FontManager` resolves a logical family and style into a concrete `FontId` for Clay text. `ImageManager`, `IconManager`, and `ViewPortManager` return `TextureRef` values that are stored through `UiManager::storeTexture()` before Clay image commands use them. `InputFieldManager` is requested by custom input elements so text, caret, focus, and selection can persist across frames. `ShortcutManager` dispatches registered chords from frame input and uses scopes to decide which callbacks should run.

`app.endFrame()` closes the UI construction phase. Internally, FlowUi finalizes Clay layout and produces Clay render commands. It also gives input fields a chance to emit caret and selection render overrides, lets icon and viewport systems inspect image commands, prepares icon raster variants, sizes viewport render targets from the UI rectangles that referenced them, and captures the interaction snapshot that will be used by the next frame's callbacks. After `endFrame()`, user code should treat UI construction for that frame as complete.

`app.drawFrame()` is the rendering phase. FlowUi takes the finalized command data and records Vulkan work for the current frame. Viewport callbacks record into managed offscreen targets, the UI renderer turns Clay commands into draw runs, texture references are resolved through the texture registry, and the swapchain image is presented through the window surface. This separation is intentional: `endFrame()` answers "what should be drawn?" while `drawFrame()` answers "submit it to the GPU and present it."

The important mental model is that the frame is rebuilt, but the runtime is not. The app loop gives FlowUi a predictable place to refresh transient frame data, while managers and stable ids hold the long-lived parts that make the app feel continuous.

## IDs and Reusable UI Blocks

FlowUi's element system exists to make reusable UI blocks feel explicit in C++. A Flow element is a typed wrapper around one or more Clay nodes. It is not a replacement for Clay; it is a way to package Clay layout, drawing, interaction behavior, persistent state, and shared resources behind one reusable definition.

The central type is:

```cpp
FlowUi::ElementDefinition<Params, State, Resources, FLOW_DEF_ID("definition_name")>
```

`Params` describe per-frame configuration for one element invocation. A button might receive a label, an icon, colors, sizing, and a font id. Params are copied or moved into the `ElementBuilder`, then passed through event, logic, construct, and build callbacks.

`State` describes per-instance data that should survive across frames. A disclosure panel might remember whether it is open. A slider might remember drag state. State is keyed by a `FlowElementId`, normally created from the element id string passed to `createElement()`.

`Resources` describe shared per-definition data. A graph element might cache common colors, icon handles, reusable buffers, or data derived from `App`. Resources are initialized once for that element definition specialization and reused by all instances.

FlowUi supports two build shapes. A `buildElement` callback emits the full Clay subtree for the element immediately. A `constructElement` callback returns only the root `Clay_ElementDeclaration`, then user code manually emits children until `UiManager::drawConstructed()` closes the root. The draw path is useful for self-contained widgets; the construct path is useful for open-ended containers.

Clay and FlowUi both care about ids. Clay ids identify layout elements for interaction and layout bookkeeping. FlowUi ids identify reusable element instances, state entries, input fields, dev capture records, and generated child ids. The intended pattern is that the string id passed to `createElement()` also represents the root Clay element id that the Flow element builds.

```cpp
ui.createElement(kProfileCard, "sidebar/profile").draw();
```

Inside that element, the root Clay id should come from the same Flow element id:

```cpp
CLAY(context.uiManager.toClayEID(context.elementID), rootDeclaration) {
    CLAY(context.uiManager.toClayEID(context.createChildElementId("avatar")), avatarDeclaration) {}
    CLAY_TEXT(context.uiManager.toClayString(context.params.name), CLAY_TEXT_CONFIG(textConfig));
}
```

That relationship is what keeps Flow elements grounded in Clay rather than floating above it as a separate UI system. The Flow element wraps a Clay root and optionally one or more Clay children. The Flow id string is the stable public name of that reusable block, and the root Clay id is the layout and interaction identity for the block in the current frame.

Reusable elements create a practical id problem. If an element is drawn in a list, a table, or a repeated section, each instance still needs stable state and stable child ids. FlowUi provides helpers like `createChildElementId()` and `createIndexedFlowId()` for that reason. These ids can be used for custom state lookup, child element naming, input fields, interaction checks, shortcuts scoped to a focused element, developer capture, and any place where a value needs to connect one frame to the next.

The result is a hybrid shape: layout stays immediate and local, but stateful behavior has explicit names. This is the core reason FlowUi code is structured around string ids, typed element definitions, and builder calls.

## Managers and Subsystems

Clay is a layout library. It does not try to be a complete desktop app runtime. A real desktop application still needs font loading and text rendering, image loading, SVG icons, editable text fields, keyboard shortcuts, offscreen render targets, frame-safe texture lifetime, cursor and clipboard access, and a renderer that can turn layout output into pixels.

FlowUi provides those pieces as app-owned managers so users can start writing app code instead of first building a text renderer or texture registry. These managers are effectively singleton subsystem objects within one `App`: not global singletons for the whole process, but one coordinated owner per application runtime. That shape solves lifetime and synchronization problems because each manager can share the same app-owned Vulkan context, frame index, texture registry, and configuration.

`UiManager` is the authoring surface for the current frame. It owns the Clay context, frame string arenas, texture reference storage, previous interaction snapshot, current and previous input snapshots, input field manager, shortcut manager, clipboard bridge, cursor requests, and font resolution bridge. It is where user code turns Flow ids and runtime data into Clay-safe frame data.

`FlowUi::FontManager` solves text resource ownership. It loads baked `.arfont` faces, optionally supports runtime font baking when compiled in, groups concrete faces into logical families, resolves family/weight/style requests into `FontId` values, and owns the Vulkan font atlas array used by the renderer. Without it, every app would need to solve font loading, atlas packing, and font fallback before drawing useful text.

`ImageManager` solves static image loading and texture registration. It loads image files, uploads them as Vulkan textures, registers them under app-defined keys, and returns `TextureRef` handles for Clay image commands. It also handles replacement, removal, and frame-safe retirement of GPU resources.

`IconManager` solves SVG icon usage. SVG icons are document-like assets, but the renderer needs raster textures at concrete sizes. The icon manager stores parsed SVGs by key, lazily rasterizes them at requested UI sizes, packs cached variants into atlas pages, and rewrites icon texture requests into concrete atlas-backed texture references during frame preparation.

`InputFieldManager` solves editable text state. Clay can lay out and render a text box, but text editing also needs focus, caret positions, selections, pointer hit testing, UTF-8 insertion, delete/backspace behavior, key repeat, max-length limits, and selection/caret rendering. FlowUi keeps that state in the manager so a custom input element can remain a reusable visual wrapper instead of becoming an entire text editor runtime.

`ShortcutManager` solves command dispatch from keyboard input. It registers key chords, evaluates press/release/down triggers from current and previous `FrameInput`, filters callbacks by scope, orders them by priority, and lets callbacks stop propagation by returning true. This gives applications a structured shortcut system instead of scattered per-element key checks.

`ViewPortManager` solves custom rendering inside UI. It owns named offscreen render targets, exposes them as `TextureRef` values, sizes them from the UI rectangles that display them, and invokes render callbacks with Vulkan context data and managed command buffers. This gives apps a path for 3D previews, graphs, editors, or custom Vulkan content while still composing the result through Clay layout.

The managers are separated because each subsystem has different lifetime and frame rules. Fonts are long-lived atlas data. Images are app assets. Icons are lazily rasterized variants. Input fields are persistent text state. Shortcuts are dispatch tables. Viewports are offscreen frame resources. Keeping those concerns separate makes the runtime easier to understand and gives advanced users clear places to integrate custom behavior.

## Developer Mode and Dev Tooling

Immediate-mode UI makes the control flow easier to reason about, but visual iteration can still be slow in C++. A common workflow is: change a color, spacing value, radius, font size, or layout parameter; recompile; run the app; inspect the result; then repeat. FlowUi's element system gives developer tooling a way to shorten that loop because visual values can be expressed as typed element parameters.

Developer mode is compiled in with `FLOW_UI_DEV_MODE` and enabled at runtime through `AppConfig::dev`. When enabled, FlowUi can capture element invocations during the frame, associate them with definition ids and instance ids, inspect registered parameter/state/resource types, and expose editable values in the developer UI. The goal is not to replace source code, but to make the running UI inspectable and tunable.

The key idea is that Flow elements produce structure the dev runtime can understand. A plain Clay block is just immediate layout code. A Flow element has a definition id, an instance id, typed params, optional state, optional resources, callback boundaries, and source location capture. That makes it possible to inspect both the definition shape and each instance drawn in the current UI.

Parameters are especially important for visual iteration. If a button's colors, spacing, corner radius, icon size, text style, and sizing are stored in its params struct, developer tooling can present those fields as editable data. A user can tune an instance while the app is running instead of recompiling for each small visual adjustment.

Developer mode also supports persistence through JSON export paths. Overrides can be written to a configured file such as `.flowui/overrides.v1.json`, then loaded or applied by the tooling path provided with the library. This gives visual iteration a path from temporary runtime edits to permanent data that can be reviewed, committed, or folded back into source defaults.

The same capture system also helps with hierarchy inspection. Because FlowUi knows which element definition produced which instance id, and because ids are stable, the developer UI can show the structure of the current frame in terms of reusable Flow elements rather than only raw render commands. Internal developer elements can be excluded from capture so app-authored UI remains the focus.

Developer mode is optional by design. Release builds can leave it out, and normal app code should not depend on it for core behavior. It is a layer on top of the same frame loop, ids, elements, params, and managers described above.

## What to Read Next

- [Frame Lifecycle](frame_lifecycle.md)
- [Element System](element_system.md)
- [Managers](managers.md)
- [Quick Start](../tutorials/quick_start.md)
