# Frame Lifecycle

## Introduction

FlowUi's frame lifecycle is the internal path that turns one loop iteration of user code into presented pixels. At the public API level this looks small: `shouldClose()`, `beginFrame()`, UI construction, `endFrame()`, and `drawFrame()`. Internally, each step coordinates the window backend, frame input, Clay layout, frame-local arenas, input fields, shortcuts, icons, viewports, texture registry state, Vulkan frame resources, swapchain images, and the UI renderer. The public configuration structs affect this path before the loop even starts: most importantly, `AppConfig::vk.framesInFlight` chooses how many frame slots FlowUi creates and then manages internally for command buffers, fences, semaphores, UI arenas, texture retirement buckets, viewport resources, renderer instance buffers, and descriptor sets.

The frame can be pictured as a pipeline:

```text
App::shouldClose()
    asks the window backend if the app should stop

App::beginFrame()
    selects the active frame slot
    reclaims frame-safe resources
    polls native window/input events
    drains input into FrameInput
    scales input into layout space
    starts UiManager and Clay layout

User UI code
    creates Flow elements
    emits Clay nodes
    requests fonts, images, icons, input fields, shortcuts, and viewports

App::endFrame()
    closes layout
    captures interaction state
    produces Clay render commands
    lets input fields, icons, and viewports prepare frame data

App::drawFrame()
    handles swapchain resize if needed
    acquires a swapchain image
    records viewport passes
    builds renderer instances/runs from Clay commands
    records and submits Vulkan commands
    presents the image
    advances to the next frame resource
```

That separation affects how user code should be written. `beginFrame()` is the point where input and transient frame memory become valid. UI should be built only between `beginFrame()` and `endFrame()`. `endFrame()` is the point where layout is finalized and render commands become available to the runtime. `drawFrame()` is the point where FlowUi consumes those commands and submits GPU work.

## TL;DR

FlowUi rebuilds UI every frame, but it does not rebuild the whole runtime every frame. The frame loop resets temporary frame data, collects input, lets user code describe UI, finalizes Clay output, prepares resource-dependent systems, records Vulkan work, presents, and then advances the active frame slot.

`framesInFlight` is the main configuration value that changes the shape of this process. A higher value gives FlowUi more independent frame resources to rotate through, while the public loop remains the same.

Interaction is intentionally snapshot-based. Clay only knows final element bounds after layout finishes, so FlowUi captures current interaction at `endFrame()` and uses the previous completed snapshot while building the next frame. That means root element callbacks such as hover and press are delayed by one frame; in practice this is small, and with `VulkanConfig::presentMode = PresentMode::Immediate` it is usually even less noticeable.

## Frame Configuration and Frame Slots

Before the first frame, `makeApplication()` creates `App::Impl`, initializes the window backend, creates Vulkan instance/surface/device state, creates the swapchain, creates `FrameVk`, initializes the UI renderer, and connects managers to shared runtime services such as the texture registry and font manager.

FlowUi uses PIMPL for `App`: public `FlowUi::App` methods are thin handles that forward into `App::Impl`. That keeps public headers small and keeps window/Vulkan/backend details out of the public `App` class. For frame lifecycle docs, the important detail is that `App::beginFrame()`, `App::endFrame()`, and `App::drawFrame()` are public entry points, but almost all real work happens inside the internal implementation.

`AppConfig::vk.framesInFlight` is clamped to at least one frame and then used by several systems:

- `FrameVk::frames` stores one command pool, command buffer, image-available semaphore, and in-flight fence per frame slot.
- `UiManager` creates one transient string/texture arena per frame slot.
- `UiTextureRegistry` creates frame-indexed retirement buckets for texture descriptor slots.
- `ImageManager` and `ViewPortManager` use frame-indexed cleanup or resource tracking.
- `VulkanUiRenderer` creates per-frame instance buffers and descriptor sets.

The user does not manually rotate these slots. The runtime uses `frames.currentFrame`, passes that slot into the managers that need it, and calls `frames.advance()` after presentation.

## Close Check and Window Backend

The frame loop usually starts with:

```cpp
while (!app.shouldClose()) {
    app.beginFrame();
    buildUi(app);
    app.endFrame();
    app.drawFrame();
}
```

`App::shouldClose()` forwards into the internal window backend. If the app is not initialized, or the backend is gone, it returns `true`. In a normal running app, the backend answers from native window state: close button, OS window events, or whatever the backend considers a close request.

The window backend also participates later in the frame. During `beginFrame()` it is polled for events. Its input queue receives mouse, keyboard, scroll, text input, window size, framebuffer size, cursor, and clipboard-related events. The close check is therefore the simplest public view of a larger responsibility: FlowUi keeps platform-specific window behavior behind `App`, but uses that backend every frame to feed input and presentation state into the rest of the runtime.

## Begin Frame

`beginFrame()` starts by measuring delta time from the previous `beginFrame()` call. This value becomes `FrameInput::dt`, which is used by systems such as input field caret blinking, key repeat, scroll updates, and any custom element code that reads current frame input.

Next, the runtime resolves the current frame slot:

```text
frameSlot = frames.currentFrame % frames.frames.size()
```

That slot is passed to frame-aware managers. The texture registry reclaims descriptor slots that were retired in the same bucket during an earlier frame. The image manager starts its frame and can retire image resources safely. The viewport manager starts its frame and resets or advances per-frame viewport tracking.

After frame-slot maintenance, FlowUi polls native events through the window backend and drains the input queue into `frameInputForCurrentFrame`. This is still window/input-space data. FlowUi then computes layout-space values using `AppConfig::ui.uiScale`, the current logical window extent, and the current framebuffer extent.

This produces two important scale relationships:

- Input is divided by `uiScale` before Clay sees it.
- UI-to-framebuffer scale is computed so render commands can later be converted into framebuffer pixels.

`UiManager::beginFrame()` then starts the UI layer for this frame. Internally it selects the active string arena, resets that arena's offset, advances previous/current interaction snapshots, stores current and previous layout input, begins input-field and shortcut processing, resets cursor request priority, sets the Clay context and dimensions, updates Clay pointer and scroll state, clears constructed-element bookkeeping, begins developer runtime capture when enabled, and calls `Clay_BeginLayout()`.

The interaction snapshot advance is important. `UiManager` keeps both a previous interaction snapshot and a current interaction snapshot. At the start of a new frame, the current snapshot from the last completed layout becomes the previous snapshot that element callbacks can read. A fresh empty current snapshot is then prepared for the layout that is about to be built.

From user code's point of view, after `beginFrame()` returns:

- `app.ui()` is ready for frame-local UI construction.
- `UiManager::toClayString()` and `storeTexture()` have fresh arena space.
- `UiManager::getCurrentFrameInput()` returns this frame's layout-space input.
- Shortcuts have already had a chance to dispatch from the current input transition.
- Input fields have already applied keyboard edits for the active field.
- Clay is inside an active layout.

## UI Construction

The code between `beginFrame()` and `endFrame()` is where user code describes the frame. This is immediate-mode UI: the tree is emitted again every frame, but long-lived data is stored elsewhere.

Typical user code touches several layers at once:

```cpp
FlowUi::UiManager& ui = app.ui();
FlowUi::TextureRef icon = app.icons().textureRef("save");
FlowUi::FontId labelFont = ui.resolveFont("Body", 600, FlowUi::FontStyle::Normal);

ui.createElement(kToolbarButton, "toolbar/save")
    .setParameters(ToolbarButtonParams{.icon = icon, .label = "Save", .fontId = labelFont})
    .draw();
```

The element builder turns this into a structured callback sequence. It converts the element id string into a Flow id and Clay root id, checks previous-frame interaction for event callbacks, optionally runs logic, applies dev-mode parameter overrides when enabled, and then calls either `buildElement` or `constructElement`.

Inside the element callback, user-authored code emits Clay nodes. The callback may also ask managers for subsystem state:

- Fonts resolve through `FlowUi::FontManager`, usually from `UiManager::resolveFont()`.
- Images and icons provide `TextureRef` values for Clay image commands.
- Input fields request persistent editable text state.
- Shortcuts may use focused element ids or input-field focus.
- Viewports provide texture refs that will later be detected and sized.
- Developer capture records element definition and instance metadata.

This phase does not present anything. It builds a layout description and registers enough frame-local metadata for later systems to interpret the Clay output correctly.

## End Frame

`endFrame()` closes UI construction and turns the frame's immediate-mode description into renderable command data.

The public `App::endFrame()` forwards into `App::Impl::endFrame()`, which first calls `UiManager::endFrame()`. The UI manager ensures constructed elements are closed. If user code called `construct()` without a matching `drawConstructed()`, FlowUi auto-closes those roots and warns. In developer mode, the dev panel can be drawn before the root is closed.

Then `Clay_EndLayout()` finalizes layout and produces a `Clay_RenderCommandArray`. At this point Clay has measured text, resolved layout sizes, generated bounding boxes, and produced commands such as rectangles, borders, text, images, and custom commands.

After layout, `UiManager` captures interaction state from Clay. It reads pointer-over ids, classifies hovered, pressed, held, and released elements from current and previous primary pointer state, then stores that snapshot for the next frame. This is why element event callbacks use the previous completed frame's interaction data: callbacks run while building the next frame, but interaction was only known after the last layout finished.

This is a deliberate tradeoff in FlowUi's immediate-mode model. During UI construction, Clay has not yet produced final bounds for the nodes currently being emitted, so FlowUi cannot know which of those new nodes are hovered or pressed until `Clay_EndLayout()` runs. The current snapshot captured at `endFrame()` therefore becomes useful on the following frame. Public element callbacks use that previous snapshot internally, which gives interaction-driven visuals and state changes a one-frame lag.

For most UI, that lag is minor because the next frame is normally only one refresh interval away. If the app is configured with `VulkanConfig::presentMode = PresentMode::Immediate`, presentation does not wait for vertical sync in the same way as FIFO-style presentation, so the perceived delay can be even less noticeable. The cost of the tradeoff is one-frame delayed callbacks; the benefit is that interaction tests are based on Clay's final, correct element bounds instead of guessed bounds from partially built UI.

Input fields also finish their frame during `endFrame()`. `InputFieldManager::endFrame()` receives the Clay render commands, checks which fields were requested this frame, clears focus for fields that disappeared, reads Clay element bounds for text/content nodes, computes caret and selection geometry, and produces render overrides for selection rectangles, carets, and selected text color. Those overrides are later passed to the renderer alongside the Clay commands.

After `UiManager::endFrame()` returns, `App::Impl::endFrame()` lets resource-dependent systems inspect or prepare the command array:

- `ViewPortManager::prepareFrameTargets()` looks at rendered image areas that reference viewport textures, calculates desired framebuffer-sized targets, and prepares target metadata for the upcoming draw.
- `IconManager::prepareFrameTextures()` looks at icon texture requests, rasterizes SVG variants at the requested draw size when needed, stores them in atlas pages, and rewrites the frame's texture references to concrete atlas-backed entries.

After `endFrame()`, the frame has a stable command array and any per-command texture or viewport remapping needed for rendering has been prepared.

## Draw Frame

`drawFrame()` consumes the prepared frame and submits GPU work. Like the other public lifecycle methods, `App::drawFrame()` is a small PIMPL forwarder; the internal `App::Impl::drawFrame()` owns the actual Vulkan flow.

The function first checks whether frame resources and swapchain views exist. If the framebuffer size changed, or if a previous acquire/present reported the swapchain as out of date, FlowUi attempts to recreate the swapchain before drawing. If the framebuffer has zero width or height, such as while minimized, swapchain recreation is deferred and drawing returns early.

After resize handling, FlowUi grabs the current `FrameVk::Frame`. That frame contains the command pool, primary command buffer, image-available semaphore, and in-flight fence for the current frame slot. The runtime waits on the frame's fence so the CPU does not overwrite resources still in use by the GPU.

The next step is swapchain image acquisition. `vkAcquireNextImageKHR()` chooses the swapchain image that will receive this frame. FlowUi tracks fences per swapchain image as well as per frame slot. If the acquired swapchain image is still associated with an older in-flight submission, FlowUi waits for that image fence before reusing it.

Once the image is safe, FlowUi resets the frame fence, resets the frame command pool, begins the primary command buffer, and transitions the acquired swapchain image into attachment layout. From here, all recorded work targets the current frame slot and current swapchain image.

Viewport rendering happens before the main UI renderer. `ViewPortManager::remapRenderCommandsForFrame()` changes viewport texture references to the correct per-frame viewport image. `ViewPortManager::recordFramePasses()` then records managed offscreen viewport passes into command buffers, invoking user render callbacks with `ViewPortRenderContext`. This is where custom rendering such as scene previews, graphs, or editors gets rendered before the UI samples those images.

Then `VulkanUiRenderer::render()` handles the main UI pass. Conceptually it does three jobs:

1. It checks and updates per-frame descriptors when texture bindings or font atlas revisions changed.
2. It builds a contiguous vector of UI instances and render runs from the Clay render command array.
3. It records Vulkan dynamic-rendering commands that flush those runs into the swapchain image.

The instance building step is where Clay command data becomes renderer data. Rectangle and border commands become solid instances. Text commands are laid out against the current font face and become MSDF glyph instances. Image and custom commands become textured instances using `TextureRef` values. Input-field selection and caret overrides are interleaved into the command stream. Runs group contiguous instances by pipeline type and scissor so the renderer can draw many instances with fewer state changes.

If the per-frame instance buffer is too small, the renderer grows it and updates the descriptor for that frame slot. The renderer then uploads the instance vector, begins dynamic rendering against the swapchain image view, binds viewport/scissor, the quad vertex buffer, descriptor sets, and the appropriate pipeline per run. Each run is flushed with one instanced draw over a quad vertex buffer.

After UI rendering, FlowUi transitions the swapchain image to present layout, ends the command buffer, submits it to the graphics queue, and signals the semaphore that presentation will wait on. Finally, it presents through the present queue. If presentation reports the swapchain as out of date or suboptimal, FlowUi marks the framebuffer as resized and attempts recreation.

At the very end, `frames.advance()` moves `currentFrame` to the next frame slot. The next loop iteration will use a different command pool, command buffer, fence, descriptors, arenas, and per-frame resource bucket according to the configured frames in flight.

## Swapchain Recreation and Resize

Window resizing is detected in two places. `beginFrame()` compares the current framebuffer extent with the previously observed extent and marks `framebufferResized` when it changes. `drawFrame()` repeats that check before acquiring the next swapchain image. Vulkan can also report resize-related state through `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR` during acquire or present.

When recreation is needed, FlowUi waits for the device to become idle, recreates the swapchain with the new framebuffer extent, updates frame data that depends on the number of swapchain images, notifies the renderer about the swapchain format, resets tracked image layouts to undefined, and clears the resize flag.

If the framebuffer extent is zero, FlowUi does not recreate immediately. This commonly happens when a window is minimized. In that case the resize flag stays set and drawing returns early until a non-zero framebuffer size is available.

From user code's perspective, this means normal resize handling is internal. Applications generally keep writing the same frame loop. Layout dimensions, UI-to-framebuffer scale, swapchain images, and renderer targets are refreshed by FlowUi as the window changes.

## User Code Rules of Thumb

Call the lifecycle methods in order:

```cpp
app.beginFrame();
buildUi(app);
app.endFrame();
app.drawFrame();
```

Build UI only between `beginFrame()` and `endFrame()`. That is when Clay layout is open and frame-local string/texture arena storage is valid.

Treat `endFrame()` as the point where UI construction is done. After it returns, FlowUi has a render command array and managers have prepared frame-dependent command data.

Treat `drawFrame()` as a consuming operation. It uses the render commands prepared by `endFrame()`, records Vulkan work, submits, presents, and advances the frame slot.

Do not store pointers returned from `UiManager::toClayString()` or `UiManager::storeTexture()` beyond the frame. They are backed by frame-local arena storage.

Use stable ids for anything that must survive across frames. The UI tree is rebuilt every frame, but state, resources, input fields, shortcut focus, viewport references, and developer capture need stable names.

Use managers for long-lived subsystem state. Font faces, image textures, icon documents, input text, shortcuts, and viewports should live in their managers rather than in temporary frame-local code.

Remember that Flow element interaction callbacks read the previous completed interaction snapshot. If an interaction must react to raw input in the same construction frame, read `UiManager::getCurrentFrameInput()` directly and do your own hit or state logic. For normal root hover, press, hold, and release behavior, prefer the built-in callbacks and accept the intentional one-frame delay.

## What to Read Next

- [Core Mental Model](mental_model.md)
- [Element System](element_system.md)
- [Managers](managers.md)
- [Viewports and Vulkan Interop](../tutorials/viewports_vulkan_interop.md)
