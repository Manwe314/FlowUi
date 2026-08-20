# FlowUi Development Monitoring: Timing Architecture

## Scope

This report defines the timing portion of the new `devSystems/devMonitoringAndReporting` subsystem. It deliberately does **not** design the later memory monitor or the general event/report engine, except where timing needs a small shared concept such as frame identity, retained records, or a future diagnostic flag.

The planned monitoring sequence is:

1. timing zones and CPU/GPU frame timelines — this report;
2. memory footprint, allocation lifetime, and churn;
3. general events, aggregation, semantic findings, and reports.

The timing system must answer:

- How long did an application/window frame take?
- Where did CPU wall time go, including waits and gaps between FlowUi API calls?
- Which element definitions and instances contributed most to UI construction?
- How long did Vulkan work actually execute on the GPU?
- Which CPU submission produced which GPU work, and which older submission caused a later fence wait?
- How expensive was startup, secondary-window creation, swapchain recreation, and shutdown?
- How much time did the monitor itself add?

The timing system should be useful on its own, but its records must be structured so the later reporting system can derive percentiles, bottleneck classifications, comparisons, and findings without changing the instrumentation API.

The requested filename is intentionally retained as `devMonitoringTimin.md`.

---

## 1. Current repository timing state

FlowUi already has the beginnings of a timing system in `include/devMode/performanceDiagnostics.hpp` and instrumentation in `src/FlowUi.cpp`, `src/managers/ViewPortManager.cpp`, and `src/Ui/Vk_UiRenderer.cpp`.

It currently measures host-side durations for:

- `beginFrame`;
- in-flight fence wait;
- `endFrame`;
- `Clay_EndLayout` and related UI finalization;
- resource/render preparation;
- swapchain image acquisition;
- viewport command recording and callbacks;
- UI command recording;
- queue submission;
- `vkQueuePresentKHR`;
- the host duration of `drawFrame`.

This is useful instrumentation to migrate, not discard. The problems are in its representation and semantics:

1. Every measurement has a dedicated field, so adding a phase changes data structures and UI code.
2. The private 180-frame array is not a navigable trace.
3. `FrameDiagnostics::frameIndex` is currently initialized from `window.frames.currentFrame`, which is a reusable frame-slot index rather than a monotonic frame identity.
4. The time between `beginFrame` returning and `endFrame` being called—the application's actual UI construction window—is not directly measured.
5. The time between `endFrame` returning and `drawFrame` being called is invisible.
6. `drawFrameMs`, `submitMs`, and `presentMs` are CPU/API-call durations. None measures device execution.
7. A fence waited during frame-slot reuse belongs causally to an older submission, not to the new frame being started.
8. Per-element callback timing and nested contribution do not exist.
9. Startup, secondary-window construction, recreation, and teardown do not have a common timeline.

The new implementation should initially emit compatibility summaries from new timing records so the current footer can survive during migration. New code should stop adding fields to `FrameDiagnostics`.

---

## 2. What a “zone” means

A **zone** is one named interval on one ordered execution track.

```text
Zone = descriptor + one invocation

descriptor: “what kind of work is this?”
invocation: “when did this particular occurrence start and finish?”
```

Examples:

- descriptor: `flowui.frame.begin`; invocation: main window frame 42 from 4.1 ms to 4.7 ms;
- descriptor: `flowui.element.build`; invocation: `ComboBox` instance `settings/theme` from 4.9 ms to 4.94 ms;
- descriptor: `flowui.gpu.ui_pass`; invocation: submission 71 from GPU tick A to tick B.

Zones have two distinct forms:

- **CPU zones** use the process monotonic clock and normally obey strict stack nesting on one thread.
- **GPU zones** use Vulkan timestamp queries on one queue and are correlated to CPU frames/submissions. They are not CPU children even when a CPU function recorded the relevant commands.

The word “zone” does not imply that the interval is useful work. Waits and deliberate gaps are zones too. Every descriptor has a role:

```cpp
enum class TimingZoneRole : uint8_t {
	Work,
	Wait,
	Gap,
	GpuWork,
	DevToolWork,
};
```

This role is important. A 4 ms fence wait and a 4 ms layout pass both occupy CPU wall time, but suggest entirely different actions.

### 2.1 Inclusive and exclusive time

For a properly nested CPU zone:

```text
inclusive duration = end - start
exclusive duration = inclusive duration - durations of direct recorded children
```

Consider:

```text
Element invocation: 120 us inclusive
├── state lookup:       8 us
├── interaction hooks: 12 us
├── own build callback: 20 us
└── child elements:     65 us

unattributed self/overhead: 15 us
```

The parent is expensive as an owner of a subtree, but only 55 μs is directly attributable to its own pipeline/uninstrumented work. The tool must retain both views:

- inclusive/subtree contribution;
- exclusive/own contribution.

Exclusive time is only exact relative to the set of recorded child zones. If a detail category is disabled, its time remains in the nearest enabled parent's exclusive value. The capture configuration must therefore be stored with every timing range.

### 2.2 A zone is not a counter or an event

The timing subsystem writes completed intervals. A later event subsystem will add instants, arbitrary causal flows, and counters. Timing records still need frame/submission/entity identity today so that future data can correlate without replacing them.

### 2.3 Static names, dynamic identity

Zone names and categories should normally be compile-time string literals. Instance names, element IDs, window IDs, and frame IDs belong in compact numeric entity fields rather than dynamically constructed zone names.

Good:

```text
name = flowui.element.build
entity = { definition: ComboBox, instance: settings/theme }
```

Bad:

```text
name = "ComboBox(settings/theme) build frame 42"
```

Static names avoid per-invocation string allocation/interning and let thousands of occurrences share one descriptor. Perfetto similarly recommends compile-time categories and names for the efficient path and models nested slices on ordered tracks ([Perfetto Track Events](https://perfetto.dev/docs/instrumentation/track-events)).

---

## 3. Ownership inside the new subsystem

`DevMonitoringAndReporting` should be an application-owned object, but it should not itself become one giant implementation class. The timing part can be introduced beneath it as follows:

```text
include/devSystems/devMonitoringAndReporting/
├── DevMonitoringAndReporting.hpp
└── timing/
    ├── DevTiming.hpp                 public/internal facade
    ├── DevTimingTypes.hpp            IDs, descriptors, records, configuration
    ├── DevTimingZone.hpp             CPU RAII scope and manual boundary token
    ├── DevTimingRecorder.hpp         producer attachment and bounded ingestion
    └── DevGpuTiming.hpp              Vulkan query planning/result conversion

src/devSystems/devMonitoringAndReporting/
├── DevMonitoringAndReporting.cpp
└── timing/
    ├── DevTiming.cpp
    ├── DevTimingRecorder.cpp
    └── DevGpuTiming.cpp
```

The intended eventual ownership is:

```cpp
struct App::Impl {
#if FLOW_UI_DEV_MODE
	devSystems::DevMonitoringAndReporting devMonitoring;
#endif
	// Vulkan, storage, managers, and windows...
};
```

`App` can later expose a development-only accessor such as:

```cpp
#if FLOW_UI_DEV_MODE
DevMonitoringAndReporting& devMonitoring() noexcept;
#endif
```

There is no `DevSession` object and no required root `DevSystems` object. The three subsystem objects are directly owned where their lifetimes make sense. `DevTooling` consumes monitoring data; monitoring must not know about tool widgets. `DevFunctionality` may publish timing identity or ask monitoring for a capture, but monitoring should not apply overrides.

### 3.1 Startup ownership

`makeApplication` must be timed before `App::Impl::init` has completed. There are two reasonable designs:

#### Option A: construct monitoring first inside `App::Impl` — recommended

Make `devMonitoring` the first development-only member. `App::Impl` construction creates its clock epoch and recorder before Vulkan/storage initialization. `makeApplication` starts the outer zone immediately after allocating `Impl`, and `Impl::init` creates all children.

The tiny `std::make_unique<App::Impl>` allocation itself remains outside the zone unless a local timestamp is captured before it and passed into monitoring.

#### Option B: bootstrap recorder transferred into `App::Impl`

Create a small stack `BootstrapTiming` before allocating `Impl`, record the complete factory call, then adopt its records into `devMonitoring`.

This captures a few more operations but creates a second record path and transfer edge cases. It is only worthwhile if “entry into `makeApplication`” must be exact. Option A is simpler and loses negligible information.

---

## 4. Identity: frame slots are not frame IDs

Timing needs three separate identities:

```cpp
struct WindowFrameKey {
	WindowId window;
	uint64_t frameNumber;       // monotonic per window
};

using AppTickId = uint64_t;   // monotonic per global poll/outer-loop iteration
using SubmissionSerial = uint64_t;
using FrameSlot = uint32_t;   // reusable Vulkan resources, not historical identity
```

- `WindowFrameKey` identifies one `beginFrame`/`endFrame`/`drawFrame` transaction.
- `AppTickId` groups the global `pollEvents` work and the window frames intentionally produced from that poll. This is necessary for multi-window applications.
- `SubmissionSerial` correlates prepared CPU data, the Vulkan submission, GPU timestamps, retirement, and later waits.
- `FrameSlot` explains which reusable resources were used, but must never label history by itself.

The no-argument `App::beginFrame()` polls events before beginning the main window. Explicit multi-window code calls `pollEvents()` once and then executes several window frame triplets serially. The monitor should preserve that distinction instead of pretending there is one universal application frame.

---

## 5. CPU timing data model

### 5.1 Descriptor

One descriptor exists for each static zone call site or built-in semantic zone:

```cpp
using TimingZoneTypeId = uint64_t;
using TimingSourceId = uint32_t;

enum class TimingCategory : uint16_t {
	Lifecycle,
	Frame,
	Input,
	Element,
	Layout,
	Prepare,
	RendererCpu,
	Gpu,
	Wait,
	User,
	DevTool,
};

struct TimingZoneDescriptor {
	TimingZoneTypeId typeId;
	std::string_view name;
	TimingCategory category;
	TimingZoneRole role;
	TimingSourceId source;
	uint8_t detailLevel;
};
```

`typeId` should be a stable 64-bit hash of a domain separator, category, file identity, line, and literal name. The registry checks collisions in developer mode. Built-in semantic descriptors can use explicitly assigned constants so moving code does not change their identity. User call-site descriptors may reasonably change when their source line moves; their display/source metadata remains authoritative.

Do not put a raw descriptor pointer into persistent trace data. Pointers are acceptable inside the process fast path but must resolve to numeric IDs before records enter retained/exported storage.

### 5.2 Invocation record

```cpp
using TimingInvocationId = uint64_t;
using TimingTrackId = uint32_t;

enum class TimingEntityKind : uint8_t {
	None,
	App,
	Window,
	ElementDefinition,
	ElementInstance,
	Viewport,
	Action,
	Resource,
	Submission,
};

struct CpuTimingRecord {
	uint64_t startNs;
	uint64_t durationNs;
	uint64_t directChildNs;
	TimingInvocationId invocationId;
	TimingInvocationId parentInvocationId;
	TimingZoneTypeId typeId;
	WindowFrameKey frame;
	AppTickId appTick;
	uint64_t entityId;
	TimingTrackId track;
	TimingEntityKind entityKind;
	uint8_t depth;
	uint16_t flags;
};
```

`exclusiveNs` can be derived as `durationNs - min(durationNs, directChildNs)`. Storing direct-child time avoids recomputing it for every live view and allows the recorder to flag an invariant failure.

The initial record is intentionally larger than an ultra-low-overhead production profiler record. This is dev-only, and explicit correlation is worth the bytes. If profiling shows record bandwidth is itself a problem, fields can be delta-encoded in retained chunks later without changing the logical schema.

### 5.3 Clock

Use one process-monotonic clock for CPU tracks, initially `std::chrono::steady_clock`. Convert to unsigned nanoseconds from a monitor-owned epoch:

```cpp
uint64_t DevTiming::nowNs() const noexcept {
	return static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::nanoseconds>(
			Clock::now() - epoch_).count());
}
```

At initialization, record clock properties and calibrate measurement cost by repeatedly timing empty clock pairs. Keep minimum/median/P95 observed overhead as metadata. Do **not** blindly subtract a constant from every zone: clock cost and scheduling noise vary, and subtraction can turn short durations into fiction. The tool should show a warning when a zone's duration approaches measured instrumentation overhead.

`steady_clock` timestamps from different threads in one process share a domain on supported standard-library implementations. If a target cannot provide a steady monotonic clock, timing support should report unavailable rather than fall back to wall-clock time.

### 5.4 Per-thread active stack

Strictly nested CPU zones need an active stack:

```cpp
struct ActiveCpuZone {
	TimingInvocationId invocationId;
	TimingZoneTypeId typeId;
	uint64_t startNs;
	uint64_t directChildNs;
	WindowFrameKey frame;
	uint8_t depth;
};
```

On begin:

1. Check the category/detail mask before reading the clock.
2. Allocate a monotonically unique invocation ID.
3. Capture the nearest active parent ID.
4. Push the active zone.

On end:

1. Read the end timestamp.
2. Verify this token is the stack top.
3. Pop it.
4. Add its inclusive duration to its active parent's `directChildNs`.
5. Append one completed record to the thread producer.

One completed record is preferable to separate begin/end records for the first implementation: it halves retained zone records and makes a frame snapshot easy to consume. The trade-off is that an unclosed zone is not retained as a normal record. The active stack is therefore flushed as `Incomplete` when a frame is canceled, an exception exits a cross-call boundary, or monitoring shuts down normally. Crash-safe partial tracing belongs to the later event/file layer.

Use a small inline/fixed active stack with an overflow path. UI nesting can be deep, so overflow must not corrupt memory. On overflow, suppress deeper zones, increment a quality counter, and preserve outer timing. Do not allocate while unwinding a zone destructor.

---

## 6. RAII zone implementation

### 6.1 Core class

```cpp
class CpuTimingZone {
public:
	CpuTimingZone(
		DevTimingRecorder& recorder,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept
		: recorder_(&recorder), token_(recorder.tryBegin(descriptor, entity)) {}

	~CpuTimingZone() noexcept {
		if (recorder_ && token_) recorder_->end(token_);
	}

	CpuTimingZone(const CpuTimingZone&) = delete;
	CpuTimingZone& operator=(const CpuTimingZone&) = delete;
	CpuTimingZone(CpuTimingZone&&) = delete;
	CpuTimingZone& operator=(CpuTimingZone&&) = delete;

private:
	DevTimingRecorder* recorder_ = nullptr;
	ActiveZoneToken token_{};
};
```

The destructor is `noexcept`. Recorder failure must never replace an application exception. On internal mismatch or full buffers, monitoring records a quality failure and disables/suppresses the affected detail; it does not throw through user code.

### 6.2 Macro expansion

The macro must create a unique static descriptor and a unique scope object without evaluating runtime arguments when developer mode is off.

```cpp
#define FLOWUI_DEV_TIMING_JOIN_IMPL(a, b) a##b
#define FLOWUI_DEV_TIMING_JOIN(a, b) FLOWUI_DEV_TIMING_JOIN_IMPL(a, b)

#if FLOW_UI_DEV_MODE

#define FLOWUI_DEV_TIMING_ZONE_IMPL(recorder, category, role, name, entity, unique) \
	static constexpr auto FLOWUI_DEV_TIMING_JOIN(_flowTimingDescriptor_, unique) = \
		::FlowUi::devSystems::makeTimingDescriptor(                           \
			category, role, name, TimingSourceLocation::current());             \
	[[maybe_unused]] ::FlowUi::devSystems::CpuTimingZone                    \
		FLOWUI_DEV_TIMING_JOIN(_flowTimingZone_, unique){                    \
			recorder, FLOWUI_DEV_TIMING_JOIN(_flowTimingDescriptor_, unique), entity}

#define FLOWUI_DEV_TIMING_ZONE(recorder, category, role, name)             \
	FLOWUI_DEV_TIMING_ZONE_IMPL(                                             \
		recorder, category, role, name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)

#define FLOWUI_DEV_TIMING_ZONE_ENTITY(recorder, category, role, name, entity) \
	FLOWUI_DEV_TIMING_ZONE_IMPL(                                             \
		recorder, category, role, name, entity, __COUNTER__)

#else

#define FLOWUI_DEV_TIMING_ZONE(recorder, category, role, name) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_ENTITY(recorder, category, role, name, entity) ((void)0)

#endif
```

The exact macro can be refined during implementation, but it must preserve these properties:

- one `__COUNTER__` value is forwarded through a helper and reused for descriptor and variable names;
- `TimingSourceLocation::current()` uses compiler source-location builtins at the call site (matching FlowUi's existing portable development metadata approach);
- the descriptor is static/constant and does not allocate per invocation;
- the disabled expansion does not reference or evaluate `recorder` or `entity`;
- the user writes a trailing semicolon naturally;
- two zones on the same source line remain distinct;
- names are literals on the fast path.

### 6.3 User-facing use

The lowest-friction API is an explicitly attached recorder object:

```cpp
#if FLOW_UI_DEV_MODE
auto timingThread = app.devMonitoring().timing().attachCurrentThread("platform");
#endif

while (!app.shouldClose()) {
	FLOWUI_DEV_TIMING_ZONE(
		timingThread,
		TimingCategory::User,
		TimingZoneRole::Work,
		"application.update");

	updateApplication();
	app.beginFrame();
	buildUi(app.ui());
	app.endFrame();
	app.drawFrame();
}
```

FlowUi's own types can retain a narrow `DevTimingRecorder*` installed during initialization, avoiding repeated lookup. A global singleton is discouraged because multiple `App` objects, tests, and offline consumers would interfere. A thread-local *current attachment* may support a convenience macro later, but its lifetime must be explicitly established by an app-owned attachment token.

### 6.4 Dynamic zones

Occasionally application data has a runtime name. Do not put runtime strings into the primary macro. Offer a slower explicit API:

```cpp
auto dynamicZone = timingThread.dynamicZone(
	TimingCategory::User,
	TimingZoneRole::Work,
	runtimeName);
```

The API interns or copies the name before starting the timed interval, and the zone is marked `DynamicName`. Tooling should make this cost visible. Most FlowUi data should use static zone names plus entity IDs.

---

## 7. Zones that cross public API calls

The frame's most important timing interval is not a C++ lexical scope. Application code runs between:

```cpp
app.beginFrame();
buildUi();
app.endFrame();
app.drawFrame();
```

The monitor therefore needs a manual boundary token owned by `AppWindow`, while normal nested work remains RAII.

```cpp
struct WindowTimingState {
	ManualTimingZone frameTransaction;
	ManualTimingZone userBuildWindow;
	ManualTimingZone preparedToDrawGap;
	WindowFrameKey key{};
};
```

State transitions:

```text
Impl::beginFrame entry
  open FrameTransaction
  time internal BeginFrame
Impl::beginFrame successful exit
  open UserBuildWindow

Impl::endFrame entry
  close UserBuildWindow
  time internal EndFrame and Prepare
Impl::endFrame successful exit
  open PreparedToDrawGap

Impl::drawFrame entry
  close PreparedToDrawGap
  time internal DrawFrame
Impl::drawFrame exit/cancel/exception
  close FrameTransaction with final status
```

This measures application work performed after `beginFrame` as part of the build window, including direct Clay calls and composition loops that are not inside a Flow element. Nested public timing macros naturally become children when the same platform-thread recorder remains attached.

The prepared-to-draw gap should be shown but not treated as FlowUi CPU work. Some applications intentionally prepare frames before other work. It is a `Gap` zone.

Every exceptional or early-return path must close active manual zones:

- failed `beginFrame` before the frame becomes active;
- `endFrame` exception and `cancelStorageFrame`;
- resized/out-of-date/minimized draw path;
- `WindowFrameExitGuard` cleanup;
- window destruction with an active/incomplete transaction;
- application shutdown.

Records carry a status flag such as `Completed`, `Canceled`, `OutOfDate`, `Minimized`, `Exception`, or `Incomplete`. Duration remains valuable even for a canceled frame, but it must not enter ordinary completed-frame statistics without an explicit filter.

---

## 8. Complete frame timeline

### 8.1 Conceptual multi-lane view

The width below represents wall-clock time, not additive cost:

```text
time ───────────────────────────────────────────────────────────────────────────────────────────────▶

App/platform CPU
  [Poll OS events][themes/storage/icons maintenance]
                  ┌────────────────────── WindowFrameKey {main, 42} ──────────────────────────────┐
                  [ BeginFrame ] [       User build window       ] [ End/Prepare ] [gap] [ Draw ]
                    ├ fence wait   ├ element invocation tree       ├ Clay layout         ├ acquire wait
                    ├ collect      ├ direct Clay/user zones        ├ viewport targets    ├ image fence wait
                    ├ storage begin│                               ├ texture bindings    ├ viewport record
                    ├ input drain  │                               ├ instance/run build  ├ UI record
                    └ manager begin└───────────────────────────────└ seal/commit          ├ queue submit
                                                                                        └ queue present call

GPU graphics queue
                                                                                     [submission waits]
                                                                                       [viewport passes]
                                                                                       [UI pass][transitions]

Presentation engine
                                                                                                      [queued/presented]

Later CPU frame-slot reuse
  [wait for fence from submission above]  ───────── causal link back to prior SubmissionSerial ───────┘
```

The key observation is that GPU execution can begin late in `Draw`, overlap the CPU after `drawFrame` returns, and complete before a future frame-slot wait. CPU and GPU durations are parallel lanes; they are not summed into “total frame time.”

### 8.2 Frame API sequence

```mermaid
sequenceDiagram
    participant A as Application
    participant F as App / FlowUi.cpp
    participant U as UiManager
    participant E as Element pipeline
    participant C as Clay
    participant R as Vulkan UI renderer
    participant G as GPU queue

    A->>F: pollEvents()
    F->>F: OS events + shared maintenance
    A->>F: beginFrame(window)
    F->>F: wait reusable frame fence
    F->>U: beginFrame(input, dimensions)
    U->>C: Clay_BeginLayout()
    F-->>A: build window opens
    loop Application composition
        A->>E: createElement(...).draw/construct()
        E->>E: state/resources/hooks/build
        E->>C: emit/configure Clay nodes
    end
    A->>F: endFrame(window)
    F->>U: endFrame()
    U->>C: Clay_EndLayout()
    C-->>U: render commands
    F->>R: prepareFrame(commands)
    R->>R: text layout + instances + runs
    A->>F: drawFrame(window)
    F->>F: acquire + record command buffers
    F->>G: vkQueueSubmit (SubmissionSerial)
    F->>G: vkQueuePresentKHR
    F-->>A: CPU frame transaction closes
    G-->>G: viewport passes + UI pass
    Note over F,G: GPU timestamps are read only after completion; no current-frame wait is added
```

### 8.3 Timing interpretation

From these zones the later report layer can compute:

```text
Window transaction wall time
  = drawFrame exit - beginFrame entry

User build-window wall time
  = endFrame entry - beginFrame exit

Known Flow element inclusive work
  = union/sum of top-level element invocation zones on the platform track

Unattributed build-window time
  = build-window duration - recorded direct children

CPU active vs wait
  = non-overlapping Work/DevToolWork vs Wait roles on the selected critical track

GPU frame work
  = last valid submitted GPU timestamp - first valid submitted GPU timestamp
```

“Unattributed” does not mean wasted. It may be application logic, direct Clay composition, or instrumentation categories that were disabled.

---

## 9. FlowUi CPU zone map

Names should be stable and semantic. Function names and source remain metadata; renaming a private helper should not necessarily break historical comparisons.

### 9.1 App and lifecycle

| Zone name | Role | Placement |
|---|---|---|
| `flowui.app.make` | Work | `makeApplication` outer interval |
| `flowui.app.init` | Work | `App::Impl::init` |
| `flowui.app.poll_events` | Work | `pollEventsAndAdvanceSharedManagers` |
| `flowui.app.shared_maintenance` | Work | theme mutations, storage collect, icon tick |
| `flowui.window.create` | Work | `createWindow` transaction |
| `flowui.window.destroy` | Work | window drain and teardown |
| `flowui.window.swapchain_recreate` | Work/Wait children | recreation path |
| `flowui.app.shutdown` | Work/Wait children | `cleanup` |

`pollEvents` can block depending on backend behavior and should have children distinguishing OS event pumping from manager maintenance.

### 9.2 Per-window frame root

| Zone name | Role | Placement |
|---|---|---|
| `flowui.frame.transaction` | Work container | begin entry through draw exit |
| `flowui.frame.begin` | Work container | internal `Impl::beginFrame` |
| `flowui.frame.user_build_window` | Work container | begin return through end entry |
| `flowui.frame.end_prepare` | Work container | internal `Impl::endFrame` |
| `flowui.frame.prepared_gap` | Gap | end return through draw entry |
| `flowui.frame.draw_submit_present` | Work container | internal `Impl::drawFrame` |

The root is a container, not itself “active CPU work.” Its children determine active, wait, and gap composition.

### 9.3 Begin frame

| Zone | Role | Current code boundary |
|---|---|---|
| `flowui.frame.input_refresh` | Work | backend `refreshInputSnapshot` |
| `flowui.wait.frame_slot_fence` | Wait | `vkWaitForFences(frame.inFlight)` |
| `flowui.storage.complete_collect` | Work | note completed submission + collect |
| `flowui.viewport.frame_start` | Work | `viewPorts.onFrameStart` |
| `flowui.storage.begin_frame` | Work | `storageSystem->beginFrame` and font frame view |
| `flowui.element.begin_window_frame` | Work | state epoch begin |
| `flowui.input.drain` | Work | input queue drain and scaling |
| `flowui.ui.begin_frame` | Work container | `UiManager::beginFrame` |
| `flowui.ui.interaction_advance` | Work | interaction snapshots |
| `flowui.popup.begin_frame` | Work | popup dismissal/suppression state |
| `flowui.input_field.begin_frame` | Work | input-field manager |
| `flowui.shortcut.begin_frame` | Work | shortcut manager |
| `flowui.layout.scroll_update` | Work | Clay scroll update |
| `flowui.layout.begin` | Work | `Clay_BeginLayout` |

The frame-slot fence zone should carry the prior `SubmissionSerial` stored in that slot. The current frame key describes where the wait occurred; the causal submission field describes why.

### 9.4 Element invocation

One outer invocation zone is opened in `ElementBuilder::invoke` after identity is resolved and before state/resource registration. It carries both definition and instance identity.

```text
flowui.element.invoke (instance, definition)
├── flowui.element.registration_state
├── flowui.element.apply_effective_params       future DevFunctionality integration
├── flowui.element.interaction_hooks
│   ├── flowui.element.on_hovered
│   ├── flowui.element.on_pressed
│   ├── flowui.element.on_held
│   └── flowui.element.on_released
├── flowui.element.run_logic
├── flowui.element.build_callback               buildElement
│   └── nested element invocations
└── flowui.element.construct_callback           constructElement
```

Only one of build/construct exists per invocation mode. Resource resolution is lazy and should have its own zone at the actual first `context.resources()` call rather than charging every invocation.

Two measurements are needed:

- **callback duration:** only execution directly inside the definition's hook/build/construct function;
- **invocation/subtree duration:** the complete element pipeline including nested elements produced synchronously.

For construct-only elements, the C++ `constructElement` callback returns before arbitrary caller-authored children are written. The ordinary invocation zone should close when `construct()` returns; a separate **constructed subtree ownership zone** may remain open until `drawConstructed()` closes the root. Do not call the latter “callback time.” This distinction prevents a container from being blamed for all child work without explanation.

#### Detail policy

| Mode | Element clocks | Retained data |
|---|---|---|
| Summary | definition aggregates, optionally sampled | count/total/max by definition |
| Balanced | every invocation outer + callback split | definition aggregates; selected/top instance records |
| Deep | every hook/pipeline child and instance | complete instance trace for bounded capture |

Timing every one of millions of elements can materially distort the workload even in dev mode. Balanced mode should measure outer invocations but compact them into thread-local definition aggregates unless an invocation exceeds a threshold or matches a selected definition/instance. Deep mode retains all records for a short byte-bounded capture.

Do not threshold *before* taking the end timestamp; that cannot work. The recorder times the zone, updates its aggregate, then decides whether to retain the individual record.

### 9.5 End and prepare

| Zone | Role | Current code boundary |
|---|---|---|
| `flowui.ui.end_frame` | Work container | `UiManager::endFrame` |
| `flowui.dev_tool.build` | DevToolWork | legacy/new developer UI construction |
| `flowui.layout.end` | Work | `Clay_EndLayout` only |
| `flowui.input.interaction_snapshot` | Work | hovered/pressed/held/released construction |
| `flowui.input_field.end_frame` | Work | input overrides/finalization |
| `flowui.popup.end_frame` | Work | popup frame finalization |
| `flowui.viewport.prepare_targets` | Work | viewport target sizing/preparation |
| `flowui.icon.prepare_textures` | Work | icon texture preparation, when enabled |
| `flowui.texture.gather_bindings` | Work | image command scan/deduplication |
| `flowui.texture.prepare_bindings` | Work | storage binding preparation |
| `flowui.renderer.prepare` | Work container | `VulkanUiRenderer::prepareFrame` |
| `flowui.renderer.build_upper_bound` | Work | command scan |
| `flowui.renderer.ensure_instance_buffer` | Work | growth/reallocation path |
| `flowui.renderer.build_instances_runs` | Work | Clay conversion, including text layout |
| `flowui.renderer.commit_instance_write` | Work | buffer write commit |
| `flowui.storage.seal_frame` | Work | seal read lease |
| `flowui.element.commit_window_frame` | Work | state commit/GC bookkeeping |

`Clay_EndLayout` is only the layout/finalization part. Resource preparation and renderer CPU conversion remain siblings rather than being hidden inside “layout.”

Text timing should initially be aggregated inside renderer preparation, with deeper zones for `TextLayoutService::layout` and cache miss/build paths in Balanced/Deep mode. Do not emit one zone per glyph.

### 9.6 Draw, submit, and present

| Zone | Role | Current code boundary |
|---|---|---|
| `flowui.swapchain.resize_check` | Work | extent comparison/recreate decision |
| `flowui.wait.acquire_image` | Wait | `vkAcquireNextImageKHR` host duration |
| `flowui.wait.swapchain_image_fence` | Wait | prior image-in-flight fence, if present |
| `flowui.renderer.command_buffer_begin` | Work | reset/begin primary command buffer |
| `flowui.viewport.record_all` | Work container | all referenced viewports |
| `flowui.viewport.record` | Work | one viewport, entity = viewport key |
| `flowui.viewport.callback` | Work/User | application render callback |
| `flowui.renderer.record_ui` | Work | `recordPreparedFrame` CPU recording |
| `flowui.renderer.command_buffer_end` | Work | final transition/end |
| `flowui.renderer.queue_submit` | Work | host call + submission bookkeeping |
| `flowui.wait.present_call` | Wait | `vkQueuePresentKHR` host duration |
| `flowui.swapchain.recreate_after_present` | Work/Wait | suboptimal/out-of-date path |

`vkQueueSubmit` normally queues work and returns; its host duration is not GPU duration. `vkQueuePresentKHR` can block for presentation pacing, but it does not directly state scanout duration. Both must retain their API semantics in names/tooltips.

Viewport callback and viewport command-recording durations are already separately measured today. Preserve that split in general zones.

---

## 10. GPU timing design

### 10.1 Why host timers are insufficient

The CPU records Vulkan commands and submits them. The GPU executes later and may overlap subsequent CPU work. Only device timestamps can measure that queue execution. Vulkan timestamp queries are asynchronous, report valid-bit and timestamp-period properties, and can be read on the host after availability ([Vulkan Queries](https://docs.vulkan.org/spec/latest/chapters/queries.html)).

### 10.2 Per-frame-slot query resources

Add to each Vulkan frame resource:

```cpp
struct GpuTimingFrameSlot {
	VkQueryPool pool = VK_NULL_HANDLE;
	uint32_t capacity = 0;
	uint32_t used = 0;
	WindowFrameKey frame{};
	SubmissionSerial submission = 0;
	std::vector<GpuZonePlan> zones;
	bool submitted = false;
};
```

At frame-slot reuse:

1. Wait for the existing `frame.inFlight` fence as FlowUi already does.
2. If that slot had a submitted timing plan, call `vkGetQueryPoolResults` **without** `VK_QUERY_RESULT_WAIT_BIT`; the fence already proves the command buffer completed.
3. Validate availability/results, convert tick deltas using `timestampPeriod`, and append GPU timing records attributed to the older `WindowFrameKey`/`SubmissionSerial`.
4. Reset/reuse the query pool for the new recording. Use host reset if the enabled Vulkan version/features support it, otherwise record `vkCmdResetQueryPool` before writes.

Never wait solely to make the profiler current. GPU data naturally appears several CPU frames late.

### 10.3 Initial GPU zones

Start coarse:

```text
flowui.gpu.submission
├── flowui.gpu.viewport_pass       one per referenced viewport, bounded
└── flowui.gpu.ui_pass
```

The submission zone brackets the recorded primary command buffer from the first relevant operation through the final present layout transition. Each viewport zone brackets the primary-command-buffer operations that execute its secondary callback and transitions. The UI pass brackets dynamic rendering and all prepared UI runs.

Do not add one timestamp pair per UI run or element initially. Query pools are finite, timestamps perturb execution, and tiny GPU regions have limited precision. A later selected-run deep mode can reserve a larger pool.

### 10.4 Writing timestamps

With Vulkan 1.3/synchronization2:

```cpp
vkCmdWriteTimestamp2(
	cmd,
	VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
	queryPool,
	beginQuery);

// Recorded GPU work for the zone.

vkCmdWriteTimestamp2(
	cmd,
	VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
	queryPool,
	endQuery);
```

This measures passage between broad pipeline points, including stalls relevant to that queue interval. More focused passes may choose the earliest/latest stages relevant to their commands, but the descriptor must store stage semantics. The Vulkan documentation notes that a timestamp write is tied to the specified pipeline stage and written into a query pool ([`vkCmdWriteTimestamp2`](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdWriteTimestamp2.html)).

Before enabling GPU timing, verify:

- the graphics queue family has non-zero `timestampValidBits`;
- query count and pool creation succeeded;
- the selected command/version path supports the chosen reset/write operations;
- timestamp wrap is masked according to valid bits;
- tick deltas are multiplied by `VkPhysicalDeviceLimits::timestampPeriod`;
- a query plan is retained only if its command buffer was submitted.

If capacity is exhausted, retain outer zones, stop allocating finer pairs, and flag truncated GPU detail. Do not wrap query indexes within the same submission.

### 10.5 CPU/GPU correlation

GPU duration does not require a shared CPU clock. To draw accurately aligned CPU and GPU lanes, use `VK_KHR_calibrated_timestamps` when available. Store calibration pairs and maximum deviation, recalibrating periodically; Vulkan explicitly warns that deviation varies and long extrapolation can become inaccurate ([`vkGetCalibratedTimestampsKHR`](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetCalibratedTimestampsKHR.html)).

When calibration is unavailable:

- show GPU zones on a submission-relative lane;
- align their container approximately at CPU submission for navigation only;
- visibly label absolute CPU/GPU overlap as uncalibrated;
- still report accurate within-GPU durations.

Each GPU record includes `WindowFrameKey`, `SubmissionSerial`, queue/track ID, timestamp stages, duration, calibration ID, and query quality flags.

### 10.6 Presentation boundary

The graphics submission timestamp does not measure display scanout. The present host-call zone reveals CPU blocking; exact presentation completion may be available through the swapchain maintenance/present-wait mechanisms already used by FlowUi, but presentation timing varies by platform/extensions.

The first timing implementation should report:

- CPU `vkQueuePresentKHR` call duration;
- graphics submission GPU duration;
- eventual present completion wait when FlowUi actually performs one for retirement/reuse;
- present ID/fence correlation when available.

It should not invent “display latency.” A later latency feature needs input timestamps and platform presentation timing.

---

## 11. Producer, buffering, and aggregation path

### 11.1 Thread attachment

Every timed thread gets one `DevTimingRecorder` attachment containing:

- stable track/thread ID and display name;
- active zone stack;
- cached category mask and configuration generation;
- bounded single-producer record buffer;
- per-definition aggregate table for element summary mode;
- dropped/suppressed/misnested counts.

The platform thread is attached during app initialization. Worker threads must attach explicitly. A recorder must not migrate across threads.

### 11.2 Hot path

The intended enabled-zone hot path is:

```text
category/detail check
  -> monotonic clock read
  -> stack push
  -> application work
  -> monotonic clock read
  -> stack pop/direct-child accumulation
  -> append fixed-size record OR update local aggregate
```

No string construction, JSON, file I/O, UI update, sorting, global mutex, or general heap allocation belongs here.

### 11.3 Buffers

Use bounded per-thread SPSC chunks/rings drained by the monitoring owner. If a producer buffer is full, drop new detailed records and increment a quality counter; do not block the application thread and do not overwrite memory the consumer may be reading.

The central retained history can later implement a byte-bounded circular flight recorder. That policy belongs above producers: producers protect the hot path, while central retention decides which old frames are evicted or pinned.

Zone metadata registration may take a lock once per descriptor's first enabled use. Prefer registering built-in descriptors during monitor initialization so ordinary frame paths never hit that lock.

### 11.4 Configuration updates

The monitor owns an atomic configuration generation and category/detail mask. Recorders check the generation at safe points—at least frame boundaries—and cache the mask. Avoid multiple atomics per zone when the configuration did not change.

Disabling a category while one of its zones is active affects new zones only; the already-open zone closes normally. A capture records which categories and detail level were active.

---

## 12. Capture levels and overhead policy

Development mode permits overhead, but the monitor must be able to distinguish the application from its measuring equipment.

### 12.1 Proposed CPU levels

| Level | CPU timing | Expected use |
|---|---|---|
| OnlyFrameTime | one total window-frame interval and no nested CPU zones | retain a minimal health value and establish monitoring's incremental cost |
| Summary | lifecycle/frame phase zones, waits, coarse renderer, compact element-definition aggregates | always-on dev health |
| Balanced | all subsystem zones, element invocation/callback timing, selected instance retention | normal investigation |
| Deep | hook-level zones, all element instances, text/cache internals, user-selected fine zones | short bounded capture |

`OnlyFrameTime` is CPU level zero, but it is intentionally not “off”: every completed window frame still produces one total duration. Comparing it with Summary/Balanced/Deep isolates the incremental cost of detailed monitoring while keeping enough information to report whether the frame became faster. With `FLOW_UI_DEV_MODE=OFF`, macros and ownership compile away completely.

GPU timing is independent of these CPU levels. `gpuTimingEnabled` controls Vulkan timestamp-query creation and recording separately, so a developer can use detailed CPU timing without GPU queries or combine `OnlyFrameTime` with GPU timing. Fine GPU detail can later receive its own configuration, but must not be inferred from the CPU timing level.

### 12.2 Element sampling option

If element clocks dominate a pathological million-element frame, Summary mode may sample invocations deterministically per definition—for example every Nth invocation—while always retaining call count. Sampled timing must be labelled and scaled estimates must include sample count. Balanced/Deep captures use exact measured invocations.

Selected definitions/instances can always be promoted to exact timing regardless of the global sampling rate.

### 12.3 Self-measurement

Record monitoring overhead separately:

- CPU time draining/merging records;
- records and bytes emitted per thread/frame;
- dropped records and stack/query exhaustion;
- metadata registration time;
- tool rendering time under `DevToolWork`;
- GPU timestamp query count;
- calibrated empty-zone cost.

Do not create a normal timing zone around every recorder operation; that recursively instruments itself. Use direct coarse counters/timestamps at drain/frame-finalization boundaries.

### 12.4 Developer tooling contamination

The existing in-window developer panel changes application layout and contributes element/build/render work. Every new tool element must be marked `DevToolWork`/dev-internal so later reports can show:

- observed total frame;
- application work excluding explicitly delimited tool CPU zones;
- tool work;
- shared/ambiguous work such as Clay layout or GPU pass cost that cannot be cleanly subtracted.

A separate companion window reduces contamination but still shares the process, GPU, and platform thread. The monitor should never claim a perfect counterfactual “without tools” value.

---

## 13. Threading and asynchronous work

FlowUi currently enforces frame triplets on the platform thread and allows only one active window triplet at a time. Design the timing system for more tracks now so later parallel text/resource work does not require a format rewrite.

Rules:

1. CPU zones are strictly nested only on their own thread track.
2. A child zone cannot end on another thread.
3. Cross-thread work uses a future causal/flow identity, not a fake parent stack.
4. Worker completion can be associated with a `WindowFrameKey` even if it occurs after the platform thread closes that frame.
5. Aggregating durations across threads must use interval unions or a critical-path model; summing thread time is utilization/work, not wall time.
6. GPU queues are independent ordered tracks.

The timing record already contains frame/entity identity. The later EventsAndReports step can add explicit flow edges for job dispatch/completion without changing zone records.

---

## 14. Failure and edge-case semantics

### 14.1 Exceptions

Lexical RAII zones close during stack unwinding. Manual frame boundaries close from existing cancellation/exit guards. Monitoring functions are `noexcept` and degrade by dropping detail.

### 14.2 Misnested zones

If an explicit/manual token ends out of order:

- mark the active stack corrupted for that track;
- close or discard affected zones as `Incomplete` at the next safe boundary;
- increment quality diagnostics;
- resume with an empty stack at the next app tick/window frame;
- do not crash the application solely for profiling.

Debug assertions may fire in timing-system unit tests, but runtime dev tooling should remain recoverable.

### 14.3 Canceled/out-of-date/minimized frames

Keep their zones with status flags. They are essential for diagnosing resize/recreation stalls. Default steady-state statistics filter them out, while a lifecycle view includes them.

### 14.4 Very short zones

Durations near clock resolution/overhead should remain raw but carry a low-resolution warning when summarized. Aggregate many samples before drawing conclusions. Do not display false decimal precision.

### 14.5 Clock anomalies

Clamp no normal duration silently. If `end < start`, mark `ClockAnomaly`, retain zero as display duration, and increment a quality diagnostic. GPU valid-bit wrap is handled explicitly before conversion.

### 14.6 Recorder and query exhaustion

Prefer outer/critical zones over fine detail. A frame with dropped records remains visible but is labelled incomplete and excluded from exact exclusive-time claims.

### 14.7 Multiple applications

Each `App` has its own timing epoch, descriptor registry, frame identities, and producers. Export can include a process/app UUID. No process-global mutable singleton owns records.

---

## 15. Detailed first implementation shape

### 15.1 Minimal types

The first implementation should introduce:

```cpp
class DevTiming;
class DevTimingRecorder;
class CpuTimingZone;
class ManualTimingZone;
class DevGpuTiming;

struct DevTimingConfig;
struct TimingZoneDescriptor;
struct CpuTimingRecord;
struct GpuTimingRecord;
struct WindowFrameKey;
struct TimingEntityRef;
```

`DevMonitoringAndReporting` initially owns only `DevTiming`:

```cpp
class DevMonitoringAndReporting {
public:
	DevMonitoringAndReporting() noexcept;
	~DevMonitoringAndReporting();

	DevTiming& timing() noexcept;
	const DevTiming& timing() const noexcept;

private:
	std::unique_ptr<DevTiming> timing_;
};
```

A PIMPL is reasonable because this is development-only and the recorder internals will evolve. If compile time and implementation simplicity favor a direct member initially, use one; ABI stability is not yet the goal.

### 15.2 Frame integration state

Add a development-only timing state to `AppWindow`, not `UiManagerState`:

```cpp
#if FLOW_UI_DEV_MODE
devSystems::WindowTimingState timing{};
#endif
```

This state spans `App::beginFrame`, application code, `App::endFrame`, and `App::drawFrame`; `AppWindow` already owns the lifecycle phase and frame/submission state that guarantee correct closure.

`UiManager` receives a recorder pointer/current frame context during `initStorage` or attachment. Element builders use the same recorder through a narrow internal bridge, analogous to—but separate from—the legacy `devModeBridge`.

### 15.3 Build flags

```cmake
option(FLOW_UI_DEV_TIMING "Compile development timing monitoring" ON)
set(FLOW_UI_DEV_TIMING_LEVEL "2" CACHE STRING "Maximum compiled timing detail")
```

These options are meaningful only when `FLOW_UI_DEV_MODE=ON`:

- level 0: `OnlyFrameTime` total window-frame timing;
- level 1: lifecycle/frame/coarse GPU;
- level 2: balanced element/subsystem zones;
- level 3: deep instrumentation compiled in.

Runtime configuration can select any level up to the compiled maximum. Production builds define timing macros to no-ops and do not compile timing sources.

---

## 16. Implementation sequence

### Step 1 — CPU foundation

1. Add timing types, stable built-in descriptors, monotonic clock epoch/calibration, recorder attachment, active stack, completed records, and bounded producer buffers.
2. Add RAII and manual zone APIs/macros with true production no-op expansions.
3. Add quality counters for drops, overflow, mismatch, incomplete zones, and timing overhead.
4. Give `DevMonitoringAndReporting` ownership of `DevTiming` but do not yet expose tool UI.

### Step 2 — lifecycle and frame boundaries

1. Add monotonic `WindowFrameKey` and `AppTickId` tracking.
2. Instrument application creation/init, polling, secondary-window creation, swapchain recreation, and shutdown.
3. Add cross-call frame transaction, user-build, prepared-gap, and cancel/status handling in `AppWindow`.
4. Replace current coarse `PerformanceDiagnostics` clock pairs with built-in zones and derive compatibility fields.

This step alone produces a useful frame timeline without touching element internals.

### Step 3 — subsystem and element zones

1. Instrument `UiManager` begin/end children, popup/input/shortcut paths, Clay begin/end, render preparation, texture bindings, storage seal/commit, viewport record/callback, and command recording.
2. Instrument `ElementBuilder::invoke`, state/resource resolution, interaction hooks, logic, build/construct callbacks, and constructed subtree ownership.
3. Implement definition-local aggregates and selected/deep instance retention.
4. Instrument timing monitor/tool overhead separately.

### Step 4 — Vulkan GPU zones

1. Validate timestamp support and create query pools per frame slot.
2. Add whole-submission, viewport-pass, and UI-pass query pairs.
3. Read completed results at existing fence reuse with no new waits.
4. Convert valid-bit tick deltas with `timestampPeriod`, attach frame/submission identity, and handle query truncation.
5. Add calibrated CPU/GPU alignment when supported, with explicit uncalibrated fallback.

### Step 5 — retained timing snapshots

1. Drain producers at safe frame/app-tick points into compact per-frame timing snapshots.
2. Preserve CPU tracks, GPU tracks, descriptor/source tables, capture configuration, and quality metadata.
3. Expose read-only snapshot/range APIs for the future `DevTooling` and `monitoringEventsAndReports` work.
4. Keep statistics/findings deliberately outside this step.

---

## 17. Acceptance criteria for timing

The timing system is ready for the next monitoring phase when all of the following are true:

- A main or secondary window has a monotonic historical frame key independent of frame slot.
- The timeline includes poll, begin internals, the complete user build window, end/layout/preparation, the prepared gap, draw/record, submit, and present call.
- Canceled and exceptional frame transactions cannot leave an active timing stack behind.
- CPU work, CPU waits, gaps, dev-tool work, and GPU work are semantically distinct.
- Element reports can distinguish callback/own time from invocation/subtree time and definition aggregates from instances.
- Vulkan GPU duration comes from timestamp queries and appears later without adding a wait.
- Fence waits link to the older submission that owns the waited fence.
- Multi-window frames and one global app tick are representable without conflation.
- Disabled categories avoid clock reads; production macros do not evaluate arguments.
- The hot path performs no string formatting, JSON, file I/O, UI work, or global locking.
- Buffer/query/stack loss is visible in quality metadata.
- Monitor and tool overhead are measurable.

---

## Final design decision

The timing monitor should be a semantic, hierarchical tracer rather than a larger collection of stopwatches. CPU zones use RAII and strict per-thread nesting; the public frame lifecycle adds guarded manual zones across API calls; element timing preserves own versus subtree cost; Vulkan zones use delayed timestamp queries tied to submission identity.

The central model is:

```text
static descriptor
    + completed invocation
    + ordered track
    + frame/app-tick/submission identity
    + semantic role
    + entity identity
    + capture-quality metadata
```

That is sufficient to draw the complete timing timeline now and to support percentiles, bottleneck classification, semantic findings, memory correlation, and developer tooling later without redesigning what a measured interval means.
