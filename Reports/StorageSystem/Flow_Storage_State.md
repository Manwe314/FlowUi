# Flow Storage System: Current Implementation State

## Scope of this step

This step adds the storage engine without adopting it in `App`, managers, `UiManager`, or `VulkanUiRenderer` yet.

Added files:

```text
include/internal/StorageSystem/
    StorageTypes.hpp
    IStorageSystem.hpp
    FlowStorageSystem.hpp

src/Storagesystem/
    FlowStorageSystem.cpp
```

`FlowStorageSystem` implements the complete current `IStorageSystem` surface. It uses the existing `VulkanContext`, logical generational handles, centralized VMA resource ownership, window/frame storage scopes, batched binding preparation, contiguous hot read views, synchronous central uploads, submission serials, deferred retirement, budgets, and diagnostics.

No existing FlowUi code or CMake source list was changed. Adoption is intentionally a later step.

---

## Core runtime protocol

The intended frame order is:

```text
registerWindow
    -> beginFrame
    -> manager/UI work and resource lookup
    -> prepareTextureBindings once with unique texture handles
    -> sealFrame
    -> obtain StorageReadView and WindowBindingView
    -> build instances through contiguous arrays
    -> noteSubmission
    -> noteCompleted when the owning fence/timeline completes
    -> collect retired resources
```

`StorageReadView` and `WindowBindingView` are the performance boundary. Calling the virtual interface is not required for every glyph, image, or quad.

```cpp
const StorageReadView storageView = storage.readView(frame);
const WindowBindingView bindingView = storage.windowBindingView(frame);

const TextureHotRecord* texture = storageView.texture(textureHandle);
const BindingHotRecord* binding = bindingView.binding(textureHandle);
if (texture && binding) {
    instance.texIndex = binding->descriptorIndex;
}
```

Both views contain spans over compact contiguous records. Handle lookup is an index bounds check and generation comparison. Cold keys, debug names, ownership counters, and submission data are not touched by this hot lookup.

Views are valid from `sealFrame()` until `noteSubmission()` or `cancelFrame()`. Resource-table mutations are rejected while any sealed frame snapshot is active, preventing vector relocation or record replacement underneath a view. Future mutation queues can replace this conservative synchronization rule without changing the public contract.

---

## Interface function implementation summary

### Lifecycle

| Function | Current implementation and policy |
|---|---|
| `initialize` | Validates the Vulkan device/VMA allocator, reserves handle-table capacities, creates CPU slabs, initializes budgets, and creates one central transient upload command pool. |
| `shutdown` | Idempotent. Waits for the device only at final shutdown, destroys views before images, destroys samplers/buffers/images and the upload pool, then clears storage state. |
| `interfaceVersion` | Returns `IStorageSystem::CurrentInterfaceVersion`, currently `1`. |
| `capabilities` | Reports runtime growth, windows, worker arenas, generational handles, batched bindings, synchronous uploads, deferred retirement, and Vulkan interop. |

### Window and frame scopes

| Function | Current implementation and policy |
|---|---|
| `registerWindow` | Creates fixed frame-slot records, one transient arena and one decode arena per slot, one arena per configured worker, and a texture-indexed binding vector. |
| `unregisterWindow` | Destroys immediately when its last-use serial is complete; otherwise marks the scope closing until `noteCompleted` advances far enough. |
| `beginFrame` | Requires a free frame slot, resets all linear arenas and used-resource lists, and issues a new frame epoch. |
| `sealFrame` | Freezes resource-table mutation and binding preparation for the frame. It establishes read-view validity. |
| `cancelFrame` | Clears the unsubmitted frame, invalidates its epoch usage, and makes the slot reusable without stamping resources. |

The implementation does not wait on Vulkan fences itself. `AppWindow` will remain responsible for proving a frame slot reusable before calling `beginFrame` and for calling `noteCompleted` after fence/timeline completion.

### CPU allocation and strings

| Function | Current implementation and policy |
|---|---|
| `allocatePersistent` | Uses a non-relocating slab pool with aligned free blocks, splitting and coalescing. Growth appends a slab using the configured factor. |
| `releasePersistent` | Returns the exact allocation ID range to its originating pool and merges adjacent ranges. |
| `frameArena` | Returns a function-pointer-based non-owning view over the frame or decode bump arena. |
| `workerArena` | Returns the selected worker's isolated bump arena. |
| `intern` | Copies a unique string once into the string slab and maps content to a compact `StringId`. String zero is empty/invalid. |
| `string` | Bounds-checks the ID and returns a stable `string_view`. |
| `createBlob` | Copies immutable bytes into persistent storage and creates a generational blob record. |
| `readBlob` | Returns a borrowed span after index/generation validation. |
| `releaseBlob` | Reference-counts, marks the final reference retiring, and reclaims only after its last-use serial. |

Transient arenas grow by appending non-relocating pages. Overflow pages are retained for reuse, while `trim` can discard them. Individual transient allocations are never freed; an entire frame/worker epoch resets together.

### Vulkan resources

| Function | Current implementation and policy |
|---|---|
| `createBuffer` | Validates the description/budget, converts storage usage to Vulkan/VMA flags, creates a buffer allocation, optionally persistently maps it, and records exact size/state. |
| `createImage` | Validates dimensions/format/usage/budget, creates a VMA image, records its allocation size and current layout, and leaves uploadable images queued until content is flushed. |
| `createImageView` | Validates image generation and subresource range, creates the Vulkan view, retains the image dependency, and publishes a compact hot native-view record. |
| `acquireSampler` | Normalizes and hashes sampler policy. Matching descriptions share one Vulkan sampler and increment its reference count. |
| `releaseBuffer` | Drops ownership and queues the buffer at the maximum explicit/observed use serial when its final reference is gone. |
| `releaseImage` | Drops image ownership; image-view dependencies keep it alive until their views are destroyed. |
| `releaseImageView` | Queues the view, then drops its retained image reference when the view is actually destroyed. |
| `releaseSampler` | Drops the shared sampler reference, removes the cache key at zero, and retires the Vulkan sampler safely. |

Images are treated as immutable by default. Mutable uses such as viewport targets are expressed through `AccessMode` and future manager policy; storage identity and retirement remain the same.

### Logical textures and bindings

| Function | Current implementation and policy |
|---|---|
| `publishTexture` | Creates a stable logical texture handle for a structured `ResourceKey`, retains its image view and sampler, and fills compact hot/cold records. |
| `replaceTexture` | Keeps the texture handle stable, increments its revision, atomically points future frames to the new backing view/sampler, and retires old dependencies after observed use. |
| `removeTexture` | Removes key lookup immediately, marks the handle retiring, and delays generation reuse until safe. |
| `findTexture` | Performs a cold structured-key lookup and returns a logical handle. |
| `textureMetadata` | Returns state, source dimensions, and revision without exposing Vulkan objects. |
| `prepareTextureBindings` | Resolves a batch once before sealing. It allocates/reuses window-local logical descriptor indices and fills `BindingHotRecord` entries by texture index. |
| `resolveTexture` | Resolves one cache miss when needed before sealing. It is not intended as the per-glyph path. |
| `invalidateWindowBindings` | Forces a window's cached texture revision to be regenerated on its next preparation pass. |
| `readView` | Returns contiguous texture/image-view/sampler hot spans for a sealed frame. |
| `windowBindingView` | Returns the selected window's texture-indexed binding span for a sealed frame. |
| `windowSnapshot` | Reports binding and transient capacity/high-water information for diagnostics. |

Descriptor indices are currently logical assignments. `FlowStorageSystem` intentionally does not create renderer descriptor layouts or sets, because those do not exist in the standalone subsystem's input. During renderer adoption, the renderer will batch-write `nativeImageView` and `nativeSampler` from prepared binding records into its per-window descriptor generation.

Index zero is the fallback binding. It currently contains null native handles. App adoption must publish/install the actual placeholder image and sampler used by slot zero.

### Uploads

| Function | Current implementation and policy |
|---|---|
| `enqueueUpload` | Validates ranges and destinations, retains the source blob and destination resource, assigns an `UploadId`, and queues a request. |
| `uploadState` | Reads the ticket state: queued, uploading, ready, failed, or invalid. |
| `flushUploads` | Uses the shared upload command pool, creates mapped staging storage, copies to a buffer/image, performs image layout transitions, submits, waits synchronously, updates states, then releases retained dependencies. |

The first implementation deliberately preserves synchronous behavior. All upload identity and status are already asynchronous-shaped. Replacing the staging allocation per request with a persistent ring and replacing `vkQueueWaitIdle` with timeline/fence completion are explicit polishing tasks.

When an image upload changes state, logical texture hot records referencing it are refreshed and revisioned. A loading/failed texture binding returns no native backing, allowing the renderer to choose slot zero.

### Submission, retirement, and reclamation

| Function | Current implementation and policy |
|---|---|
| `trackUse` | Adds a buffer/image once to the frame's compact used-resource list. Texture resolution automatically tracks the texture and its transitive view/image/sampler. |
| `noteSubmission` | Requires a sealed frame, issues a globally monotonic serial, stamps all used resources, and ends the frame epoch. |
| `noteCompleted` | Accepts out-of-order completion, stores gaps, and advances only the contiguous completed watermark. It also releases closing windows. |
| `completedSerial` | Returns the contiguous safe-destruction watermark. |
| `retire` | Typed dispatch for blob, buffer, image, image view, sampler, or texture release. |
| `collect` | Extracts all safe records before destruction, then destroys them dependency-first. Newly generated dependency retirements are processed in the same collection pass when safe. |
| `trim` | Releases retained transient overflow pages until the requested target is approached. |

Routine collection never calls `vkDeviceWaitIdle`. Only explicit synchronous upload flushing waits for its queue, and only final shutdown waits for the device.

### Diagnostics and interop

| Function | Current implementation and policy |
|---|---|
| `stats` | Reports CPU pool reservation/live/peak/growth, GPU live/retired/peak bytes, uploads, binding hits/misses, invalid handles, budgets, windows, and serials. |
| `resourceStats` | Scans one typed table and reports slots, state counts, live bytes, and retired bytes. This is cold diagnostic work. |
| `validateHandle` | Dispatches to the typed index/generation/state check. |
| `setBudget` | Replaces non-zero CPU/GPU soft limits. GPU creation observes the current GPU budget. |
| `nativeBuffer` | Returns a borrowed opaque native buffer value, byte size, and optional mapped pointer. |
| `nativeImage` | Returns a borrowed opaque image value plus format and dimensions. |

Native values are stored as `uint64_t` so general storage headers do not expose Vulkan types. Their meaning is valid only for the Vulkan-backed `FlowStorageSystem` and the lifetime of the corresponding storage handle.

---

## Identifier and integer map

### Global identifiers

| Name/type | Width | Zero meaning | Represents | Allocation/reuse policy |
|---|---:|---|---|---|
| `WindowId` | 64-bit | Invalid/app-shared depending on field | One registered native-window storage scope | Assigned by future `App`; storage never generates or reuses it |
| `SubmissionSerial` | 64-bit | No submitted GPU use | Global order of submissions across every window | Monotonic; never reused during one storage lifetime |
| `FrameEpoch` | 64-bit | Invalid | One activation of one window frame slot | Monotonic; changes every `beginFrame`, detecting stale frame/arena/view use |
| `StringId` | 32-bit | Empty/invalid string | One app-wide interned string | Append-only for the storage lifetime; never reused |
| `AllocationId` | 32-bit | Invalid allocation | One live persistent-pool allocation | Monotonic ID; current implementation does not recycle IDs |
| `UploadId` | 64-bit | Invalid upload ticket | One queued upload operation | Monotonic; never reused during one storage lifetime |

### Generational resource handles

Every typed handle is two 32-bit integers:

```text
packed 64-bit handle
63                       32 31                        0
+--------------------------+--------------------------+
| generation               | table index              |
+--------------------------+--------------------------+
```

| Field | Meaning |
|---|---|
| `index` | Direct position in that resource kind's table. Index zero is invalid/fallback and is never a normal allocation. |
| `generation` | Identity incarnation of that table slot. It begins at one and increments when a retired record is recycled. Zero is never a valid generation. |

The index provides constant-time contiguous access. The generation prevents a stale handle from resolving to an unrelated resource later placed in the same index.

Typed aliases and their meanings:

| Handle | Resource table/meaning |
|---|---|
| `BlobHandle` | Immutable CPU byte blob |
| `BufferHandle` | VMA/Vulkan buffer record |
| `ImageHandle` | VMA/Vulkan image record |
| `ImageViewHandle` | Vulkan image view plus retained image dependency |
| `SamplerHandle` | Deduplicated Vulkan sampler |
| `TextureHandle` | Stable logical UI texture view; not a descriptor slot |
| `FontFaceHandle` | Reserved typed identity for font storage adoption |
| `FontAtlasHandle` | Reserved typed identity for font atlas adoption |
| `SvgDocumentHandle` | Reserved typed identity for parsed SVG adoption |
| `IconVariantHandle` | Reserved typed identity for an atlas region/variant |
| `ViewportHandle` | Reserved typed identity for window-scoped viewport storage |

The reserved handle types establish the integer contract now. Their semantic manager records will be added when those managers migrate; they cannot be accidentally interchanged with existing resource types because the C++ handle types differ.

### Structured keys and tags

`ResourceKey` replaces namespaced concatenated strings.

| Field | Meaning |
|---|---|
| `domain` | `Image`, `Icon`, `Font`, `Viewport`, `Renderer`, `Layout`, `Input`, `Development`, or `Internal` |
| `name` | Interned user/debug name as `StringId` |
| `window` | Zero for app-shared resources, otherwise the owning `WindowId` |

`AllocationTag` attributes bytes:

| Field | Meaning |
|---|---|
| `memoryClass` | Persistent, metadata, strings, window persistent, frame transient, worker transient, decode transient, or upload staging |
| `resourceKind` | The owning logical/backend resource category |
| `window` | Zero for app-shared or a window owner |
| `frameSlot` | `UINT32_MAX` when not frame-specific, otherwise the reusable frame slot |
| `debugName` | Optional interned diagnostic name |

### Frame and binding integers

| Integer | Meaning/reuse |
|---|---|
| `frameSlot` | Small reusable index in `[0, framesInFlight)`. It is safe to reuse only after the app observes completion. |
| `frameNumber` | Caller-provided logical frame counter used for telemetry/debugging; storage does not derive safety from it. |
| `workerIndex` | Stable index into a frame's isolated worker arenas. |
| `descriptorIndex` | Window-local logical descriptor slot. Zero is fallback. It may be reused only after the old texture binding is safely retired. |
| `revision` | Version of the backing data behind a stable texture handle. It changes on replacement or resource-state transition. |
| `bindingRevision` | Window-local version assigned whenever a binding record is regenerated or invalidated. |

`TextureHandle` is app-scoped and stable across windows. `descriptorIndex` is execution state and can differ for that texture in every window.

### Counts, capacities, and byte values

- Resource counts and table indices use 32 bits because shader/descriptors and contiguous tables naturally use 32-bit indexing.
- Byte sizes, budgets, upload ranges, native handles, frame numbers, and serials use 64 bits.
- Dimensions/layers/mips use 32 bits and are validated before Vulkan creation.
- `InvalidFrameSlot` is `UINT32_MAX`; normal frame slots start at zero.
- `ResourceKind::Count` and `MemoryClass::Count` are array extents only and never stored as resource values.

---

## Data organization

### Hot records

The hot arrays are:

- `TextureHotRecord[]`
- `ImageViewHotRecord[]`
- `SamplerHotRecord[]`
- one `BindingHotRecord[]` per window

They contain generations, revisions, dependent handle indices, resource state, dimensions, descriptor indices, and opaque native handles. They are compact and contiguous. The frame renderer fetches them by handle index.

### Cold records

Cold storage holds:

- structured keys;
- full creation descriptions;
- reference counts;
- last-use serials;
- VMA allocations and layouts;
- debug/interned names;
- upload requests;
- free lists and retirement records.

These records are touched for loading, replacement, diagnostics, and destruction, not per glyph.

### Reference graph

```text
TextureHandle
    retains ImageViewHandle
        retains ImageHandle
    retains SamplerHandle

UploadTicket
    temporarily retains BlobHandle
    temporarily retains BufferHandle or ImageHandle
```

User/manager ownership is one reference. Dependencies add references. Releasing a manager handle cannot destroy a resource still used by a view, texture, or queued upload.

---

## Synchronization policy in this implementation

The implementation uses one recursive synchronization mutex for control-path safety. This is deliberately conservative for the first isolated storage backend.

Performance is preserved by moving steady-state reads outside virtual calls and mutexes after sealing:

- mutation and binding preparation occur before `sealFrame`;
- hot arrays are borrowed as spans;
- the render loop performs direct index/generation checks;
- each worker has an isolated arena;
- shared resources are immutable after publication for that frame epoch.

Current rules:

1. Resource creation/replacement that can resize hot tables is forbidden while any sealed frame is active.
2. Binding preparation is forbidden after the target frame is sealed.
3. Read views require a sealed active frame.
4. Submission ends view validity.
5. Completion can arrive out of order, but reclamation uses only the contiguous global watermark.
6. Arena allocation assumes its frame/worker owner follows the single-writer rule.

Future polish is to replace mutation-phase rejection with per-thread mutation queues drained at a publish barrier and to replace the global control mutex with table/window-specific locks or a single storage owner thread.

---

## What remains to polish before full adoption

The functions are implemented, but the following design/engineering work remains intentionally visible.

### Storage metadata allocation

Resource payloads, strings, blobs, transient pages, and Vulkan allocations are storage-owned. The implementation's bootstrap STL containers (`vector`, `unordered_map`, `deque`) still obtain their own host allocations through the standard allocator.

Before claiming exact control of every FlowUi host byte, these containers should move to tracking `std::pmr` resources backed by storage metadata slabs. A small bootstrap allocator will remain necessary to create the storage system itself, but it can be bounded and reported separately.

### Descriptor backend integration

Window bindings allocate logical indices and native binding pairs, not actual descriptor pools/sets. Renderer adoption must add a descriptor-writer capability or provide per-window descriptor generation objects to storage. Growth should create a new descriptor generation and retire the old generation by serial, never wait for device idle.

### Actual fallback resource

Logical slot zero exists, but the standalone storage system does not create a colored placeholder image. App/renderer adoption must decide the fallback format and publish its image/view/sampler during initialization.

### Upload scheduler

The upload interface and tickets are ready, but the first implementation:

- creates one staging buffer per request;
- records one command buffer per request;
- waits for the graphics queue;
- does not use a transfer queue or timeline semaphore.

The next scheduler should use a persistently mapped staging ring, batch compatible copies/transitions, submit once, and release pages by completion value.

### Mutation queues and read leases

The current phase rule makes span validity clear but rejects mutation when another window has sealed a frame. Multi-threading should introduce queued mutations and an explicit immutable resource-table snapshot/publish barrier.

An RAII read lease may be preferable to implicit validity through `FrameToken`, especially if frame conversion becomes detached from submission.

### Fine-grained data layout

Hot records are compact contiguous Array-of-Struct records. Profiling may justify splitting them into Structure-of-Arrays fields for scans such as retirement/state updates. Direct lookup already touches one compact record, so the split should be measurement-driven rather than automatic.

### CPU budget enforcement and detailed attribution

GPU creation enforces the soft GPU budget. CPU statistics report major pool ownership, but CPU budget enforcement and attribution should include:

- STL/PMR metadata capacity;
- all window/frame/worker pages;
- staging allocations;
- Vulkan object host estimates;
- third-party opaque allocations.

`AllocationTag` already defines the required attribution schema.

### Reserved semantic handles

Font, SVG, icon-variant, and viewport handle types exist, but their typed semantic table APIs are not part of this first generic storage interface. Manager migration should add narrow capability interfaces or generic typed-record publication without turning `IStorageSystem` into a manager.

### Imported/borrowed resources

Swapchain images and user-provided Vulkan viewport resources need explicit imported-resource descriptions with ownership flags. They must never pass through normal destroy paths.

### Image layout and queue ownership

The current image layout record supports the synchronous upload path. Viewport/render-graph adoption needs per-subresource layout and queue-family ownership tracking, or an explicit contract declaring which subsystem owns transitions during each phase.

### Error/result surface

This implementation follows existing FlowUi style and throws `std::runtime_error`. The eventual internal API may benefit from typed `StorageError`/`expected` results, particularly for budget pressure, async upload failure, invalid handles, and device loss.

### Tests and build adoption

This step intentionally did not add the source to CMake or compile it. Before integration, add focused tests for:

- generation reuse and stale handles;
- reference dependency destruction;
- out-of-order completion gaps;
- replacement during in-flight submissions;
- two-window binding differences;
- frame/view epoch violations;
- upload retention and failure cleanup;
- arena growth/trim and alignment;
- shutdown idempotence.

---

## Immediate next adoption step

The safest first consumer is `ImageManager` in a still-single-window app:

1. Construct `FlowStorageSystem` after `VulkanContext` device/VMA creation.
2. Register a main-window storage scope.
3. Install the real fallback texture.
4. Replace `ImageManager` image/view/sampler/upload ownership with storage handles.
5. Preserve the current public `ImageManager` API.
6. Change `TextureRef` internally from descriptor slot to logical `TextureHandle`.
7. Adapt the existing renderer registry through batched `prepareTextureBindings` and binding views.
8. Only after this path is proven, migrate icons, fonts, viewports, UI arenas, and the renderer's own buffers.

That sequence validates the identity, upload, binding, and retirement protocols before the broader multi-window `AppWindow` refactor.
