# UI Renderer and Window Storage Integration Report

**Repository audit date:** 2026-07-30  
**Scope:** `VulkanUiRenderer`, application/window orchestration, per-window Vulkan frame resources, and the new `IStorageSystem` / `FlowStorageSystem`. Managers and manager-owned resources are discussed only where their current contracts cross the renderer boundary.

## Executive summary

FlowUi should adopt the storage system for renderer and window memory, but the generated storage implementation is not ready to be wired into the live renderer yet. The correct next step is a short storage-hardening phase followed by a renderer/window integration performed behind the existing single-window public API.

The target architecture is:

- one app/device-level storage system and one set of immutable renderer resources;
- one `AppWindow` execution context per native window;
- one storage window scope and one renderer window state per `WindowId`;
- one independent instance buffer, descriptor set state, scratch arena, command pool, and synchronization record per window/frame slot;
- app-shared logical `TextureHandle` values resolved to window-local descriptor indices once per frame batch;
- no storage virtual calls, locks, string lookups, or hash-map lookups inside command, glyph, instance, or draw-run loops;
- new resource generations on growth/replacement, with old generations retired only after the actual GPU submission serial completes;
- no routine `vkDeviceWaitIdle()` for descriptor growth, buffer growth, swapchain recreation, or closing one window.

The largest immediate blockers are concrete, not theoretical:

1. `src/Storagesystem/FlowStorageSystem.cpp` is absent from the `flowui` CMake target, so normal builds do not compile or link the implementation.
2. `StorageTypes.hpp` does not compile because `Handle::fromPacked()` is missing its closing brace before `operator<=>`.
3. Storage does not expose a correct mapped-buffer write/flush contract. `nativeBuffer()` returns a mapped pointer but hides the VMA allocation needed to flush non-coherent memory.
4. Storage assigns logical descriptor indices but does not create/update descriptor sets, enforce descriptor capacity, or expose a delta batch suitable for renderer writes.
5. the current global “any sealed frame” mutation prohibition serializes unrelated windows and is incompatible with the intended multi-window/multi-threaded model.
6. `VulkanContext`, `App::Impl`, `Swapchain`, `FrameVk`, `VulkanUiRenderer`, and `IWindowBackend::pollEvents()` all assume one window/surface.

The integration should therefore be delivered in phases. Phase 0 makes storage compile, testable, and semantically safe. Phase 1 introduces `AppWindow` while behavior remains single-window. Phase 2 moves renderer buffers, placeholders, and scratch memory to storage. Phase 3 changes UI texture identity from a renderer slot to a logical handle and connects batched per-window descriptor resolution. Phase 4 enables multiple windows. Phase 5 introduces parallel build/record jobs after ownership and snapshots are proven.

## Audit basis and boundaries

The report follows the live paths in:

- `include/internal/StorageSystem/IStorageSystem.hpp`
- `include/internal/StorageSystem/StorageTypes.hpp`
- `include/internal/StorageSystem/FlowStorageSystem.hpp`
- `src/Storagesystem/FlowStorageSystem.cpp`
- `include/Ui/Vk_UiRenderer.hpp`
- `src/Ui/Vk_UiRenderer.cpp`
- `src/FlowUi.cpp`
- `include/Vulkan/Vk_Context.hpp` and `src/Vulkan/Vk_Context.cpp`
- `include/Vulkan/Vk_Frames.hpp` and `src/Vulkan/Vk_Frames.cpp`
- `include/Vulkan/Vk_Swapchain.hpp` and `src/Vulkan/Vk_Swapchain.cpp`
- `include/window/IWindow.hpp`, `include/window/Window.hpp`, and `include/internal/InputQueue.hpp`
- the renderer-facing `TextureRef` and `UiManager::storeTexture()` path.

Existing architecture documents were used as intent, but findings below were checked against implementation. This report does not propose migrating `ImageManager`, `FontManager`, `IconManager`, `ViewPortManager`, or their heavy resources now. It does define the renderer-side contract those later migrations must satisfy.

## Current system: ownership and memory narrative

### Current application/window ownership

`App::Impl` currently combines app-global and window-local state in one object:

```cpp
struct App::Impl {
    AppConfig config{};

    std::unique_ptr<detail::IWindowBackend> window;
    detail::InputQueue inputQueue;

    VulkanContext vk;
    Swapchain swap;
    FrameVk frames;

    UiManager ui;
    VulkanUiRenderer renderer;
    UiTextureRegistry textureRegistry;
    FontManager fonts;
    ImageManager imageManager;
    ViewPortManager viewPortManager;

    FrameInput frameInputForCurrentFrame{};
    Clay_RenderCommandArray renderCommandsForCurrentFrame{};
    std::vector<VkImageLayout> swapchainImageLayouts;
    // timing, scaling, resize state...
};
```

This shape has one native window, one Vulkan surface, one swapchain, one current frame counter, one UI/Clay context, one renderer scratch set, and one texture slot registry. Creating a second `App::Impl` would duplicate the Vulkan instance/device, VMA allocator, pipelines, shared textures, fonts, and other expensive resources. Sharing the existing object would instead race on its mutable window/frame fields.

### Current renderer-owned GPU memory

`VulkanUiRenderer` owns raw Vulkan/VMA allocations directly:

```cpp
struct VulkanUiRenderer {
    struct AllocatedBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation_T* allocation = nullptr;
        void* mapped = nullptr;
        VkDeviceSize size = 0;
    };

    std::vector<AllocatedBuffer> instanceBuffersByFrame_;
    AllocatedBuffer quadVertexBuffer_;
    AllocatedImage placeholderFontAtlas_;
    AllocatedImage placeholderUiTexture_;
    VkSampler linearSampler_ = VK_NULL_HANDLE;

    std::vector<UiInstance> instancesScratch_;
    std::vector<UiRun> runsScratch_;
};
```

Allocation and destruction bypass `IStorageSystem`:

```cpp
vmaCreateBuffer(vk.allocator, &bufferInfo, &allocationCreateInfo,
                &outBuffer.buffer, &outBuffer.allocation, &allocationInfo);
outBuffer.mapped = allocationInfo.pMappedData;

// Destruction
vmaDestroyBuffer(vk.allocator, buffer.buffer, buffer.allocation);
```

At initialization the renderer allocates:

- a 1 MiB persistently mapped storage buffer for every frame in flight;
- a mapped quad vertex buffer;
- a placeholder font atlas image and view;
- a placeholder UI texture image and view;
- a linear sampler;
- descriptor layouts, pool, and two descriptor sets per frame;
- CPU vectors reserved for 4,096 `UiInstance` values and 256 `UiRun` values.

`UiInstance` is currently 88 bytes on the intended layout, so the initial 1 MiB buffer holds approximately 11,915 instances. The new `StorageConfig::initialInstanceBytesPerFrame` defaults to only 256 KiB (approximately 2,978 instances) and is not used anywhere in `FlowStorageSystem`. That configuration mismatch must be resolved from measurements rather than silently reducing capacity.

### Current per-frame CPU hot path

The renderer clears reusable vectors, creates additional temporary vectors, emits instances, then copies the complete instance vector to mapped GPU memory:

```cpp
outInstances.clear();
outRuns.clear();

std::vector<RectF> scissorStack;
scissorStack.push_back(fullScissor);

for (int32_t i = 0; i < commands.length; ++i) {
    // command dispatch
    instances.push_back(inst);
    runs.emplace_back(...);
}

std::memcpy(activeInstanceBuffer.mapped,
            instancesScratch_.data(),
            instancesScratch_.size() * sizeof(UiInstance));
vmaFlushAllocation(vk.allocator, activeInstanceBuffer.allocation, 0, size);
```

Every text command also creates and reserves a new glyph vector:

```cpp
std::vector<GlyphQuad> glyphs;
glyphs.reserve(static_cast<size_t>(std::max(1, textData.stringContents.length)));
LayoutMsdfTextToGlyphs(..., glyphs, ...);
```

Consequences:

- the steady-state renderer can still allocate from the general heap for `scissorStack` and every text command;
- output is written once to `instancesScratch_` and copied a second time to the mapped instance buffer;
- vector growth can relocate and copy all previously emitted instances/runs;
- text performs a glyph materialization pass and then an instance emission pass;
- input text color overrides can scan every override range for every glyph;
- development diagnostics scan commands and instances again after conversion;
- renderer scratch is one mutable object and therefore cannot serve two windows or worker threads concurrently.

The reusable outer vectors are better than allocating them from scratch each frame, but they do not provide an allocation-free guarantee and their memory cannot be attributed to a window/frame storage scope.

### Current instance-buffer growth

The current cold path destroys the frame-slot buffer immediately and allocates a larger one:

```cpp
if (instanceBuffer.size < requiredBytes) {
    VkDeviceSize newSize = instanceBuffer.size;
    while (newSize < requiredBytes) newSize *= 2;

    DestroyBuffer(vk, instanceBuffer);
    CreateMappedBuffer(vk, newSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, instanceBuffer);
    UpdateInstanceBufferDescriptorForFrame(renderer, vk.device, frameSlot);
}
```

This is only safe because `drawFrame()` waits for the current frame-slot fence before calling `render()`. The safety rule is implicit and local; the buffer has no recorded generation, window, last-use submission, or storage accounting. Moving this code without preserving the fence/serial rule would create a use-after-free.

### Current texture identity and descriptor memory

`TextureRef::id` is a renderer descriptor slot:

```cpp
struct TextureRef {
    uint32_t id = 0; // manager-owned texture registry slot
    float uv0x = 0.0f;
    float uv0y = 0.0f;
    float uv1x = 1.0f;
    float uv1y = 1.0f;
    // fit, sampling, tint, dimensions...
};

// Renderer conversion
inst.texIndex = textureRef.id;
```

The `UiTextureRegistry` in `src/FlowUi.cpp` maps heap-owned `std::string` keys to slots, maintains an unordered map of native bindings, and retires slots by frame bucket. Capacity growth calls `vkDeviceWaitIdle()`, doubles the descriptor array, recreates descriptor and pipeline objects, copies all slot infos, and rewrites bindings.

This identity cannot survive multi-window use. Descriptor slot 17 in window A is unrelated to slot 17 in window B. A render command must hold app-level logical texture identity, and each window must resolve it to its own descriptor index.

### Current window/frame memory

`FrameVk` owns vectors of raw Vulkan synchronization and command objects:

```cpp
struct FrameVk {
    struct Frame {
        VkCommandPool pool;
        VkCommandBuffer cmd;
        VkSemaphore imageAvailable;
        VkFence inFlight;
    };

    std::vector<Frame> frames;
    std::vector<VkFence> imageInFlight;
    std::vector<VkSemaphore> renderFinishedBySwapImage;
    uint32_t currentFrame = 0;
};
```

`Swapchain` owns a single `VkSwapchainKHR` plus heap vectors of borrowed swapchain images and owned image views. `VulkanContext` owns a single `VkSurfaceKHR`. Swapchain recreation destroys the old swapchain and views after a device-wide idle. These resources have no `WindowId`, allocation tag, submission serial, or storage snapshot.

The native GLFW backend also mixes two operations in `pollEvents()`:

1. global `glfwPollEvents()`, which should execute once per app iteration on the platform thread;
2. per-window cursor/button sampling, which should execute once for each window.

Calling the current method for N windows would poll the global event queue N times. `GlfwLibrary::refCount` is a non-atomic process-global integer, and `InputQueue` is unsynchronized, so the current window layer must remain platform-thread-owned until its handoff model is changed.

### Current frame lifetime

The live sequence is:

1. choose `frames.currentFrame`;
2. let registries reclaim that frame bucket;
3. poll events and build UI;
4. obtain a Clay command array;
5. wait for the current frame-slot fence;
6. acquire a swapchain image and possibly wait for its fence;
7. reset command pool, convert UI, upload instances, and record commands;
8. submit with the frame fence;
9. present and advance the frame index.

This already provides the synchronization fact storage needs: a frame slot is reusable only after its fence has completed. The upgrade should make this explicit using `SubmissionToken` and a stored token on each `FrameVk::Frame`.

## Audit of the generated storage system

### What is already well shaped

The new interface contains several useful foundations:

- typed generational handles (`BufferHandle`, `ImageHandle`, `TextureHandle`, and others);
- explicit `WindowId`, `FrameToken`, frame slot, frame number, and epoch;
- non-relocating paged linear arenas and slabbed persistent pools;
- per-window/per-frame/per-worker scopes;
- app-shared texture records with per-window binding records;
- hot/cold resource table separation;
- a batched `prepareTextureBindings()` entry point;
- borrowed `StorageReadView` and `WindowBindingView` spans for hot reads;
- submission serials, out-of-order completion tracking, and deferred retirement;
- GPU and CPU statistics and configurable budgets;
- sampler deduplication and logical texture revisions.

Those concepts should be retained. In particular, the renderer should call the interface a small fixed number of times per frame, then use the borrowed contiguous records directly.

### Build-blocking findings

#### A1 — Storage implementation is not built

`CMakeLists.txt` lists the current runtime sources but does not list `src/Storagesystem/FlowStorageSystem.cpp`, and the storage headers are not listed with the target either. Normal builds therefore give a false impression of success.

Required correction:

```cmake
target_sources(flowui
    PRIVATE
        src/Storagesystem/FlowStorageSystem.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/internal/StorageSystem/IStorageSystem.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/internal/StorageSystem/FlowStorageSystem.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/internal/StorageSystem/StorageTypes.hpp>
)
```

These are internal headers, so whether they are installed/exported should be decided separately; the key requirement is that the implementation be compiled by CI.

#### A2 — `StorageTypes.hpp` is syntactically invalid

The current code is missing the closing brace of `fromPacked()`:

```cpp
static constexpr Handle fromPacked(uint64_t value) noexcept {
    return Handle{
        .index = static_cast<uint32_t>(value),
        .generation = static_cast<uint32_t>(value >> 32u),
    };

auto operator<=>(const Handle&) const = default;
```

It must be:

```cpp
static constexpr Handle fromPacked(uint64_t value) noexcept {
    return Handle{
        .index = static_cast<uint32_t>(value),
        .generation = static_cast<uint32_t>(value >> 32u),
    };
}

auto operator<=>(const Handle&) const = default;
```

A direct C++20 syntax check currently fails with this and cascading errors. Storage adoption must have its own compilation and unit-test target so this cannot recur.

### Correctness and contract gaps to fix before renderer adoption

#### A3 — Mapped writes cannot be made portable or correct

`nativeBuffer()` returns:

```cpp
struct NativeBufferView {
    uint64_t nativeBuffer = 0;
    uint64_t size = 0;
    void* mapped = nullptr;
};
```

The renderer can copy to `mapped`, but it cannot flush a non-coherent VMA allocation because the allocation is deliberately hidden. The current renderer explicitly calls `vmaFlushAllocation()`. Do not assume host coherence.

Add a storage operation with one commit per produced range, not one call per instance:

```cpp
struct MappedBufferWrite {
    BufferHandle buffer{};
    std::byte* data = nullptr;
    uint64_t capacity = 0;
};

virtual MappedBufferWrite beginMappedWrite(
    const FrameToken&, BufferHandle, uint64_t offset, uint64_t bytes) = 0;
virtual void endMappedWrite(
    const FrameToken&, const MappedBufferWrite&, uint64_t bytesWritten) = 0;
```

`endMappedWrite()` performs the necessary VMA flush and tracks buffer use. The write lease also documents that the pointer is borrowed and frame-scoped.

#### A4 — Descriptor assignment is only logical, not operational

`FlowStorageSystem::resolve()` fills `BindingHotRecord` with an integer descriptor index and native handles. It does not:

- allocate descriptor pools/sets;
- know the descriptor-array capacity;
- reject or grow beyond the Vulkan limit;
- record which binding revision has been applied to each frame-slot descriptor set;
- produce only changed bindings;
- substitute the real fallback image/sampler when a texture is queued, failed, or stale.

The renderer and storage need an explicit division of responsibility. Recommended division:

- storage owns logical texture-to-window-binding cache, index allocation, revisions, and lifetimes;
- `WindowUiRendererResources` owns Vulkan descriptor pool/sets;
- storage returns a compact dirty-binding batch before sealing;
- renderer applies that batch once to the current safe frame-slot descriptor set.

Suggested API shape:

```cpp
struct DescriptorWriteRecord {
    uint32_t descriptorIndex;
    uint32_t bindingRevision;
    uint64_t nativeImageView;
    uint64_t nativeSampler;
    ResourceState state;
};

struct PreparedTextureBindings {
    WindowBindingView allBindings;
    std::span<const DescriptorWriteRecord> dirty;
    uint32_t requiredDescriptorCapacity;
};

virtual PreparedTextureBindings prepareTextureBindings(
    const FrameToken&, std::span<const TextureHandle>) = 0;
```

Alternatively keep the existing return type and add a `dirtyWindowBindings()` view. The important properties are batched output, per-frame-slot application state, bounded capacity, and no per-texture virtual call.

#### A5 — The sealed-frame rule globally serializes windows

`requireResourceMutationPhase()` scans every frame of every window and rejects mutation if any frame is sealed. This protects vector-backed borrowed spans from relocation, but it means a sealed window A prevents texture publication, view/sampler release, binding preparation, and collection for window B.

That is incompatible with independent multi-window workers and render recording. Replace it with one of these designs:

1. **Preferred:** stable paged/chunked hot tables with immutable published pages and a frame read lease/epoch. Appending a page never relocates prior records. Retired slots are not reused until all relevant leases/submissions finish.
2. **Acceptable first version:** drain all shared mutations at a documented app-wide publish barrier, then create snapshots for all windows. Per-window binding tables remain independently locked/single-writer. This supports parallel reads after the barrier but not arbitrary concurrent publication.

Do not keep an unexpressed global phase hidden behind a recursive mutex.

#### A6 — Borrowed views escape their lock without a complete lease contract

`readView()`, `windowBindingView()`, `readBlob()`, `string()`, and `nativeBuffer()` return pointers/spans/views after releasing the mutex. Some underlying memory is stable by implementation, while vector-backed hot tables are stable only because the global sealed-frame rule prevents mutation. This relationship must be contractual and testable.

Add explicit rules:

- a `StorageReadView` is valid from successful `sealFrame(token)` until `noteSubmission(token)` or `cancelFrame(token)`;
- a `WindowBindingView` has the same lifetime and is owned by that window/frame;
- no vector operation can relocate a table covered by a live view;
- `readBlob()` requires retained ownership or a `BlobReadLease`;
- native/mapped views remain valid only while their handle generation is retained.

In debug builds every view should carry an epoch/lease ID checked at use boundaries.

#### A7 — Use tracking is quadratic

`addUse()` performs `std::find` over `frame.used` for every resolved resource. With U unique textures this becomes O(U²), in addition to any renderer gather pass.

Use generation/epoch marking indexed by handle slot, or sort/unique one arena-backed handle array once:

```cpp
if (lastUsedEpochByTexture[texture.index] != frame.epoch) {
    lastUsedEpochByTexture[texture.index] = frame.epoch;
    frame.used.push_back({ResourceKind::TextureView, texture.packed()});
}
```

This makes deduplication O(1) with contiguous access.

#### A8 — Upload flushing is a global stop

`flushUploads()` holds the global recursive mutex while allocating a staging buffer, recording, submitting, and executing `vkQueueWaitIdle()` for every upload. It blocks all window/storage activity and serializes the graphics queue.

This is acceptable only as an explicitly temporary bootstrap path. Renderer placeholder/quad uploads may use it during initialization, but normal rendering must move to batched staging pages, one or a few submissions, timeline/fence completion, and no mutex held during queue waits.

#### A9 — Window-persistent attribution is incomplete

`allocatePersistent()` accepts a `MemoryClass` but no `WindowId`; its `AllocationTag` is constructed with `window = 0`. `WindowPersistent` allocations therefore cannot be attributed to a window. The same generic persistent pool backs most classes, and CPU soft budgets are reported but not enforced.

Add a scoped allocation form:

```cpp
virtual MemoryBlock allocatePersistent(
    size_t bytes,
    size_t alignment,
    AllocationTag tag) = 0;
```

or a `windowPersistentAllocator(WindowId)`/PMR resource. Validate power-of-two alignment, enforce/define soft-budget behavior, and protect `AllocationId` overflow.

#### A10 — Storage does not cover all renderer/window native objects

The current resource kinds cover buffers, images, image views, samplers, and logical textures, but not:

- descriptor set layouts, pools, and sets;
- pipeline layouts and pipelines;
- shader modules/bytecode blobs;
- command pools/buffers;
- fences and semaphores;
- swapchains, borrowed swapchain images, and owned swapchain views;
- surfaces and platform windows.

It is reasonable for OS windows, surfaces, swapchains, queues, and synchronization to be controlled by a window/device backend rather than forced into a byte allocator. However, if the stated goal is accounting for all library memory, the facade must either:

- provide typed backend services/handles for these objects and serial-based retirement; or
- explicitly classify them as native externally allocated objects, keep one clear owner, use Vulkan allocation callbacks where possible, and include native object counts/estimated bytes in diagnostics.

Do not use `allocatePersistent()` for C++ wrappers while allowing duplicate raw Vulkan ownership. Storage adoption is an ownership change, not only an allocation-source change.

### Additional implementation issues to address

- `initialInstanceBytesPerFrame` is unused.
- `initialUploadStagingBytes` and several memory-class statistics do not correspond to reserved reusable staging storage in the implementation.
- `refreshTexturesForImage()` scans all textures for each image state change; maintain image-to-texture dependents instead.
- descriptor indices grow without a configured/device limit.
- `prepareTextureBindings()` returns `void`, forcing another call/view to discover results.
- the only `trackUse()` overloads are buffer and image, although the internal submission stamping understands image views, samplers, and textures.
- `trim()` can erase arena overflow pages without checking whether frames are active; restrict it to inactive scopes.
- `WindowStorageSnapshot::bindingCapacity` reports vector capacity, not Vulkan descriptor capacity.
- the placeholder texture record at index zero contains no native fallback binding. The renderer needs a real, always-valid fallback resource.
- a single recursive mutex hides nested calls but increases contention and makes accidental long critical sections easy. Split locks by shared resource table, window bindings, uploads, retirement, and telemetry, or enforce a single-owner mutation queue.
- exceptions are used for budget exhaustion and operational failure. Renderer frame code needs a transactional result/fallback policy so an allocation failure does not leave half-updated descriptor/buffer state.

## Target ownership architecture

### Ownership rule

The central design rule is:

> Share immutable device resources; duplicate window execution state; duplicate frame-slot writable state; isolate worker scratch; resolve logical resources into window-local bindings only at frame preparation.

### Target object graph

```text
App::Impl
├── DeviceContext                         app/device lifetime
│   ├── VkInstance, VkPhysicalDevice, VkDevice, queues, VMA
│   └── present-support policy for registered surfaces
├── unique_ptr<IStorageSystem>            app/device lifetime
│   ├── shared buffers/images/views/samplers/textures
│   ├── persistent/string pools
│   ├── upload + retirement services
│   └── WindowStorageState[WindowId]
├── SharedUiRendererResources             app/device lifetime
│   ├── descriptor set layouts
│   ├── pipeline layout + pipelines by target-format key
│   ├── immutable quad buffer handle
│   └── fallback texture/sampler handles
└── AppWindow[WindowId]                   per-window lifetime
    ├── IWindowBackend + InputQueue + UiManager
    ├── VkSurfaceKHR + Swapchain
    ├── FrameVk::Frame[framesInFlight]
    │   ├── command pool/buffer + sync objects
    │   ├── SubmissionToken
    │   └── BufferHandle instance stream
    ├── WindowUiRendererResources
    │   ├── descriptor pool/sets per frame slot
    │   ├── descriptor applied-revision arrays
    │   └── target format/pipeline selection
    ├── FrameToken currentStorageFrame
    ├── Clay commands + window UI state
    └── resize/layout/timing/diagnostic state
```

### Shared renderer state

Recommended shape:

```cpp
struct SharedUiRendererResources {
    storage::BufferHandle quadVertices{};
    storage::TextureHandle fallbackTexture{};
    storage::TextureHandle fallbackFontAtlas{};

    VkDescriptorSetLayout globalsLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout texturesLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    // One immutable pipeline bundle per compatible target format.
    FlatMap<VkFormat, UiPipelines> pipelinesByFormat;
};
```

The quad is only four vertices and could be generated from `gl_VertexIndex`, eliminating the buffer and bind entirely. That is preferable if shader compatibility permits it. If retained, create it once through storage and upload once.

### Per-window renderer state

```cpp
struct WindowUiRendererResources {
    storage::WindowId windowId = 0;
    uint32_t descriptorCapacity = 0;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> globalsSetsByFrame;
    std::vector<VkDescriptorSet> texturesSetsByFrame;

    // [frameSlot][descriptorIndex] = binding revision already written.
    std::vector<std::vector<uint32_t>> appliedBindingRevision;
};

struct UiFrameResources {
    storage::BufferHandle instanceBuffer{};
    storage::NativeBufferView cachedNative{}; // refreshed only on generation change
    uint64_t capacityBytes = 0;
    storage::SubmissionToken lastSubmission{};
};
```

Descriptor sets are duplicated per window and frame slot because each set is writable/reusable on a different fence cadence. The logical texture/image/sampler remains shared once in storage.

### Per-window application state

Introduce `AppWindow` before exposing any multi-window public API:

```cpp
struct AppWindow {
    storage::WindowId id = 0;
    WindowConfig config{};

    std::unique_ptr<detail::IWindowBackend> backend;
    detail::InputQueue input;
    UiManager ui;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    Swapchain swapchain;
    FrameVk frames;
    WindowUiRendererResources renderer;

    storage::FrameToken storageFrame{};
    uint64_t frameNumber = 0;
    Clay_RenderCommandArray renderCommands{};
    std::vector<VkImageLayout> swapchainImageLayouts;

    // extent, scale, resize, timing, diagnostics...
};
```

Then `App::Impl` owns a collection keyed by stable `WindowId`. Do not expose pointers/references to entries in a relocatable `std::vector<AppWindow>`; use `std::unique_ptr<AppWindow>` in a vector, a slot map, or another stable registry.

## Renderer integration: complete frame memory narrative

### Initialization

1. Create the main platform window and surface only far enough to select a compatible Vulkan device.
2. Create app-level device/VMA state.
3. Construct `FlowStorageSystem(vk)` and call `initialize()` with measured reservations.
4. Create fallback image, view, sampler, and logical texture through storage. Descriptor index zero in every window is permanently bound to this fallback.
5. Create shared pipeline/layout objects once per target format compatibility key.
6. Create/register the main `AppWindow` and call `storage.registerWindow(id, desc)`.
7. Create per-window descriptor objects and per-frame mapped instance buffers through storage.

Proposed setup:

```cpp
storage = std::make_unique<storage::FlowStorageSystem>(device.vk);
storage->initialize(makeStorageConfig(config));

sharedRenderer = createSharedUiRendererResources(*storage, device, config);

AppWindow& main = createMainWindow(config.window);
storage->registerWindow(main.id, storage::WindowStorageDesc{
    .framesInFlight = frameCount,
    .workerCount = configuredWorkerCount,
    .initialTextureBindings = config.storage.expectedBindingsPerWindow,
    .transientBytesPerFrame = measuredRendererScratchBytes,
    .transientBytesPerWorker = measuredWorkerScratchBytes,
    .debugName = storage->intern(config.window.title),
});
createWindowRendererResources(main, sharedRenderer, *storage);
```

### Frame-slot acquisition and completion

The window frame slot owns the bridge between Vulkan fences and storage serials:

```cpp
FrameVk::Frame& gpuFrame = window.frames.getCurrentFrame();
vkWaitForFences(device, 1, &gpuFrame.inFlight, VK_TRUE, UINT64_MAX);

if (gpuFrame.lastSubmission) {
    storage.noteCompleted(gpuFrame.lastSubmission);
    gpuFrame.lastSubmission = {};
    storage.collect();
}

window.storageFrame = storage.beginFrame(window.id, {
    .frameSlot = window.frames.currentFrame,
    .frameNumber = window.frameNumber++,
});
```

This ordering is essential: `beginFrame()` resets that slot's arenas, so it must happen only after the slot fence signals and its prior `SubmissionToken` is completed. The current app begins UI work before waiting for the fence; after storage owns per-slot UI/transient memory, either move the wait earlier or use a different build slot whose completion is already known.

Recommended pipeline for low stalls:

- poll fence status for all windows at the beginning of the app iteration;
- mark completed tokens and collect;
- choose only reusable frame slots;
- allow UI build/conversion work for a window only after its target slot is reusable;
- if a window is blocked, process another window rather than waiting immediately.

### Gather logical textures once

Before instance conversion, scan image commands into an arena-backed handle array. Avoid a heap `unordered_set`. Use an epoch-mark table keyed by texture index or sort/unique packed handles.

```cpp
ArenaView arena = storage.frameArena(frame, MemoryClass::FrameTransient);
TextureHandle* gathered = arena.allocateArray<TextureHandle>(imageCommandUpperBound).data();

size_t textureCount = gatherUniqueTextureHandles(
    renderCommands,
    gathered,
    imageCommandUpperBound,
    textureSeenEpoch,
    frame.epoch);

PreparedTextureBindings prepared = storage.prepareTextureBindings(
    frame,
    std::span{gathered, textureCount});
```

This is one interface call for arena acquisition and one for all bindings. The gather loop reads compact handles only.

### Ensure descriptor capacity outside the hot loop

If `prepared.requiredDescriptorCapacity` exceeds the window capacity, do not recreate shared pipelines and do not wait for the whole device.

Preferred options, in order:

1. choose the descriptor array maximum at device initialization based on configuration and Vulkan limits, allocate per-window sets at that capacity, and never grow the layout;
2. use descriptor buffers or a sufficiently large bindless global layout when supported;
3. if descriptor pool/set growth is unavoidable, create a new window descriptor generation, populate it, use it for future frame slots, and retire the old pool after the last submission that used it.

The current path recreates descriptor layouts and pipelines because the descriptor count is part of the layout. That is too expensive for runtime capacity growth.

### Apply only dirty descriptor bindings

Each window/frame descriptor set records the last applied revision per descriptor index. For the current frame slot:

```cpp
for (const DescriptorWriteRecord& change : prepared.dirty) {
    auto& applied = window.renderer.appliedBindingRevision[frameSlot][change.descriptorIndex];
    if (applied == change.bindingRevision) continue;

    const bool ready = change.state == ResourceState::Ready &&
                       change.nativeImageView != 0 && change.nativeSampler != 0;
    appendVkDescriptorWrite(
        writeBatch,
        frameTextureSet,
        change.descriptorIndex,
        ready ? change : fallbackBinding);
    applied = change.bindingRevision;
}

vkUpdateDescriptorSets(device, writeCount, writes, 0, nullptr);
```

Allocate `VkDescriptorImageInfo` and `VkWriteDescriptorSet` arrays from the frame arena. Do one Vulkan update for the batch. Do not rewrite all `maxUiImageDescriptors_` entries when one texture changes.

### Size output without moving it

There are two reasonable strategies.

#### Recommended: upper-bound then write directly to mapped GPU memory

Perform a cheap sizing pass over Clay commands:

- rectangle/image: at most one instance;
- border: one instance in the current shader representation;
- text: decoded glyph count or string byte length as a conservative bound;
- input overrides: known vector counts.

Ensure the current frame-slot buffer can hold that upper bound, then obtain one mapped write lease. Emit `UiInstance` values directly into mapped memory. Store only `UiRun` and small stacks in the CPU frame arena.

```cpp
const RenderUpperBound bound = countRenderUpperBound(commands, overrides);
ensureInstanceCapacity(window, frameSlot, bound.instances, lastCompletedSerial);

MappedBufferWrite write = storage.beginMappedWrite(
    frame,
    slot.instanceBuffer,
    0,
    bound.instances * sizeof(UiInstance));

UiInstance* instances = reinterpret_cast<UiInstance*>(write.data);
std::span<UiRun> runs = arena.allocateArray<UiRun>(bound.runs);

RenderBuildResult built = buildInstancesAndRuns(
    commands,
    bindings,
    instances,
    bound.instances,
    runs);

storage.endMappedWrite(frame, write, built.instanceCount * sizeof(UiInstance));
```

This removes the large CPU scratch vector and the final `memcpy`. If direct writes to the mapped memory benchmark poorly on a particular memory type, keep a storage-owned host-cached staging span and copy once; make the choice a backend policy, not renderer logic.

#### Simpler transition: arena spans then one copy

Allocate upper-bound `UiInstance` and `UiRun` spans from `FrameTransient`, emit in place, and copy the used prefix once. This is already a significant improvement because it removes heap growth and moves; it retains one CPU-to-mapped-buffer copy.

Do not use `std::pmr::vector` without reserving a proven upper bound: arena-backed vector growth still copies old elements and leaks the abandoned region until frame reset.

### Eliminate per-text text allocations

Emit text layout directly to instances through the layout callback:

```cpp
LayoutTextLine(request, [&](const TextLayoutGlyphQuad& glyph) {
    UiInstance& instance = output.emplace();
    fillMsdfInstance(instance, glyph, ...);
});
```

There is no need to materialize `std::vector<GlyphQuad>` first. If color override range lookup is sorted, advance one range cursor as glyph byte offsets increase instead of scanning all ranges per glyph.

### Hot-loop binding resolution

After `sealFrame()` the renderer obtains each read view once:

```cpp
storage.sealFrame(frame);
const StorageReadView resources = storage.readView(frame);
const WindowBindingView bindings = storage.windowBindingView(frame);
```

Textured command conversion becomes a bounds/generation-checked array lookup:

```cpp
const BindingHotRecord* binding = bindings.binding(textureRef.handle);
instance.texIndex = binding ? binding->descriptorIndex : 0u;
```

There are no interface calls, mutex operations, maps, strings, or native-handle queries in this loop. `TextureRef` should store the full 64-bit generational handle, not only an index:

```cpp
struct TextureRef {
    detail::storage::TextureHandle handle{};
    float uv0x = 0.0f;
    float uv0y = 0.0f;
    float uv1x = 1.0f;
    float uv1y = 1.0f;
    TextureFitMode fitMode = TextureFitMode::Contain;
    TextureSamplingMode samplingMode = TextureSamplingMode::Linear;
    bool tintEnabled = false;
    int32_t sourceWidth = 0;
    int32_t sourceHeight = 0;
};
```

If public ABI compatibility matters, introduce a versioned/packed `uint64_t resource` field or a new `TextureRefV2`; do not pack generation away. A stale index without generation validation can bind an unrelated reused resource.

### Instance buffer growth and retirement

Buffer growth must be transactional:

```cpp
void ensureInstanceCapacity(AppWindow& window, uint32_t slotIndex, uint64_t required) {
    UiFrameResources& slot = window.uiFrames[slotIndex];
    if (slot.capacityBytes >= required) return;

    const uint64_t newCapacity = growGeometrically(slot.capacityBytes, required, 1.5f);
    BufferHandle replacement = storage.createBuffer(BufferDesc{
        .size = newCapacity,
        .usage = BufferUsage::Storage,
        .memory = MemoryPreference::HostVisible,
        .sharing = ResourceSharing::WindowLocal,
        .access = AccessMode::CpuWrite,
        .persistentlyMapped = true,
        .window = window.id,
        .debugName = uiInstanceBufferName,
    });

    NativeBufferView replacementNative = storage.nativeBuffer(replacement); // cold path
    updateGlobalsDescriptorForSlot(window, slotIndex, replacementNative.nativeBuffer, newCapacity);

    BufferHandle old = std::exchange(slot.instanceBuffer, replacement);
    slot.cachedNative = replacementNative;
    slot.capacityBytes = newCapacity;
    if (old) storage.releaseBuffer(old, slot.lastSubmission.serial);
}
```

Create and validate the replacement before changing the live slot. Never destroy the old buffer first. On failure, keep the old generation and either render a bounded prefix, skip UI with diagnostics, or report a recoverable frame error according to policy.

### Record runs efficiently

`UiRun` is already compact and preserves strict command/scissor order. Continue batching adjacent instances by `(pipeline type, scissor)`, but avoid redundant Vulkan state:

- bind descriptor sets once per UI pass;
- bind the vertex buffer once, or remove it via `gl_VertexIndex`;
- cache the last pipeline and do not rebind it when consecutive runs use the same type but differ only in scissor;
- set scissor only when changed;
- keep push constants per run only for `instanceBaseIndex` unless shader/data layout allows using `firstInstance` directly;
- consider `vkCmdDraw(..., firstInstance)` and read `gl_InstanceIndex`, eliminating the base-index push update.

These changes are independent of storage but become easier once the conversion data is contiguous and immutable.

### Submission and completion

After command recording succeeds:

```cpp
storage.sealFrame(frame); // if not already sealed before read views
// record commands using borrowed views

vkQueueSubmit(..., gpuFrame.inFlight);
gpuFrame.lastSubmission = storage.noteSubmission(window.id, frame.frameSlot);
```

If acquisition or recording aborts before submission, call `cancelFrame(frame)`. If Vulkan submission fails, cancel the frame or mark it failed before unwinding. A frame token must have exactly one terminal action: `noteSubmission` or `cancelFrame`.

When the fence later signals, call `noteCompleted()` and `collect()`. Do not infer completion from frame count alone.

## Window-management integration

### Split device and surface ownership

Current `VulkanContext` stores one `surface`. Change it to device-only app state:

```cpp
struct DeviceContext {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VmaAllocator allocator;
    QueueSet queues;
};

struct AppWindow {
    VkSurfaceKHR surface;
    Swapchain swapchain;
    // ...
};
```

The main surface may be used to select the initial present queue, but every additional surface must be checked for present support. If the chosen device/queue family cannot present to a requested window surface, window creation must fail cleanly or use a supported present queue created at device initialization. This policy cannot be fixed after device creation unless the necessary queues were requested up front.

### Split platform event polling from window refresh

Target backend interface:

```cpp
struct IWindowSystem {
    virtual void pollEvents() = 0; // exactly once, platform thread
};

struct IWindowBackend {
    virtual void refreshInputSnapshot() = 0; // per window
    virtual VkSurfaceKHR createSurface(VkInstance) = 0;
    // extent/title/cursor/clipboard...
};
```

GLFW callbacks remain on the platform thread and write only their owning window's queue. Worker threads receive an immutable drained `FrameInput` snapshot. Do not put a mutex operation on every input callback unless platform and consumer really run concurrently; a double-buffered handoff at frame start is cheaper and easier to reason about.

### Per-window swapchain and frame resources

`Swapchain::create()` must accept the window surface explicitly rather than reading `vk.surface`. Keep borrowed swapchain images separate from FlowUi-owned views:

```cpp
struct Swapchain {
    VkSurfaceKHR surface = VK_NULL_HANDLE;      // borrowed from AppWindow
    VkSwapchainKHR handle = VK_NULL_HANDLE;     // owned
    std::vector<VkImage> images;                // borrowed from swapchain
    std::vector<VkImageView> views;             // owned
};
```

`FrameVk` stays per window. Extend each slot with `SubmissionToken` and, optionally, a timeline value. The storage `FrameToken::frameSlot` must match the Vulkan frame slot exactly.

### Swapchain recreation without device-wide idle

Current code calls `vkDeviceWaitIdle()` and immediately destroys the old views/swapchain. For multi-window operation this stalls unrelated windows.

Progressive migration:

1. initial refactor may keep a per-window fence wait for every outstanding submission of that window;
2. create the new swapchain using `oldSwapchain` and a new resource generation;
3. switch future frames to new views/pipelines;
4. retire old swapchain-dependent objects after the maximum submission serial that used them;
5. destroy only when all relevant submissions complete.

Pipeline sharing should be keyed by swapchain format. A format already present in another window reuses the same immutable pipeline bundle. A new format creates one shared bundle; closing a window decrements its reference and retires the bundle only when unused.

### Window close

Closing window A must not wait for or invalidate window B:

1. stop starting new frames for A;
2. cancel any begun but unsubmitted storage frame;
3. record A's highest submission serial;
4. unregister/mark its storage scope closing with that serial;
5. detach platform callbacks and prevent new input;
6. after A's fences/tokens complete, destroy its descriptor pools, frame objects, swapchain views/swapchain, surface, UI context, and backend;
7. shared images/pipelines remain until their app-level references and submissions end.

`unregisterWindow()` should not erase arena/binding memory while any worker or renderer holds a view; window close therefore also needs job cancellation/join or an ownership epoch.

## Multi-threading model

### Thread roles

Recommended eventual roles:

- **platform/main thread:** GLFW initialization, window creation/destruction, `glfwPollEvents`, clipboard/cursor calls, immutable input snapshot publication;
- **UI workers:** one job per window, owning that window's `UiManager`/Clay context for the job duration;
- **conversion/record workers:** one job per window/frame slot, using only its worker arena, window binding snapshot, command buffer, and descriptor set;
- **resource mutation/upload owner:** drains manager mutations and upload requests at a defined publish point;
- **submission owner:** serializes `vkQueueSubmit`/present when sharing one queue, stores submission tokens, and processes completion.

Vulkan command pools are externally synchronized, so each recording worker must use a command pool not concurrently touched by another worker. A per-window/frame-slot pool meets this rule if only one job owns the slot.

### Snapshot/publish sequence

```text
Platform thread: poll events once
        |
        +--> drain storage/resource mutations (shared publish barrier)
        |
        +--> for each reusable window frame slot:
                beginFrame(window, slot)
                publish immutable FrameInput
                dispatch UI build job
                gather logical handles
                prepare per-window bindings
                apply descriptor deltas
                seal frame
                dispatch conversion/record job with read views
                queue completed command buffer for submission

Submission owner:
        submit -> noteSubmission -> store token on fence/timeline
        observe completion -> noteCompleted -> collect
```

An advanced implementation can loosen the app-wide publish barrier with stable paged tables and RCU-like epochs. It is not necessary for the first correct multi-window version.

### Locking rules

- no global storage mutex in command/glyph/instance/run loops;
- shared resource mutations occur through a queue or short table-specific locks;
- a window binding table has one writer during preparation and any number of readers only after sealing;
- frame arena has one owner; worker arenas each have one distinct owner;
- `LinearArena` is intentionally not thread-safe and must remain so for speed; the ownership contract supplies safety;
- telemetry should use per-thread/per-window counters folded outside hot loops, not one contended atomic for every cache lookup;
- never hold a storage mutex across Vulkan queue waits, shader file I/O, decode work, or memory copies proportional to resource size.

## Performance and data-layout recommendations

### Fixed number of interface dispatches

Target per rendered window/frame:

1. `beginFrame()`
2. `frameArena()` and zero or N `workerArena()` calls
3. one `prepareTextureBindings()` batch
4. zero or one mapped-buffer growth call on the cold path
5. one `beginMappedWrite()` and one `endMappedWrite()`
6. `sealFrame()`
7. `readView()` and `windowBindingView()`
8. `noteSubmission()` or `cancelFrame()`

That is a small constant independent of command/glyph count. Cache the interface reference/pointer in the orchestrator; do not call `resolveTexture()` per image.

### Output shape

Keep instance data array-of-structs initially because each draw consumes the full record and the shader layout already matches it. Add static assertions:

```cpp
static_assert(std::is_trivially_copyable_v<UiInstance>);
static_assert(sizeof(UiInstance) == 88);
static_assert(alignof(UiInstance) == 4);
```

If device benchmarks show bandwidth pressure, consider a packed shader-facing struct, for example half precision or packed radii/borders where quality allows. Do not introduce a CPU SoA-to-GPU-AoS transpose without evidence; it creates another pass.

### Capacity planning

Record per window/frame slot:

- command count;
- instance and run count;
- maximum scissor depth;
- unique texture count;
- dirty descriptor count;
- scratch bytes requested/committed/high-water;
- instance-buffer used/capacity/growth count;
- bytes flushed;
- binding hit/miss count;
- time in gather, prepare, convert, descriptor update, upload/flush, record, submit, and fence wait.

Use a high percentile plus headroom to choose defaults. Keep growth geometric (1.5× is less wasteful than the current 2×) and retain overflow pages for several frames before trimming to avoid growth/thrash cycles.

### Avoid wasteful copies/moves

- write instances directly to the mapped lease or one arena span;
- emit glyph instances directly from layout callbacks;
- use spans and counts rather than returning vectors by value;
- reserve/allocate exact upper bounds once per frame;
- use stable `WindowId`/handles instead of string keys in frame code;
- intern debug/resource names only on creation;
- cache native buffer handles per handle generation, outside loops;
- use sorted/epoch-marked texture gathers, not node maps;
- store descriptor applied revisions in contiguous arrays;
- build diagnostic counts during conversion instead of rescanning output;
- avoid `std::vector<bool>` for concurrent/per-frame dirty state; use bytes or revision integers.

## Proposed interface changes before adoption

The minimum additions/changes are:

```cpp
class IStorageSystem {
public:
    // Existing lifecycle/window/frame methods remain.

    virtual PreparedTextureBindings prepareTextureBindings(
        const FrameToken&,
        std::span<const TextureHandle>) = 0;

    virtual MappedBufferWrite beginMappedWrite(
        const FrameToken&,
        BufferHandle,
        uint64_t offset,
        uint64_t bytes) = 0;

    virtual void endMappedWrite(
        const FrameToken&,
        const MappedBufferWrite&,
        uint64_t bytesWritten) = 0;

    virtual MemoryBlock allocatePersistent(
        size_t bytes,
        size_t alignment,
        AllocationTag tag) = 0;
};
```

Also define either:

- a typed native-object/backend service for descriptor/pipeline/window Vulkan objects; or
- explicit ownership/accounting hooks for objects outside buffer/image storage.

Do not expand the hot interface with getters that invite per-item virtual calls. Prefer coarse operations returning compact borrowed views.

## Concrete before-and-after renderer shape

### Before

```cpp
void VulkanUiRenderer::render(...) {
    BuildInstancesAndRunsFromClay(
        renderCommands,
        inputFieldOverrides,
        extent,
        fontManager_,
        pointsToPixelsScale_,
        scaleX,
        scaleY,
        instancesScratch_,
        runsScratch_);

    EnsureInstanceBufferCapacity(vk, *this, frameSlot, instancesScratch_.size());
    UploadBytesToMappedBuffer(
        vk,
        instanceBuffersByFrame_[frameSlot],
        instancesScratch_.data(),
        instancesScratch_.size() * sizeof(UiInstance));

    for (const UiRun& run : runsScratch_) {
        FlushRun(cmd, *this, extent, run);
    }
}
```

### After

```cpp
UiRenderResult VulkanUiRenderer::record(
    const SharedUiRendererResources& shared,
    WindowUiRendererResources& window,
    IStorageSystem& storage,
    const FrameToken& frame,
    VkCommandBuffer cmd,
    const UiRenderInput& input)
{
    ArenaView scratch = storage.frameArena(frame, MemoryClass::FrameTransient);

    const RenderUpperBound bound = countUpperBound(input.commands, input.overrides);
    auto textures = gatherUniqueTextures(input.commands, scratch, frame.epoch);
    PreparedTextureBindings prepared = storage.prepareTextureBindings(frame, textures);
    applyDescriptorDeltas(window, frame.frameSlot, prepared, scratch);

    UiFrameResources& slot = window.frames[frame.frameSlot];
    ensureInstanceCapacity(storage, window, slot, bound.instances * sizeof(UiInstance));
    MappedBufferWrite mapped = storage.beginMappedWrite(
        frame, slot.instanceBuffer, 0, bound.instances * sizeof(UiInstance));

    std::span<UiRun> runs = scratch.allocateArray<UiRun>(bound.runs);

    storage.sealFrame(frame);
    WindowBindingView bindings = storage.windowBindingView(frame);

    RenderBuildResult built = buildDirect(
        input,
        bindings,
        reinterpret_cast<UiInstance*>(mapped.data),
        bound.instances,
        runs);

    storage.endMappedWrite(frame, mapped, built.instanceCount * sizeof(UiInstance));
    recordUiCommands(shared, window, slot, cmd, input.extent,
                     runs.first(built.runCount), built.instanceCount);
    return {built.instanceCount, built.runCount};
}
```

There is one important ordering choice to settle in implementation: if `endMappedWrite()` mutates storage tracking, it must be legal after sealing, or the write must be ended before sealing. Prefer completing all writes and binding preparation before sealing, then use sealed views strictly read-only during command recording.

## Failure and transactional behavior

Every growth/publication path should follow “create, validate, publish, retire,” never “destroy, then try to create.”

- **Instance buffer growth failure:** keep the old handle/descriptor and report required versus available capacity. Do not leave a null slot.
- **Descriptor generation failure:** keep current descriptor generation for frames that fit; bind fallback for unresolved textures; surface diagnostics.
- **Swapchain recreation failure:** retain the old swapchain if Vulkan permits and keep the window paused; do not affect other windows.
- **Invalid/stale texture handle:** bind slot zero fallback and increment an invalid-handle counter; never index native arrays unchecked.
- **Frame exception before submit:** `cancelFrame()` through an RAII frame guard.
- **Submission failure:** do not issue a normal `SubmissionToken`; transition app/device error state deliberately.
- **Shutdown:** stop jobs, wait only as required, destroy window scopes before shared resources, shut storage down before VMA/device destruction, and make repeated cleanup safe.

Suggested guard:

```cpp
class StorageFrameGuard {
public:
    ~StorageFrameGuard() { if (active_) storage_.cancelFrame(token_); }
    void submitted(SubmissionToken token) noexcept { active_ = false; submission_ = token; }
private:
    IStorageSystem& storage_;
    FrameToken token_;
    bool active_ = true;
    SubmissionToken submission_{};
};
```

## Migration plan

### Phase 0 — Make storage real and verified

1. fix `Handle::fromPacked()`;
2. add storage sources/headers to CMake and CI;
3. add storage unit tests using Vulkan where necessary and a non-Vulkan fake for lifecycle logic;
4. add mapped write/flush semantics;
5. define descriptor dirty-batch/capacity semantics and a real fallback binding;
6. replace or document the global sealed-frame barrier;
7. fix O(U²) use tracking and forbid trimming active arenas;
8. enforce attribution/alignment/budget rules;
9. validate with ASan, UBSan, TSan where applicable, and Vulkan validation layers.

**Exit condition:** storage compiles in every supported configuration and passes handle, arena, binding, upload, retirement, and multi-window lifecycle tests independently of renderer adoption.

### Phase 1 — Introduce `AppWindow`, preserve one public window

1. split device context from surface;
2. move window backend, input, UI, surface, swapchain, frames, renderer window state, and timing into `AppWindow`;
3. split platform polling from per-window input refresh;
4. add stable internal `WindowId` and main-window lookup;
5. store submission token per `FrameVk::Frame`;
6. keep existing no-argument `App` methods routed to the main window.

**Exit condition:** behavior is unchanged, but no per-window field remains directly in app-shared state.

### Phase 2 — Move renderer byte memory to storage

1. create instance buffers with `createBuffer()` per window/frame slot;
2. create placeholder images/views/sampler and optional quad buffer through storage;
3. replace renderer CPU vectors and per-text glyph/scissor vectors with frame-arena spans/direct emission;
4. implement transactional generation-based buffer growth;
5. connect fence completion to `noteCompleted()` and collection;
6. retain current texture slot semantics temporarily through an adapter.

**Exit condition:** the renderer owns no VMA allocation and steady-state conversion performs no general heap allocation.

### Phase 3 — Adopt logical textures and per-window bindings

1. change `TextureRef` to a generational logical `TextureHandle`;
2. gather unique handles once per frame;
3. prepare bindings in one storage call;
4. update only dirty descriptors for the current frame slot;
5. use sealed binding spans in the hot conversion loop;
6. remove `UiTextureRegistry` slot ownership and device-idle growth;
7. keep a compatibility adapter only at manager boundaries while managers are still unmigrated.

**Exit condition:** the same texture handle renders in two internal windows with different local descriptor indices, and no command stores a window-specific slot.

### Phase 4 — Enable multiple windows

1. add internal creation/destruction first, then public `WindowId` APIs;
2. verify present support for each surface;
3. create per-window swapchain/frame/descriptor/storage scope;
4. poll events once and process windows independently;
5. close/recreate one window without device-wide idle or impact on another;
6. share pipeline bundles by compatible format.

**Exit condition:** windows can advance at different rates, block on different fences, resize, and close independently while sharing logical resources.

### Phase 5 — Parallel build and recording

1. establish mutation publish barrier or stable-page snapshot system;
2. assign one worker arena and command pool owner per job;
3. dispatch one UI/conversion/record job per reusable window;
4. serialize only queue submission/presentation as required;
5. replace global telemetry contention with local counters;
6. run race tests and measure scaling.

**Exit condition:** parallelism adds no storage calls/locks in hot loops and produces deterministic output equivalent to single-threaded recording.

## Required validation and benchmarks

### Build/static checks

- storage translation unit compiled by all normal library targets;
- public/internal header self-compilation tests;
- static assertions for handle and renderer record sizes/alignment;
- no duplicate VMA implementation definitions;
- storage shutdown occurs before `VulkanContext::destroy()`.

### Storage correctness tests

- stale generations never resolve after slot reuse;
- frame arena pages do not relocate prior allocations during growth;
- arenas reset only after slot completion;
- worker arenas never overlap;
- view epochs reject stale access in debug mode;
- replacement retains the old generation through its last submission;
- out-of-order completion advances the watermark correctly;
- closing one window preserves shared resources used by another;
- descriptor indices respect configured/device capacity;
- queued/failed/invalid textures resolve to a valid fallback;
- mapped write flush is visible on non-coherent memory;
- budget failure is transactional;
- shutdown reports no live FlowUi-owned resources.

### Renderer tests

- representative steady-state frame causes zero general heap allocations in conversion/recording;
- 0 commands, only scissor commands, deeply nested scissors, and custom commands;
- very long text and many small text commands without per-text allocation;
- input text overrides with sorted ranges;
- exact instance-buffer capacity boundary and multiple growth generations;
- texture replacement between two frame submissions;
- per-frame descriptor revisions are not overwritten while in flight;
- stale texture handle binds fallback rather than another resource;
- instance/direct-write output byte-for-byte matches the current renderer for a fixture set;
- validation layers report no lifetime, synchronization, or descriptor errors.

### Multi-window tests

- one logical texture produces distinct descriptor indices in two windows;
- two windows use different frame counts/slots and progress independently;
- one minimized window does not block another;
- resize one window while the other submits continuously;
- close a window with frames in flight;
- two windows with the same and different target formats;
- per-window input, cursor, clipboard ownership, and UI/Clay context isolation;
- additional surface present-support failure is reported without corrupting the app.

### Performance gates

Measure before and after on fixed UI fixtures:

- CPU allocations and allocated bytes per frame;
- conversion time per command and per glyph;
- instance bytes written and copied;
- descriptor writes per dirty texture, not descriptor capacity;
- interface calls and mutex acquisitions per frame;
- fence-wait, queue-submit, and swapchain-recreate stalls;
- instance/scratch/descriptor high-water and wasted capacity;
- single-window regression and 2/4-window scaling.

Recommended acceptance gates:

- zero general heap allocations in steady-state UI conversion/recording;
- zero virtual calls and locks in command/glyph/run loops;
- at most one large instance-data write/copy and one flush per rendered window/frame;
- descriptor update work proportional to changed bindings;
- no routine `vkDeviceWaitIdle()` outside final shutdown/device-loss recovery;
- no duplicate heavy renderer resource per window unless format/device compatibility requires it.

## File-level implementation map

| File | Required change |
|---|---|
| `CMakeLists.txt` | Compile/link storage and add tests/header checks. |
| `include/internal/StorageSystem/StorageTypes.hpp` | Fix syntax; add mapped-write and descriptor-delta types; clarify leases/capacity. |
| `include/internal/StorageSystem/IStorageSystem.hpp` | Add coarse mapped-write/descriptor batch/scoped allocation contracts. |
| `src/Storagesystem/FlowStorageSystem.cpp` | Harden tables, leases, per-window concurrency, binding batches, flush, attribution, and retirement. |
| `include/Ui/Vk_UiRenderer.hpp` | Split shared/window/frame renderer state; replace owned VMA objects/vectors with handles/views. |
| `src/Ui/Vk_UiRenderer.cpp` | Arena/direct emission, batched binding resolution, dirty descriptor writes, storage buffers, state-change minimization. |
| `src/FlowUi.cpp` | Add `AppWindow`, storage lifecycle, frame-token/submission bridge, per-window orchestration. |
| `include/Vulkan/Vk_Context.hpp` / `.cpp` | Remove single-surface ownership; expose device/queue compatibility checks. |
| `include/Vulkan/Vk_Swapchain.hpp` / `.cpp` | Accept explicit surface; generation/retirement-friendly recreation. |
| `include/Vulkan/Vk_Frames.hpp` / `.cpp` | Store submission token/timeline value per slot; remain per window. |
| `include/window/IWindow.hpp` / `Window.hpp` | Split global event polling from per-window refresh; maintain platform-thread ownership. |
| `include/FlowUi/PublicStructs.hpp` | Replace texture slot ID with logical generational handle, with ABI migration plan. |
| `UiManager` texture storage boundary | Copy logical handle values into frame storage; manager internals can migrate later. |

## Definition of done for this renderer/window upgrade

The renderer/window portion of the storage upgrade is complete when:

- the storage implementation is part of the real build and independently tested;
- every renderer-owned buffer/image/view/sampler is represented by and retired through storage;
- remaining Vulkan control objects have explicit owners and accounting/retirement policy;
- `App::Impl` contains app-shared state plus stable `AppWindow` entries, not one embedded window;
- every window has an independent storage scope, UI context, swapchain, frames, descriptor state, scratch, and submission lifecycle;
- shared pipelines/fallbacks/immutable buffers exist once per compatible app/device configuration;
- `TextureRef` holds logical generational identity and is resolved per window;
- renderer hot loops use only contiguous frame-local output and sealed read views;
- steady-state renderer conversion/recording allocates no general heap memory;
- instance growth is transactional and old buffers are serial-retired;
- descriptor updates are batched and proportional to changes;
- frame arena reset, resource destruction, window close, and swapchain retirement are driven by actual completion;
- two windows can build/record concurrently without shared mutable renderer state;
- resizing or closing one window does not call device-wide idle or stall unrelated windows;
- diagnostics can explain where CPU/GPU memory resides by app, window, frame slot, memory class, resource kind, live bytes, retired bytes, and high-water mark.

## Final recommendation

Do not begin by passing `IStorageSystem&` into the existing monolithic `VulkanUiRenderer` and mechanically replacing `vmaCreateBuffer()` calls. That would centralize allocation while preserving the wrong ownership boundaries, retain window-specific texture IDs, and add lock/virtual overhead without delivering multi-window safety.

Begin with storage Phase 0 and the `AppWindow` split. Then make the renderer a consumer of storage-owned generations and borrowed frame views. The intended steady-state data path should be simple:

```text
logical Clay commands
    -> gather logical texture handles
    -> one per-window binding preparation batch
    -> one sealed contiguous binding view
    -> direct instance emission into one frame-slot write stream
    -> compact run array in frame arena
    -> Vulkan recording with shared pipelines + window-local descriptors
    -> submission serial
    -> completion-driven arena reuse and resource retirement
```

That architecture turns the storage upgrade into a performance upgrade: fewer heap allocations, fewer copies, no descriptor-wide rewrites, no per-item abstraction overhead, explicit GPU lifetimes, independent windows, and a clear path to parallel recording.
