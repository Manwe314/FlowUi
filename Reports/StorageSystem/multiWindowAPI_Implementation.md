# Phase 4 multi-window API implementation proposal

## Purpose and recommended result

Phase 4 should turn the internal `AppWindow` model created in Phase 1 into a
real public multi-window API without changing the established single-window
path. An application should be able to create a secondary native window, build
and render its UI through a stable `WindowId`, resize or close that window, and
continue rendering every other window without a device-wide idle.

The target rule remains:

> `App` owns the device and shared resources. Each `AppWindow` owns one native
> window's execution state. Storage resolves shared logical resources into that
> window's frame-local bindings.

This phase should deliver explicit user-driven windows. Callback-managed
windows, element windows, drag-out behavior, and parallel window jobs are not
part of this phase even though the older `MULTI_WINDOW_DESIGN.md` used different
phase numbering.

The recommended result is:

- `AppConfig::window` still creates the semantic main window with
  `MainWindowId == 1`;
- existing no-argument APIs behave as they do now and still target the main
  window;
- secondary windows are created and destroyed explicitly with non-reused
  `WindowId` values;
- every window has its own backend, input queue, Clay/UI context, viewport
  manager, surface, swapchain generation, Vulkan frame slots, descriptor bundle,
  instance buffers, timing, and diagnostics;
- logical images, icons, fonts, storage, the device/VMA context, and compatible
  renderer pipelines remain app-shared;
- platform events are polled once per application tick, not once per window;
- only the target window's fences and presentation completion are involved in
  its resize or destruction;
- no public secondary-window operation calls `vkDeviceWaitIdle()`;
- Phase 3 texture handle, pre-seal binding, sealing, submission, completion,
  cancellation, and collection semantics remain unchanged.

## Current-code findings that shape Phase 4

The current tree is already materially newer than the old multi-window design:

- `FlowUi::WindowId`, `InvalidWindowId == 0`, and `MainWindowId == 1` already
  exist in `include/FlowUi/WindowId.hpp`;
- `App::Impl` already owns a stable
  `unordered_map<WindowId, unique_ptr<AppWindow>>`, a main-window ID, and a
  monotonic next ID;
- window-specific state is already inside `AppWindow`;
- internal `beginFrame(WindowId)`, `endFrame(WindowId)`, and
  `drawFrame(WindowId)` already perform explicit lookup;
- `VulkanContext` no longer owns one surface, and it can check whether its
  selected present queue family supports another surface;
- swapchain construction already takes an explicit surface and effective window
  configuration;
- storage already supports multiple registered window scopes and permanently
  rejects reuse of a `WindowId`;
- logical `TextureHandle` values already resolve independently for each window;
- storage already has generation-checked renderer layout, pipeline bundle, and
  window descriptor bundle APIs, including serial retirement;
- the renderer remains one mutable facade per `AppWindow`, but it still creates
  and destroys duplicate native layout/pipeline objects itself;
- `beginFrame()` currently combines global GLFW polling with main-window frame
  startup, while the internal frame method correctly performs only per-window
  input refresh;
- shared `ImageManager` retirement still advances from the main frame slot, and
  `IconManager` advances its cache frame counter once per prepared window;
- swapchain recreation and `ViewPortManager::destroy()` still use
  `vkDeviceWaitIdle()`;
- current frame-slot fences prove graphics submission completion, but they do not
  by themselves prove that the presentation engine has finished using an old
  swapchain.

Therefore Phase 4 is not primarily an ownership move. Its difficult work is
public lifecycle semantics, transactional secondary-window creation, exact WSI
retirement, shared renderer adoption, and removal of the remaining per-window
device-idle paths.

Several Phase 3 comments in `src/FlowUi.cpp` still describe manager textures as
renderer-local slots. Those comments are stale and should be corrected while
implementing this phase; texture commands already carry logical handles.

## Public API shape

### Add overloads, not default arguments

Keep every existing no-argument symbol and add explicit overloads. This avoids
source ambiguity, preserves the current ABI shape as far as possible, and lets
the compatibility wrapper retain its special event-polling behavior.

Recommended additions to `App`:

```cpp
class App {
public:
    [[nodiscard]] WindowId mainWindowId() const noexcept;
    [[nodiscard]] WindowId createWindow(const WindowConfig& config);
    void destroyWindow(WindowId id);
    [[nodiscard]] bool hasWindow(WindowId id) const noexcept;

    void pollEvents();

    [[nodiscard]] bool shouldClose(WindowId id) const;
    void setShouldClose(WindowId id, int value);

    void beginFrame(WindowId id);
    void endFrame(WindowId id);
    void drawFrame(WindowId id);

    UiManager& ui(WindowId id);
    const UiManager& ui(WindowId id) const;

#if FLOWUI_PUBLIC_VULKAN_INTEROP
    ViewPortManager& viewPorts(WindowId id);
    const ViewPortManager& viewPorts(WindowId id) const;
#endif

    void setWindowTitle(WindowId id, std::string_view title);
    [[nodiscard]] std::pair<int, int> windowSize(WindowId id) const;
    [[nodiscard]] std::pair<int, int> framebufferSize(WindowId id) const;
    void setWindowInputConfig(WindowId id, const WindowInputConfig& config);
    [[nodiscard]] WindowInputConfig windowInputConfig(WindowId id) const;
    [[nodiscard]] void* nativeWindowHandle(WindowId id) const;
    [[nodiscard]] bool supportsRawMouseMotion(WindowId id) const;
    void setClipboardText(WindowId id, std::string_view text);
    [[nodiscard]] std::string clipboardText(WindowId id) const;

    // Existing no-argument overloads remain unchanged.
};
```

`fonts()`, `images()`, and `icons()` remain app-shared accessors and must not gain
a `WindowId` overload. A viewport manager is window-local and must gain one.

An optional `windowIds()` allocation-returning enumeration is not necessary for
this phase. Callers receive and own their secondary `WindowId` values, while
`hasWindow()` provides a cheap stale-ID check.

### Window identity and invalid IDs

`WindowId` should remain a monotonic 64-bit identity, not become an array index.
Zero remains invalid/root-shared and IDs must never be reused during one `App`
lifetime, including after a failed creation that reached storage registration.

Recommended behavior:

- `hasWindow(InvalidWindowId)` and `hasWindow(staleId)` return `false`;
- accessors, mutators, and frame methods throw `std::invalid_argument` for an
  unknown ID;
- `createWindow()` throws on ID exhaustion rather than wrapping;
- `destroyWindow()` invalidates a successful secondary ID before returning;
- `destroyWindow(MainWindowId)` is rejected. The main window requests app
  shutdown through its normal close flag and is destroyed with the `App`;
- a secondary native close button sets `shouldClose(id)`, but does not silently
  erase the window or invalidate user-held references. The user calls
  `destroyWindow(id)` explicitly.

This keeps destruction observable and avoids a callback or `UiManager&` becoming
dangling merely because `pollEvents()` ran.

### Polling compatibility

Multi-window code needs a public once-per-tick polling operation:

```cpp
void App::pollEvents() {
    impl_->pollEventsAndAdvanceSharedManagers();
}

void App::beginFrame() {
    if (!impl_) return;
    // Transitional: the legacy main-window entry point retains implicit polling.
    impl_->pollEventsAndAdvanceSharedManagers();
    impl_->beginFrame(impl_->mainWindowId);
}

void App::beginFrame(WindowId id) {
    if (impl_) impl_->beginFrame(id); // never polls globally
}
```

Thus old code remains unchanged. New multi-window code calls `pollEvents()` once,
then uses only explicit `beginFrame(id)` overloads. Documentation must warn users
not to mix an explicit `pollEvents()` with the no-argument `beginFrame()` in the
same tick unless a second platform poll is intentional.

## Configuration policy

`WindowConfig` is already the correct minimal public creation input. Do not
expose the internal `AppWindowConfig` or `storage::WindowStorageDesc`.

```cpp
AppWindowConfig makeWindowConfig(
    const AppConfig& appDefaults,
    const WindowConfig& native) {
    AppWindowConfig result{
        .native = native,
        .vulkan = appDefaults.vk,
        .ui = appDefaults.ui,
        .uiTextureDescriptorCapacity = 256,
    };
    result.vulkan.framesInFlight = std::max(1u, result.vulkan.framesInFlight);
    return result;
}
```

The rules should be:

- `AppConfig::window` remains source-compatible and configures only the main
  window;
- secondary `WindowConfig` supplies native size, title, decoration, fullscreen,
  DPI, and input settings;
- secondary windows inherit the app's effective Vulkan scheduling and UI config;
- the fixed Phase 3 descriptor capacity remains 256 for every window;
- physical device selection, validation layers, queues, VMA, shared font atlas,
  and manager configurations cannot vary per window;
- a richer `WindowCreateInfo` with per-window UI scale or presentation overrides
  can be additive later if a real use case requires it.

Do not copy the old proposal's optional public storage/cache budgets into this
phase. Those are app policy, not a prerequisite for correct multi-window APIs.

## AppWindow lifecycle and frame state

Add an explicit internal lifecycle rather than inferring it from nullable storage
tokens:

```cpp
enum class WindowFramePhase : uint8_t {
    Idle,
    Building,
    Prepared,
    Closing,
};

struct AppWindow {
    WindowId id = InvalidWindowId;
    AppWindowConfig config{};
    WindowFramePhase phase = WindowFramePhase::Idle;
    uint64_t lastSubmissionSerial = 0;

    // backend, input, surface, active swapchain generation, frames,
    // UiManager, ViewPortManager, renderer facade, storage tokens, timing...
};
```

The phase transitions are:

```text
Idle -> Building -> Prepared -> Idle
           |            |
           +--cancel----+

Idle -> Closing -> destroyed
```

`beginFrame(id)` requires `Idle`, `endFrame(id)` requires `Building`, and
`drawFrame(id)` requires `Prepared`. Every early return or exception after
storage `beginFrame()` must cancel the exact token, clear prepared spans and
leases, reset the public frame gate, and return the window to `Idle`.

### Phase 4 serialization boundary

Phase 4 remains a single-threaded explicit API. Because the current storage
shared-mutation policy and manager publication bridge do not allow arbitrary
mutation while another window has a sealed frame, permit only one begun but not
yet submitted/cancelled window frame at a time:

```cpp
WindowId activeWindowFrame = InvalidWindowId;
```

Trying to `beginFrame(B)` while A is still building or prepared should throw a
clear lifecycle error. This still permits windows to render at different rates,
skip ticks, use different frame slots, wait on different fences, resize, and
close independently. It only requires each `begin/end/draw` triplet to complete
before another begins.

```cpp
//Transitional: Phase 5 replaces this app-thread frame gate with one ownership
//epoch/job per AppWindow plus a defined app-shared mutation publish barrier.
```

This constraint is preferable to implying thread or overlapping-frame safety
that the shared managers do not yet provide.

## Transactional secondary-window creation

Extract the main window's post-device initialization into a reusable helper, but
keep main startup's device-bootstrap step separate. Secondary creation happens
after the instance, physical device, logical device, allocator, storage, shared
byte resources, and shared managers already exist.

Recommended sequence:

1. require the platform thread and no active window frame;
2. validate `WindowConfig` and reserve a fresh monotonic `WindowId`;
3. allocate a stable `unique_ptr<AppWindow>` with effective config;
4. create its backend and install callbacks into its own `InputQueue`;
5. attach clipboard/cursor closures to that stable `AppWindow` address;
6. create the surface;
7. verify the already-created device's present queue family supports the surface;
8. query formats, modes, and surface capabilities before committing anything;
9. register a translated `WindowStorageDesc` under the new ID;
10. transactionally create its swapchain generation and frame-slot resources;
11. acquire/adopt compatible shared renderer objects and its own descriptors;
12. initialize its window-local viewport manager and connect shared fonts;
13. insert it into `App::Impl::windows` only after the runtime is complete.

Use a rollback owner so every failure unwinds in reverse order:

```cpp
struct PendingWindow {
    std::unique_ptr<AppWindow> value;
    App::Impl* app = nullptr;
    bool committed = false;

    ~PendingWindow() {
        if (!committed && value) app->destroyUnsubmittedWindow(*value);
    }
};
```

If present support, swapchain creation, storage registration, descriptor
allocation, or renderer creation fails, the existing windows and app-shared
state must remain usable. The attempted ID should not later identify another
window.

The existing device is not reselected and the logical device is not recreated
for a secondary surface. Initially, support only surfaces accepted by the
already-selected `presentQFamily`; otherwise throw a precise error after
destroying the temporary surface/backend. Supporting multiple present queue
families would require requesting and storing those queues at initial device
creation and can be added later without changing the public API.

## Renderer sharing and per-window consumption

Keep one `VulkanUiRenderer` facade per `AppWindow`. It owns/caches mutable
window/frame state, but it must stop owning duplicate immutable native renderer
objects.

Phase 4 should use the storage APIs that already exist:

- `publishRendererLayout()` / `acquireRendererLayout()`;
- `publishRendererPipelineBundle()` / `acquireRendererPipelineBundle()`;
- `adoptWindowDescriptorBundle()`;
- generation-checked native queries and serial-aware release calls.

Recommended renderer state:

```cpp
struct VulkanUiRenderer {
    storage::RendererLayoutHandle layout{};                 // shared
    storage::RendererPipelineBundleHandle pipelineBundle{}; // shared by format
    storage::WindowDescriptorBundleHandle descriptors{};    // one window

    storage::NativeRendererLayout nativeLayout{};
    storage::NativeRendererPipelineBundle nativePipelines{};
    storage::NativeWindowDescriptorView nativeDescriptors{};

    std::vector<UiFrameResources> frameResources; // one storage buffer per slot
    WindowId window = InvalidWindowId;
    // font revision cache and other window-local state
};
```

Use one authoritative layout key containing descriptor capacity 256, shader
interface revision, push constant size, and descriptor feature flags. Pipeline
bundles are keyed by that layout, color format, sample count, pipeline-state
revision, and shader fingerprint. Two windows with the same compatible target
format acquire the same pipeline generation. A different format creates one new
shared bundle.

Native creation/publication must respect the existing ownership-transfer result:
destroy locally created candidates when storage reports that ownership was not
transferred. Cache native handles only while the strong storage handles remain
alive.

The descriptor pool and sets remain per-window/per-frame-slot. Create them from
the shared layouts, then transfer the pool to storage with
`adoptWindowDescriptorBundle()`. Storage already implicitly tracks the active
descriptor bundle during `sealFrame()`. Renderer preparation should additionally
track the shared layout and active pipeline bundle so the submitted serial stamps
their exact use.

`onSwapchainFormatChanged()` should become a transactional pipeline-bundle
acquire/swap/release operation. Descriptor layouts and per-slot descriptor sets
do not need recreation merely because the color target format changed.

The shader interface does not change in Phase 4. The CPU still resolves logical
textures into the current window's descriptor index, and the separate MSDF font
`sampler2DArray` remains shared.

## Global event polling and manager cadence

`detail::pollWindowSystemEvents()` is already global and should be called once
from `App::pollEvents()`. `IWindowBackend::refreshInputSnapshot()` remains a
per-window call from `beginFrame(id)`. GLFW callbacks already route through each
backend's own `InputQueue`.

Move app-shared manager maintenance away from a particular window frame slot:

- replace `ImageManager::onFrameStart(vk, mainFrameSlot)` with a safe-point
  maintenance method; its stored frame index is currently not used for resource
  choice, while exact logical retirement already comes from storage;
- advance/reset the shared icon cache counter once per app tick, then let each
  window's pre-seal preparation only resolve and touch variants;
- call `storage.collect()` and manager retirement maintenance at the quiescent
  app safe point as well as after exact fence completion;
- keep `ViewPortManager::onFrameStart()` per window because its targets are
  explicitly keyed by that window and Vulkan frame slot.

```cpp
void App::Impl::pollEventsAndAdvanceSharedManagers() {
    requireNoActiveWindowFrame();
    detail::pollWindowSystemEvents();
    storageSystem->collect();
    imageManager.onSafePoint(vk);
#if FLOWUI_INCLUDE_ICON_MANAGER
    icons.beginAppTick();
#endif
    collectRetiredWindowGpuObjects();
}
```

Manager mutations invoked directly by users must occur outside an active
window-frame triplet in Phase 4. The borrowed-native publication bridge and its
`//Transitional:` comments remain until the managers' VMA stores migrate to
storage in a later storage phase.

## Swapchain generations without device-wide idle

### Why frame fences are insufficient for WSI destruction

Waiting for all of one window's `FrameVk::Frame::inFlight` fences proves that its
graphics command buffers finished. It does not necessarily prove that the
presentation engine has released the old swapchain or consumed every
render-finished semaphore. Correct independent destruction therefore needs an
exact per-present completion mechanism.

At device creation, probe and enable one of these supported mechanisms:

1. `VK_EXT_swapchain_maintenance1` present fences, preferred; or
2. `VK_KHR_present_id` plus `VK_KHR_present_wait`, using monotonically increasing
   present IDs per swapchain generation.

If neither mechanism is available, preserve the existing single-window path but
make `createWindow()` fail with a clear unsupported-independent-retirement error.
The main-only compatibility path may retain its current device-idle recreation
on such a device; that fallback must be unreachable once public secondary
creation is supported. Do not silently use it for multi-window resize or close,
and do not claim that a graphics fence alone makes swapchain destruction safe.

An alternative would be to retain every closed window's backend, surface, and
swapchain until final app shutdown. That avoids an invalid destroy but does not
provide real window destruction and can accumulate resources across resizes, so
it is not recommended.

### Own image-specific synchronization with the generation

Refactor the current split between `Swapchain` and `FrameVk`:

```cpp
struct SwapchainGeneration {
    Swapchain swapchain{};
    std::vector<VkImageLayout> layouts;
    std::vector<VkFence> imageInFlight;
    std::vector<VkSemaphore> renderFinished;
    std::vector<VkFence> presentComplete; // maintenance1 path
    uint64_t lastPresentId = 0;           // present-wait path
    storage::SubmissionSerial lastGraphicsUse = 0;
};
```

Command pools, command buffers, image-available semaphores, graphics fences, and
storage submission tokens stay in the window's frame slots. Swapchain-image
layouts, image ownership fences, render-finished semaphores, and present
completion state belong to the swapchain generation.

### Transactional recreation

Recreation should:

1. cancel the current prepared-but-unsubmitted storage frame;
2. wait only for this window's outstanding graphics fences and report their
   tokens through `noteCompleted()`;
3. create a replacement swapchain using the active handle as `oldSwapchain`;
4. create replacement image views and image-specific sync without destroying the
   active generation;
5. acquire the compatible shared pipeline bundle for the replacement format;
6. atomically install the new swapchain/pipeline state;
7. move the old generation to a window retirement list;
8. destroy the old generation only after its exact presentation completion;
9. call `collect()` without waiting for any unrelated window.

`Swapchain::create()` should accept an optional old handle, and `Swapchain`
should become safely movable so a failed replacement leaves the active
generation untouched.

Minimized zero-size windows should return from their own draw/recreate path after
cancelling the unsubmitted storage frame. They must not spin-wait for a non-zero
extent and must not prevent another window from rendering.

Final application shutdown may still use one `vkDeviceWaitIdle()` after all new
work has stopped. Device loss recovery and the explicitly unsupported
main-window-only compatibility fallback above are separate paths.

## Independent destruction

`destroyWindow(id)` should synchronously remove a secondary window from the
public registry while allowing storage-owned retired generations to finish
collection internally.

Recommended sequence:

1. reject the main ID and unknown IDs;
2. prevent new frames and mark the window `Closing`;
3. if that same window owns an unsubmitted frame, cancel it and invalidate all
   frame-arena views; reject destruction if another window owns the transitional
   app frame gate;
4. detach/disable native callbacks so no new input targets the closing object;
5. wait only for that window's frame fences and call `noteCompleted()` for every
   stored token;
6. wait for that window's exact outstanding present completion;
7. release its renderer pipeline/descriptor/instance handles with the highest
   window submission serial;
8. destroy its drained viewport command pools and borrowed VMA targets without a
   device idle;
9. destroy swapchain generations, frame objects, surface, and backend;
10. call `storage.unregisterWindow(id, lastSubmissionSerial)` and `collect()`;
11. erase the stable registry entry and clear the app frame gate.

Storage may conservatively retain its closing `WindowStorageState` until the
global completed serial watermark passes its retirement serial. That does not
require keeping the public `AppWindow`, native backend, or surface alive, because
storage owns the descriptor bundle and its other retired native resources.

`ViewPortManager::destroy()` must gain a drained variant or a documented
precondition and remove its internal `vkDeviceWaitIdle()`. Likewise, live
viewport removal must defer command pools/targets until the owning window's
relevant submissions complete instead of idling the device. The current exact
logical-handle retirement bridge can continue to guard borrowed target image
lifetime.

## Queue and threading rules

Phase 4 API calls remain on the platform/main thread. `vkQueueSubmit()` and
`vkQueuePresentKHR()` continue using the app's shared queues serially, satisfying
Vulkan external synchronization. Do not add internal per-window threads here.

Waiting for a target window's fence may naturally wait behind earlier work on
the same queue, including work submitted for another window. That is normal
queue ordering and is different from idling the whole device or explicitly
waiting for unrelated later work.

The first implementation should keep the selected main present queue family and
reject a secondary surface unsupported by it. A future device bootstrap can
request queues from additional families and select a present queue per surface;
the `WindowId` API does not need to change.

## User-visible behavior

### Existing single-window code remains valid

```cpp
while (!app.shouldClose()) {
    app.beginFrame(); // retains implicit global event polling
    buildMainUi(app.ui());
    app.endFrame();
    app.drawFrame();
}
```

Every no-argument window, UI, frame, viewport, input, size, clipboard, and native
handle method remains a wrapper around `mainWindowId()`.

### Explicit two-window code

```cpp
FlowUi::WindowConfig inspectorConfig{};
inspectorConfig.title = "Inspector";
inspectorConfig.width = 520;
inspectorConfig.height = 720;

const FlowUi::WindowId inspector = app.createWindow(inspectorConfig);

while (!app.shouldClose()) {
    app.pollEvents();

    app.beginFrame(app.mainWindowId());
    buildMainUi(app.ui(app.mainWindowId()));
    app.endFrame(app.mainWindowId());
    app.drawFrame(app.mainWindowId());

    if (app.hasWindow(inspector)) {
        if (app.shouldClose(inspector)) {
            app.destroyWindow(inspector);
        } else {
            app.beginFrame(inspector);
            buildInspectorUi(app.ui(inspector));
            app.endFrame(inspector);
            app.drawFrame(inspector);
        }
    }
}
```

The inspector may be rendered every tick, every tenth tick, or only after model
changes. Its skipped frames, minimized extent, fence waits, resize, and close do
not change the main window's frame slot or storage scope.

### Shared images and window-local viewports

```cpp
const FlowUi::TextureRef logo = app.images().getTexture("logo");

app.beginFrame(app.mainWindowId());
drawLogo(app.ui(app.mainWindowId()), logo);
app.endFrame(app.mainWindowId());
app.drawFrame(app.mainWindowId());

app.beginFrame(inspector);
drawLogo(app.ui(inspector), logo); // same logical handle, local descriptor index
app.endFrame(inspector);
app.drawFrame(inspector);
```

The image/font/icon managers remain shared. `app.viewPorts(windowId)` is required
because viewport render targets and callbacks are window/frame-local.

Raw Clay image payloads still require `UiManager::imageData()` for the owning
window's frame arena. A logical `TextureHandle` does not remove that pointer
lifetime requirement.

## Failure and exception rules

- Window creation has a strong guarantee for all previously existing windows.
- A surface/present mismatch reports the new surface and selected queue-family
  incompatibility clearly.
- Failed creation never leaves a public registry entry, storage scope, surface,
  pipeline reference, descriptor pool, or backend behind.
- A frame call on a stale ID throws before touching storage or Vulkan state.
- Calling frame methods out of order throws and leaves other windows unchanged.
- Any unsubmitted frame is cancelled exactly once on exception or early return.
- After successful `vkQueueSubmit`, the storage lease must be consumed with
  `noteSubmission()` and stored on that exact `FrameVk::Frame` as today.
- Descriptor application still acknowledges only successful dirty writes.
- A failed swapchain replacement leaves the old generation active if it is still
  usable; otherwise the window becomes close-requested without affecting others.
- Window destruction is idempotent only through an explicit `hasWindow()` check;
  directly destroying a stale ID should report misuse.

## File-level change plan

### `include/FlowUi/App.hpp`

- add public create/destroy/poll/has APIs;
- add `WindowId` overloads for every window-local method;
- document explicit polling and reference invalidation rules;
- retain all no-argument overloads.

### `include/FlowUi/PublicStructs.hpp`

- keep `WindowConfig`, `AppConfig::window`, `VulkanConfig`, and `UiConfig`
  source-compatible;
- clarify that secondary windows inherit app Vulkan/UI defaults;
- do not expose internal storage configuration.

### `src/FlowUi.cpp`

- centralize effective window config and storage descriptor translation;
- add frame/lifecycle state and the transitional one-active-window gate;
- add transactional secondary initialization and exact destruction helpers;
- expose per-window wrappers while keeping no-argument main adapters;
- move shared manager maintenance to the app polling safe point;
- remove device-idle recreation and use swapchain generations;
- correct stale Phase 3 transitional comments.

### `include/window/IWindow.hpp` and `include/window/Window.hpp`

- keep one backend/input queue per window;
- add callback detachment or shutdown protection for closing windows;
- keep GLFW polling global;
- ensure window creation/destruction stays on the platform thread.

### `Vulkan/Vk_Context.*`

- probe/enable exact present completion support;
- retain the explicit surface support query;
- expose the enabled WSI retirement mode;
- initially reject surfaces unsupported by the selected present queue family.

### `Vulkan/Vk_Swapchain.*` and `Vulkan/Vk_Frames.*`

- make swapchains move-only and replacement transactional with `oldSwapchain`;
- move image-specific layouts/fences/semaphores into a generation;
- add present fences or present IDs;
- keep command/fence/storage-token frame slots per window;
- provide target-window drain helpers without device idle.

### `Ui/Vk_UiRenderer.*`

- adopt storage renderer layout/pipeline/descriptor handles;
- share pipelines by compatible target format;
- retain per-window descriptors and per-slot instance buffers;
- track shared renderer resources in each storage frame;
- replace format recreation with transactional pipeline generation switching;
- remove direct destruction of storage-adopted native objects.

### Managers

- make image/icon shared maintenance use an app safe point rather than main-window
  frame cadence;
- keep `ViewPortManager` per window and remove its internal device-idle teardown;
- preserve the `//Transitional:` borrowed-native publication/retirement bridge;
- do not migrate image, icon, viewport, or font allocations in this phase.

### Tests and documentation

- extend public API and lifecycle documentation with explicit-window examples;
- add internal seams or fixtures so rollback and close ordering can be tested
  without relying only on interactive windows;
- update source regressions for removed device-idle and duplicate renderer paths.

## Scoped implementation sequence

1. Add public overload declarations, lifecycle state, `hasWindow()`, and explicit
   polling while keeping all no-argument wrappers unchanged.
2. Refactor main-window config/storage/GPU setup into reusable helpers and add a
   failure-safe internal secondary creation path.
3. Validate secondary-surface presentation support and expose exact WSI
   completion capability selected during device creation.
4. Adopt storage-owned shared renderer layouts/pipeline bundles and per-window
   descriptor bundles before creating a real second window.
5. Add transactional swapchain generations and remove device-idle recreation.
6. Add the public `createWindow()` path and validate two fully initialized windows.
7. Move shared manager cadence to the app safe point and add all window-local
   accessor/mutator overloads.
8. Implement exact secondary destruction, viewport drained teardown, storage
   unregister, and registry erasure.
9. Add focused unit/integration/Vulkan tests, update docs, and write `phase4.md`.

This ordering deliberately proves shared renderer and WSI retirement before
making secondary creation a supported public operation.

## Tests and validation required

### Public API and compatibility

- existing no-argument application loop compiles and behaves unchanged;
- `AppConfig::window` remains the main initial configuration;
- public headers compile with and without icon/Vulkan interop options;
- all no-argument accessors resolve `mainWindowId()`;
- explicit polling occurs once in the documented multi-window loop;
- IDs are non-zero, monotonic, never reused, and reject stale lookup;
- main destruction is rejected and main close still terminates the app loop.

### Creation and rollback

- two secondary windows receive stable distinct IDs and registry addresses;
- backend, surface, present check, swapchain, storage, renderer, viewport, and
  descriptor failure injection each roll back without affecting the main window;
- unsupported secondary-surface presentation reports a clean failure;
- storage registration is removed or retired correctly after partial failure;
- ID exhaustion and invalid configs fail without wraparound.

### Frame and input isolation

- two windows use independent input queues, Clay contexts, dimensions, scales,
  timing, diagnostics, frame numbers, and frame slots;
- one window can render less frequently or remain minimized while another
  advances;
- out-of-order frame API calls and overlapping Phase 4 triplets are rejected;
- clipboard, cursor, title, input config, native handle, and size APIs target the
  requested backend;
- an exception in one window cancels only that storage frame.

### Storage, textures, and managers

- one logical image and one icon atlas page resolve to valid bindings in two
  windows, including different local descriptor indices;
- a frame-local viewport handle is accepted only by its owning window and exact
  frame slot;
- dirty descriptor acknowledgement remains isolated per window/slot;
- stale/fallback textures behave identically in both windows;
- closing one window preserves shared resources used by another;
- shared manager maintenance advances once per app tick rather than once per
  rendered window.

### Renderer sharing

- same-format windows share one native layout and pipeline bundle generation;
- different-format windows share the layout and acquire distinct pipeline
  bundles;
- descriptor pools/sets and instance buffers remain per window/frame slot;
- submitted frames track layout, pipeline, descriptor, buffer, and texture use;
- closing or format-switching one window does not retire a bundle still acquired
  by another;
- publication/adoption failure leaves previous renderer generations intact.

### Resize, presentation, and destruction

- resize one window repeatedly while another continues submitting;
- replacement uses `oldSwapchain` and retains the prior generation until exact
  present completion;
- zero-size/minimized windows do not spin or block another window;
- close a window with submissions and presents in flight;
- only the closing window's fences/present completion are waited;
- viewport teardown after a window drain performs no device-wide idle;
- validation reports no destroyed-in-use swapchain, view, semaphore, descriptor,
  command pool, viewport image, surface, or backend objects;
- source regression confirms the multi-window recreate/close path cannot reach
  `vkDeviceWaitIdle()`. Final shutdown and the no-secondary-capability main-only
  compatibility path remain explicit exceptions.

### Full validation

- release and development builds;
- storage types, storage lifecycle, renderer conversion, managers, and Vulkan
  integration tests;
- two-real-window validation-layer run when the environment supports presentation;
- ASan/UBSan builds when configured, especially for stale window/UI references;
- `git diff --check`.

Phase 4 exits only when two public windows can advance at different rates, share
logical resources and compatible pipelines, resize independently, and close one
without device-wide idle or corruption of the other.

## Deliberately deferred work

- callback-owned windows and automatic `frameSecondaryWindows()` helpers;
- `createElementWindow()` and detachable/drag-out UI behavior;
- parallel UI build, conversion, recording, submission, or event threads;
- overlapping active/sealed window frames and the Phase 5 publish-barrier/job
  ownership model;
- manager VMA allocation/upload migration to storage;
- removal of synchronous upload queue waits in the current managers;
- font atlas storage migration;
- logical sampler variants and non-no-op `TextureSamplingMode`;
- descriptor capacity beyond the authoritative 256 shader/layout value;
- per-window Vulkan device, MSAA, validation, or arbitrary queue-family policy;
- automatic device recreation when a secondary surface is unsupported;
- replacing raw Clay pointer payloads with a by-value FlowUi image element.

## Proposed future execution prompt

> Implement Phase 4 exactly as scoped in `multiWindowAPI_Implementation.md`,
> first inspecting the working tree and preserving unrelated changes. Keep all
> current single-window no-argument behavior and keep `AppConfig::window` as the
> semantic main-window configuration. Add explicit public `WindowId` APIs for
> transactional secondary creation/destruction, existence and close queries,
> once-per-app-tick event polling, per-window frame/UI/viewport access, and every
> existing window-local query or mutator. Do not add callback-managed or element
> windows.
>
> Keep IDs monotonic and non-reusable, reject destruction of the semantic main
> window, and add an explicit `AppWindow` lifecycle. Preserve implicit polling
> only in the legacy no-argument `beginFrame()` wrapper; explicit
> `beginFrame(WindowId)` must never poll globally. For Phase 4, serialize begun
> window frame triplets with a clearly marked `//Transitional:` app-thread gate
> that Phase 5 will replace. Move shared image/icon maintenance to the quiescent
> app polling safe point while keeping viewport state per window.
>
> Make secondary creation strongly transactional: create its backend/input,
> surface, verify the existing present queue supports it, register a translated
> storage window, create its swapchain/frame slots/viewport state, and initialize
> its renderer before publishing the stable registry entry. Roll back every
> partial failure without changing existing windows. Do not recreate the device
> for an unsupported surface and do not expose storage configuration.
>
> Refactor `VulkanUiRenderer` to use the existing storage-backed renderer layout,
> pipeline bundle, and window descriptor bundle APIs. Share immutable layouts and
> compatible-format pipelines across windows, keep descriptors and instance
> buffers window/frame-local, track the renderer generations in each storage
> frame, and make format switching transactional. Preserve the Phase 3 logical
> texture, dirty-write, fallback, direct-emission, seal/submission/completion,
> cancellation, and collection lifecycle and do not change the shaders.
>
> Remove device-wide idle from window resize and destruction. Add move-only,
> transactional swapchain generations using `oldSwapchain`, with image-specific
> synchronization owned by each generation. Probe and enable exact per-present
> completion through `VK_EXT_swapchain_maintenance1` present fences or
> `VK_KHR_present_id` plus `VK_KHR_present_wait`; if neither is available, keep
> the legacy main window functional but fail public secondary creation clearly.
> A main-only device-idle resize fallback may remain only on that unsupported
> capability path and must be unreachable after any secondary window can be
> created. Drain only the target window's frame fences/tokens and retire its old
> WSI objects after exact present completion. Apart from that explicit
> compatibility case, keep `vkDeviceWaitIdle()` only for final whole-app
> shutdown/device loss.
>
> Remove `ViewPortManager`'s internal device-idle destruction/removal behavior by
> using a drained-window teardown and exact deferred window retirement. Preserve
> the `//Transitional:` borrowed-native manager publication bridge; do not migrate
> image, icon, viewport, or font allocations/uploads in this phase.
>
> Add focused compatibility, ID/lifecycle, two-window input/UI/frame isolation,
> creation rollback, present-support failure, shared renderer, logical texture,
> viewport scope, different frame-rate/slot, minimized, resize, in-flight close,
> exact WSI retirement, and no-device-idle tests. Run release and development
> builds, storage/types/renderer/manager/Vulkan tests, two-window Vulkan
> validation and sanitizers when available, and `git diff --check`. Finish with
> `phase4.md` describing changes, deviations, validation, transitional limits,
> and the Phase 5 parallel-window-job outline.
