# Phase 2 implementation report

## Result

Phase 2 is implemented while preserving the existing single-window public `App` behavior. `VulkanUiRenderer` no longer owns or calls VMA allocations, renderer conversion is performed before the storage frame is sealed, and command recording consumes immutable prepared frame data afterward.

## Changes made

### Storage interop and upload path

- Corrected the `imageRegionlock` typo in the image upload barrier.
- Added generation-checked `nativeImageView()` and `nativeSampler()` cold queries.
- Documented their native values as borrowed identities whose lifetime is tied to a retained storage handle.
- Incremented the internal storage interface version to 3.
- Added stale-generation and retiring-handle coverage for both queries.

### Storage-owned renderer resources

- Added one app-shared `SharedUiByteResources` containing:
  - the immutable four-vertex quad buffer;
  - separate font-array and UI placeholder images/views;
  - the shared linear sampler;
  - cached borrowed Vulkan handles for descriptor/recording interop.
- Quad and placeholder bytes are uploaded through storage blobs and synchronous storage uploads. Placeholder texels are deterministically transparent.
- Shared resources are created once after storage initialization and released after all window renderers have stopped using them.
- Each main-window Vulkan frame slot now owns a `ResourceSharing::FrameLocal`, host-visible, persistently mapped storage instance buffer.
- The internal storage configuration preserves the previous 1 MiB initial capacity per frame slot.

### Renderer preparation and recording

- Replaced monolithic `render()` with:
  - `prepareFrame()`, called before `sealFrame()`;
  - `recordPreparedFrame()`, called after sealing during Vulkan command recording.
- Moved viewport render-command remapping into pre-seal frame preparation so renderer-local texture IDs are finalized before conversion.
- `PreparedUiFrame` retains only an arena-backed run span, instance count, and frame epoch. `drawFrame()` verifies that the epoch matches the sealed storage lease.
- The existing submission, exact-slot fence completion, cancellation guard, swapchain recreation, and shutdown token-draining behavior remains in place.

### Direct conversion and growth

- Removed renderer instance/run scratch vectors, the per-text glyph vector, and the scissor vector.
- Added an overflow-checked capacity pass based on current Clay command behavior.
- Runs and scissor state are allocated from the storage frame arena.
- `UiInstance` values are emitted directly into one `DirectMapped` buffer write and committed once; storage performs non-coherent flushing and generation-safe frame-use tracking.
- Text layout now emits glyph instances through its callback without an intermediate glyph collection. Sorted text-color override ranges use a forward cursor.
- Development diagnostic counts are accumulated during conversion rather than rescanning instance output.
- Instance-buffer growth uses checked 1.5x geometric growth. A replacement storage generation is created and native-validated before the descriptor and live frame-slot record are changed. Failure releases the replacement and preserves the previous record.
- Shared quad/placeholder/view/sampler handles are tracked before sealing for frames that contain a UI pass.
- Added ABI/layout assertions for the 88-byte trivially copyable `UiInstance` shader record.

### Transitional Phase 3 boundary

The current `TextureRef.id`, `UiTextureRegistry`, manager raw Vulkan bindings, descriptor slot arrays, and raw pipeline/descriptor control objects remain intentionally unchanged. Their boundaries are marked with `//Transitional:` comments. Phase 3 will replace renderer-local integer slots with logical generational texture handles and per-window storage binding batches.

## Tests added

- Cold native image-view and sampler query tests, including stale and retiring generations.
- Vulkan lifecycle test that creates, uploads, native-resolves, releases, and collects the shared UI byte resources.
- Bounded direct-conversion tests covering rectangles, borders, input overrides, scaling, scissor/run ordering, text fallback, zero-sized images, unsupported commands, undersized output, and malformed input.
- Checked instance-capacity growth and overflow tests.
- Source regression test ensuring `Vk_UiRenderer.cpp` contains no `vmaCreate`, `vmaDestroy`, or `VmaAllocation` ownership.

## Validation

Release configuration (`build-diagnostics-release-check`):

- complete build: passed;
- `flowui.storage.types`: passed;
- `flowui.ui.renderer_conversion`: passed;
- `flowui.storage.system`: passed on the available Vulkan device;
- all configured tests: 3/3 passed.

Development configuration (`build-tempclay-dev`):

- complete build, including `flowui_font_baker`: passed;
- `flowui.storage.types`: passed;
- `flowui.ui.renderer_conversion`: passed;
- `flowui.storage.system`: passed on the available Vulkan device;
- all configured tests: 3/3 passed.

`git diff --check` also passes.

## Remaining limitations

- `FontManager`, `ImageManager`, `IconManager`, and `ViewPortManager` still own their existing VMA resources. Phase 2 only removes renderer byte-memory ownership.
- Logical texture identity is not yet adopted; texture commands still contain renderer-local descriptor slots.
- Descriptor arrays and pipeline/descriptor control objects remain renderer/window state and can allocate on initialization or explicit cold-path texture-capacity growth.
- The library still exposes only the semantic main window publicly.
- Swapchain recreation still uses the Phase 1 transitional device-wide idle path.
- Frame-arena capacity may itself grow through storage when unusually large frames exceed its configured reserve; the renderer conversion path no longer creates per-command, per-glyph, instance, run, or scissor heap containers.

## Phase 3 outline

Phase 3 should migrate texture identity and descriptor binding without expanding public multi-window creation yet:

1. Change internal UI texture references from renderer slot IDs to full generational `TextureHandle` values, with a source-compatible public migration strategy if required.
2. Gather unique logical textures once during pre-seal preparation using frame-arena storage.
3. Call `prepareTextureBindings()` once per window/frame and grow descriptor capacity transactionally when required.
4. Apply only dirty descriptor records for the current frame slot and acknowledge exactly those writes.
5. Resolve texture handles from the sealed per-window binding view during direct instance emission.
6. Remove `UiTextureRegistry`, renderer-owned slot allocation/retirement, descriptor-wide rebuilds, and device-idle slot growth.
7. Keep temporary adapters only at manager stores until those managers publish storage textures directly.

Phase 3 is complete when a logical texture can resolve to independent descriptor indices in different internal windows and no Clay render command stores a renderer-local slot.
