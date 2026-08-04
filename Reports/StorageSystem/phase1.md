# Phase 1 AppWindow implementation report

## Result

Phase 1 is implemented while preserving the existing single-window application API. The initial native window is now represented internally by a stable `AppWindow` registry entry, has the canonical ID `MainWindowId == 1`, owns its window-specific Vulkan/UI state, and participates in the `FlowStorageSystem` window/frame/submission lifecycle.

No public secondary-window creation API was added. Existing calls such as `beginFrame()`, `ui()`, `drawFrame()`, window queries, clipboard operations, and viewport access explicitly route through the main-window ID.

## Changes made

### Canonical window identity

- Added the lightweight public `FlowUi/WindowId.hpp` header.
- Defined `WindowId`, `InvalidWindowId == 0`, and `MainWindowId == 1` once and reused that type from storage.
- Added `App::mainWindowId()` as an additive read-only query.
- Added a unit check proving storage and the public API use the same ID type and constants.

### `AppWindow` and `App::Impl`

- Added an internal `AppWindowConfig` snapshot translated from the existing source-compatible `AppConfig`.
- Added a stable `unordered_map<WindowId, unique_ptr<AppWindow>>` registry to `App::Impl`.
- Created only the semantic main window in Phase 1.
- Moved the backend, input queue, surface, swapchain, `FrameVk`, `UiManager`, `ViewPortManager`, legacy renderer/texture registry, render commands, swapchain layouts, timing, scaling, resize state, and active storage frame/lease into `AppWindow`.
- Kept Vulkan device/VMA state, storage ownership, ID allocation, fonts, images, and icons on `App::Impl`.
- Made `App::Impl` cleanup idempotent and destructor-owned, so move assignment and failed initialization also clean up the old/partial implementation.

### Vulkan and platform boundaries

- Removed the single `VkSurfaceKHR` field from `VulkanContext`.
- `VulkanContext::createSurface()` now returns a surface for `AppWindow` to own.
- Physical-device selection receives the main surface explicitly, and a present-support query seam was added for future windows.
- Swapchain creation/recreation receives an explicit window config, Vulkan config, and surface.
- `FrameVk::create()` now receives the effective per-window frame count directly.
- Global GLFW event polling is separate from per-window input snapshot refresh.

### Storage lifecycle integration

- `App` creates one `FlowStorageSystem` after device/VMA creation and registers the main window scope.
- Storage and Vulkan use the same frame-slot index.
- `beginFrame()` waits for the slot fence, completes its prior `SubmissionToken`, collects retired storage, then begins a new storage frame epoch.
- `endFrame()` seals the storage frame after UI/resource preparation.
- A successful Vulkan queue submission is followed by `noteSubmission()`, and the returned token is stored on the exact `FrameVk::Frame` slot.
- Unsubmitted frames are cancelled on early returns and exceptions through an RAII cancellation guard.
- Device-idle swapchain recreation and shutdown drain all outstanding storage submission tokens before resource reset/destruction.
- Window scopes are unregistered and storage is shut down while the Vulkan device/VMA are still alive.

### Transitional Phase 1 code

Temporary Phase 1-only paths are marked with `//Transitional:` comments:

- the legacy renderer and descriptor-slot registry remain monolithic and window-owned;
- app-shared image/icon manager facades still bind through the main window's legacy renderer/registry;
- shared manager frame advancement still follows the main window cadence;
- swapchain recreation still uses device-wide idle.

These paths are intentionally retained only until the renderer/resource migrations in later phases.

## Validation

- Release changed-object compilation: passed.
- Development changed-object compilation: passed.
- Canonical window/storage identity unit test: passed.
- Clean full release build: passed with a command-line-only compatibility define described below.
- `flowui.storage.types`: passed.
- `flowui.storage.system`: passed on the available Vulkan device.
- `git diff --check`: passed.

The working tree already contained an unrelated edit changing `upload.request.imageRegion` to `upload.request.imageRegionlock` in `FlowStorageSystem.cpp`. It was deliberately not overwritten. A clean validation build used `-DimageRegionlock=imageRegion` only on the compiler command line, allowing the complete Phase 1 tree and tests to be verified without modifying that user-owned source change. A normal full build will remain blocked until that unrelated line is resolved.

## Phase 2 outline

Phase 2 should move renderer-owned byte/GPU memory behind the storage system without yet changing public texture identity:

1. Create per-window/per-frame instance buffers through `IStorageSystem::createBuffer()`.
2. Move placeholder images, image views, samplers, and the optional quad buffer into storage handles.
3. Replace renderer scratch vectors and per-text temporary allocations with frame/worker arenas or direct mapped writes.
4. Add generation-based transactional instance-buffer growth and serial-gated retirement.
5. Use the Phase 1 fence/submission bridge to call `noteCompleted()` and `collect()` for real renderer resources.
6. Split immutable renderer layout/pipeline objects from per-window descriptor/frame state where required for storage adoption.
7. Retain the current texture-slot behavior through a clearly marked adapter; logical `TextureHandle` commands and per-window descriptor resolution remain Phase 3.

Phase 2 is complete when the renderer owns no direct VMA buffer/image allocations, steady-state conversion avoids general heap allocation, and current single-window rendering still behaves identically through the temporary texture-slot adapter.
