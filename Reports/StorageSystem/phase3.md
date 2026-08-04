# Phase 3 implementation report

## Result

Phase 3 is implemented while preserving the existing single-window `App`
execution model. UI commands now carry app-level, generation-checked
`FlowUi::TextureHandle` values. Storage resolves them into descriptor indices for
the current `AppWindow` and Vulkan frame slot before direct instance emission.

`UiTextureRegistry`, renderer-local texture slot allocation, slot retirement,
full-array descriptor rewrites, runtime descriptor-layout growth, and the
texture-capacity device-idle path were removed. No public secondary-window
creation API was added.

## Changes made

### Public texture identity and Clay payloads

- Added the lightweight public `FlowUi/TextureHandle.hpp` with zero invalid,
  index/generation identity, packing, comparison, and stale-generation safety.
- The internal storage `TextureHandle` now uses the same canonical public type.
- Changed `TextureRef::id` to `TextureRef::handle`; UV, fit, tint, sampling, and
  source-size fields remain per-draw metadata.
- Added `UiManager::imageData(TextureRef)` as the raw-Clay payload-copy API.
- Retained `storeTexture()` only as a deprecated `//Transitional:` alias.
- Resource lifetime and Clay pointer lifetime remain explicit and separate:
  storage retains resolved textures through submission, while `imageData()` keeps
  the per-command `TextureRef` bytes alive until Clay rendering is prepared.

### Storage binding protocol

- Bumped the internal storage interface to version 4.
- Published the Phase 2 placeholder UI image/view/sampler as the ready app-shared
  storage fallback bound at descriptor index zero.
- Extended `PreparedTextureBindings` with a pre-seal, epoch-checked dense binding
  span compatible with Phase 2 direct mapped emission.
- Added a complete capacity preflight before descriptor assignment. An exhausted
  batch throws without changing existing descriptor indices or revisions.
- Added a transitional borrowed-native texture backing for managers that still own
  VMA resources. It records scope, dimensions, native view/sampler identities, and
  participates in normal frame-use/submission retirement.
- Added `textureRetirementComplete()` so manager resources are destroyed only
  after the retiring logical generation has completed and been collected. This is
  distinct from `validateHandle()`, which intentionally rejects retiring handles
  for new work.
- External replacement creates a new logical generation transactionally before
  retiring the previous published generation.

### Per-window descriptor binding

- Added one internal effective UI texture capacity of 256 to `AppWindowConfig`.
  The same value configures storage registration, renderer descriptor layouts,
  and the existing shader array.
- Renderer initialization validates the required font-plus-UI sampled-image count
  against Vulkan core and update-after-bind descriptor-indexing limits.
- `App::Impl::endFrame()` now:
  1. resolves icon and viewport request handles;
  2. gathers unique final logical handles in the storage frame arena;
  3. calls `prepareTextureBindings()` once;
  4. applies only dirty writes to the current safe frame-slot descriptor set;
  5. acknowledges only the successfully applied writes;
  6. emits instances from the prepared binding span;
  7. seals the frame afterward.
- The dirty Vulkan write batch uses fixed stack arrays bounded by the authoritative
  capacity, so it performs no heap allocation and cannot exceed the shader layout.
- Direct conversion performs a checked handle-index/generation span lookup and
  writes only the resolved descriptor index to `UiInstance::texIndex`. There are
  no storage calls, locks, strings, or maps in the image conversion loop.
- Existing exact-slot fence completion, submission tokens, cancellation guard,
  sealing, and collection behavior is unchanged.

### Current texture producers

- `ImageManager` publishes one app-shared logical generation per image resource.
  Replace/remove queues the old VMA resource until storage reports exact logical
  generation retirement.
- `IconManager` publishes fallback-backed request handles and app-shared atlas-page
  handles. Pre-seal preparation resolves requests to a page handle plus UVs and
  source dimensions. All variants on a page share one descriptor binding.
- `ViewPortManager` publishes one frame-local handle per Vulkan target. Pre-seal
  sizing now transactionally creates and publishes resized targets, then remaps
  commands to the exact current frame-slot handle. Old target images retire by
  logical generation completion rather than frame-bucket guessing.
- The font atlas remains a separate app-shared MSDF `sampler2DArray`; each window
  descriptor set tracks its font revision independently.
- `TextureSamplingMode` remains a documented `//Transitional:` no-op pending a
  logical sampler-variant or separate sampler-index policy.

## Deliberate deviations and transitional boundaries

- The original roadmap suggested resolving a sealed binding view during instance
  emission. Phase 2 moved direct emission before sealing, so Phase 3 returns a
  stable pre-seal binding span and verifies its frame epoch instead.
- Descriptor capacity is fixed at 256 rather than grown transactionally at runtime.
  This removes layout/pipeline recreation and device-idle growth. Supporting a
  larger configurable capacity later requires a synchronized shader/layout policy
  or descriptor-bundle generations.
- Manager images/views/samplers and uploads remain VMA-owned. The internal
  `IUiTexturePublisher`/`ExternalTextureDesc` bridge is marked `//Transitional:`
  and never destroys borrowed Vulkan objects itself.
- Raw renderer pipeline/layout and descriptor control objects remain in each
  `VulkanUiRenderer`. Phase 4 can share/adopt immutable pipeline bundles by target
  format while retaining per-window/per-frame descriptor sets.
- Raw Clay still requires pointer-shaped `imageData`; the deprecated alias can be
  removed after a by-value FlowUi image element hides payload staging completely.
- Viewport removal and application shutdown still use their existing deliberate
  device-idle paths for command-pool/whole-app teardown. Texture descriptor growth
  no longer uses device idle.

## Tests added or strengthened

- canonical public/internal handle identity and packing;
- pre-seal binding lookup and generation mismatch fallback;
- transactional capacity failure preserving an existing descriptor assignment;
- one logical texture resolving to descriptor 2 in one internal window and
  descriptor 1 in another;
- dirty-write acknowledgement per frame slot;
- borrowed external replacement staying allocated until both submitted windows
  complete, then becoming retirement-complete after collection;
- cross-window rejection of a frame-local viewport-style texture;
- direct renderer conversion from a logical handle to a prepared descriptor index,
  including stale-generation fallback to index zero;
- source regression checks for removed registry/slot/growth/full-table paths and
  continued absence of renderer VMA ownership.

## Validation

Release configuration (`build-diagnostics-release-check`):

- full library and test build: passed;
- `flowui.storage.types`: passed;
- `flowui.ui.renderer_conversion`: passed;
- `flowui.storage.system`: passed, including 15 Vulkan scenarios;
- complete CTest: 3/3 passed.

Development configuration (`build-tempclay-dev`):

- full library, tests, and `flowui_font_baker` build: passed;
- `flowui.storage.types`: passed;
- `flowui.ui.renderer_conversion`: passed;
- `flowui.storage.system`: passed, including 15 Vulkan scenarios;
- complete CTest: 3/3 passed.

The Vulkan suite ran on llvmpipe. The Khronos validation layer is not installed in
the environment, so validation-layer coverage was unavailable and is not claimed.
`git diff --check` passes.

## Phase 4 outline

Phase 4 should expose controlled multi-window creation and destruction after the
internal model is already correct:

1. add internal then public `WindowId`-based create/destroy and explicit-window
   frame/UI/query APIs while keeping no-argument methods routed to `MainWindowId`;
2. create and register one surface, swapchain, `FrameVk`, descriptor state,
   `UiManager`, viewport manager, and storage scope per window;
3. verify device presentation support for every new surface;
4. share immutable renderer layout/pipeline bundles by compatible target format;
5. remove device-wide idle from one-window resize/close and drain only that
   window's exact submissions and borrowed resources;
6. advance shared manager publication at an app-level safe point while windows can
   render at different rates;
7. test independent resize, close, skipped/blocked frames, descriptor indices,
   viewport scope, and out-of-order completion across multiple real windows.

Manager VMA-store migration and asynchronous uploads remain separate later storage
phases; they should remove the borrowed-native publisher without changing public
logical texture identity.
