# Viewports and Vulkan Interop

This tutorial builds a custom FlowUi element that displays a Vulkan-rendered 3D scene inside the UI. The example target is a model preview panel: FlowUi owns the window, swapchain, viewport images, command buffers, and UI rendering, while app code creates scene resources and records draw commands for one offscreen viewport.

## Contents

- [Chapter 1: The Example App](#chapter-1-the-example-app)
  - [What We Are Building](#what-we-are-building)
  - [What FlowUi Owns](#what-flowui-owns)
  - [What App Code Owns](#what-app-code-owns)
- [Chapter 2: Create the Viewport](#chapter-2-create-the-viewport)
  - [Create Info](#create-info)
  - [Create and Look Up the Viewport](#create-and-look-up-the-viewport)
  - [Clear Behavior](#clear-behavior)
- [Chapter 3: Create Vulkan Scene Resources](#chapter-3-create-vulkan-scene-resources)
  - [Get Interop Handles](#get-interop-handles)
  - [Scene Renderer Shape](#scene-renderer-shape)
  - [Resource Lifetime](#resource-lifetime)
- [Chapter 4: Install Render Callbacks](#chapter-4-install-render-callbacks)
  - [Simple Callback](#simple-callback)
  - [Typed Shared Payload Callback](#typed-shared-payload-callback)
  - [Render Context Rules](#render-context-rules)
- [Chapter 5: Draw the Viewport in a Flow Element](#chapter-5-draw-the-viewport-in-a-flow-element)
  - [Preview Element Definition](#preview-element-definition)
  - [Using ViewPortManager::getTexture](#using-viewportmanagergettexture)
  - [Using ViewPort::textureRef](#using-viewporttextureref)
- [Chapter 6: Frame Lifecycle for Viewports](#chapter-6-frame-lifecycle-for-viewports)
- [Chapter 7: Removal and Fallbacks](#chapter-7-removal-and-fallbacks)
- [Final Shape](#final-shape)

## Chapter 1: The Example App

### What We Are Building

Assume the app is a small material editor. The left side has controls, and the right side has a live 3D preview of a model from a camera point of view.

```text
+----------------------+  +---------------------------+
| material parameters  |  |                           |
| roughness, color     |  |      rendered 3D object   |
| texture slots        |  |      inside FlowUi UI     |
+----------------------+  +---------------------------+
```

The preview is not drawn by Clay. Clay only draws an image node that samples a viewport texture. App Vulkan code records the 3D scene into that viewport texture before the main UI pass samples it.

### What FlowUi Owns

FlowUi manages the infrastructure:

- The Vulkan instance, physical device, logical device, allocator, graphics queue, and swapchain.
- Per-frame viewport render target images.
- Per-frame viewport command pools and secondary command buffers.
- Image layout transitions around the viewport pass.
- Texture slots used by UI image commands.
- The final UI pass that samples the viewport image.

App code does not destroy any FlowUi-owned handles.

### What App Code Owns

The app owns scene-specific Vulkan resources:

- Mesh buffers for the 3D object.
- Descriptor set layouts and descriptor sets.
- Uniform buffers for camera/model data.
- Pipeline layout and graphics pipeline compatible with the viewport color format.
- Any scene textures or samplers it creates.

FlowUi exposes enough Vulkan interop handles for the app to create those resources against the same device and allocator.

## Chapter 2: Create the Viewport

### Create Info

A viewport starts with `FlowUi::ViewPortCreateInfo`.

```cpp
FlowUi::ViewPortCreateInfo previewInfo{
    .colorFormat = VK_FORMAT_R8G8B8A8_UNORM,
    .clearColor = {0.02f, 0.02f, 0.03f, 1.0f},
    .clearEveryFrame = true,
};
```

The color format is the offscreen render target format. `clearEveryFrame` controls whether FlowUi clears before invoking your callback or preserves previous contents where possible.

### Create and Look Up the Viewport

Create the named viewport after `makeApplication()`.

```cpp
FlowUi::App app = FlowUi::makeApplication(config);

const bool created = app.viewPorts().create("scene/preview", previewInfo);
(void)created;
```

Use `contains()` for lightweight checks.

```cpp
if (!app.viewPorts().contains("scene/preview")) {
    (void)app.viewPorts().create("scene/preview", previewInfo);
}
```

Use `getViewPort()` when you need to configure the viewport object.

```cpp
FlowUi::ViewPort* preview = app.viewPorts().getViewPort("scene/preview");
if (!preview) {
    throw std::runtime_error("Preview viewport was not created.");
}
```

The const overload is useful for read-only inspection:

```cpp
const FlowUi::ViewPort* readOnlyPreview = app.viewPorts().getViewPort("scene/preview");
```

The viewport also stores its own key. That is useful when helper code receives a viewport pointer and still needs the stable name for logging, lookup tables, or element parameters.

```cpp
std::string_view previewKey = preview->getKey();
```

### Clear Behavior

Clear color can be changed later.

```cpp
preview->setClearColor(0.01f, 0.015f, 0.025f, 1.0f);
std::array<float, 4> clear = preview->clearColor();
```

Persistent viewports can disable per-frame clear.

```cpp
preview->setClearEveryFrame(false);
const bool clears = preview->clearEveryFrame();
```

Most scene previews should keep clearing enabled. Disabling clear is more useful for accumulation or feedback-style render targets.

## Chapter 3: Create Vulkan Scene Resources

### Get Interop Handles

Ask the viewport manager for shared Vulkan handles.

```cpp
const FlowUi::ViewPortVulkanInterop& vk = app.viewPorts().getVulkanInterop();
```

The returned struct includes:

- `instance`
- `physicalDevice`
- `device`
- `allocator`
- `graphicsQueue`
- `graphicsQueueFamily`
- `framesInFlight`

These handles are owned by FlowUi. Use them to create compatible resources, but do not destroy the handles themselves.

### Scene Renderer Shape

For a real app, wrap scene resources in a renderer object.

```cpp
struct PreviewSceneRenderer {
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator_T* allocator = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation_T* vertexAllocation = nullptr;

    explicit PreviewSceneRenderer(const FlowUi::ViewPortVulkanInterop& vk)
        : device(vk.device), allocator(vk.allocator) {
        createMeshResources(vk);
        createPipelineResources(vk);
    }

    ~PreviewSceneRenderer() {
        destroyResources();
    }

    void render(const FlowUi::ViewPortRenderContext& context);
};
```

The constructor uses FlowUi's device and allocator to create app-owned resources. The destructor destroys only the resources created by this renderer, never the handles borrowed from FlowUi.

### Resource Lifetime

Keep the renderer alive as long as the viewport callback may use it.

```cpp
auto previewRenderer =
    std::make_shared<PreviewSceneRenderer>(app.viewPorts().getVulkanInterop());
```

The `shared_ptr` callback overload shown in the next chapter is useful because the viewport retains the renderer for you.

## Chapter 4: Install Render Callbacks

### Simple Callback

For a stateless example, install a plain callback.

```cpp
preview->setRenderCallback([](const FlowUi::ViewPortRenderContext& context) {
    VkViewport viewport{};
    viewport.width = static_cast<float>(context.extent.width);
    viewport.height = static_cast<float>(context.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = context.extent;

    vkCmdSetViewport(context.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(context.commandBuffer, 0, 1, &scissor);
});
```

This records into the secondary command buffer FlowUi provides.

### Typed Shared Payload Callback

For an actual scene renderer, use the typed shared payload overload.

```cpp
preview->setRenderCallback(
    previewRenderer,
    [](const FlowUi::ViewPortRenderContext& context, PreviewSceneRenderer& renderer) {
        renderer.render(context);
    });
```

FlowUi stores the `shared_ptr`, so `PreviewSceneRenderer` stays alive until the callback is replaced, cleared, or the viewport is removed.

Inside `render()`, record scene commands. The command buffer is already inside a dynamic rendering pass for the viewport target.

```cpp
void PreviewSceneRenderer::render(const FlowUi::ViewPortRenderContext& context) {
    if (context.extent.width == 0 || context.extent.height == 0) {
        return;
    }

    VkViewport viewport{};
    viewport.width = static_cast<float>(context.extent.width);
    viewport.height = static_cast<float>(context.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = context.extent;

    vkCmdSetViewport(context.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(context.commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(context.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindVertexBuffers(context.commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdDraw(context.commandBuffer, 36, 1, 0, 0);
}
```

### Render Context Rules

`FlowUi::ViewPortRenderContext` gives callback code:

- `commandBuffer`: secondary command buffer already begun by FlowUi.
- `extent`: current render target size in pixels.
- `colorFormat`: viewport target format.
- `frameIndex`: frame-resource index.
- `key`: viewport key.
- `vulkan`: borrowed interop handles.

Do not call `vkBeginCommandBuffer()` or `vkEndCommandBuffer()` on `context.commandBuffer`. FlowUi handles that. Also do not transition the viewport image layout; FlowUi transitions it into a renderable layout before your callback and into a sampled layout afterward.

Use `hasRenderCallback()` when setup code may run more than once.

```cpp
if (!preview->hasRenderCallback()) {
    preview->setRenderCallback(previewRenderer, renderPreviewScene);
}
```

Use `clearRenderCallback()` when the preview panel is disabled but the viewport texture can remain.

```cpp
preview->clearRenderCallback();
```

## Chapter 5: Draw the Viewport in a Flow Element

### Preview Element Definition

The Flow element only displays a texture reference. It does not record Vulkan commands.

```cpp
struct ScenePreviewParams {
    std::string viewportKey = "scene/preview";
    FlowUi::ViewPortManager* viewPorts = nullptr;
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIXED(480.0f),
        .height = CLAY_SIZING_FIXED(320.0f),
    };
};

using ScenePreviewDefinition = FlowUi::ElementDefinition<
    ScenePreviewParams,
    void,
    void,
    FLOW_DEF_ID("tutorial_scene_preview")>;
```

The build callback gets the viewport texture and stores it for Clay.

```cpp
inline const ScenePreviewDefinition kScenePreview = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    +[](ScenePreviewDefinition::BuildContext& context) {
        if (!context.params.viewPorts) {
            return;
        }

        FlowUi::TextureRef sceneTexture =
            context.params.viewPorts->getTexture(context.params.viewportKey);
        sceneTexture.fitMode = FlowUi::TextureFitMode::Contain;

        Clay_ImageElementConfig image{};
        image.imageData = context.uiManager.storeTexture(sceneTexture);

        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.backgroundColor = FlowUi::Flow_Color("#05070cff");
        root.cornerRadius = CLAY_CORNER_RADIUS(8);

        CLAY(context.uiManager.toClayEID(context.elementID), root) {
            CLAY_IMAGE(image);
        }
    },
};
```

### Using ViewPortManager::getTexture

`ViewPortManager::getTexture(key)` is the most convenient way to draw by key.

```cpp
FlowUi::TextureRef sceneTexture = app.viewPorts().getTexture("scene/preview");
```

If the key is missing, it returns fallback texture id `0` and logs a warning once.

### Using ViewPort::textureRef

If you already have a viewport pointer, you can ask it for the current frame texture directly.

```cpp
if (FlowUi::ViewPort* viewport = app.viewPorts().getViewPort("scene/preview")) {
    FlowUi::TextureRef sceneTexture = viewport->textureRef();
}
```

The manager form is usually simpler in UI code; the object form is useful when setup code already looked up the viewport.

## Chapter 6: Frame Lifecycle for Viewports

Viewports are sized from the UI.

During UI construction, the scene preview element emits a Clay image command that references `"scene/preview"`. At `endFrame()`, Clay has final image bounds. FlowUi then sees that the viewport texture was used and chooses a render target size from the largest UI image area that referenced it, scaled into framebuffer pixels.

During `drawFrame()`, FlowUi:

1. Resizes viewport images if the desired size changed.
2. Remaps viewport texture refs to the current frame's viewport image.
3. Begins the viewport secondary command buffer.
4. Sets up dynamic rendering for the viewport target.
5. Calls the viewport render callback.
6. Ends the secondary command buffer.
7. Executes viewport work before the main UI pass.
8. Transitions the viewport image for sampling by the UI renderer.

This means a render callback usually runs only when the viewport texture was referenced by UI for that frame. If the preview panel is closed and no image command uses the texture, FlowUi does not need to render that viewport.

You can inspect current size from the viewport object.

```cpp
if (const FlowUi::ViewPort* viewport = app.viewPorts().getViewPort("scene/preview")) {
    if (viewport->hasValidSize()) {
        VkExtent2D size = viewport->getSize();
        (void)size;
    }
}
```

Before the viewport has been referenced by UI, its size may be invalid or still at the initial placeholder size.

## Chapter 7: Removal and Fallbacks

Remove a viewport when the scene preview is permanently closed.

```cpp
const bool removed = app.viewPorts().remove("scene/preview");
```

Removal destroys per-frame viewport resources and can block because the implementation waits for the Vulkan device to become idle before releasing resources.

If the app only wants to stop drawing custom scene commands, keep the viewport and clear the callback instead.

```cpp
if (FlowUi::ViewPort* viewport = app.viewPorts().getViewPort("scene/preview")) {
    viewport->clearRenderCallback();
}
```

The viewport texture can still be drawn, but no app scene commands are recorded.

## Final Shape

Setup creates the viewport, creates renderer resources through Vulkan interop, and installs the typed render callback.

```cpp
FlowUi::App app = FlowUi::makeApplication(config);

(void)app.viewPorts().create("scene/preview", FlowUi::ViewPortCreateInfo{
    .colorFormat = VK_FORMAT_R8G8B8A8_UNORM,
    .clearColor = {0.02f, 0.02f, 0.03f, 1.0f},
    .clearEveryFrame = true,
});

FlowUi::ViewPort* preview = app.viewPorts().getViewPort("scene/preview");
auto renderer = std::make_shared<PreviewSceneRenderer>(app.viewPorts().getVulkanInterop());

preview->setRenderCallback(
    renderer,
    [](const FlowUi::ViewPortRenderContext& context, PreviewSceneRenderer& renderer) {
        renderer.render(context);
    });
```

The frame loop draws the preview element like ordinary UI.

```cpp
while (!app.shouldClose()) {
    app.beginFrame();

    app.ui()
        .createElement(kScenePreview, "material/preview")
        .setParameters(ScenePreviewParams{
            .viewportKey = "scene/preview",
            .viewPorts = &app.viewPorts(),
        })
        .draw();

    app.endFrame();
    app.drawFrame();
}
```

The key model is that app code records Vulkan scene commands only inside the viewport callback. FlowUi owns the viewport image lifecycle and exposes that image as a `TextureRef`, so the UI can display a custom-rendered scene exactly like any other image.
