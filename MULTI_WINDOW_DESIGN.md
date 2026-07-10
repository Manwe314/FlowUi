# FlowUi Multi-Window Architecture Design

This document describes the target architecture for adding multi-window support to FlowUi while preserving the current simple single-window API. The core direction is:

> Duplicate cheap execution state. Share immutable heavy resources. Bind shared resources per window.

This model is intended to support ordinary single-window apps, explicit secondary windows, callback-driven windows, detachable Flow elements, and a future multi-threaded backend without forcing a later rewrite.

## Goals

- Preserve the existing single-window usage:

```cpp
while (!app.shouldClose()) {
	app.beginFrame();
	// draw main window UI
	app.endFrame();
	app.drawFrame();
}
```

- Add explicit multi-window usage by passing a `WindowId`:

```cpp
FlowUi::WindowId inspector = app.createWindow(inspectorConfig);

while (!app.shouldClose()) {
	app.beginFrame();
	drawMain(app.ui());
	app.endFrame();
	app.drawFrame();

	if (!app.shouldClose(inspector)) {
		app.beginFrame(inspector);
		drawInspector(app.ui(inspector));
		app.endFrame(inspector);
		app.drawFrame(inspector);
	}
}
```

- Allow convenience APIs later, such as:

```cpp
app.createWindow(config, [](FlowUi::UiManager& ui) {
	drawInspector(ui);
});
```

and:

```cpp
app.createElementWindow(config, kInspectorPanel, "inspector", InspectorParams{});
```

- Treat the first window as the "main window" semantically, but not architecturally. It is just the first entry in the window registry, created from `AppConfig::window`.
- Closing the main window should request app shutdown. Closing secondary windows should close only those windows unless configured otherwise.
- Keep the architecture compatible with a future multi-threaded backend.

## Current Architecture

The current `App::Impl` in [src/FlowUi.cpp](src/FlowUi.cpp) is single-window oriented. It owns one instance of the major frame resources:

- `std::unique_ptr<detail::IWindowBackend> window`
- `detail::InputQueue inputQueue`
- `VulkanContext vk`
- `Swapchain swap`
- `FrameVk frames`
- `UiManager ui`
- `VulkanUiRenderer renderer`
- `UiTextureRegistry textureRegistry`
- `FontManager fonts`
- `ImageManager imageManager`
- `IconManager icons`
- `ViewPortManager viewPortManager`
- one `Clay_RenderCommandArray`
- one swapchain image layout list
- one timing/input/layout scale state

This is correct for one native window. For multiple windows, it mixes:

- app-global Vulkan/device state,
- per-window surface/swapchain/frame state,
- per-window UI/input state,
- heavy resources that should be shared,
- light binding/cache state that should be per-window.

The multi-window refactor should separate these concerns instead of duplicating the entire `App::Impl` or sharing everything globally.

## Target Ownership Model

### App-Shared State

These resources should live once per `App`:

- Vulkan instance
- physical device
- logical device
- graphics/present queues, if compatible across all created surfaces
- VMA allocator
- shared renderer immutable objects:
  - shader modules or shader bytecode
  - pipeline layouts
  - pipelines
  - descriptor set layouts
  - immutable/default samplers
- shared GPU resource stores:
  - images
  - font atlases
  - icon rasters/textures
  - placeholder textures
- GPU upload queue
- GPU resource retirement queue
- global resource handle tables
- main window id / window registry

These are expensive, immutable after creation, or naturally app-wide.

### Per-Window State

These resources should be owned per window:

- native window backend
- input queue
- Vulkan surface
- swapchain
- swapchain image views and image layout tracking
- frame fences, semaphores, command pools, and command buffers
- `UiManager` and Clay context
- render command array
- window-local viewport manager by default
- window-local texture binding registry
- window renderer frame resources:
  - descriptor sets
  - instance buffers
  - per-frame descriptor dirty flags
  - scratch vectors for instances/runs
- UI-to-framebuffer scale
- framebuffer resize state
- per-window diagnostics
- per-window frame timing

These are cheap compared to large GPU images, or they are inherently tied to one surface/window/frame lifecycle.

## Managers vs Resources

The critical distinction is that managers and resources are not the same thing.

Managers are execution/control state. They own maps, queues, frame tracking, descriptor bindings, and API logic. Managers are usually cheap enough to duplicate per window, and duplicating them gives cleaner parallelism.

Resources are heavy GPU objects or large CPU-side backing data. These should generally be shared, especially when immutable.

The target model is:

- `ImageManager`-like API may be window-facing, but heavy image storage should be shared.
- Each window owns a lightweight registry that maps shared image handles to window-local descriptor slots.
- The same shared image can be used by multiple windows without duplicating GPU memory.
- Each window independently binds that shared image into its own descriptor set.

## Shared Immutable Resource Store

Heavy resources should be represented by opaque generation handles.

Example shape:

```cpp
struct TextureHandle {
	uint32_t index = 0;
	uint32_t generation = 0;
};

struct SharedGpuImage {
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;

	uint32_t width = 0;
	uint32_t height = 0;
	VkFormat format = VK_FORMAT_UNDEFINED;
	uint64_t byteSize = 0;

	uint32_t generation = 0;
	bool immutable = true;
	bool pinned = false;
};
```

Most resources should be immutable after upload. If an image changes, FlowUi should create a new generation instead of mutating the existing GPU image in place.

Benefits:

- Windows can safely read the same image handles concurrently.
- Descriptor caches can key by `(index, generation)`.
- Resource replacement does not require walking every window and forcibly invalidating old slots.
- Old generations can be retired safely after GPU use completes.

## Per-Window Texture Binding

Each window should have its own texture binding registry. It resolves shared resource handles into descriptor slots local to that window.

Example shape:

```cpp
struct WindowTextureBindingKey {
	uint32_t resourceIndex = 0;
	uint32_t generation = 0;
};

struct WindowTextureRegistry {
	std::unordered_map<WindowTextureBindingKey, uint32_t> handleToSlot;
	std::vector<VkDescriptorImageInfo> slotInfos;
	std::vector<bool> descriptorDirtyByFrame;
};
```

Resolution should be lazy:

```cpp
uint32_t WindowTextureRegistry::resolve(TextureHandle handle) {
	if (uint32_t* existing = findSlot(handle)) {
		return *existing;
	}

	const SharedGpuImage& image = sharedTextureStore.get(handle);
	const uint32_t slot = allocateSlot();
	slotInfos[slot] = VkDescriptorImageInfo{
		.sampler = image.sampler,
		.imageView = image.view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	handleToSlot.emplace(bindingKey(handle), slot);
	markDescriptorsDirty();
	return slot;
}
```

This keeps descriptor mutation per-window, which is important for future multi-threaded command recording.

## Renderer Split

The current [VulkanUiRenderer](include/Ui/Vk_UiRenderer.hpp) mixes shared renderer objects and per-frame mutable render state.

Target split:

```cpp
struct SharedUiRendererResources {
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline solidPipeline = VK_NULL_HANDLE;
	VkPipeline msdfPipeline = VK_NULL_HANDLE;
	VkPipeline texturedPipeline = VK_NULL_HANDLE;

	VkDescriptorSetLayout globalsLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout texturesLayout = VK_NULL_HANDLE;

	VkSampler defaultSampler = VK_NULL_HANDLE;
};

struct WindowUiRendererResources {
	std::vector<AllocatedBuffer> instanceBuffersByFrame;
	std::vector<VkDescriptorSet> globalsSets;
	std::vector<VkDescriptorSet> texturesSets;
	std::vector<uint32_t> boundFontAtlasRevisionByFrame;
	std::vector<bool> textureDescriptorsDirtyByFrame;

	std::vector<UiInstance> instancesScratch;
	std::vector<UiRun> runsScratch;

	WindowTextureRegistry textureRegistry;
};
```

The render path should take both:

```cpp
renderUi(
	const SharedUiRendererResources& shared,
	WindowUiRendererResources& windowResources,
	const Clay_RenderCommandArray& commands,
	...
);
```

This avoids duplicating pipelines while keeping mutable descriptor/frame/scratch state per window.

## Window Context

Add an internal `AppWindow` type. It should contain only resources that are unique to a native window.

```cpp
struct AppWindow {
	WindowId id = 0;
	WindowConfig config{};

	std::unique_ptr<detail::IWindowBackend> window;
	detail::InputQueue inputQueue;

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	Swapchain swap;
	FrameVk frames;

	UiManager ui;
	WindowUiRendererResources rendererResources;
	WindowTextureRegistry textureRegistry;
	ViewPortManager viewPortManager;

	Clay_RenderCommandArray renderCommands{};
	std::vector<VkImageLayout> swapchainImageLayouts;

	VkExtent2D observedFramebufferExtent{};
	bool framebufferResized = false;

	float uiToFramebufferScaleX = 1.0f;
	float uiToFramebufferScaleY = 1.0f;

	std::chrono::steady_clock::time_point previousBeginFrameTimestamp{};
	bool hasPreviousBeginFrameTimestamp = false;

	bool closeRequested = false;
};
```

Then `App::Impl` becomes a window orchestrator:

```cpp
struct App::Impl {
	AppConfig config{};

	VulkanContext vk;
	SharedUiRendererResources sharedRenderer;
	SharedTextureStore textures;
	SharedFontStore fonts;
	SharedIconStore icons;
	GpuUploadQueue uploads;
	GpuResourceRetirementQueue retirements;

	std::vector<std::unique_ptr<AppWindow>> windows;
	WindowId mainWindowId = 1;
	WindowId nextWindowId = 2;
};
```

The existing `AppConfig::window` still configures the first/main window.

## Public API Shape

Preserve current API by adding default window id arguments.

```cpp
using WindowId = uint64_t;

class App {
public:
	static constexpr WindowId MainWindow = 1;

	WindowId mainWindow() const;
	WindowId createWindow(const WindowConfig& config);
	void closeWindow(WindowId id);

	bool shouldClose() const;
	bool shouldClose(WindowId id) const;

	void beginFrame(WindowId id = MainWindow);
	void endFrame(WindowId id = MainWindow);
	void drawFrame(WindowId id = MainWindow);

	UiManager& ui(WindowId id = MainWindow);
	const UiManager& ui(WindowId id = MainWindow) const;
};
```

`shouldClose()` with no parameter should keep its current semantic: main window close means app close.

Secondary windows should be checked with `shouldClose(windowId)`.

## Callback Windows

After explicit windows work, add callback windows.

```cpp
using WindowDrawCallback = std::function<void(UiManager&)>;

WindowId createWindow(const WindowConfig& config, WindowDrawCallback draw);
void frameWindow(WindowId id);
void frameSecondaryWindows();
```

This enables simple app-managed windows without making the user manually call `beginFrame(id)` and `drawFrame(id)` for each secondary window.

## Element Windows

Element windows can be implemented on top of callback windows. They should not copy a built Clay subtree. Instead, they should own a draw callback that creates the requested root element each frame.

Example target API:

```cpp
template <typename Params, typename State, typename Resources, uint64_t DefId, bool IsInternal>
WindowId createElementWindow(
	const WindowConfig& config,
	const ElementDefinition<Params, State, Resources, DefId, IsInternal>& definition,
	std::string elementId,
	Params params);
```

Implementation concept:

```cpp
return createWindow(
	config,
	[definition, elementId = std::move(elementId), params = std::move(params)](UiManager& ui) mutable {
		ui.createElement(definition, elementId)
			.setParameters(params)
			.draw();
	});
```

For live app state, users can use the callback-window API and capture a model pointer.

## Viewports

Viewports should be window-local by default.

Reason: viewport size is derived from the image area where it appears. The same viewport key used in two windows can legitimately need two different render target sizes.

Target:

```cpp
struct WindowScopedViewPortKey {
	WindowId windowId;
	std::string key;
};
```

Default `ViewPortManager` should operate on the active window.

Shared viewport textures may be added later as a separate explicit feature, not the default.

## Resource Uploads

Resource uploads should eventually move through a central upload queue.

Initial implementation can remain synchronous, but the handle/status model should allow async later.

```cpp
enum class ResourceStatus {
	Loading,
	Ready,
	Failed,
};

TextureHandle requestImage(std::filesystem::path path);
ResourceStatus imageStatus(TextureHandle handle) const;
```

While loading, windows resolve the handle to a shared placeholder texture.

## Resource Retirement

Shared resources must not be destroyed while any submitted window command buffer might still use them.

Use a global submission serial:

```cpp
struct SubmittedFrame {
	WindowId windowId = 0;
	uint64_t serial = 0;
	VkFence fence = VK_NULL_HANDLE;
};
```

When a window submits work, assign a serial:

```cpp
uint64_t serial = nextSubmissionSerial++;
window.frames.current().submissionSerial = serial;
```

When replacing/destroying a shared GPU resource, retire it:

```cpp
struct RetiredGpuResource {
	uint64_t retireAfterSerial = 0;
	std::function<void()> destroy;
};
```

Destroy only when every window has completed all submissions up to that serial:

```cpp
uint64_t completedSerial = minCompletedSerialAcrossWindows();
destroyRetiredResources(completedSerial);
```

This avoids routine `vkDeviceWaitIdle()` calls and supports multi-window/multi-threaded rendering.

## Multi-Threading Compatibility

The architecture should allow this future split:

1. Main thread polls native events once.
2. Worker jobs build UI per window.
3. Worker jobs convert Clay commands to render instances per window.
4. Worker jobs record command buffers per window.
5. Render/main thread submits and presents windows.

This requires:

- per-window `UiManager`
- per-window Clay context
- per-window frame resources
- per-window descriptor sets
- per-window scratch buffers
- shared resources that are immutable/read-mostly
- mutation through queues or locked resource stores

Hot frame paths should avoid global locks. Most lookups should hit per-window caches. Shared stores should be accessed mostly when a cache miss occurs or a new resource is loaded.

## Existing Code Changes Required

### `FlowUi/PublicStructs.hpp`

- Add `WindowId`.
- Add multi-window config/cache config structs if needed.
- Keep `AppConfig::window` as the main-window config.
- Optionally add:

```cpp
struct ResourceCacheConfig {
	bool shareImagesAcrossWindows = true;
	bool shareFontAtlasesAcrossWindows = true;
	bool shareIconsAcrossWindows = true;
	uint64_t maxSharedImageBytes = 512ull * 1024ull * 1024ull;
	uint64_t maxSharedFontBytes = 128ull * 1024ull * 1024ull;
	bool asyncUploads = false;
};
```

### `FlowUi/App.hpp`

- Add `WindowId`-aware overloads/default parameters:
  - `mainWindow()`
  - `createWindow()`
  - `closeWindow()`
  - `shouldClose(WindowId)`
  - `beginFrame(WindowId)`
  - `endFrame(WindowId)`
  - `drawFrame(WindowId)`
  - `ui(WindowId)`
- Keep existing no-argument calls source-compatible.

### `src/FlowUi.cpp`

- Introduce internal `AppWindow`.
- Move per-window fields from `App::Impl` into `AppWindow`.
- Change `App::Impl` initialization to:
  1. initialize GLFW/window system enough to create main window,
  2. create main `AppWindow`,
  3. create main surface,
  4. choose Vulkan device using main surface,
  5. initialize shared app resources,
  6. initialize main window swapchain/frame/render resources.
- Update frame methods to look up `AppWindow&` by id.
- Poll GLFW/native events once per app frame, not once per window.
- Add closed-window cleanup.

### `Vulkan/Vk_Context`

- Stop assuming exactly one surface is owned by `VulkanContext`, or rename that ownership.
- Device selection may use the main surface for initial present support.
- Additional windows must create their own surfaces and verify present support.
- Surface destruction should be window-owned.

### `Vulkan/Vk_Swapchain`

- Already naturally per-surface. It should become owned by `AppWindow`.
- Ensure swapchain recreation takes the window's surface/extent.

### `Vulkan/Vk_Frames`

- Keep per-window.
- Add submission serial tracking to frame entries for global resource retirement.

### `Ui/Vk_UiRenderer`

- Split shared immutable renderer resources from per-window renderer resources.
- Move mutable frame resources out of the shared renderer object:
  - instance buffers
  - descriptor sets
  - descriptor dirty flags
  - scratch vectors
- Render function should receive both shared and window-local renderer state.

### `internal/UiTextureRegistry`

- Replace the current renderer-bound registry with a per-window binding registry.
- It should map shared resource handles to window-local descriptor slots.
- It should not own heavy `VkImage` resources.

### `ImageManager`

- Split into:
  - shared image store, app-level;
  - optional window-facing manager/binding helper.
- Image loading should return immutable generation handles.
- Window rendering resolves handles to local descriptor slots.

### `IconManager`

- Split icon source/cache from window binding.
- SVG parsing/rasterization and uploaded icon images should be shared where possible.
- Window-local descriptor slot assignment should happen through the window texture registry.

### `FontManager`

- Font face data and atlas GPU images should be shared.
- Window render resources should bind the shared atlas into that window's descriptor sets.
- Atlas rebuilds should produce new revisions/generations.

### `ViewPortManager`

- Make default viewports window-local.
- Move viewport render targets and callbacks into `AppWindow` or key them by `WindowId`.
- Later, optionally add explicit shared viewport resources.

### `UiManager`

- Keep per-window.
- Each window has its own Clay context, frame arenas, interaction state, input fields, shortcuts, dev runtime, and diagnostics.
- Any future app/window access API from `UiManager` should identify its owning `WindowId`.

### `window/Window.hpp` and `IWindow.hpp`

- Ensure each native window owns its callbacks and input queue.
- GLFW event polling should be orchestrated once by `App`, while each window refreshes its own cursor/mouse state.
- Window close should update the owning `AppWindow`.

## Resource Sharing Policy

Avoid exposing low-level manager sharing choices to users. The public knobs should be high-level cache policies only.

Good user-facing options:

```cpp
struct ResourceCacheConfig {
	bool shareImagesAcrossWindows = true;
	bool shareFontAtlasesAcrossWindows = true;
	bool shareIconsAcrossWindows = true;
	uint64_t maxSharedImageBytes = 512ull * 1024ull * 1024ull;
	bool asyncUploads = false;
};
```

Do not expose these as user policy:

- share vs duplicate `UiManager`
- share vs duplicate Clay context
- share vs duplicate swapchain/frame state
- share vs duplicate command buffers
- share vs duplicate per-window descriptor sets

Those should remain implementation details.

## Phased Implementation Plan

### Phase 1: Refactor Without Public Behavior Change

- Add `AppWindow`.
- Move single-window resources into it.
- Keep only one window.
- Preserve existing public API.

### Phase 2: Shared Resource Store and Per-Window Bindings

- Split texture/image ownership from window descriptor bindings.
- Introduce immutable handles/generations.
- Add window-local binding registry.

### Phase 3: Explicit Multi-Window API

- Add `WindowId`.
- Add `createWindow`, `closeWindow`, `ui(id)`, `beginFrame(id)`, `endFrame(id)`, `drawFrame(id)`.
- Keep default id as main window.

### Phase 4: Callback Windows

- Add draw callbacks.
- Add helper to frame callback-managed windows.

### Phase 5: Element Windows

- Add `createElementWindow`.
- Implement using callback windows.

### Phase 6: Drag-Out UX

- Add detachable interaction helpers.
- Treat drag-out as moving/capturing app model or a draw callback, not copying a Clay subtree.

### Phase 7: Multi-Threaded Backend

- Add per-window jobs for UI build, instance generation, and command recording.
- Keep shared resources immutable/read-mostly.
- Use resource mutation queues and deferred retirement.

## Main Design Rule

The central rule for the migration is:

> Window execution state is duplicated. Heavy GPU resource storage is shared and immutable. Descriptor bindings are per-window.

This gives FlowUi:

- source compatibility for simple apps,
- natural multi-window semantics,
- low duplication of large resources,
- clean concurrency boundaries,
- a path to multi-threaded rendering without redesigning resource ownership later.
