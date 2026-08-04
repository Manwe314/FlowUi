# Phase 4: explicit multi-window API

## Result

Phase 4 adds explicit, application-owned secondary windows while preserving the
existing single-window API. `AppConfig::window` still configures the semantic
main window (`MainWindowId == 1`), and every no-argument window/frame accessor
continues to target it. Only the legacy no-argument `beginFrame()` polls platform
events implicitly; explicit multi-window loops call `pollEvents()` once per app
tick and then use the `WindowId` frame overloads.

The public API now provides transactional `createWindow()` and synchronous
`destroyWindow()`, `hasWindow()`, explicit close queries/mutation, explicit
frame/UI/viewport access, and `WindowId` overloads for every existing
window-local title, size, framebuffer, input, native-handle, raw-mouse, and
clipboard operation. IDs are monotonic and never reused. Explicit main-window
destruction and stale/invalid IDs are rejected.

```cpp
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

Do not combine explicit `pollEvents()` with the no-argument `beginFrame()` in
the same tick unless a second global platform poll is intentional.

## Runtime and lifecycle changes

Each `AppWindow` has an explicit `Idle -> Building -> Prepared -> Idle`
lifecycle plus `Closing`, independent timing, frame number, frame slots, input
queue, UI context, viewport manager, surface, active swapchain generation,
renderer facade, and last submission serial. A clearly marked transitional
app-thread gate allows one begun-but-unsubmitted window triplet at a time. It
rejects overlapping or out-of-order frame calls without changing another
window.

Secondary creation reserves a fresh ID, creates the backend and callback-routed
input queue, creates and probes the surface, verifies the selected present queue,
requires exact present completion support, registers a translated internal
storage scope, creates WSI/frame resources, adopts shared renderer resources,
initializes the window-local viewport/UI state, and only then publishes the
stable registry entry. Failure unwinds the partial window in reverse without
reselecting/recreating the device or changing an existing public window.

Close buttons only set the window close flag. `destroyWindow()` detaches native
callbacks, cancels that window's unsubmitted frame if necessary, drains only its
frame fences and storage tokens, waits its exact outstanding presentation,
releases its renderer/viewport/WSI resources, unregisters storage, destroys its
surface/backend, and finally erases the public identity.

Shared image retirement, storage collection, and icon cache tick advancement
now happen at the quiescent app polling safe point. Viewport frame-slot state
remains window-local. The borrowed-native image/icon/viewport publication bridge
and its transitional comments remain; manager VMA allocation/upload migration
is intentionally not part of Phase 4.

## Renderer storage adoption

`VulkanUiRenderer` now holds strong storage handles and cached native views for
one shared renderer layout, one compatible-format shared pipeline bundle, and
one window-local descriptor bundle. Layout and pipeline candidates are
published transactionally; duplicate candidates are destroyed when storage does
not accept ownership. Descriptor pools/sets and instance buffers remain
window/frame-local. Every prepared storage frame tracks the exact layout,
pipeline, descriptor generation, buffers, and logical textures that it uses.

Swapchain format changes first acquire or publish a compatible replacement
pipeline bundle, validate its native generation, atomically switch the facade,
and only then release the previous strong generation. The Phase 3 logical
texture preparation, dirty descriptor acknowledgement, fallback, direct
instance emission, seal/submission/completion/cancellation, and collection
lifecycle is unchanged. Shader source and interfaces were not changed.

## WSI retirement and device-idle policy

Swapchains and swapchain generations are move-only. Replacement uses
`oldSwapchain` and leaves the active generation intact on construction or
pipeline failure. Image layouts, image ownership fences, render-finished
semaphores, present fences/present IDs, and last-use serials belong to the
generation; command pools, image-available semaphores, graphics fences, and
storage submission tokens remain in window frame slots.

Device creation probes and prefers `VK_EXT_swapchain_maintenance1` present
fences. If unavailable it enables `VK_KHR_present_id` together with
`VK_KHR_present_wait`. The local Vulkan SDK predates the maintenance1 structure
declarations, so guarded declarations matching the published extension ABI are
provided while runtime support is still probed normally. If neither exact mode
is available, the legacy main window remains usable and secondary creation fails
with a clear capability error. Only that main-only compatibility resize path may
use `vkDeviceWaitIdle()`; it is guarded by a one-window/main-ID assertion.
Otherwise resize and close drain only the target window and wait exact present
completion. The other device-idle call is final whole-app shutdown. Storage's
own final shutdown idle remains its existing whole-storage shutdown behavior.

`ViewPortManager::remove()` now defers image and command-pool destruction until
the corresponding logical texture generations retire. Its drained teardown has
an explicit owner precondition and contains no device-wide idle.

## Tests and validation

Focused Phase 4 tests cover public overload compatibility, stable monotonic IDs,
invalid/stale/main-destruction behavior, lifecycle ordering and the transitional
gate, two real secondary windows, independent UI/backend/viewport state,
different render rates and frame slots, per-window input/configuration queries,
resize, minimize isolation, close flags, in-flight exact destruction, non-reused
IDs, renderer storage adoption, move-only/`oldSwapchain` source regressions, and
the absence of viewport device-idle teardown. Existing storage tests cover
multi-window progress, logical texture bindings/fallback/dirty writes,
window/frame resource scope, shared renderer generations, descriptor adoption,
submission completion, cancellation, and retirement.

Validation performed:

- Debug/development build (`FLOW_UI_DEV_MODE=ON`): build and all five tests pass.
- Release build: build and all five tests pass.
- Release compatibility build with public Vulkan interop and IconManager both
  disabled: build and all five tests pass.
- ASan+UBSan development build with leak detection and halt-on-error: build and
  all five tests pass.
- The Vulkan storage integration suite passes on the host device.
- The real two-secondary-window WSI suite passes against the host presentation
  system, including different cadence, resize, minimize, and in-flight close.
- The host does not install `VK_LAYER_KHRONOS_validation`; the requested
  validation-layer run was therefore unavailable. Runtime Vulkan debug-utils
  and all non-layer WSI checks still ran.
- Scoped `git diff --check` passes for every Phase 4 file. The whole-tree command
  continues to report trailing whitespace in the pre-existing user edit to
  `To-Do.txt`, which Phase 4 deliberately preserves.

The real-window test is configured to skip with code 77 only when a future host
lacks a usable presentation environment or exact retirement capability.

## Deviations and environmental limits

- The implementation supports both exact mechanisms requested by the design and
  prefers maintenance1. No product-scope behavior was intentionally omitted.
- Failure rollback is exercised directly for invalid creation and indirectly by
  the storage renderer publication/adoption failure tests. The current public
  backend/Vulkan objects do not expose test-only failure injection switches, so
  every individual native creation step is not independently injected.
- Vulkan validation-layer execution depends on `VK_LAYER_KHRONOS_validation`
  being installed. The real two-window run still executes when the layer is
  unavailable and reports that environmental limitation.

## Transitional limits and Phase 5 outline

Phase 4 remains a platform/app-thread API. Shared Vulkan queues are externally
synchronized by serialized calls, and only one window may own a Building or
Prepared frame at once. It does not add callback-managed windows, element
windows, drag-out behavior, overlapping sealed frames, or internal rendering
threads.

Phase 5 should replace the app-thread gate with one ownership epoch/job per
`AppWindow`. Each job will own UI build/conversion/recording state, while an
app-shared publish barrier will define when manager/storage mutations become
visible. Queue submission/presentation will remain explicitly serialized unless
dedicated queue ownership is introduced. The Phase 5 work must also define
cancellation and exception propagation across jobs, per-window diagnostic
aggregation, safe shared manager publication batches, and deterministic shutdown
of outstanding jobs before reusing any Phase 4 lifecycle or WSI generation.
