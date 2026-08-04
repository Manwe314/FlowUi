# Phase 2: Renderer Byte-Memory Migration Proposal

## Goal

Phase 2 should move the byte and GPU-memory resources owned directly by `VulkanUiRenderer` into `FlowStorageSystem`, while preserving the current single-window API and the current integer texture-slot behavior.

The practical completion criteria are:

- `VulkanUiRenderer` contains no `VmaAllocation`, `vmaCreate*`, or `vmaDestroy*` ownership;
- instance buffers, the quad buffer, placeholder images/views, and the placeholder sampler are storage resources;
- Clay-to-instance conversion performs no general heap allocation after initialization;
- instance-buffer replacement is transactional and old generations retire through storage submission serials;
- the Phase 1 frame/fence bridge remains the only authority for arena reuse and GPU retirement;
- rendering and public single-window behavior remain unchanged.

This is deliberately narrower than migrating every VMA user in the library. `FontManager`, `ImageManager`, `IconManager`, and `ViewPortManager` still own resources directly today. Migrating those stores belongs to later manager/logical-texture work, not this renderer-byte-memory phase.

## Current-code findings that affect the design

The current tree already has the necessary Phase 1 ownership and lifecycle foundation:

- `App::Impl` owns the device/VMA context, one `FlowStorageSystem`, and shared managers.
- `AppWindow` owns `VulkanUiRenderer`, its swapchain/frame objects, the Clay command array, and the active storage frame/lease.
- `beginFrame()` waits for the exact Vulkan frame-slot fence, completes that slot's storage submission, calls `collect()`, and then starts the matching storage frame slot.
- successful queue submission stores `noteSubmission()` on the corresponding `FrameVk::Frame`.
- early returns and exceptions cancel unsubmitted storage frames.

The renderer still directly owns:

- one persistently mapped VMA instance buffer per Vulkan frame slot;
- a mapped four-vertex quad buffer;
- two VMA placeholder images and their Vulkan image views;
- one raw Vulkan sampler;
- persistent CPU vectors for instances and runs;
- a fresh glyph vector for every text command and a vector-backed scissor stack.

Two current details make a simple allocator substitution incorrect:

1. `App::Impl::endFrame()` currently calls `sealFrame()` before `drawFrame()` calls `VulkanUiRenderer::render()`. Storage frame-arena allocation, `beginBufferWrite()`, `commitBufferWrite()`, and `trackUses()` are only valid before sealing. Renderer conversion and storage writes must therefore move into a preparation step before `sealFrame()`.
2. Storage can cold-query a `VkBuffer` with `nativeBuffer()` and an image with `nativeImage()`, but it cannot cold-query an `ImageViewHandle` or `SamplerHandle`. The legacy Vulkan descriptor adapter needs those native objects while descriptors are created or updated. A small typed interop addition is required.

There is also a current source issue that must be fixed before implementing or validating Phase 2: `FlowStorageSystem.cpp` refers to `upload.request.imageRegionlock.baseArrayLayer`. The upload path otherwise uses `imageRegion`; placeholder and quad initialization will exercise this path, so Phase 2 cannot rely on the previous command-line compatibility define.

## Proposed ownership after Phase 2

### App-shared byte resources

Add an internal `SharedUiByteResources` owned by `App::Impl` beside the shared managers and storage system:

```cpp
struct SharedUiByteResources {
    storage::BufferHandle quadBuffer{};
    storage::ImageHandle placeholderFontImage{};
    storage::ImageViewHandle placeholderFontView{};
    storage::ImageHandle placeholderUiImage{};
    storage::ImageViewHandle placeholderUiView{};
    storage::SamplerHandle linearSampler{};

    VkBuffer nativeQuadBuffer = VK_NULL_HANDLE;
    VkImageView nativePlaceholderFontView = VK_NULL_HANDLE;
    VkImageView nativePlaceholderUiView = VK_NULL_HANDLE;
    VkSampler nativeLinearSampler = VK_NULL_HANDLE;
};
```

These objects are immutable and device-compatible, so they should be created once with `ResourceSharing::AppShared`, not duplicated in each `AppWindow`. Strong storage handles remain the owners; cached Vulkan handles are borrowed interop values valid only while those strong handles remain alive.

The renderer still owns raw Vulkan pipelines, descriptor layouts/pools/sets, and their persistent descriptor bookkeeping. Those are control objects rather than VMA byte allocations. Moving them into renderer bundle records is useful later, but is not required to complete Phase 2 safely.

### Per-window, per-frame-slot resources

Replace `AllocatedBuffer` with storage-backed frame records:

```cpp
struct UiFrameResources {
    storage::BufferHandle instanceBuffer{};
    storage::NativeBufferView nativeBuffer{};
    uint64_t capacityBytes = 0;
    PreparedUiFrame prepared{};
};

struct PreparedUiFrame {
    std::span<const UiRun> runs{};
    uint32_t instanceCount = 0;
    storage::FrameEpoch epoch = 0;
};
```

Each instance buffer should use:

```cpp
storage::BufferDesc{
    .size = initialInstanceBytes,
    .usage = storage::BufferUsage::Storage,
    .memory = storage::MemoryPreference::HostVisible,
    .sharing = storage::ResourceSharing::FrameLocal,
    .access = storage::AccessMode::CpuWrite,
    .persistentlyMapped = true,
    .window = windowId,
    .frameSlot = frameSlot,
    .debugName = instanceBufferName,
};
```

`FrameLocal` is more precise than `WindowLocal`: the current code already has one buffer per exact frame slot and waits for that slot before reusing it.

The current renderer starts each slot at 1 MiB, while `StorageConfig::initialInstanceBytesPerFrame` defaults to 256 KiB and is presently unused. Phase 2 should wire the config field into renderer initialization but set the app's internal effective value to 1 MiB initially. Reducing it should be a later measured tuning decision, not an accidental behavior change.

## Required storage interop addition

Add cold-path, typed native queries to `IStorageSystem`, `FlowStorageSystem`, and the storage types:

```cpp
struct NativeImageViewInfo {
    uint64_t nativeImageView = 0;
    ImageHandle image{};
};

struct NativeSamplerInfo {
    uint64_t nativeSampler = 0;
};

virtual NativeImageViewInfo nativeImageView(ImageViewHandle) const noexcept = 0;
virtual NativeSamplerInfo nativeSampler(SamplerHandle) const noexcept = 0;
```

These functions must validate the full generation and return zero for invalid or retiring handles. Their documentation must say that the native object is borrowed and remains usable only while the caller retains the corresponding strong storage handle. They are initialization/descriptor-update operations and must not be called per Clay command or glyph.

The existing `NativeImageView` returned by `nativeImage(ImageHandle)` is misleading because its `nativeImageView` field is always zero. Phase 2 need not rename that API and create unrelated churn; the new explicit query removes the ambiguity for renderer use.

## Initialization and shutdown

The workable initialization order is:

1. create the Vulkan device and VMA allocator;
2. initialize `FlowStorageSystem`;
3. register the main window storage scope;
4. create the shared quad, placeholder images/views, and sampler through storage;
5. upload deterministic resource contents and synchronously `flushUploads()` while no frame is active;
6. create each main-window frame-slot instance buffer;
7. cache validated native handles and create the existing descriptor/pipeline objects;
8. initialize the existing managers and temporary renderer/texture-registry adapter as today.

Keep the four-vertex quad and existing shaders in this phase. Removing the vertex buffer through `gl_VertexIndex` is possible, but combines a shader/pipeline change with the ownership migration for little immediate benefit.

The quad should be a device-local `Vertex | TransferDestination` app-shared buffer populated through a storage blob/upload request. The two one-pixel images should be device-local `Sampled | TransferDestination` resources, with the existing 2D-array font view and 2D UI view topology. Unlike the current code, which only transitions uninitialized images, upload a defined transparent texel. This makes invalid/fallback sampling deterministic and invisible; it should be covered by a visual/validation test.

Shutdown should first ensure the device is idle and drain all window frame tokens as Phase 1 already requires. Then:

- destroy renderer Vulkan control objects;
- release every per-slot instance `BufferHandle`;
- release shared image views before their backing images;
- release the sampler and quad buffer;
- call `collect()` after the completion state is current;
- shut down storage only after no renderer handle or borrowed native value can be used.

No storage-owned object should be passed to `vmaDestroy*` or `vkDestroyImageView`/`vkDestroySampler` by the renderer.

## Split renderer preparation from recording

Replace the monolithic `render()` with two operations:

```cpp
PreparedUiFrame prepareFrame(
    storage::IStorageSystem& storage,
    const storage::FrameToken& frame,
    uint32_t frameSlot,
    const Clay_RenderCommandArray& commands,
    const InputFieldFrameOverrides& overrides,
    VkExtent2D extent,
    float scaleX,
    float scaleY);

void recordPreparedFrame(
    VkCommandBuffer commandBuffer,
    VkExtent2D extent,
    VkImageView targetView,
    uint32_t frameSlot,
    const PreparedUiFrame& prepared);
```

`App::Impl::endFrame()` should become:

```cpp
window.renderCommands = window.ui.endFrame();
window.viewPorts.prepareFrameTargets(...);
icons.prepareFrameTextures(...);

window.preparedUi = window.renderer.prepareFrame(
    *storageSystem,
    window.storageFrame,
    window.storageFrame.frameSlot,
    window.renderCommands,
    window.ui.inputFieldFrameOverrides(),
    window.observedFramebufferExtent,
    window.uiToFramebufferScaleX,
    window.uiToFramebufferScaleY);

window.storageReadLease = storageSystem->sealFrame(window.storageFrame);
```

`drawFrame()` then acquires the swapchain image and records only the already-prepared spans. The run span was allocated before sealing and remains physically valid until that frame slot is completed/cancelled and reset. Store its frame epoch and, in development builds, reject a stale epoch or missing read lease before recording.

If acquisition, swapchain recreation, recording, or submission exits early, the existing cancellation guard invalidates the prepared data together with the storage frame. Clear `PreparedUiFrame` whenever a frame is submitted or cancelled.

## Heap-free direct conversion

Before acquiring a write, perform an overflow-checked upper-bound pass:

- rectangle, border, image, and input-rectangle override: at most one instance each;
- text: at most `max(1, UTF-8 byte length)` instances with the current layout/fallback behavior;
- runs: at most drawable command count plus input-rectangle override count;
- scissor depth: at most command count plus the root scissor.

Allocate the `UiRun` array and scissor stack once from `frameArena()`. Do not allocate `UiInstance` scratch. Ensure buffer capacity for the upper bound, then call `beginBufferWrite(..., DirectMapped)` and placement-write instances directly into its mapped span.

The text layout helper should accept an emitter/callback or bounded output cursor instead of filling `std::vector<GlyphQuad>`. It should immediately turn each glyph into one `UiInstance`. The fallback glyph uses the same cursor. Text-color override ranges are sorted in current frame data, so advance a range cursor rather than rescanning all ranges for every glyph.

After conversion:

```cpp
storage.commitBufferWrite(
    frame,
    write,
    built.instanceCount * sizeof(UiInstance));
```

`commitBufferWrite()` already flushes non-coherent mapped memory and generation-safely tracks the buffer as a frame use. Track the shared quad, placeholder images, image views, and sampler once before sealing whenever the UI pass can bind/use them. `trackUses()` deduplicates repeated handles.

If there are no instances, do not open a zero-byte write. Return an empty prepared frame and preserve the existing no-UI draw behavior.

Add layout assertions near `UiInstance`:

```cpp
static_assert(std::is_trivially_copyable_v<UiInstance>);
static_assert(sizeof(UiInstance) == 88);
static_assert(alignof(UiInstance) == 4);
```

Persistent vectors for descriptor sets, texture-slot information, descriptor dirty revisions, and frame-slot resource records are allowed because they are initialized/grown on cold paths. Shader-file vectors are also startup-only. The Phase 2 no-allocation condition applies to steady-state Clay conversion and command recording.

## Transactional instance-buffer growth

Growth must leave the old generation fully usable if any step fails:

```cpp
void ensureInstanceCapacity(uint32_t slotIndex, uint64_t requiredBytes) {
    UiFrameResources& slot = frames_[slotIndex];
    if (requiredBytes <= slot.capacityBytes) return;

    const uint64_t replacementSize = checkedGeometricGrowth(
        slot.capacityBytes, requiredBytes, storageConfig.growthFactor);

    BufferHandle replacement = storage.createBuffer(makeFrameBufferDesc(
        windowId, slotIndex, replacementSize));
    try {
        NativeBufferView native = storage.nativeBuffer(replacement);
        requireValid(native, replacementSize);
        updateInstanceDescriptor(slotIndex, native.nativeBuffer, replacementSize);

        BufferHandle old = std::exchange(slot.instanceBuffer, replacement);
        slot.nativeBuffer = native;
        slot.capacityBytes = replacementSize;
        storage.releaseBuffer(old);
    } catch (...) {
        storage.releaseBuffer(replacement);
        throw;
    }
}
```

Use checked arithmetic and reject sizes beyond `VkDeviceSize`, `size_t`, descriptor range, or `uint32_t` instance counts. Do not destroy or release the old buffer before creation, native validation, and descriptor update all succeed.

The frame slot fence has already completed before this slot is prepared, and storage has stamped every earlier use on submission. Releasing the old strong handle lets storage retire it at its recorded last-use serial; the renderer should not invent completion from a frame count.

## Temporary texture-slot adapter

Phase 2 must keep `TextureRef.id`, `UiTextureRegistry`, manager-provided raw `VkImageView`/`VkSampler` bindings, and the renderer's existing slot-index descriptors. Logical `TextureHandle` identity and batched per-window binding resolution belong to Phase 3.

Mark each retained boundary explicitly, for example:

```cpp
//Transitional: Phase 2 preserves renderer-local texture slots; Phase 3 replaces
// these raw bindings with logical TextureHandle resolution per AppWindow.
```

The same marker should be placed on the shared-manager-to-main-window registry adapter and any renderer method that still accepts raw manager bindings. Do not mark genuinely permanent storage/native interop code as transitional.

## File-level change plan

- `include/internal/StorageSystem/StorageTypes.hpp`: add native image-view/sampler result types and document borrowed lifetime; retain existing public/internal config separation.
- `include/internal/StorageSystem/IStorageSystem.hpp`: add the two cold native queries.
- `include/internal/StorageSystem/FlowStorageSystem.hpp` and `src/Storagesystem/FlowStorageSystem.cpp`: implement generation-checked queries; fix the `imageRegionlock` upload-path typo before using uploads.
- `include/Ui/Vk_UiRenderer.hpp`: remove VMA declarations and `AllocatedBuffer`/`AllocatedImage`; add shared byte-resource input, frame-slot handles, prepared-frame data, and prepare/record methods.
- `src/Ui/Vk_UiRenderer.cpp`: remove renderer VMA helpers, create/release resources through storage, implement upper-bound/direct emission, transactional growth, and record prepared runs.
- `src/FlowUi.cpp`: own/create/destroy `SharedUiByteResources`, pass storage/window identity into renderer initialization, prepare before sealing, and record after sealing.
- `CMakeLists.txt` and tests: add focused renderer/storage migration tests without disturbing unrelated current working-tree changes.

No public `App` or `AppConfig` API change is required. `AppConfig::window` remains the main-window configuration, and internal storage sizing must remain hidden from public configuration in this phase.

## Failure rules

- Any preparation exception leaves the frame unsealed and is handled by the existing storage cancellation path.
- A pending mapped write is released by frame cancellation; successful conversion must commit it exactly once before sealing.
- If replacement buffer creation or descriptor update fails, keep the old buffer/descriptor generation untouched and release only the failed replacement.
- If an upper bound or byte-size calculation overflows, fail the frame before opening a write.
- Once sealed, no renderer code may allocate from the frame arena, begin/commit writes, grow resources, or call `trackUses()`.
- A prepared frame has exactly one terminal path: successful submission through `noteSubmission()` or cancellation.
- Placeholder/quad upload failure aborts renderer initialization and releases already-created handles in reverse dependency order.

## Validation plan

1. Extend storage tests for valid, stale, wrong-kind, and retiring native image-view/sampler queries.
2. Add pure conversion tests comparing direct emission counts/data/run order with current rectangle, border, image, text, nested-scissor, custom-command, and input-override behavior.
3. Test empty frames, malformed scissor nesting, invalid UTF-8, zero-length text fallback, upper-bound overflow, and exact-capacity writes.
4. Inject buffer-creation/native-query/descriptor-update failure and prove the previous generation remains installed.
5. Force repeated instance growth, submit frames, complete their exact fences, and verify old generations retire only after the corresponding serial.
6. Run several warm frames with allocation instrumentation and assert no general heap allocation occurs in conversion/recording.
7. Run Vulkan validation with placeholder uploads, font/UI views, sampler descriptors, swapchain recreation, early acquisition returns, and shutdown.
8. Add a source-level regression check that `Vk_UiRenderer.cpp` has no `vmaCreate`, `vmaDestroy`, or renderer-owned `VmaAllocation`.
9. Build and test both release and development configurations, then run `git diff --check`.

## Phase 2 implementation order

1. Fix and test the storage upload typo and add native view/sampler queries.
2. Add app-shared storage-backed quad/placeholders/sampler with exception-safe initialization and shutdown.
3. Replace per-slot instance VMA buffers with storage handles, initially retaining the old scratch vectors to isolate GPU-ownership changes.
4. Add transactional growth and its failure tests.
5. Split `prepareFrame()` from `recordPreparedFrame()` and move preparation before `sealFrame()`.
6. Replace instance/run/scissor/glyph vectors with arena spans and direct mapped emission.
7. Mark and verify the transitional texture-slot adapter.
8. Run allocation, lifecycle, Vulkan, and compatibility validation; remove the temporary old conversion path.

This sequence keeps each intermediate state testable and avoids changing resource ownership, frame ordering, and the hot conversion algorithm in one indivisible edit.

## Proposed future execution prompt

> Implement Phase 2 exactly as scoped in `byteMeomoryMove_Implementation.md`, preserving current single-window public behavior and inspecting the working tree before editing so unrelated changes are not overwritten. First fix the existing `imageRegionlock` upload-path typo, then add generation-checked cold native queries for storage image views and samplers. Create one app-shared storage-backed quad buffer, placeholder font/UI images and views, and sampler; create frame-local persistently mapped instance buffers for every main-window Vulkan frame slot. Preserve the current 1 MiB initial instance capacity through the internal storage configuration.
>
> Split `VulkanUiRenderer` into pre-seal preparation and post-seal recording. In `App::Impl::endFrame()`, prepare resources and direct-emitted instances before `sealFrame()`; in `drawFrame()`, record only immutable prepared runs. Replace renderer instance/run/scissor/glyph scratch vectors with frame-arena spans and direct `beginBufferWrite()`/`commitBufferWrite()` emission, with checked upper bounds and no steady-state general heap allocation. Implement transactional generation-based buffer growth so failures leave the old buffer and descriptor intact. Track shared renderer resources before sealing and retain the existing Phase 1 submission/completion/cancellation lifecycle.
>
> Keep raw Vulkan pipeline/descriptor control objects, `TextureRef.id`, `UiTextureRegistry`, manager resource stores, and current descriptor-slot behavior in this phase. Mark every temporary manager/renderer texture-slot boundary with `//Transitional:` and explain its Phase 3 replacement. Remove all renderer-owned VMA allocation code, add focused lifecycle/growth/direct-conversion/native-query tests, run release and development builds plus storage/Vulkan tests and `git diff --check`, and finish with `phase2.md` describing changes, validation, remaining limitations, and the Phase 3 outline.
