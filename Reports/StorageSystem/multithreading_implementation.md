# Phase 6: configurable internal multithreading implementation plan

## Purpose

This report defines the final phase of the current StorageSystem and
multi-window architecture upgrade: a configurable internal execution system
with two modes:

```cpp
InternalExecutionMode::SingleThreaded
InternalExecutionMode::InternalWorkers
```

Both modes use the same storage ownership, frame snapshots, task entry points,
resource generations, and error paths. `SingleThreaded` executes those tasks
inline on the application thread. `InternalWorkers` executes eligible,
library-owned work on a fixed internal worker pool.

The central user-facing rule is:

> FlowUi application code continues to behave as app-thread code. Phase 6 does
> not execute user UI construction, application callbacks, shortcut callbacks,
> viewport render callbacks, clipboard/cursor callbacks, or public manager calls
> on internal workers.

The user does not need to add locks around ordinary FlowUi usage merely because
`InternalWorkers` is enabled. Public API ordering remains explicit and
synchronous at documented completion boundaries. Worker threads consume only
immutable snapshots or exclusively owned frame-slot data.

The “Phase 5 — Parallel build and recording” section of
`Ui_renderer_storage_integration.md` becomes Phase 6 in the repository's actual
sequence, because manager migration is now the completed Phase 5 described by
`phase5.md`.

## Baseline after Phase 5

The implementation already has most of the ownership structure needed for
threading:

- one app/device Vulkan context and StorageSystem;
- stable `AppWindow` entries keyed by monotonic `WindowId`;
- app-shared image, icon, font, fallback, renderer-layout, and pipeline
  resources;
- per-window UI/input/shortcut/viewport state;
- per-window/frame-slot swapchain, descriptor, instance-buffer, command, fence,
  and submission state;
- logical generational textures resolved into per-window descriptor bindings;
- destructor-aware StorageSystem manager records;
- frame tokens, frame arenas, worker arenas, read leases, submission tokens,
  exact completion, and generational retirement;
- captured font and manager revision views;
- renderer conversion that writes directly to storage-backed instance buffers;
- exact WSI retirement and no routine device-wide idle;
- one app polling safe point for shared maintenance and collection.

The remaining barriers are also concrete:

- `activeWindowFrame` serializes the entire `beginFrame/endFrame/drawFrame`
  triplet across all windows;
- StorageSystem still uses one `std::recursive_mutex` around unrelated app,
  window, upload, and retirement operations;
- shared mutation is rejected while sealed frames exist rather than publishing
  immutable generations for later readers;
- `flushUploads()` performs submission and a queue-wide wait while holding the
  storage lock;
- the renderer records UI into the primary window command buffer, whose owner is
  also responsible for viewport callbacks and swapchain transitions;
- worker arenas exist but no executor assigns them an exclusive worker owner;
- there is no task cancellation, error handoff, or shutdown/join protocol;
- manager nested allocations and some frame scratch still require the
  measurement/PMR tuning pass recorded in `phase5.md`.

Phase 6 removes these barriers without moving manager data again.

## Public configuration

Add one small public configuration block to `AppConfig`. Do not add independent
booleans for every subsystem; two coherent execution modes are easier to test,
document, and reason about.

```cpp
namespace FlowUi {

enum class InternalExecutionMode : uint8_t {
    SingleThreaded = 0,
    InternalWorkers = 1,
};

struct InternalExecutionConfig {
    // Compatibility and deterministic-reference default.
    InternalExecutionMode mode = InternalExecutionMode::SingleThreaded;

    // Number of library worker threads, excluding the application thread.
    // Zero means choose from hardware concurrency.
    uint32_t workerCount = 0;
};

struct AppConfig {
    WindowConfig window{};
    VulkanConfig vk{};
    UiConfig ui{};
    IconManagerConfig iconManager{};
    DevToolsConfig dev{};
    InternalExecutionConfig execution{};
};

} // namespace FlowUi
```

Rules:

- `SingleThreaded` creates no worker threads. `workerCount` is ignored.
- `InternalWorkers` creates a fixed pool during `App` construction.
- An explicit nonzero count is honored after validation against a conservative
  implementation maximum. Invalid/excessive values fail app construction with
  a clear error rather than being silently truncated.
- With `workerCount == 0`, use
  `max(1, std::thread::hardware_concurrency() - 1)` when hardware concurrency is
  known, subject to an internal safety maximum. Unknown hardware concurrency
  selects one worker.
- Failure to create the requested pool fails app construction transactionally.
  Do not silently fall back to single-thread execution after the user explicitly
  selected workers.
- Execution mode and worker count are immutable for the lifetime of `App`.
  Runtime pool replacement is out of scope.
- Storage `expectedWorkerCount` and every window's `workerCount` are derived from
  the effective executor count. Inline mode still provisions logical worker
  arena index zero so it runs the identical task implementation.

An optional read-only query is useful for diagnostics:

```cpp
struct InternalExecutionInfo {
    InternalExecutionMode mode = InternalExecutionMode::SingleThreaded;
    uint32_t workerCount = 0;
};

InternalExecutionInfo App::executionInfo() const noexcept;
```

It must reveal only immutable execution facts, not queues or native thread
handles.

## User-visible threading contract

### Application thread

`App` records the construction thread as its platform/application thread. The
following remain application-thread-only in both modes:

- all `App` methods, including window creation/destruction, polling, frame
  lifecycle, queries, and manager accessors;
- all public manager methods and mutable `ViewPort` methods;
- UI element event, logic, construct, and build callbacks;
- shortcut callbacks;
- viewport render callbacks;
- cursor and clipboard callbacks;
- developer capture/export callbacks and file operations;
- GLFW/platform window and input operations;
- Vulkan swapchain acquire, graphics queue submission, presentation, and WSI
  retirement operations.

Calls from a different thread are rejected with `std::logic_error` before
touching state. This check must exist in release builds as defined behavior, not
only as a debug assertion. Manager facades therefore need a small internal
execution/thread-affinity pointer or token; no thread object appears in public
manager headers.

The `App` destructor must run on the construction/platform thread. Since a
destructor cannot usefully report a recoverable thread-affinity exception, debug
builds assert and all documentation states this as a precondition.

### Values versus live facade access

Plain returned values such as `WindowId`, `TextureHandle`, `TextureRef`, font
IDs, dimensions, and copied strings may be moved or copied by user code like
ordinary values. This does not make the originating manager thread-safe.

Borrowed pointers, references, and views retain their current lifetime and
thread restrictions:

- `FontFaceData*` is app-lifetime stable but is not a license for concurrent
  manager mutation and arbitrary user reads;
- `ViewPort*` is valid until removal/window destruction and remains app-thread
  access;
- input-field `string_view` results remain valid only until their documented
  mutation/removal boundary and remain owning-window/app-thread views;
- frame/callback native Vulkan handles are valid only for the callback and frame
  generation that supplied them.

### Callback guarantee

No user-provided callable is invoked by the worker pool in Phase 6. Callback
order and thread identity remain the same in both execution modes.

This includes callback destruction: shortcut captures, viewport callback
captures, and manager record destructors are released on the app thread at a
quiescent lifecycle point.

### Synchronous public manager behavior

Public manager calls remain synchronous from the user's perspective. A call
such as image registration may use internal decode work, but it does not return
until its documented result is committed or it has failed. Exceptions are
rethrown on the application thread.

No public future, job handle, completion callback, or “resource eventually
appears” mode is introduced in Phase 6.

### Frame completion boundary

In worker mode, `endFrame(window)` may enqueue callback-free frame preparation
and return before that internal CPU work finishes. `drawFrame(window)` is the
mandatory join/error boundary for that frame:

- it waits for the window's internal job;
- it rethrows a captured worker failure on the app thread;
- it performs viewport callbacks, submission, and presentation only after a
  successful join;
- it leaves the window in `Idle` or an explicit failed/closing state before
  returning.

Inline mode runs the same job during `endFrame()`, so failures that are known
there can still surface there. Code must not depend on the exact call at which
an internal preparation error is reported; the full frame triplet is the
transactional unit. Errors in user callbacks continue to surface at the call
that invoked the callback.

## What Phase 6 parallelizes

| Work | Worker eligible now? | Phase 6 rule |
|---|---:|---|
| GLFW polling and per-window input refresh | No | Platform thread only. |
| User UI construction and Clay API calls | No | Remain synchronous on the app thread; avoids Clay current-context races. |
| UI element/shortcut/developer callbacks | No | App thread, existing order. |
| Clay render-command finalization | No | App thread at `endFrame()`. |
| Texture-handle gather and exact snapshot publication | Initially no | App-thread handoff step; cheap and establishes deterministic resource identity. |
| Text layout for independent text commands | Yes | Immutable font view; outputs written to disjoint preassigned spans or worker scratch. |
| UI instance conversion | Yes | Callback-free immutable command input and disjoint output ranges. |
| UI run compaction | Yes, final continuation | Stable command order regardless of task completion order. |
| Descriptor delta application | Yes | One safe window/frame-slot descriptor generation owned by the job. |
| UI Vulkan command recording | Yes | Dedicated per-window/frame-slot secondary command pool/buffer. |
| Viewport render callbacks | No | App thread during `drawFrame()`. |
| Viewport target sizing/remapping | No initially | App-thread frame handoff; it mutates window policy state. |
| Viewport target image allocation | Conditionally | May use resource-producer work, but publication/commit stays app-thread and callbacks never move. |
| Image file decode | Yes | Isolated candidate production; public call waits and commits on app thread. |
| SVG parsing/rasterization | Yes | Independent candidates/demands; deterministic app-lane commit. |
| Font parsing/baking | Yes where third-party APIs permit | Immutable candidate production; final ID/atlas publication is serialized. |
| Icon demand raster misses | Yes | Batch independent misses, then commit in stable key/size order before frame snapshot. |
| Upload byte packing/staging preparation | Yes | No Vulkan queue access from producer jobs. |
| Storage resource/manager publication | No | Serialized app resource/publication lane. |
| Vulkan upload/graphics queue submission | No | App submission owner with explicit queue synchronization. |
| Fence/present completion observation | No | App polling/frame boundaries; retirement classification may use workers only over copied data. |
| Telemetry aggregation | Yes | Per-worker counters folded at safe points; no per-item contended atomics. |

Phase 6 should prioritize coarse tasks whose work dominates scheduling overhead:

1. one renderer preparation/secondary-recording job per prepared window;
2. parallel text-layout subjobs for text-heavy single-window frames;
3. batched icon raster misses;
4. large image/font decode or bake candidates;
5. background telemetry folding and measured container tuning.

Do not dispatch a task for trivial lookups, one rectangle, one descriptor write,
or a tiny memcpy. Internal thresholds may be derived from benchmarks; they are
not public configuration in this phase.

## Work that remains deliberately single-threaded

Phase 6 does not add:

- worker-owned `UiManager` or concurrent Clay contexts;
- worker execution of arbitrary user UI code;
- worker execution of viewport render callbacks;
- a public task/future API;
- a user-supplied executor;
- simultaneous calls into one public manager from several user threads;
- a dedicated queue-submission or presentation thread;
- concurrent frames for the same window;
- runtime switching between execution policies.

Those features require separate public contracts. In particular, future
parallel UI building must first solve or replace Clay's process-current-context
behavior, and future worker viewport callbacks require an explicit callback
thread-safety opt-in. They must not be smuggled into `InternalWorkers`.

## Internal executor architecture

### One interface, two implementations

Add a private executor package under `include/internal/Execution` and matching
sources:

```cpp
namespace FlowUi::detail::execution {

enum class TaskStatus : uint8_t {
    Empty,
    Queued,
    Running,
    Completed,
    Cancelled,
    Failed,
};

struct WorkerContext {
    uint32_t workerIndex = 0;
    std::stop_token stop;
};

using TaskFunction = void (*)(WorkerContext&, void* context);

struct TaskDesc {
    TaskFunction function = nullptr;
    void* context = nullptr;
    uint64_t sequence = 0;
};

class IInternalExecutor {
public:
    virtual ~IInternalExecutor() = default;
    virtual TaskHandle submit(TaskDesc task) = 0;
    virtual void requestCancel(TaskHandle task) noexcept = 0;
    virtual void wait(TaskHandle task) = 0;
    virtual void drain() = 0;
    virtual void stopAndJoin() noexcept = 0;
    virtual uint32_t workerCount() const noexcept = 0;
};

} // namespace FlowUi::detail::execution
```

Concrete names may differ, but the semantics must not.

`InlineExecutor` invokes the task entry point immediately with logical worker
index zero, captures failures in the same task state, and uses all the same
completion code.

`WorkerPoolExecutor` owns fixed `std::jthread` workers, a bounded queue, task
state slots, one queue mutex, and condition variables. A mutex-based queue is
preferred for the first implementation: task dispatch is a coarse boundary and
correct bounded shutdown is more valuable than a custom lock-free queue.

### Task representation and allocation

Avoid heap-allocating a `std::function` per task. Task callables are a function
pointer plus a pointer to stable context owned by an app/window/frame slot.
Task-state slots are non-relocating, generational, and pre-reserved from:

```text
expected windows × frames in flight × maximum tasks per frame
```

Cold overflow may use tagged persistent storage if runtime growth is enabled.
Task handles are internal generational values so a stale completion or cancel
cannot affect a recycled slot.

Each task state contains:

- monotonic submission sequence;
- generation and status;
- owning window/frame epoch, if any;
- cancellation flag/stop source;
- `std::exception_ptr`;
- completion notification;
- debug timing and worker index;
- an optional task-group counter/dependency link.

Worker entry catches every exception and records it. No exception escapes a
worker and no failure calls `std::terminate()`.

### Bounded scheduling and no nested waits

The initial executor must not rely on recursive task submission followed by a
worker blocking on its own children; a saturated fixed pool can deadlock that
way.

Frame task graphs are created by the app-thread handoff:

1. enqueue zero or more independent text/resource producer tasks;
2. decrement a task-group counter when each finishes;
3. make the final conversion/record continuation runnable when the counter
   reaches zero;
4. only the app thread waits for the final group at `drawFrame`, destruction, or
   shutdown.

If a queue is full, app-thread submission may execute an eligible task inline
or wait for queue space according to one documented policy. It must not discard
work or allocate an unbounded fallback queue. Worker-thread producers do not
block while holding storage or Vulkan locks.

### Worker identity and arenas

Worker index is stable for a thread's lifetime. A task for frame F running on
worker N uses `storage.workerArena(F, N)`. Only that worker touches that arena,
and jobs on the same worker are sequential. Output that must outlive a producer
is allocated in the owning frame's worker arena or in a preallocated frame
output span, never on the worker stack.

The app-thread handoff finishes all `FrameTransient` allocation before workers
start. Workers use only their assigned `WorkerTransient` arenas and explicitly
reserved output spans. This prevents app/worker allocation races inside the
non-thread-safe linear arenas.

## App and frame state machine

Replace the single `activeWindowFrame` triplet gate with two levels:

- `buildingWindowId`: at most one window is executing app-thread UI authoring at
  a time;
- an independent per-window CPU frame/job state: different windows may be
  preparing concurrently after their UI authoring is complete.

Suggested per-window phases:

```cpp
enum class AppWindowPhase : uint8_t {
    Idle,
    Building,          // app thread owns UI/manager authoring
    Handoff,           // app thread freezes exact frame inputs
    WorkerQueued,
    WorkerRunning,
    CpuReady,
    Submitting,        // app thread owns viewport callbacks/queue/present
    Closing,
    Failed,
};
```

Rules:

- only one frame for a given window may be non-idle;
- multiple windows may be `WorkerQueued`, `WorkerRunning`, or `CpuReady`;
- a new window may enter `Building` after the previous window leaves `Handoff`;
- `drawFrame(id)` joins only that window's CPU task;
- a minimized/blocked/slow window does not prevent another window's CPU job;
- public main-window no-argument wrappers retain exactly the same routing;
- implicit event polling remains only in the legacy no-argument
  `beginFrame()` wrapper;
- explicit `beginFrame(WindowId)` never performs global polling.

## Immutable frame handoff

### Why a separate freeze step is required

The current `sealFrame()` refreshes manager revisions at seal time. That is safe
under the global frame gate but becomes nondeterministic if a worker seals after
the app thread publishes a later shared resource mutation.

Phase 6 splits handoff from final sealing:

```cpp
FrameBuildLease freezeFrameForWorkers(const FrameToken& frame);
FrameReadLease sealFrame(const FrameBuildLease& frame);
```

Equivalent concrete names are acceptable. The required semantics are:

- freeze captures shared and window manager revisions once;
- freeze closes app-thread frame-arena allocation and manager mutation for that
  window epoch;
- the frame's Clay commands, input overrides, font view, texture handles,
  descriptor preparation records, target extent, and renderer generations
  become immutable;
- worker-local buffer writes and use tracking remain legal;
- final seal verifies that all writes and jobs are complete and preserves the
  frozen revisions instead of refreshing them;
- submission or cancellation remains the exactly-once terminal action.

The immutable work packet should resemble:

```cpp
struct WindowFrameWork {
    WindowId window = InvalidWindowId;
    storage::FrameBuildLease buildLease{};
    uint32_t frameSlot = 0;
    uint64_t epoch = 0;
    VkExtent2D extent{};

    Clay_RenderCommandArray commands{};              // frame-arena backing
    InputFieldFrameOverridesView inputOverrides{};   // immutable view
    manager_storage::FontFrameView fonts{};
    storage::PreparedTextureBindings bindings{};     // frame-local copies/views

    storage::BufferHandle instanceBuffer{};
    storage::WindowDescriptorBundleHandle descriptors{};
    storage::RendererPipelineBundleHandle pipelines{};

    UiFrameOutput output{};
    execution::TaskGroup tasks{};
};
```

No field points into user stack memory or a relocatable manager container.

### Shared mutations after handoff

When the app thread replaces an image/font/icon after window A freezes:

- A continues using the exact generations captured by its packet;
- the old generation is retained by A's frame use list;
- the mutation publishes a new app-shared revision;
- a later window B freeze sees the new revision;
- A's worker is never forced to restart;
- retirement occurs only after A's submission or cancellation releases the old
  generation.

This is the deterministic rule users observe. Publication must not depend on
which worker happens to seal first.

## StorageSystem concurrency changes

### Remove the global recursive critical section

The single recursive mutex must be replaced with ownership and narrower locks:

- string interner lock for cold string insertion;
- app-shared resource registry/publication lock;
- upload queue lock;
- retirement/completion lock;
- one window-state lock for cold lifecycle transitions;
- exclusive frame-slot owner after `beginFrame()`;
- immutable sealed/frozen tables for worker reads;
- per-worker arenas with no internal lock.

Do not mechanically replace the recursive mutex with several locks and retain
nested call cycles. Refactor internal helpers into “caller holds lock” and
lock-free stable-view operations, define a global lock order for cold paths, and
assert it in development builds.

No mutex may be held across:

- queue/fence/present waits;
- file I/O or image/font/SVG decode;
- memory copies proportional to asset size;
- user callbacks;
- task waits or executor shutdown;
- Vulkan command recording;
- app-thread window-system operations.

### Stable published records

Worker-visible tables must not relocate or mutate records in place while a
frame lease exists. Use paged/non-relocating slots and immutable generation
records. Logical key maps may change under a short publication lock because
workers never perform keyed lookup; their packets already contain handles and
views.

Slot reuse waits for both CPU lease release and exact GPU submission completion.
Debug views carry frame epoch/lease validation.

Manager revision capture changes from “latest at seal” to “fixed at freeze.”
App-shared managers publish copy-on-replace generations. Window-local manager
state is exclusively app-owned until freeze and immutable to its worker until
the frame terminates.

### Use tracking

Replace linear `std::find` deduplication with an epoch marker indexed by handle
slot, or sort/unique an arena-backed handle list once. Task producers append to
worker-local use lists; the final continuation merges them in deterministic
resource-kind/handle order before final seal.

No per-glyph, per-instance, or per-run storage call is permitted.

### Upload scheduler

Replace `flushUploads()` queue-wide waiting with batches and exact tickets:

1. producer jobs create CPU candidates/staging payloads without storage locks;
2. the app resource lane validates destinations and builds a bounded upload
   batch;
3. record one transfer command buffer for the batch;
4. submit through the app queue owner with a fence/timeline value;
5. release the storage lock before any wait;
6. report exact upload completion at polling/frame boundaries;
7. a synchronous manager call waits only for its own ticket/batch when current
   API behavior requires readiness before return;
8. completed uploads publish resource generations in stable request-sequence
   order.

No worker calls `vkQueueSubmit`, `vkQueueWaitIdle`, `vkDeviceWaitIdle`, or
presentation functions.

### Deterministic publication lane

Every resource candidate receives an app-monotonic request sequence before it is
dispatched. Worker completion order does not determine public order. The app
lane commits ready candidates by sequence, or by a documented stable key/size
order for a same-frame icon-demand batch.

Failures also resolve deterministically: when a task group has several failures,
report the lowest input command/request sequence and retain the remaining
failures in diagnostics.

## Renderer task graph

### App-thread handoff work

`endFrame(window)` performs:

1. finish Clay/UI state and all user UI callbacks;
2. finalize input-field overrides and interaction snapshots;
3. analyze viewport target sizes and perform required transactional target
   publication;
4. collect icon demands; worker-rasterize independent misses if useful, join
   that demand batch, then commit aliases deterministically;
5. remap viewport/icon commands to exact logical texture handles;
6. gather/unique logical textures and prepare the window binding snapshot;
7. ensure descriptor and instance-buffer capacity transactionally;
8. reserve immutable output spans/task descriptors;
9. freeze the frame packet;
10. enqueue the renderer task graph or run it through `InlineExecutor`.

This handoff may allocate from the frame arena. After freeze, the app does not
write the packet or its backing frame memory.

### Parallel text layout

Clay calls never occur on workers. The finalized render commands and immutable
`FontFrameView` are sufficient for text layout.

For text-heavy frames:

1. pre-scan commands in stable command order;
2. identify text commands above a measured size threshold;
3. allocate one output description per text command;
4. dispatch independent text layout jobs using worker arenas;
5. store glyphs/metrics by original command index;
6. let the final conversion continuation consume results in command order.

Do not let task completion order determine glyph, instance, or run order. Small
text commands remain in the final conversion job to avoid scheduler overhead.

### Instance conversion

There are two acceptable implementations, selected by measurement:

- precompute exact per-command counts and prefix offsets, then let tasks write
  disjoint ranges of the mapped/storage output; or
- parallelize expensive text layout only and run the strict ordered instance/run
  emission as one continuation.

The second is the recommended first implementation because scissor-stack state,
input overrides, and adjacent-run merging are order-sensitive. It gives useful
parallelism without adding a compaction copy or changing output bytes.

The final continuation:

- walks commands in original order;
- consumes precomputed glyph spans where present;
- resolves textures through the frozen contiguous binding view;
- writes the mapped instance stream once;
- creates compact `UiRun` values in stable order;
- commits one buffer write/flush;
- merges worker-local resource-use and telemetry records;
- records a secondary UI command buffer;
- seals the frame and publishes `CpuReady` with release semantics.

### Secondary UI command recording

Add a dedicated UI worker command pool and secondary command buffer to every
window/frame slot. It is separate from:

- the app-owned primary command pool/buffer;
- viewport target command pools/buffers;
- upload command resources.

The frame-slot fence must be complete before the worker pool is reset. One task
owns the pool until recording ends. Pipelines/layouts are immutable shared
generations; descriptor sets and instance buffers are the frozen window/frame
generation.

Use dynamic-rendering secondary inheritance with the exact compatible target
format. The secondary records only UI draw state and commands. It does not:

- transition swapchain images;
- begin/end the primary swapchain rendering transaction unless Vulkan's
  secondary-content rules explicitly require matching inheritance metadata;
- invoke viewport or any other user callback;
- submit to a queue.

### App-thread draw and submission

`drawFrame(window)` performs:

1. join the window task group and rethrow deterministic failure;
2. verify the packet epoch, window phase, extent, renderer generation, and
   swapchain generation;
3. if resize/out-of-date invalidates the packet, cancel it and recreate the
   swapchain transactionally;
4. acquire a swapchain image and wait only for its exact prior fence if needed;
5. reset/begin the app-owned primary command buffer;
6. run viewport render callbacks on the app thread and execute their secondary
   buffers;
7. apply required viewport-to-UI sampling barriers;
8. begin the swapchain UI rendering pass and execute the prepared UI secondary
   command buffer;
9. transition for presentation and end the primary command buffer;
10. submit on the app-owned queue;
11. call `noteSubmission()` exactly once and attach the token to the frame slot;
12. present and advance the window frame slot;
13. release CPU task state and return the window to `Idle`.

The worker must be joined before invoking viewport callbacks. User Vulkan code
therefore does not overlap FlowUi's UI worker recording in Phase 6.

## Multi-window scheduling

Cross-window concurrency is the safest and highest-value first layer:

```text
application thread
    build/end window A ─────── build/end window B ───── draw A ─ draw B
                    │                         │             ▲        ▲
worker pool         └─ prepare/record A ──────┼─────────────┘        │
                                              └─ prepare/record B ───┘
```

Existing full-triplet code remains valid:

```cpp
app.beginFrame(a);
buildA();
app.endFrame(a);
app.drawFrame(a);
```

It may expose less cross-window overlap because `drawFrame(a)` immediately
joins A. Applications with several windows may optionally stage frames for
better overlap while still executing all their own code on one thread:

```cpp
for (WindowId id : visibleWindows) {
    app.beginFrame(id);
    buildUiFor(id);       // application thread
    app.endFrame(id);     // schedules only internal work
}

for (WindowId id : visibleWindows) {
    app.drawFrame(id);    // joins, callbacks, submits, presents
}
```

This is a performance option, not a correctness requirement.

One window may have one active CPU frame. A second `beginFrame(id)` before its
previous `drawFrame(id)` completes is rejected. Different frame rates and
minimized windows retain independent scheduling.

## Resource producer jobs

### Images

Image registration remains a synchronous public transaction:

1. app thread validates key/path and reserves request sequence;
2. a worker may read/decode the file into decode-owned candidate bytes;
3. app thread joins, validates dimensions/overflow, and creates storage GPU
   resources;
4. upload scheduler submits the exact batch;
5. publication replaces the logical generation only after readiness;
6. failure releases the candidate without changing the committed key.

Use a measured byte/format threshold so tiny images do not pay task overhead.

### Fonts

Font parsing/baking candidates may run on workers only where artery-font,
FreeType, msdfgen, and configured allocators are verified thread-safe for
independent contexts. IDs, family mappings, atlas placement/growth, upload, and
publication remain app-lane operations in request order. Failed tasks consume no
published ID.

Existing internal parallelism in third-party baking must be accounted for when
choosing pool size; avoid nested oversubscription by constraining that library's
thread count or running its task as one coarse executor job.

### Icons

At `endFrame`, deduplicate misses by `(document handle, width, height)`. Raster
candidates are independent and worker-safe when every task owns its PlutoSVG
surface and output buffer. Commit variants in stable key/size order, allocate
atlas regions on the app resource lane, upload centrally, and preserve exact
old-region retirement.

Different windows may contribute demands in the same app tick. The publication
lane merges them deterministically so the result does not depend on which
window's worker finished first.

### Viewport resources

Target generation sizing and publication remain app/window policy. Expensive
zero-fill/staging preparation may use workers. Storage image/view creation and
command-pool generation may later move to a resource-construction lane after
Vulkan object external-synchronization and VMA behavior are verified.

Viewport render callbacks themselves remain explicitly out of scope.

## Manager mutation rules while jobs exist

Public calls still occur on the app thread, but workers may hold older immutable
views.

- App-shared image/font/icon publication creates a new generation. Existing
  packets keep the old generation; later freezes see the new one.
- A window-local UI/input/shortcut/viewport facade cannot mutate its window
  while that window is `WorkerQueued`, `WorkerRunning`, `CpuReady`, or
  `Submitting`, except for the internal app-controlled draw lifecycle.
- A different idle/building window may be mutated independently.
- Removing a shared resource is logically immediate for later lookups but old
  packets retain their strong generation until cancellation/submission
  completion.
- Destruction of callback captures and manager records is deferred to an
  app-thread collection point.

Attempted illegal window-local mutation fails before changing state. Do not
silently queue arbitrary public mutations whose timing the user cannot observe.

## Vulkan synchronization policy

Vulkan permits concurrent command recording when externally synchronized
objects are distinct. Phase 6 enforces:

- one worker owner for each UI secondary command pool/buffer;
- one app owner for each primary command pool/buffer;
- one callback/app owner for each viewport secondary command buffer during its
  callback;
- immutable shared pipeline/layout objects during recording;
- one frame-slot owner for descriptor set updates and instance-buffer writes;
- no descriptor generation destruction until exact submission completion;
- one app submission owner for shared graphics/present queues;
- one explicit queue mutex only if upload and graphics submissions can originate
  from different internal call paths; never hold it across fence waits;
- no routine `vkDeviceWaitIdle()` and no `vkQueueWaitIdle()` upload path.

If the graphics and present queue handles are identical, their submit/present
operations are serialized by the same owner. If they differ, ownership remains
explicit rather than assuming Vulkan queue calls are automatically thread-safe.

## Memory and telemetry completion

Phase 6 should finish the measured items left by Phase 5 because thread-local
ownership makes their allocation category unambiguous:

- convert durable manager nested containers to tagged PMR/flat/paged storage
  where measurements justify it;
- move remaining input selection/hit-test and icon packing scratch to frame or
  worker arenas;
- add opaque third-party allocation counters;
- report manager-domain live/retired bytes;
- report task queue depth/high-water, task count, worker busy time, steal/inline
  fallback count, app wait time, cancellation, and failure count;
- report UI text task count, icon raster task count, decode bytes, and upload
  batch size;
- fold per-worker counters at `pollEvents()` or frame completion;
- keep hot counters worker-local, with no contended atomic increment per glyph,
  instance, lookup, or cache hit.

The implementation should benchmark before replacing font glyph/kerning maps or
other domain data layouts. Central ownership alone is not evidence that a new
container is faster.

## Error propagation

Every worker task ends in `Completed`, `Cancelled`, or `Failed`.

- worker exceptions are captured as `std::exception_ptr`;
- `drawFrame`, synchronous manager join, window destruction, or shutdown
  observes each task exactly once;
- a frame task failure cancels dependent tasks and calls `cancelFrame()` after
  all producers have stopped touching its arenas/resources;
- partial instance/descriptor/command output is never submitted;
- the window returns to `Idle` when recovery is possible, otherwise `Failed`;
- app/device loss transitions the entire app deliberately;
- error text includes task kind, window, frame epoch, and original exception;
- multiple failures are ordered by input/request sequence for deterministic
  reporting;
- worker mode does not swallow an error or print-and-continue where inline mode
  would throw.

Use RAII guards for frame/task terminal actions. Exactly one of submission or
cancellation releases the frozen frame lease.

## Cancellation, window destruction, and shutdown

### Window destruction

`destroyWindow(id)` remains an app-thread synchronous operation:

1. reject semantic-main destruction as before;
2. mark only the target window `Closing` and stop new work submission;
3. request cancellation of queued/running CPU tasks for that window;
4. join those tasks without holding storage, window-registry, or Vulkan locks;
5. cancel any frozen but unsubmitted storage frame;
6. detach platform callbacks;
7. drain only the target window's submitted frame fences/tokens and exact
   present completion;
8. release its UI/viewport/renderer/storage scope;
9. leave other windows and shared managers running.

Cancellation is cooperative. A task checks its stop token between substantial
units such as text commands, icon raster requests, or conversion stages. It does
not abandon a Vulkan command buffer halfway without ending/resetting it through
the owner cleanup path.

### App shutdown

Shutdown order is:

1. require the platform thread and transition app state to `Closing`;
2. stop accepting public manager/frame operations;
3. request cancellation of all queued tasks;
4. join all running tasks;
5. cancel unsubmitted frame leases;
6. stop and join the worker threads while device/storage/window state is still
   valid;
7. drain and destroy windows;
8. release app-shared managers and renderer resources;
9. collect and shut down StorageSystem;
10. destroy VMA/device/instance and the platform window system.

Repeated cleanup remains safe. No worker may outlive StorageSystem or
`VulkanContext`.

## Polling and publication safe point

`pollEvents()` remains exactly once per app tick and app-thread-only. It:

- polls the platform event queue;
- reports completed frame/upload tokens;
- folds completed worker telemetry;
- commits ready app-shared resource candidates in stable order;
- advances icon LRU/retired-region maintenance;
- performs storage collection and budget trimming only for inactive scopes;
- surfaces any background failure not already owned by a pending synchronous
  frame/manager operation.

The normal documented loop completes frame triplets before the next poll.
Calling `pollEvents()` while a window is in `Building` remains an error. If
prepared CPU jobs exist because an application staged several windows,
`pollEvents()` may join them but must not submit or silently discard their
frames; the recommended implementation instead requires those frames to be
drawn/cancelled first so the tick boundary remains unambiguous.

## Performance model

### Why both modes remain useful

`SingleThreaded` remains valuable for:

- deterministic debugging and race isolation;
- tiny UIs where scheduling overhead exceeds useful work;
- single-core or heavily constrained environments;
- tooling that benefits from a simple profiler trace;
- serving as the byte-for-byte reference implementation for worker tests.

`InternalWorkers` is expected to help most when:

- several windows finish UI authoring before submission;
- a frame contains substantial independent text layout;
- several icon sizes miss the cache in one tick;
- image/font decode or bake dominates a synchronous resource call;
- renderer conversion and command recording overlap continued app-thread work.

It must not be sold as an automatic speedup for every application. A single
small window using immediate `endFrame(); drawFrame();` may see no gain or a
small scheduling cost. Thresholds and inline fallback should keep that cost
bounded.

### Required measurements

Measure both modes against the same fixtures and the pre-StorageSystem main
branch where practical:

- frame CPU time and p50/p95/p99;
- app-thread active time and wait-at-draw time;
- worker utilization and queue latency;
- UI conversion time per command/glyph;
- icon raster and image/font decode latency;
- task count and average task size;
- heap allocation count/bytes;
- arena high-water and waste;
- storage interface calls and mutex acquisitions;
- upload batches, bytes, and exact wait time;
- queue submission/present/fence stalls;
- one-window overhead and 2/4-window scaling;
- deterministic output hashes and callback-order traces.

Recommended acceptance gates:

- worker mode never regresses the standard single-window fixture by more than a
  small benchmark-defined tolerance;
- two or more substantial windows demonstrate measurable CPU overlap;
- text-heavy and icon-miss fixtures demonstrate positive scaling;
- steady-state renderer conversion/recording performs zero general heap
  allocations;
- no lock or virtual call occurs in command/glyph/instance/run loops;
- no unbounded task queue or task allocation exists;
- output bytes, run ordering, callback order, logical resource selection, and
  error classification match inline mode;
- no routine device/queue idle appears.

## Examples

### Existing single-threaded behavior

```cpp
FlowUi::AppConfig config{};
config.execution.mode = FlowUi::InternalExecutionMode::SingleThreaded;

FlowUi::App app(config);
while (!app.shouldClose()) {
    app.beginFrame();
    buildApplicationUi(app.ui());
    app.endFrame();
    app.drawFrame();
}
```

The legacy no-argument `beginFrame()` retains implicit polling. All callbacks
and all internal tasks execute on this thread.

### Worker-backed internals with unchanged application structure

```cpp
FlowUi::AppConfig config{};
config.execution.mode = FlowUi::InternalExecutionMode::InternalWorkers;
config.execution.workerCount = 0; // automatic

FlowUi::App app(config);
while (!app.shouldClose()) {
    app.beginFrame();
    buildApplicationUi(app.ui()); // still this thread
    app.endFrame();               // may enqueue internal preparation
    app.drawFrame();              // joins, runs callbacks, submits, presents
}
```

The application does not synchronize `buildApplicationUi()` data with FlowUi
workers because workers never call it or retain its stack objects.

### Multi-window overlap without multithreaded user code

```cpp
app.pollEvents();

for (FlowUi::WindowId window : windowsToRender) {
    app.beginFrame(window);
    buildUiForWindow(window, app.ui(window));
    app.endFrame(window);
}

for (FlowUi::WindowId window : windowsToRender) {
    app.drawFrame(window);
}
```

Every call and every user UI function still runs sequentially on the app thread.
Only the immutable internal work launched by each `endFrame()` overlaps.

### Shared resource replacement while an older frame prepares

```cpp
app.endFrame(firstWindow); // firstWindow captured the current background generation

app.images().registerImage(
    FlowUi::ResourceKey{.name = "background"},
    "assets/new-background.png");

app.beginFrame(secondWindow);
```

The first window renders its captured generation. The later second-window frame
sees the replacement. The old image remains alive until the first window's exact
submission completes.

### Invalid cross-thread manager access

```cpp
std::jthread thread([&] {
    // Defined failure: public managers are app-thread-only in Phase 6.
    app.images().removeImage({.name = "background"});
});
```

The call throws `std::logic_error` before mutation. Users may perform their own
unrelated computation on their threads, then marshal the FlowUi manager call to
the app thread.

### Viewport callbacks remain simple

```cpp
viewport->setRenderCallback([&](const FlowUi::ViewPortRenderContext& context) {
    // Still invoked by drawFrame() on the app thread in both modes.
    recordScene(context.commandBuffer, sceneState);
});
```

No additional locking is required for `sceneState` as long as the application
itself keeps it app-thread-owned.

## Implementation stages

### Stage 0 — Baseline and invariants

- Preserve the current dirty working tree and identify unrelated changes.
- Add deterministic renderer output hashes, callback-order traces, allocation
  counters, and single-/two-/four-window benchmark fixtures.
- Record main-branch and Phase 5 inline baselines before changing scheduling.
- Add source checks for user callback invocation sites and queue ownership.

**Exit:** behavior and performance baselines can detect reordering, output
changes, and overhead.

### Stage 1 — Public config and inline executor

- Add `InternalExecutionMode`, `InternalExecutionConfig`, and `AppConfig` field.
- Add the internal execution package and task state machine.
- Implement `InlineExecutor` first.
- Add app-thread tokens/checks to every public App/manager/viewport entry.
- Route existing renderer/resource preparation through task entry functions
  inline without changing frame behavior.

**Exit:** `SingleThreaded` passes every existing test using the new executor
abstraction and creates no thread.

### Stage 2 — Frame handoff and per-window job states

- Replace `activeWindowFrame` with `buildingWindowId` and per-window phases.
- Add `FrameBuildLease`/freeze semantics and fixed revision capture.
- Build immutable `WindowFrameWork` packets in frame storage.
- Add RAII frame/task terminal guards.
- Permit different windows to be prepared concurrently while retaining one
  app-thread UI authoring owner.

**Exit:** inline mode can stage several windows and draw them later with exact
Phase 5 output/lifetimes.

### Stage 3 — Storage concurrency hardening

- Remove global sealed-frame shared-mutation prohibition.
- Move worker-visible records to stable immutable generations/pages.
- Split the recursive mutex and remove nested lock cycles.
- Add worker-local use lists and O(1)/sort-unique deduplication.
- Make manager revisions freeze-time values.
- Replace queue-wide synchronous upload flushing with exact upload batches.
- Add debug lease/owner/lock-order validation.

**Exit:** two frozen windows can read old/new shared generations while the app
lane publishes safely, and no long operation holds a storage lock.

### Stage 4 — Worker-ready UI renderer

- Add per-window/frame-slot UI secondary command pools/buffers.
- Split renderer preparation from app-owned primary recording/submission.
- Make all renderer task input immutable and all output frame-slot-owned.
- Implement parallel text layout plus stable ordered conversion.
- Apply descriptor deltas only to the job-owned frame-slot sets.
- Record the UI secondary and seal through the inline executor.

**Exit:** inline execution uses the exact worker-ready path, produces identical
instances/runs, and invokes no user callback from renderer tasks.

### Stage 5 — Fixed worker pool

- Implement bounded `WorkerPoolExecutor` with fixed `std::jthread` workers.
- Bind stable worker indices to StorageSystem worker arenas.
- Dispatch cross-window render jobs and text subjobs.
- Add deterministic task groups, failure selection, cancellation, and joins.
- Keep Vulkan queue/WSI operations on the app thread.

**Exit:** `InternalWorkers` produces byte-identical renderer output and callback
traces, with concurrent CPU work visible in two-window and text-heavy tests.

### Stage 6 — Resource producer work

- Add thresholded image decode candidates.
- Parallelize verified-safe font parse/bake work without nested
  oversubscription.
- Batch and parallelize icon raster misses.
- Commit all candidates on the deterministic app publication lane.
- Keep public calls synchronous and strongly transactional.

**Exit:** worker completion order cannot change IDs, keys, atlas placement,
visible generations, or exception selection.

### Stage 7 — Lifecycle and failure completion

- Add target-window cancel/join in destruction.
- Add whole-app stop/join order before storage/device teardown.
- Exercise resize/out-of-date cancellation after a completed worker job.
- Make frame, task, upload, submission, and present terminal states exactly once.
- Preserve semantic-main destruction rejection and monotonic window IDs.

**Exit:** in-flight close, cancellation, exceptions, and repeated cleanup are
race-free and do not affect another window.

### Stage 8 — Memory, telemetry, and tuning

- Move remaining frame scratch to frame/worker arenas.
- Add tagged PMR/flat containers where benchmarks justify them.
- Add third-party opaque allocation accounting and manager-domain statistics.
- Tune task thresholds, queue capacity, arena sizes, and worker auto-count from
  measurements.
- Document expected workloads where workers help or add overhead.

**Exit:** performance claims are backed by allocation and timing data rather
than assumed thread-count gains.

### Stage 9 — Remove transitional paths

- Delete the `//Transitional: app-thread gate` and any compatibility executor
  branches that bypass the common task path.
- Remove obsolete global shared-mutation checks and queue-idle upload path.
- Update API documentation and examples.
- Write `phase6.md` with changes, deviations, validation, measured results, and
  remaining future user-callback threading work.

**Exit:** no manager/resource ownership needs another migration for the two
execution modes.

## File-level implementation map

| Area | Concrete changes |
|---|---|
| `include/FlowUi/PublicStructs.hpp` | Public execution enum/config and `AppConfig::execution`. |
| `include/FlowUi/App.hpp` | Optional immutable execution-info query and threading documentation. |
| `include/internal/Execution/*` | Executor interface, inline executor, worker pool, task handles/groups, thread-role checks. |
| `src/Execution/*` | Bounded queue, workers, cancellation, joins, failure handoff, telemetry. |
| `src/FlowUi.cpp` | Executor lifetime, building gate, window phases, immutable handoff, join/draw, app resource lane, shutdown. |
| `include/internal/StorageSystem/StorageTypes.hpp` | Build lease/frozen packet types, task-safe views, upload batch/ticket metadata, execution telemetry. |
| `IStorageSystem` / `FlowStorageSystem` | Freeze/seal split, stable generations, narrow locking, worker-local use merge, exact upload completion. |
| `include/Ui/Vk_UiRenderer.hpp` | Immutable task input/output and per-slot secondary command resources. |
| `src/Ui/Vk_UiRenderer.cpp` | Parallel text preparation, ordered direct emission, descriptor delta update, secondary recording. |
| `TextLayoutEngine` | Pure immutable text-job entry point and arena-backed result shape. |
| Manager internal controllers | Thread-affinity checks, candidate producer/commit split, deterministic sequence handling. |
| `ViewPortManager` controller | App-thread callback assertion and primary/secondary execution ordering; no worker callbacks. |
| `FrameVk` | Separate app primary and worker UI-secondary pools; task epoch/generation. |
| Tests/CMake | executor/unit/race tests, TSan target, allocation benchmarks, worker configuration matrix. |

Public manager headers remain minimal. Executor, task, lock, and candidate types
belong under `include/internal`, not `include/managers`.

## Required validation

### Configuration and compatibility

- default config creates no worker thread and preserves legacy no-argument
  behavior;
- explicit `SingleThreaded` and `InternalWorkers` compile in all feature
  configurations;
- `workerCount` auto, one, several, invalid, and thread-creation failure paths;
- IconManager-off, public-Vulkan-interop-off, development, and Release builds;
- public manager/API method families remain recognizable;
- `AppConfig::window` remains semantic main-window configuration.

### Thread-affinity and callback tests

- every public App/manager/viewport mutation rejects a non-app thread before
  state changes;
- every user callback records the app thread ID in both modes;
- callback ordering is byte-for-byte/event-for-event identical;
- callback register/unregister/destruction during dispatch remains correct;
- no worker stack appears in callback traces;
- borrowed public views retain documented lifetimes.

### Executor tests

- inline and pool task lifecycle equivalence;
- bounded queue saturation without loss, unbounded allocation, or deadlock;
- monotonic/generational task handles and stale-handle rejection;
- task group dependency and deterministic continuation;
- exception capture/rethrow and multiple-failure ordering;
- queued and running cancellation;
- stop/join with empty, busy, and failed queues;
- no nested-wait deadlock;
- worker index/arena exclusivity;
- task state destruction occurs on the intended thread.

### Storage race and lifetime tests

- freeze fixes revisions even if publication occurs before worker seal;
- old/new shared resource generations used concurrently by two windows;
- shared image/font/icon replace/remove during worker reads;
- window-local mutation rejected while its job owns the epoch;
- different-window local mutation proceeds independently;
- frame/worker arenas never overlap or reset early;
- worker-local use merge is exact and deterministic;
- exact cancellation and submission release;
- upload batches complete without queue-wide idle;
- out-of-order CPU job, upload, graphics, and present completion;
- collection/destructor callbacks occur on app thread;
- no deadlock under forced slow decode, recording, completion, and close.

### Renderer equivalence

- hash full `UiInstance` streams and `UiRun` arrays in both modes;
- compare recorded operation traces and descriptor revisions;
- zero/one/many commands and deeply nested scissors;
- many small and very long text commands;
- sorted input-field overrides;
- stale/failed/replaced textures and fallback binding;
- transactional instance/descriptor growth;
- target format switching while jobs exist;
- resize/out-of-date after endFrame but before drawFrame;
- no general heap allocation in steady-state conversion/recording;
- Vulkan validation reports no command-pool, descriptor, lifetime, or queue
  synchronization errors.

### Multi-window and lifecycle

- staged A/B endFrame jobs overlap and draw in either order;
- windows use different rates, frame counts/slots, formats, and extents;
- minimized A does not block B;
- close queued/running/ready/in-flight secondary window;
- semantic main destruction still rejected;
- shared resources survive secondary close;
- one window failure does not corrupt another;
- exact swapchain/descriptor/pipeline/viewport retirement;
- no device idle except final shutdown/device loss and the already documented
  unsupported exact-present main-only compatibility path.

### Sanitizers and tools

- development and Release builds;
- ASan and UBSan with leak/error halt;
- ThreadSanitizer on executor, storage, manager producer, and two-window CPU
  tests; use a Vulkan-free/fake backend where driver internals make TSan noisy;
- real two-window Vulkan validation;
- repeated stress loops with randomized scheduling/yields;
- `git diff --check` and source guards for worker callback invocation,
  queue/device idle, global frame gate, and manager-owned thread primitives.

## Definition of done

Phase 6 is complete when:

- users select exactly `SingleThreaded` or `InternalWorkers` at App creation;
- single-threaded mode creates no threads and is the deterministic reference;
- both modes use the same storage/task/renderer implementation;
- user application code and every public callback remain app-thread-only;
- wrong-thread public access has defined rejection behavior;
- different windows can prepare internally in parallel after app-thread UI
  authoring;
- text-heavy single-window frames have useful internal parallel work;
- renderer workers use immutable packets, distinct arenas, command pools,
  descriptor generations, and frame slots;
- app thread alone owns windowing, viewport callbacks, queue submission, and
  presentation;
- shared manager resources publish immutable generations while old frame
  snapshots remain alive;
- task failure, cancellation, window close, and shutdown join before resource
  destruction;
- no global recursive storage lock, global sealed-frame mutation ban,
  queue-wide upload idle, or Phase 4/5 triplet gate remains;
- inline/worker outputs, callbacks, IDs, and resource generations are
  deterministic and equivalent;
- worker mode demonstrates measured gains on applicable fixtures without a
  material standard-fixture regression;
- manager and worker allocations/telemetry are attributable by domain, window,
  frame, and worker;
- all required builds, Vulkan tests, sanitizers, TSan-capable tests, and diff
  checks pass;
- future user-UI and viewport-callback threading can be added as a separate
  explicit policy without moving storage ownership again.

## Implementation prompt

The following prompt is intended to be used for the Phase 6 implementation:

```text
Implement Phase 6 exactly as scoped in multithreading_implementation.md, using
phase5.md and Ui_renderer_storage_integration.md as architectural context. First
inspect the working tree and preserve every unrelated or user-owned change.

Add the public AppConfig execution configuration with exactly two modes:
SingleThreaded and InternalWorkers. SingleThreaded must remain the default,
create no threads, and execute the same internal task entry points inline.
InternalWorkers must use a bounded fixed worker pool with validated auto/explicit
worker counts, non-relocating generational task state, deterministic task groups,
cooperative cancellation, exception capture, app-thread rethrow, and complete
stop/join semantics. Keep all executor/task/lock internals under include/internal
and matching private sources; keep public manager headers minimal.

Keep all public App and manager calls, UI/element/shortcut/developer callbacks,
clipboard/cursor operations, viewport render callbacks, GLFW/window work,
swapchain acquire, queue submission, presentation, and public destruction on the
App construction thread. Add defined wrong-thread rejection before mutation. Do
not add public jobs, futures, async resource APIs, user executors, worker UI
construction, worker viewport callbacks, or concurrent same-window frames.

Replace the global activeWindowFrame triplet gate with one app-thread Building
owner plus independent per-window worker phases. Add an immutable frame handoff
and freeze/build lease so shared/window manager revisions and resource
generations are captured before dispatch and never refreshed according to worker
completion timing. Allow different windows to prepare concurrently after their
app-thread UI builds. drawFrame(WindowId) must join only its target job, propagate
failure deterministically, invoke viewport callbacks on the app thread, record
the primary command buffer, submit, present, and terminate the storage frame
exactly once. Preserve legacy no-argument behavior and implicit polling only in
the no-argument beginFrame() wrapper.

Refactor StorageSystem for actual concurrent frozen readers: remove the global
sealed-frame shared-mutation ban and monolithic recursive mutex, use stable
immutable/paged generations and narrowly owned locks, fix use deduplication,
provide worker-local use merging, and keep frame/worker arenas exclusive. Never
hold a storage lock across task waits, user callbacks, file/decode work,
proportional copies, Vulkan recording, or queue/fence/present waits. Replace
flushUploads queue-wide idle with batched exact upload tickets and an app-owned
submission/publication lane. App-shared manager replacement must let old frozen
frames keep old generations while later frames see the new revision.

Make VulkanUiRenderer worker-ready without changing shaders: add a dedicated UI
secondary command pool/buffer per window/frame slot, immutable frame work
packets, parallel immutable text-layout candidates, stable ordered direct
instance/run emission, frame-slot descriptor deltas, one committed instance
write, and secondary UI command recording. Inline mode must use the identical
secondary/task path. Workers must never call user code, WSI, queue submit,
present, device idle, or queue idle. The app primary command buffer must execute
app-thread viewport callbacks/secondary buffers and then the prepared UI
secondary in correct order.

Add thresholded internal producer jobs for image decode, verified-safe font
parse/bake work, and batched icon raster misses while preserving synchronous
public manager behavior, strong transactions, monotonic IDs, deterministic
commit order, exact retirement, and app-thread publication/destruction. Do not
oversubscribe third-party font workers. Finish the Phase 5 allocation/telemetry
items where worker ownership makes attribution clear, but change font/icon data
layouts only when supported by benchmarks.

Implement target-window cancellation/join, resize/out-of-date cancellation,
worker failure cleanup, and whole-app stop/join before storage/device teardown.
Do not regress Phase 3 logical textures, Phase 4 multi-window/exact WSI
retirement, Phase 5 ResourceKey/manager storage, transactional creation, or the
main-only unsupported-present compatibility rule. Keep vkDeviceWaitIdle only in
the already allowed final shutdown/device-loss and unreachable-after-secondary
main-only fallback path.

Add focused configuration, thread-affinity, callback-thread/order, executor,
freeze/revision, immutable-generation, upload, renderer equivalence,
single-/multi-window scheduling, resource producer, cancellation, close,
shutdown, allocation, and performance tests. Hash instances/runs and compare
SingleThreaded with InternalWorkers. Run development, Release, icon-off and
public-Vulkan-interop-off builds, storage/types/renderer/manager/Vulkan tests,
real two-window Vulkan validation, ASan, UBSan, ThreadSanitizer where supported,
randomized scheduling stress, benchmark comparisons, source guards, and git diff
--check. Finish with phase6.md describing changes, deviations, validation,
measured single-window overhead and multi-window/text/icon gains, remaining
limits, and the explicitly deferred future worker-safe UI/viewport callback
policies.
```
