# Phase 5: manager storage migration

## Status

Phase 5 migrates the durable state and GPU-resource ownership of FlowUi's public
managers to the StorageSystem architecture described in
`managersStorage_Implementation.md`. The public managers remain recognizable,
small facades. App-shared resources are physically shared, window-local state is
registered under its owning `WindowId`, frame consumers use captured storage
views, and the transitional borrowed-native texture publication path is gone.

This phase intentionally does not introduce worker threads. It establishes the
publication revisions, immutable frame views, window ownership boundaries, and
exact retirement rules that the next executor phase will consume.

## Public resource identity

`FlowUi::ResourceKey` is now the uniform public logical key for every keyed
manager except `ShortcutManager`, whose chord and monotonic `ShortcutId` remain
the semantically correct identity.

```cpp
struct ResourceKey {
    std::string_view name;
    ResourceDomain domain = ResourceDomain::Auto;
    WindowId window = InvalidWindowId;
};
```

The normalization rules are centralized under `internal/ManagerStorage`:

- `ResourceDomain::Auto` resolves to the receiving manager's domain.
- `ImageManager`, `IconManager`, and `FontManager` are app-shared. Any supplied
  window is ignored and the internal key uses `InvalidWindowId` (`0`).
- `UiManager`, `InputFieldManager`, and `ViewPortManager` infer their owning
  window when the public key uses window `0`.
- An explicit different window on a window-local manager is rejected.
- An explicit domain that does not match the receiving manager is rejected.
- Names are exact byte strings and are interned before the call returns.

Consequently, the familiar concise form works:

```cpp
auto background = app.images().getTexture({.name = "background"});
auto panel = app.viewPorts(window).getTexture({.name = "panel"});
```

Legacy string overloads remain and delegate to `ResourceKey`. ResourceKey
overloads were added to image, icon, font, UI, input-field, and viewport method
families. No storage handle, descriptor identity, allocator, or storage
configuration is exposed publicly.

## Stage 1: common manager storage foundation

Storage interface version 5 adds destructor-aware manager records. A manager
record has a generational handle, a structured interned key, a resource kind,
non-relocating persistent backing storage, and an explicit destructor invoked
by StorageSystem on the app thread. Construction is unpublished until it
succeeds.

The implementation adds:

- manager record create/find/data/remove operations;
- complete release of a window's manager-record scope;
- app-shared and per-window manager publication revisions;
- revision capture in `FrameToken` and `ManagerFrameView`;
- revision refresh at `sealFrame()` so legal same-frame inline mutations are
  included in the sealed view;
- diagnostic once-sets keyed by domain/name/window;
- deterministic manager transaction failure checkpoints;
- anonymous logical textures with generational handles and exact retirement;
- `ResourceKind` values for manager roots and manager-owned domains.

The internal `ManagerStateAccess` helper constructs typed state/controllers in
these records and validates the expected record kind on access. Facades cache a
non-relocating state pointer, so normal per-frame calls do not perform repeated
virtual storage lookups.

Manager mutation is currently legal only while no sealed frame is outstanding.
That matches the Phase 4 serialized app-thread frame gate. It is a publication
contract, not a mutex-based promise of concurrent mutation.

## Stage 2: ImageManager

`ImageManager` is now a thin app-shared facade over generic StorageSystem
resources:

- stb decoding is temporary;
- decoded pixels are copied to a storage blob;
- image, image view, and normalized sampler are StorageSystem resources;
- upload goes through the central upload queue;
- logical publication uses storage-owned texture views;
- replacement is published only after decode, resource creation, and upload
  complete;
- removal delegates logical invalidation and exact physical retirement to
  StorageSystem;
- missing-key warning suppression uses the storage diagnostic once-set.

The old Vulkan context pointer, VMA image ownership, upload command pool,
publisher link, and private retired-resource list were removed. The logical
texture record already contains the compact dependency graph and metadata, so
the implementation does not add a redundant second `ImageAssetRecord` wrapper.

## Stage 3: UI, input fields, and shortcuts

### UiManager

Each window has one storage-owned `UiManagerState`. It owns the persistent Clay
arena/context, interaction generations, window UI state, developer state, and
the exact font view captured for the frame. Clay strings and stored
`TextureRef` values come from the StorageSystem frame arena associated with the
active `FrameToken`.

The two interaction generations rotate by swapping their storage rather than
copying and reallocating it. `UiManager` itself retains only facade plumbing and
the platform clipboard/cursor callbacks. Input and shortcut facades bind to the
same storage root and window scope.

### InputFieldManager

The complete persistent editing model now lives in a window-scoped
`InputFieldManagerState`: field text, carets, focus, drag state, repeat state,
configuration, touch epochs, frame overrides, and the captured font view.
Per-field presence uses an epoch instead of clearing every cached field each
frame. The manager retains UTF-8 editing, hit testing, selection, caret, and
repeat policy.

Direct `FontManager*` borrowing was removed. Measurement receives the exact
`FontFrameView` captured by the owning UI frame.

### ShortcutManager

Shortcut registrations, callback captures, chord indexes, ID indexes, focus,
and monotonic counters now live in a window-scoped storage record. IDs and
registration order are never reset by `clear()`, preventing ABA reuse.

Chord buckets are immutable `shared_ptr<const ShortcutBucket>` publications.
Dispatch retains one snapshot pointer rather than copying a vector of
`std::function` values. Unregistration tombstones a callback immediately, so a
later callback in the same dispatch is skipped, while structural changes are
published for later dispatch. Registered-key traversal uses a fixed keyboard
array instead of allocating a key list per frame. Callback captures are
destroyed on the app thread.

## Stage 4: fonts and all font consumers

The private `FontCatalogController` is a storage-owned app-shared manager
record. It owns font policy and parsing, while StorageSystem owns atlas images,
views, samplers, pixel blobs, uploads, and generation retirement. The public
`FontManager` contains only the storage/controller binding and its recognizable
API.

Implemented behavior includes:

- stable monotonic `FontId` and `FontFamilyId` publication;
- non-relocating deque storage for public `FontFaceData*` compatibility;
- immutable `FontFrameView` values with captured face/family counts;
- exact atlas image/view/sampler handles and native binding generation in each
  frame view;
- transactional face parsing and atlas upload;
- rollback of family, face, name, layer-visibility, and ID state when a
  multi-face family fails;
- one shared publication revision for a successful multi-face family;
- central upload scheduling and transactional atlas growth;
- no font-owned Vulkan command pool and no device-idle operation.

`UiManager`, `InputFieldManager`, `TextLayoutEngine`, and `VulkanUiRenderer` no
longer retain or query `FontManager`. They consume the same captured font view.
The renderer tracks the exact atlas storage generation in the frame before
sealing and updates descriptors only when its binding generation changes.

The public borrowed `getAtlasResource()` compatibility query remains. Internal
frame/render code does not use it.

## Stage 5: icons

The private `IconCacheController` is an app-shared storage-owned manager record.
It retains SVG parsing, size-tolerance selection, rectangle packing, free-list
merging, LRU policy, and demand resolution. StorageSystem owns source blobs,
atlas images/views/sampler, uploads, logical request aliases, variant textures,
and the exact logical-texture lifetime.

Request aliases remain stable keyed logical textures. Size-specific atlas
variants use anonymous logical textures. Eviction and removal invalidate a
variant immediately but do not return its atlas rectangle to the allocator
until `textureRetirementComplete()` proves that every submitted reference to
that generation has completed. Page uploads use the central upload scheduler.

App-wide icon maintenance runs once from the quiescent app polling safe point.
The Phase 4 frame gate serializes current two-window demand processing, so
same-tick results are deterministic without adding premature worker locks.

The icon-owned Vulkan/VMA image path, sampler, upload command pool, native
publication bridge, and direct resource destruction were removed.

## Stage 6: ViewPortManager

Each window has a storage-owned `ViewportStorageController` record. Public
`ViewPort` objects are non-owning facades over non-relocating internal state.
The controller provides the focused viewport capability described by the plan:
complete target creation, candidate disposal, atomic generation replacement,
frame acquisition, use tracking, exact retirement, and drained teardown.

A target generation contains every frame slot's StorageSystem image/view,
anonymous logical texture, native frame view, and exclusive Vulkan command
resources. Creation and resize first build a complete unpublished generation.
Only a complete candidate becomes active. Failure preserves the previous
generation and removes every partial candidate resource.

Resize and removal release the old logical generations and wait for exact
texture-generation completion before destroying their command resources or
reusing physical targets. They do not call `vkDeviceWaitIdle()` and do not drain
another window. Window destruction calls `destroyDrained()` only after the
existing Phase 4 target-window fence/token/present drain.

`ViewPortVulkanInterop` remains the explicitly borrowed device-identity surface,
and the render callback receives only its active frame/generation view. It is
not an ownership or publication bridge.

## Stage 7: removed compatibility and ownership paths

The following are deleted:

- `IUiTexturePublisher` and `UiTexturePublisher`;
- `publishExternalTexture()` and `ExternalTextureDesc`;
- the `BorrowedNativeTextures` storage capability;
- every manager `setTexturePublisher()` function;
- manager VMA allocations and manager-owned images/samplers/upload pools;
- direct manager-to-manager font pointers;
- renderer `setFontManager()`;
- device-idle manager resize/removal/destruction behavior.

Source-guard tests reject reintroduction of these symbols or direct manager VMA
and device-idle calls.

The only `vkDeviceWaitIdle()` uses left in the application are unchanged Phase
4 policy: final whole-app shutdown/device-loss handling and the explicitly
documented main-only resize compatibility path when exact present completion is
unsupported. Public secondary creation is disabled on that capability path, so
the fallback is unreachable after a secondary window can be created.

## Renderer and frame integration

The Phase 4 storage-backed renderer layout, compatible-format pipeline bundle,
window descriptor bundle, logical texture lifecycle, and per-window instance
buffers remain intact. Manager migration adds exact font and manager revision
capture; it does not change shaders.

App polling remains the quiescent safe point for completion collection, shared
image/icon maintenance, and deferred controller collection. `beginFrame(id)`
captures shared/window manager revisions. UI and manager mutations complete
before `sealFrame()`. Rendering consumes sealed storage views and window-local
descriptor/instance state.

## Tests added or extended

Focused Phase 5 tests cover:

- the aggregate/trivially-copyable ResourceKey surface and Auto defaults;
- public ResourceKey overload compilation;
- domain/window normalization and two-window viewport isolation;
- app-shared font key inference;
- wrong-domain and wrong-window rejection;
- manager record construction/destruction and generational lookup;
- all manager-record failure checkpoints with revision and destructor rollback;
- complete window-scope record release;
- anonymous logical texture submission and exact retirement;
- source guards for manager VMA ownership, device idle, borrowed publication,
  and renderer font-manager borrowing.

The Phase 4 two-window integration test continues to exercise different frame
slots/rates, minimized and resized windows, close queries, in-flight secondary
destruction, non-reusable window IDs, remaining-main-window operation, and
Vulkan validation requests. Existing storage logical-texture,
seal/submission/completion/cancellation/collection and renderer-conversion tests
continue to pass.

## Validation performed

All commands completed successfully with six of six tests passing:

- development build and CTest;
- Release build and CTest;
- `FLOW_UI_DEV_MODE=ON` build and CTest;
- compatibility build with `FLOWUI_INCLUDE_ICON_MANAGER=OFF` and
  `FLOWUI_PUBLIC_VULKAN_INTEROP=OFF`;
- AddressSanitizer plus UndefinedBehaviorSanitizer, with leak detection and
  halt-on-error enabled;
- real two-window Vulkan integration with validation requested when installed;
- storage/types, renderer conversion, Phase 4 API, Phase 5 manager, two-window,
  and Vulkan StorageSystem test targets;
- `git diff --check`;
- source audits for manager VMA/device-idle/native-publication ownership.

ThreadSanitizer was not run because Phase 5 deliberately retains the inline
executor and has no worker execution path to exercise. The shader build used
`glslangValidator` because `glslc` was unavailable.

## Current limitations and deviations from the idealized plan

The ownership migration and bridge removal are complete, but several tuning
items from the plan are intentionally not misrepresented as finished:

- Manager roots/controllers are allocated, scoped, destructed, and counted by
  StorageSystem, but nested capacities owned by standard containers and opaque
  third-party parser/font libraries are not yet individually attributed in
  `StorageStats`. Moving those containers to tagged PMR/flat storage is the
  remaining memory-accounting pass.
- Font glyph/codepoint and kerning lookup retain the existing baked
  `FontFaceData` representation. They were not converted to a benchmark-selected
  flat/sorted format in this phase, avoiding an asset-format and shader-adjacent
  semantic change without measurements.
- Some input-field selection/hit-test scratch and icon upload packing scratch
  still use temporary standard vectors. Durable state is storage-owned, but a
  dedicated allocation-count benchmark is still needed before claiming a
  zero-general-heap steady state.
- Icon retired-region bookkeeping and viewport retired-generation bookkeeping
  live inside their StorageSystem-owned typed controllers. Exact reuse and
  destruction are nevertheless decided by StorageSystem texture-generation
  completion. They can become generic retirement payload callbacks later if
  telemetry shows a benefit.
- The focused viewport operation group is a typed controller capability stored
  behind the single storage root rather than additional virtual methods on
  `IStorageSystem`. This keeps the generic interface small while preserving the
  transaction and ownership semantics required by the plan.
- Failed font transactions fully restore published IDs, mappings, visible
  faces, and layer counts. A successfully allocated larger atlas capacity may
  be retained as reusable reserve after a later publication failure; no partial
  family or visible face is published.

These limitations do not require manager data to move back out of StorageSystem
and do not restore any native publication bridge.

## What remains after Phase 5

The next conceptual phase is the configurable executor and measurement pass:

1. Add `SingleThreaded` and `InternalWorkers` execution policies over the same
   frame/publication interfaces. Keep single-threaded execution as the
   deterministic reference implementation.
2. Resolve Clay's process-current-context restriction by making context
   selection thread-local or keeping Clay layout serialized while parallelizing
   safe conversion/recording stages.
3. Replace the `//Transitional: app-thread gate` with per-window job ownership,
   app-resource publication, explicit join/cancel points, and externally
   synchronized queue submission.
4. Use per-window icon demand/use lists and merge them at the app publication
   lane once window jobs can actually overlap.
5. Add tagged PMR/flat container storage, third-party opaque-allocation
   telemetry, manager-domain live/retired byte reporting, and cache/churn
   counters.
6. Add allocation-count and main-branch comparison benchmarks for image lookup,
   font layout, cached icons, input editing, shortcut dispatch, unchanged
   viewports, and two-window frame preparation; tune reservations from the
   measurements.
7. Add ThreadSanitizer and adversarial worker mutation/cancellation tests when
   the worker executor exists.

The public manager APIs and ResourceKey rules need not change for this next
phase. The remaining Phase 4 serialization marker is the explicit transition
point; manager ownership, snapshots, resource generations, and window scopes
are already positioned for either inline or worker-backed execution.
