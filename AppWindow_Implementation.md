# Phase 1 AppWindow implementation proposal

## Purpose and scope

Phase 1 should change ownership, not visible behavior. FlowUi will still create exactly one public window and existing code will continue to call `app.beginFrame()`, `app.ui()`, and `app.drawFrame()` without a window argument. Internally, however, that window becomes a real `AppWindow` with a stable `WindowId`, its own storage window scope, and all state tied to its native surface and frame cadence.

The design rule is:

> `App` owns app/device-wide services and shared resource policy. `AppWindow` owns one window's execution state. `FlowStorageSystem` is app-owned and provides both shared resource storage and window/frame-scoped storage.

Phase 1 should **not** also migrate renderer VMA allocations, replace `TextureRef::id`, implement per-window descriptor resolution, or expose secondary-window creation. Those are Phases 2–4. The legacy renderer and manager bindings may remain temporarily main-window-only, but their ownership must be placed on the correct side of the new boundary.

## Main-window identity must be explicit

Storage already reserves `WindowId == 0` for app-shared/root attribution, so the first real window should use ID 1:

```cpp
namespace FlowUi {

using WindowId = uint64_t;
inline constexpr WindowId InvalidWindowId = 0;
inline constexpr WindowId MainWindowId = 1;

} // namespace FlowUi
```

The main window is semantically special but structurally ordinary:

- it is stored in the same registry and has the same `AppWindow` type as future secondary windows;
- its ID is always nonzero and stable for the lifetime of the `App`;
- no code should infer it from registry order or use `windows.begin()`;
- no window ID is reused during one app lifetime, matching `FlowStorageSystem::registerWindow()`;
- closing the main window means `App::shouldClose()` becomes true; future secondary-window close requests will affect only those windows.

`App::Impl` should keep `mainWindowId`, even if it is initially always `MainWindowId`. This prevents the semantic default from becoming an implicit assumption spread throughout the implementation.

It is reasonable to add the public `WindowId` type and a read-only `App::mainWindowId()` query in Phase 1 because both are additive and establish the permanent identity model. Public `createWindow`, `closeWindow`, and window-parameter frame APIs should wait until Phase 4.

## Configuration changes

### Public configuration: preserve source compatibility

Do not replace `AppConfig::window` in Phase 1. It should be documented explicitly as the initial configuration of the main window:

```cpp
struct AppConfig {
    WindowConfig window{}; // main window's initial native-window config
    VulkanConfig vk{};     // app/device defaults plus current window-render defaults
    UiConfig ui{};         // defaults used to create each UiManager
    IconManagerConfig iconManager{};
    DevToolsConfig dev{};
};
```

Existing expressions such as `config.window.width` and `config.vk.framesInFlight` must continue to work. A large public configuration redesign would add migration cost without enabling anything needed by the single-window Phase 1.

The implementation should stop treating `AppConfig` as mutable window runtime state. `AppWindow` receives an effective configuration snapshot. Calls such as `setWindowInputConfig()` update the main `AppWindow::config.native.input`, not `AppConfig::window.input`.

### Internal effective window configuration

Some fields currently in `VulkanConfig` are consumed per swapchain/window (`presentMode`, `srgbBackbuffer`, and `framesInFlight`), while validation and device selection are app-wide. Phase 1 does not need to break the public struct, but it should translate it once:

```cpp
struct AppWindowConfig {
    WindowConfig native{};
    PresentMode presentMode = PresentMode::Fifo;
    bool srgbBackbuffer = true;
    uint32_t framesInFlight = 2;
    UiConfig ui{};
};

AppWindowConfig makeMainWindowConfig(const AppConfig& app) {
    return {
        .native = app.window,
        .presentMode = app.vk.presentMode,
        .srgbBackbuffer = app.vk.srgbBackbuffer,
        .framesInFlight = std::max(1u, app.vk.framesInFlight),
        .ui = app.ui,
    };
}
```

This can remain an internal type. When public secondary-window creation is added, a public window-creation config can be designed without changing the meaning of the original `AppConfig`.

### Storage configuration

Do not expose internal `storage::StorageConfig` directly in `AppConfig`. Phase 1 should construct it from existing configuration and measured/default values:

```cpp
storage::StorageConfig makeStorageConfig(const AppConfig& app) {
    storage::StorageConfig result{};
    result.framesInFlight = std::max(1u, app.vk.framesInFlight);
    result.expectedWindows = 2; // capacity expectation, not a public-window limit
    return result;
}

storage::WindowStorageDesc makeWindowStorageDesc(
    const AppWindowConfig& window,
    storage::StringId debugName) {
    storage::WindowStorageDesc result{};
    result.framesInFlight = window.framesInFlight;
    result.debugName = debugName;
    return result;
}
```

Storage tuning should become public only if users have a demonstrated need for it. The current storage defaults are implementation policy, and exposing them now would freeze an interface before renderer measurements from Phase 2 exist.

## Proposed ownership model

### `App::Impl`: shared state and orchestration

```cpp
struct App::Impl {
    AppConfig config{};

    // App/device lifetime. VulkanContext no longer owns a surface.
    VulkanContext vk;
    std::unique_ptr<storage::IStorageSystem> storage;

    // Semantically app-shared managers/resources.
    FontManager fonts;
    ImageManager imageManager;
#if FLOWUI_INCLUDE_ICON_MANAGER
    IconManager icons;
#endif

    // Stable window objects. Only MainWindowId exists in Phase 1.
    std::unordered_map<WindowId, std::unique_ptr<AppWindow>> windows;
    WindowId mainWindowId = MainWindowId;
    WindowId nextWindowId = MainWindowId + 1;

    AppWindow& requireWindow(WindowId id);
    const AppWindow& requireWindow(WindowId id) const;
    AppWindow& mainWindow() { return requireWindow(mainWindowId); }
};
```

The registry stores `unique_ptr<AppWindow>` so an `AppWindow` address stays stable if the hash table rehashes. This matters because window callbacks, UI accessors, and future jobs may hold pointers to the window object. Returning references to values stored directly in a relocatable vector would be unsafe.

`App` owns the storage system because storage owns app-shared resource tables, upload/retirement state, and all `WindowStorageState`s. An `AppWindow` uses that service but must not independently own or shut it down.

The current `FontManager`, `ImageManager`, and `IconManager` public accessors are app-level, and their heavy data is intended to be shared. They should therefore remain on `App::Impl`. Their current renderer/texture-registry pointers can temporarily target the main window's legacy renderer and registry. That coupling must be clearly marked transitional and removed in Phases 2–3.

### `AppWindow`: one window's execution state

```cpp
struct AppWindow {
    WindowId id = InvalidWindowId;
    AppWindowConfig config{};
    storage::IStorageSystem* storage = nullptr; // non-owning; App outlives windows

    std::unique_ptr<detail::IWindowBackend> backend;
    detail::InputQueue inputQueue;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    Swapchain swapchain;
    FrameVk frames;

    UiManager ui;
    ViewPortManager viewPorts;

    // Transitional Phase 1 ownership. Split/migrate in Phases 2-3.
    VulkanUiRenderer renderer;
    UiTextureRegistry textureRegistry;

    FrameInput frameInput{};
    Clay_RenderCommandArray renderCommands{};
    std::vector<VkImageLayout> swapchainImageLayouts;

    storage::FrameToken storageFrame{};
    storage::FrameReadLease storageReadLease{};
    uint64_t frameNumber = 0;

    VkExtent2D observedFramebufferExtent{};
    bool framebufferResized = false;
    float uiToFramebufferScaleX = 1.0f;
    float uiToFramebufferScaleY = 1.0f;

    std::chrono::steady_clock::time_point previousBeginFrameTimestamp{};
    bool hasPreviousBeginFrameTimestamp = false;
    bool closeRequested = false;
};
```

This is intentionally a container of window-specific handles and execution state. It owns its native window, Vulkan surface, swapchain, synchronization objects, command buffers, UI/Clay context, input queue, viewport targets, current render commands, timing, scale, and storage frame state.

The whole current `VulkanUiRenderer` is moved into `AppWindow` as a transitional step because it mixes shared pipelines/placeholders with per-window descriptors, instance buffers, and scratch. Splitting all of it while also adopting storage would turn Phase 1 into Phases 1–3. Before multiple windows become public, later phases must extract its immutable shared pieces to an app-owned `SharedUiRendererResources` and migrate its buffer/image ownership to storage.

The `storage` pointer is an internal service reference, not ownership. Alternatively, `App::Impl` can perform every storage call while passing `AppWindow&`; either shape is acceptable, but there must be exactly one storage owner and window destruction must precede storage shutdown.

## Immediate core changes

### 1. Separate device and surface ownership

`VulkanContext::surface` is incompatible with multiple windows and must be removed in Phase 1. Surface creation should return a handle owned by `AppWindow`:

```cpp
struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = nullptr;
    // queues and queue-family indices...

    VkSurfaceKHR createSurface(detail::IWindowBackend& backend) const;
    void pickPhysicalDevice(const AppConfig&, VkSurfaceKHR mainSurface);
    bool supportsPresentation(VkSurfaceKHR surface) const;
};
```

`Swapchain::create()` and `recreate()` must accept an explicit `VkSurfaceKHR` and effective window config rather than reading `vk.surface` or `AppConfig::window`:

```cpp
void Swapchain::create(
    const AppWindowConfig& config,
    VulkanContext& vk,
    VkSurfaceKHR surface,
    VkExtent2D preferredExtent = {});
```

The main surface is still used for physical-device and queue-family selection. Future window creation must verify that the selected present queue supports its surface; Phase 1 should add the query seam even though it only checks the main surface.

### 2. Split global event polling from per-window refresh

`IWindowBackend::pollEvents()` currently calls global `glfwPollEvents()` and then refreshes one window's mouse/button snapshot. That cannot be called once per window. Split it into:

```cpp
struct IWindowSystem {
    virtual ~IWindowSystem() = default;
    virtual void pollEvents() = 0; // once per app iteration/platform thread
};

struct IWindowBackend {
    virtual ~IWindowBackend() = default;
    virtual void refreshInputSnapshot() = 0; // this window only
    // existing window operations...
};
```

For Phase 1, `App::beginFrame()` can still poll once because only the main window is exposed. The separation is required now so adding another window later does not duplicate global polling or mix input queues.

### 3. Create and register the main storage window

After device/VMA creation, `App::Impl` constructs and initializes `FlowStorageSystem`. The main window title can be interned for its debug name, then ID 1 is registered:

```cpp
storage = std::make_unique<storage::FlowStorageSystem>(vk);
storage->initialize(makeStorageConfig(config));

AppWindow& main = mainWindow(); // backend/surface already exist
const storage::StringId name = storage->intern(main.config.native.title);
storage->registerWindow(main.id, makeWindowStorageDesc(main.config, name));
```

Registration must be rolled back if later main-window initialization fails. A partially constructed application must not leave a registered scope or native Vulkan objects behind.

### 4. Connect frame-slot completion and storage lifecycle

Each Vulkan frame slot needs the storage submission associated with its previous use:

```cpp
struct FrameVk::Frame {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkFence inFlight = VK_NULL_HANDLE;
    storage::SubmissionToken storageSubmission{};
};
```

At the start of a window frame, wait for the selected slot's fence before opening the storage frame. If that slot has a submission token, report it complete and clear it. Then begin a new storage epoch using the exact same slot index:

```cpp
void beginWindowFrame(AppWindow& window) {
    FrameVk::Frame& slot = window.frames.getCurrentFrame();
    waitForFence(slot.inFlight);

    if (slot.storageSubmission) {
        storage->noteCompleted(slot.storageSubmission);
        slot.storageSubmission = {};
        storage->collect();
    }

    const uint32_t slotIndex = window.frames.currentFrame;
    window.storageFrame = storage->beginFrame(window.id, {
        .frameSlot = slotIndex,
        .frameNumber = ++window.frameNumber,
    });

    // Refresh input and begin UiManager after the storage frame exists.
}
```

`endWindowFrame()` seals the storage frame after UI/resource preparation:

```cpp
window.renderCommands = window.ui.endFrame();
// Current legacy viewport/icon preparation...
window.storageReadLease = storage->sealFrame(window.storageFrame);
```

After a successful `vkQueueSubmit`, convert that lease to the slot's submission token:

```cpp
vkCheck(vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, slot.inFlight),
        "Failed to submit UI command buffer.");
slot.storageSubmission = storage->noteSubmission(window.storageReadLease);
window.storageReadLease = {};
window.storageFrame = {};
```

Although Phase 1 does not yet track renderer resources through storage, wiring this lifecycle now establishes the correct completion protocol for Phase 2. `noteSubmission()` should follow successful queue submission: until then the active storage frame retains its resources. Its validated transition is designed to be failure-free after internal capacity reservation.

Every pre-submission early return or exception—minimized window, out-of-date acquire, cancelled frame, UI failure—must cancel an active storage frame. Use an internal RAII guard rather than duplicating cleanup across branches:

```cpp
class StorageFrameGuard {
public:
    ~StorageFrameGuard() {
        if (storage_ && frame_) storage_->cancelFrame(frame_);
    }
    void submitted() noexcept { frame_ = {}; }
    // ...
};
```

Swapchain recreation currently waits for the whole device. Phase 1 may retain that behavior because only one public window exists, but after an idle wait it must report every outstanding window submission complete before resetting/destroying slot resources. Removing device-wide idle belongs to Phase 4.

### 5. Route all existing `App` methods through the main window

No-argument methods become explicit forwarding operations:

```cpp
bool App::shouldClose() const {
    return !impl_ || impl_->mainWindow().backend->shouldClose();
}

void App::beginFrame() {
    if (impl_) impl_->beginFrame(impl_->mainWindowId);
}

UiManager& App::ui() {
    if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
    return impl_->mainWindow().ui;
}
```

The same routing applies to title, size, framebuffer size, native handle, raw mouse support, clipboard, input configuration, `endFrame()`, `drawFrame()`, and `viewPorts()`. `fonts()`, `images()`, and `icons()` continue to return app-shared managers.

Internally, `Impl::beginFrame(WindowId)`, `endFrame(WindowId)`, and `drawFrame(WindowId)` should take an ID now. They need not be public until actual multi-window behavior is implemented.

### 6. Make initialization and shutdown ordering explicit

Proposed creation order:

1. validate `AppConfig` and create the main `AppWindow` object/input queue/backend;
2. collect required platform instance extensions;
3. create the Vulkan instance;
4. create the main surface and store it in `AppWindow`;
5. select the physical device/queues using that surface and create device/VMA;
6. create and initialize app-owned `FlowStorageSystem`;
7. register `MainWindowId` in storage;
8. create the main swapchain, frame slots, legacy renderer/registry, and window-local UI/viewport state;
9. initialize app-shared fonts/images/icons, temporarily binding image/icon compatibility paths to the main window renderer/registry;
10. publish the fully initialized app.

Proposed shutdown order:

1. stop new frames and cancel any active but unsubmitted storage frame;
2. wait for current GPU work during the Phase 1 single-window shutdown path;
3. report all stored `SubmissionToken`s complete and collect;
4. destroy app-shared legacy managers while their temporary main-renderer bindings are valid;
5. destroy each window's viewport, registry, renderer, frame, and swapchain resources;
6. unregister each storage window scope;
7. destroy each `VkSurfaceKHR`, then native window backend;
8. shut down storage while the Vulkan device/VMA still exist;
9. destroy the Vulkan device, allocator, debug messenger, and instance.

Initialization should use the same reverse-order cleanup so exceptions at any step do not leak a surface, registered storage scope, or Vulkan resource.

## What moves where in Phase 1

| Current `App::Impl` member | Phase 1 owner | Reason |
|---|---|---|
| `config` | `App::Impl`; effective window copy in `AppWindow` | Original app defaults versus mutable per-window runtime config. |
| `window`, `inputQueue` | `AppWindow` | Native/input identity is window-local. |
| `vk` | `App::Impl` | Instance/device/queues/VMA are shared. Surface is removed from it. |
| `swap`, `frames`, image layouts | `AppWindow` | Bound to one surface and frame cadence. |
| `ui`, frame input, render commands | `AppWindow` | Each window needs an independent Clay/input lifecycle. |
| `viewPortManager` | `AppWindow` | Viewport targets and callbacks are window/render-target local by default. |
| `renderer`, `textureRegistry` | `AppWindow` temporarily | Their current mutable descriptor/frame state is window-local; later phases extract shared immutable resources. |
| `fonts` | `App::Impl` | Font faces/atlases are intended to be shared; current renderer pointer is a temporary main-window adapter. |
| `imageManager` | `App::Impl` | Image identity/content should be shared; direct Vulkan and registry ownership migrates in Phases 2–3. |
| `icons` | `App::Impl` | Icon source/raster cache should be shared; per-window binding work is separated later. |
| timing, resize, scale | `AppWindow` | These values depend on one window's frame rate and extents. |
| storage object | `App::Impl` | One authority for shared and per-window storage. |
| storage frame/lease | `AppWindow` | One active lifecycle per window. |
| storage submission token | `FrameVk::Frame` | Completion belongs to the exact reusable GPU frame slot. |

## Deliberately deferred work

Phase 1 should leave the following for later phases:

- moving renderer buffers, placeholder images, samplers, and VMA allocations into storage;
- splitting `VulkanUiRenderer` into app-shared immutable and per-window mutable components;
- changing `TextureRef::id` into a logical generational texture handle;
- applying `prepareTextureBindings()` output to renderer descriptor sets;
- separating image/icon/font heavy storage from their current renderer-bound manager implementations;
- exposing `createWindow()`, `closeWindow()`, or public frame methods taking a `WindowId`;
- supporting two windows with different present support, swapchain formats, or frame rates;
- eliminating device-wide idle during swapchain recreation and shutdown;
- parallel UI build, command recording, or storage mutation.

The temporary main-window adapter between app-shared managers and the main window's renderer/registry must be called out in code comments. It is acceptable only because Phase 1 still prevents creation of a second window.

## Validation and Phase 1 exit criteria

Phase 1 is complete when:

- existing single-window examples and public APIs behave as before;
- `App::Impl` contains no directly embedded native-window, surface, swapchain, frame, UI, input, timing, or resize state;
- the main window is a stable registry entry with ID 1, while ID 0 remains reserved for app-shared storage;
- `VulkanContext` no longer owns or implicitly reads one surface;
- swapchain creation takes an explicit surface and window-effective configuration;
- global event polling and per-window input refresh are separate operations;
- storage is created by `App`, the main storage scope is registered, and every submitted frame receives/completes a `SubmissionToken` tied to its Vulkan frame slot;
- cancelled/minimized/out-of-date frames do not leave an active storage frame or lease;
- all existing no-argument `App` methods route through `mainWindowId`;
- partial initialization and normal shutdown destroy resources in dependency order;
- no public secondary-window API exists yet and no Phase 2/3 renderer migration has been pulled into this change.

Recommended tests:

1. compile-time/API regression tests for all existing no-argument `App` methods;
2. an internal registry test proving `MainWindowId == 1`, zero is rejected, and lookup does not depend on container order;
3. initialization-failure tests at surface, device, storage registration, swapchain, and renderer stages;
4. frame lifecycle tests for begin/seal/submit/complete and begin/seal/cancel paths;
5. minimized/out-of-date swapchain tests proving storage frame cancellation;
6. shutdown tests with active, sealed, and submitted frame states;
7. validation-layer run proving surfaces, swapchains, and device resources are destroyed in valid order;
8. existing storage tests plus the normal development/sanitizer build matrix.

## Instructions for a future Phase 1 implementation prompt

Use the following as the execution brief:

> Implement Phase 1 from `AppWindow_Implementation.md` while preserving current single-window public behavior. First inspect the working tree and do not overwrite unrelated changes. Add a canonical `FlowUi::WindowId` with zero invalid/root-shared and `MainWindowId == 1`. Introduce an internal stable `AppWindow` registry in `App::Impl`, create only the main window, and route every existing no-argument `App` window/UI/frame method through explicit main-window lookup.
>
> Move the native backend, input queue, Vulkan surface, swapchain, `FrameVk`, `UiManager`, `ViewPortManager`, current legacy renderer/texture registry, render commands, image-layout tracking, timing, scaling, resize state, and current storage frame/lease into `AppWindow`. Keep the device/VMA context, one `FlowStorageSystem`, registry/ID allocation, and semantically shared font/image/icon managers on `App::Impl`. Preserve and clearly mark the temporary main-window renderer/registry adapter used by the shared managers.
>
> Remove single-surface ownership from `VulkanContext`: return/store surfaces on `AppWindow`, pass the main surface explicitly to device selection, and pass an explicit surface/effective window config into swapchain creation. Split global platform event polling from per-window input refresh. Do not expose public secondary-window creation yet.
>
> Initialize storage after device/VMA creation, register the main window using a translated `WindowStorageDesc`, and wire storage frame lifecycle to the exact Vulkan frame slot. Wait for a slot fence and call `noteCompleted()` before `beginFrame()`, seal after UI preparation, store `noteSubmission()` on `FrameVk::Frame` after successful queue submission, call `collect()`, and cancel all unsubmitted frames on early return or exception using an RAII guard. Drain tokens correctly around device-idle swapchain recreation and shutdown.
>
> Keep `AppConfig::window` source-compatible and treat it as main-window initial configuration. Use an internal effective `AppWindowConfig`; do not expose internal `storage::StorageConfig`. Do not migrate renderer VMA resources, logical textures, descriptor binding, or manager resource stores in this phase.
>
> Add focused tests for main-window identity/lookup, surface ownership, storage registration and frame-token transitions, early-return cancellation, partial-init cleanup, and existing API behavior. Run the relevant build/tests and Vulkan validation where available. Finish by reporting files changed, validation results, and any intentionally deferred Phase 2/3 coupling.
