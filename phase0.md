# Phase 0 storage-system implementation report

## Outcome and scope

Phase 0 is complete. `FlowStorageSystem` is now part of the library build, has an integration-ready version-2 internal contract, and is covered by isolated production, developer, sanitizer, concurrency, and Vulkan ownership tests.

As requested, this phase did **not** modify `App`, the UI renderer, any manager, or any manager-owned resource. It only prepared the storage layer and its tests for those consumers.

The shared UI renderer resources were moved into the storage ownership model. This is implemented with typed root resource tables rather than by putting C++ Vulkan wrappers into the raw persistent byte pool. Storage owns their native lifetimes, while a future renderer can cache the returned immutable native values during setup and make no storage virtual calls in draw loops. This gives centralized ownership without adding hot-path indirection.

## Changes made

### Build and interface

- Added `src/Storagesystem/FlowStorageSystem.cpp` and the internal storage headers to the `flowui` target.
- Fixed handle packing/parsing and bumped `IStorageSystem::CurrentInterfaceVersion` to 2.
- Added top-level test options and a `tests` CMake target. Tests default on for a top-level FlowUi build and off when FlowUi is consumed as a subproject.
- Kept all new APIs internal; no public application API changed.

### CPU pools, arenas, and allocation identity

- Persistent pools and linear arenas now honor power-of-two over-alignment using aligned allocation.
- Arena array allocation checks multiplication overflow.
- Persistent allocation IDs detect exhaustion, and releases validate the exact ID, pointer, size, and complete allocation tag. Duplicate or forged releases are safe no-ops.
- `AllocationTag` can attribute persistent allocations to a registered window.
- `FrameTransient`, `DecodeTransient`, and per-worker arenas are isolated per window/frame slot. Active or in-flight arenas cannot be trimmed or reused.
- Debug builds invalidate stale arena views. The validation state and high-water/growth counters do not exist in production layouts.

### Mapped buffer writes and coherence

The interface now supports both intended producer shapes:

- `DirectMapped`: construct UI instances directly in the mapped destination range;
- `HostScratchThenCopy`: construct into frame-arena memory, then perform one copy at commit;
- `writeBuffer(span)`: upload an already-built vector/span with exactly one direct copy.

`StorageConfig::defaultBufferWriteMode` selects the default producer lease. Commit performs a VMA flush only for non-coherent allocations, tracks the written buffer once for the frame, and never exposes the VMA allocation or mapped pointer through `NativeBufferView`.

Bulk copy/flush work runs without the root metadata lock. A temporary reference pins the allocation, cancellation waits for in-progress commits, overlapping writes are rejected, and non-overlapping ranges can be produced concurrently. A caller holding an uncommitted write view must still coordinate against cancellation; shutdown requires caller quiescence.

### Window, frame, and submission lifetime

- A sealed frame now produces a generation-checked `FrameReadLease`.
- A lease is consumed exactly once by `noteSubmission()` or invalidated by cancellation.
- Each frame slot records its exact in-flight submission serial and cannot be reused until the matching `SubmissionToken` completes.
- Out-of-order completion is supported. The normal in-order path advances the watermark without allocating a hash node.
- Resource retirement is serial-gated and dependency ordered. Retirement capacity is reserved before state changes, making release, submission, completion, and collection transitions recoverable on allocation failure.
- `WindowId` values cannot be reused during a storage-system lifetime. This prevents surviving window-local handles from becoming visible to a later window with the same numeric ID.
- `FrameLocal` means local to one frames-in-flight slot across that slot's epochs. Buffer, image, image-view, and texture use validates app/window/frame-slot visibility.
- Shutdown is idempotent but terminal: a shut-down instance cannot be reinitialized. It waits for commits already in progress, while the wider API still requires callers to be quiescent.

### Batched descriptor protocol and fallback

- Every window has a bounded descriptor capacity and slot 0 is reserved for a real, ready, root-shared fallback texture.
- `prepareTextureBindings()` returns an arena-backed compact dirty-write span and the required descriptor capacity.
- `acknowledgeTextureBindings()` validates that the renderer applied the current native handles and revisions; per-slot state can be reset safely.
- Descriptor indices, texture generations, and binding revisions are checked independently.
- One descriptor bundle generation is active per window. Adoption requires exact frame count and capacity agreement with the window and renderer layout, copies all supplied set spans, and transfers ownership of the pool only on success.
- Replacing a bundle clears every frame slot's applied revisions and retires the previous generation. Sealing automatically retains the active descriptor generation, so an in-flight frame cannot lose its pool even if the window adopts a replacement.

### Shared UI renderer resources in storage

Storage now has typed root ownership for:

- descriptor-set layouts and pipeline layouts through `RendererLayoutHandle`;
- the three UI pipeline variants through `RendererPipelineBundleHandle`;
- per-window descriptor pools and descriptor sets through `WindowDescriptorBundleHandle`.

Layouts and pipelines are keyed and reusable across windows. Duplicate publication returns the existing generation without taking ownership of the caller's duplicate objects; acquire-by-key returns a new strong reference without constructing duplicates. Pipeline and descriptor records retain their layout dependency. Destruction order is descriptor pools/pipelines first, then layouts.

All adopted native objects must originate from the storage instance's exact `VkDevice` and use null Vulkan allocation callbacks. This is a documented precondition because Vulkan does not provide a runtime provenance query.

The intended Phase 1/2 consumer pattern is to acquire/publish once, cache the native values in window renderer state, batch `trackUses()` for the selected layout/pipeline, and use borrowed array views in conversion loops. The active descriptor bundle is tracked automatically. There is no per-element virtual dispatch.

### Multi-window and threading policy

Phase 0 uses the documented publish-barrier design:

- shared texture/native-resource publication and collection happen when no frame is active;
- binding mutation rules are scoped to the affected window, so a sealed window no longer semantically blocks another window's lifecycle;
- short metadata operations are currently serialized by the storage mutex;
- mapped bulk production is unlocked, and sealed read/binding spans are consumed without virtual calls or locks in renderer loops;
- independent windows and frame slots may be submitted and completed out of order.

The expected handle-marker arrays and descriptor revision arrays are allocated from configuration bounds during window registration. Frame-use deduplication is O(1) by handle index, generation, and frame epoch instead of the previous O(U²) search.

### Developer telemetry versus production

`FLOW_UI_DEV_MODE` now controls storage diagnostics at compile time. Development builds include lease validation, debug names, high-water/growth values, cache hit/miss counts, invalid-handle counts, upload totals, resource scans, and window snapshots.

Production builds omit those fields and counter updates. `stats()`, `resourceStats()`, and `windowSnapshot()` remain ABI-safe internal calls but return empty values immediately without acquiring the storage lock. Reference counts, allocation totals needed for budgets, generations, state, ownership dependencies, and submission serials remain because they are functional correctness data rather than telemetry.

## Budget decision: option 1

The existing budget behavior was intentionally preserved:

- the CPU budget rejects new **live requested bytes** in the persistent and string pools;
- it does not include reserved slab capacity or window/frame/worker arena reservations;
- the GPU budget rejects new allocations based on live plus serial-retired VMA allocation bytes.

Consequently, the current `*SoftBudgetBytes` names describe compatibility fields, but exceeding them is an allocation rejection. Initial reserved CPU pages may also exceed the configured CPU live-byte budget. This is documented rather than silently changing behavior during Phase 0.

## Tests conducted

| Configuration | Result |
|---|---:|
| Production Release build, full `flowui` target | Passed |
| Production CTest | 2/2 passed |
| Developer Debug build, full `flowui` target | Passed |
| Developer CTest | 2/2 passed |
| Vulkan storage scenarios in each mode | 12/12 passed |
| ASan + UBSan developer build and CTest | 2/2 passed |
| TSan developer build and CTest | 2/2 passed |
| Strict prod/dev syntax audit (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion`) | Passed, no diagnostics |
| `git diff --check` | Passed |

The Vulkan suite ran on `llvmpipe (LLVM 15.0.7, 256 bits)` and covers initialization/terminal shutdown, aligned pools and forged releases, strings/blobs, leases and arena invalidation, exact slot completion, independent windows, direct/scratch/coherent writes, concurrent non-overlapping writes, frame-held deduplication, fallback and dirty descriptors, scope locality and window-ID tombstones, real renderer layouts and three distinct real compute pipelines, descriptor-pool replacement, ownership transfer, dependency retirement, and production/developer telemetry behavior.

The available device exposes only host-coherent host-visible memory, so the non-coherent flush branch compiled and was contract-audited but could not be executed on this machine. The Khronos validation layer is not installed, so validation-layer coverage was not claimed.

## Deliberately deferred boundaries

- The bootstrap `flushUploads()` path remains synchronous, holds the storage mutation lock, and calls `vkQueueWaitIdle()`. Replacing it with paged asynchronous staging is the deferred A8 work.
- `initialUploadStagingBytes` and `initialInstanceBytesPerFrame` are reserved for that upload work and Phase 2 renderer-buffer adoption. The global frame/worker sizing fields are currently expectations; explicit `WindowStorageDesc` values own actual per-window sizing. `detailedTracking` is also reserved for finer developer-only filtering.
- OS windows, surfaces, swapchains, command buffers, fences, and semaphores remain owned by the future `AppWindow`/Vulkan window backend, not the storage byte allocator.
- No existing renderer or manager consumes storage yet; that was intentionally outside Phase 0.

## Phase 1 footnote

Phase 1 should introduce `AppWindow` while preserving one public window: keep the device context and shared renderer layout/pipeline handles at app scope; move surface, swapchain, input/UI state, per-frame Vulkan objects, descriptor-bundle handle, and submission token into each `AppWindow`; split global event polling from per-window refresh; and wire each completed frame fence to its exact storage `SubmissionToken`. Renderer buffer migration remains Phase 2.
