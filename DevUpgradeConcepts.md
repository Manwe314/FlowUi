# FlowUi Developer Experience Upgrade Concepts

## Purpose

FlowUi's next developer-mode system should make a programmer able to answer three questions without leaving the running application:

1. **What is happening?** Inspect elements, layout, resources, actions, memory, and the complete CPU/GPU frame history.
2. **Why is it happening?** Move from a slow frame or suspicious allocation to the responsible subsystem, definition, instance, source location, and render work.
3. **Can I safely change it?** Try a change live, understand its scope and provenance, then either discard it or turn it into typed build input.

This is not an FSEL-only feature. FSEL should provide excellent built-in metadata and previews, but every valid Flow element and every application using the core runtime must be able to participate. The developer system is therefore a library subsystem with an FSEL-based Interface on top, not a property of the FSEL catalog.

The recommended architecture is an **application-scoped developer session** with lightweight per-window and per-subsystem producers. It should replace the current collection of mostly window-local facilities without turning `UiManager` into a universal manager. A separate FlowUi-built tool window should be the default surface; in-application overlays remain available for picking and constrained environments.

The production boundary is equally important:

- `FLOW_UI_DEV_MODE=OFF` removes recording, reflection, live editing, the tool UI, debug strings, and the development protocol.
- Only changes that the programmer explicitly **bakes** cross that boundary.
- Baked changes are typed generated C++ (or changes promoted to authored source), not a JSON parser and mutable development registry shipped in the application.

The aim is not to replace Tracy, Perfetto, VTune, RenderDoc, or platform profilers. It is to provide the missing FlowUi semantic layer: those tools can show that a C++ function or GPU pass is expensive; FlowUi should show which window, element definition, element instance, layout decision, render-run split, theme value, or resource lifetime caused it.

---

## 1. Existing developer systems

This section is intentionally brief. It records the pre-upgrade baseline from
which the architecture was designed. Its present-tense observations are retained
as historical context; the completion stamp below supersedes them for
`DevMonitoringAndReporting`.

The three major development subsystems are named `monitoringAndReporting`, `Tooling`, and `Interface`. `Tooling` is the subsystem previously called Functionality; `Interface` is the subsystem previously called Tooling. These names are used throughout the reports from this point onward.

### Development status stamp — `DevMonitoringAndReporting` closed

`DevMonitoringAndReporting` is accepted as the first completed subsystem of the
new DevSystems architecture for the current development stage.

Its three monitoring domains now form one application-owned, correlated system:

- timing provides bounded CPU/GPU histories, frame and invocation identities,
  capture levels, quality metadata, and retained reporting;
- memory provides semantic source accounting, capacity/lifetime events,
  checkpoints, retention, statistics, and configuration previews;
- errors provide the frozen production-contract sidecar, source and breadcrumb
  histories, raw stacks, deferred state evidence, timing/memory correlation,
  triggered captures, ranked static advice, fatal capsules, and measurable
  capture/reporting overhead.

The subsystem boundary is also accepted: producers remain narrow and bounded;
reporters own retention and derivation; monitoring never applies runtime changes;
and future consumers receive read-only models rather than inspecting managers or
monitor buffers directly. `FLOW_UI_DEV_MODE=OFF` remains the production boundary.

This stamp means the chapter is closed, not that every conceivable probe is now
implemented. Already-recorded refinements—legacy DEV build migration, additional
source coverage, optional deeper attribution/symbolization, and eventual export
consumers—remain valid follow-up work and do not require reopening the subsystem's
ownership or data-model design.

The next subsystem is canonically **`DevTooling`**, previously named
`DevFunctionality`. The former `DevTooling` presentation shell is canonically
**`DevInterface`**. Existing empty shell class/file names may retain the old names
until that mechanical migration is performed as part of the next chapter; they do
not redefine the architecture.

### 1.1 `monitoringAndReporting`

`PerformanceDiagnostics` is a useful first frame summary. It records CPU-side durations around `beginFrame`, Clay layout/resource preparation, fence waiting, image acquisition, viewport/UI command recording, submission, and presentation. It also records Clay command counts, UI instances/runs, glyphs/images, viewport work, buffer growth, descriptor rebuilds, and a 180-frame rolling average/P95/maximum.

Its present limits are substantial:

- The retained 180 samples are private; the Interface receives a latest-frame and aggregate view, not a navigable timeline.
- `deltaMs` is the interval between frame starts, while `drawFrameMs` and its children are host durations. Neither is GPU execution time.
- Fence/acquire/present waits contain useful synchronization evidence, but cannot by themselves identify the application as GPU-bound.
- Only one P95 calculation and a few means are exposed. There is no custom percentile, distribution, comparison, or per-frame drill-down.
- There are no general nested spans, flow edges, instant events, subsystem categories, thread tracks, or element build timings.
- Startup and `makeApplication` are not timed.

The storage system already exposes valuable development-only snapshots: reserved, committed, live and peak bytes; allocation and growth counts per memory class; GPU live/retired bytes; budgets; resource-slot states; upload bytes; binding-cache hits/misses; invalid handles; submission serials; window bindings; and transient high-water marks. These are current/cumulative counters rather than an allocation timeline. They do not yet reveal churn, lifetime, owner, or call site.

The important conclusion is that FlowUi has instrumentation points and counters, but not yet a profiler data model or reporting engine.

### 1.2 Tooling

The current `DevRuntime`, registry macros, and element capture provide:

- a flat pre-order element tree with depth, definition and instance identity, source location, authored keys, labels, and internal/floating flags;
- reflected parameter fields with definition-scoped and instance-scoped overrides;
- current parameter snapshots;
- maps and UI affordances intended for state and resource overrides;
- JSON export and a separate `flowui_devChange_updater` source-patching program.

The runtime already has exact-instance parameter override keys. This is a good answer to the case where many instances originate at one line, provided those instances have stable distinct Flow identities. The missing piece is durable persistence: a source expression at one call site cannot encode different values for loop-created instances, and automatically derived identities may change when structure or order changes.

There are also two correctness/integration gaps to fix before expanding the Interface:

- `ElementBuilder::invoke` currently runs interaction hooks before applying development parameter overrides. Rendering can therefore use an overridden value while `onPressed`, `onHeld`, or `runLogic` saw the authored value.
- State/resource override maps and snapshot APIs are present, but the normal element state/resource paths do not apply or populate them. The property UI currently exposes a promise the runtime does not fulfil.

Reflection is deliberately narrow: scalar types, `std::string`, `uint8_t` enums, and several hard-coded Clay composite shapes. It is not yet a general schema for constraints, nested values, optionals, resources, actions, validation, provenance, or safe mutation semantics.

### 1.3 Interface

The current debug view provides a resizable in-application panel, instance/definition hierarchy modes, reflected property editors, export, and a compact performance footer. Internal dev elements can be hidden from capture.

It is now architecturally behind the rest of the library:

- Much of the panel is built from the older `ElementDefinition`/`devFlowElements` stack rather than the current element system and FSEL conventions.
- There is no point-and-click element picker, application overlay, computed box/layout inspection, timeline, flame view, frame history, memory/resource explorer, action/theme editor, isolated element viewer, findings center, or offline trace viewer.
- Hiding developer nodes from the element tree does not remove their CPU/GPU work from the measured frame. A docked panel also changes the application's available layout area.
- Source updates require exporting and manually running another compiled executable.

The existing panel proves that FlowUi can build its own tools. It should be treated as a prototype to migrate, not as the UI foundation to keep extending.

### 1.4 Overall conclusion

The repository has four strong seeds: stable definition/instance identities, compile-time element knowledge, centralized storage, and a Vulkan renderer whose stages are already explicit. What is missing is a coherent development session that correlates them.

The upgrade should not be a larger `PerformanceDiagnostics` struct or a larger `DevRuntime`. It should separate collection, retained data, derived reports, safe interventions, and UI consumers so each can evolve independently.

---

## 2. Proposed upgrade

## 2.1 `monitoringAndReporting`

### 2.1.1 Ownership: one session, many producers

Introduce an app-owned `DevSession` (the exact public name can change). `App` owns data that spans initialization, storage, themes, actions, resources, windows, and GPU queues. Each `UiManager` registers a `DevWindowProbe` for element/layout/input data. Storage, the renderer, `ActionManager`, `ThemeManager`, and `ElementManager` publish through narrow producer interfaces.

```cpp
namespace FlowUi::devMode {

class DevSession {
public:
    DevTraceRecorder& trace() noexcept;
    DevInspectionModel& inspection() noexcept;
    DevOverrideStore& overrides() noexcept;
    DevReportEngine& reports() noexcept;
    DevChangeSet& changes() noexcept;

    DevWindowProbe& attachWindow(WindowId, UiManager&);
    void beginStartupCapture();
    void endStartupCapture();
};

} // namespace FlowUi::devMode
```

`UiManager` may expose convenience accessors, but it should not own application-wide history. This matters for multi-window applications: shared allocations and GPU submissions must be recorded once, while selection and layout remain window-specific.

A tool is a consumer of the session, not the session itself. The same producer data can feed:

- the live FlowUi tool window;
- a native `.flowtrace` writer;
- an optional Perfetto exporter;
- headless regression reports;
- eventually a local socket consumer, without putting networking into the first implementation.

### 2.1.2 A small, uniform event vocabulary

Use a trace model rather than adding one field for every new measurement. Perfetto's Track Event model is a useful precedent: nested time slices, counters, and flows are simple primitives that support rich analysis, while categories allow expensive instrumentation to be disabled ([Perfetto Track Events](https://perfetto.dev/docs/instrumentation/track-events)). FlowUi needs five record kinds:

| Record | Meaning | Examples |
|---|---|---|
| Span | Time-bounded work, nested where appropriate | element callback, Clay layout, command recording |
| Counter | Value at a time | live bytes, run count, cache hit rate |
| Instant | A discrete fact | buffer grew, invalid handle, popup dismissed |
| Flow | Causal link across tracks or frames | upload request to GPU use, action to state change |
| Allocation | Specialized lifetime event | allocate/free/arena reset/resource retire |

Hot records should be fixed-size POD values containing numeric IDs, timestamps, and compact attributes. Names, source locations, categories, and type descriptions are interned once. Do not allocate strings or mutate shared hash maps at every scope boundary.

```cpp
struct DevSpanEvent {
    uint64_t startNs;
    uint64_t durationNs;
    uint64_t frameId;
    uint64_t parentSpanId;
    uint64_t entityId;
    uint32_t nameId;
    uint32_t sourceId;
    uint16_t category;
    uint16_t track;
    uint16_t window;
    uint16_t flags;
};
```

Each producing thread writes to a thread-local chunk or single-producer buffer. A session consumer merges chunks by timestamp outside the hot path. Dropped records, buffer saturation, merge lag, and recorder time must themselves be counters; invisible profiler loss is worse than an explicit incomplete trace.

### 2.1.3 Instrumentation API and compile boundary

Use RAII zones plus explicit begin/end for asynchronous work. Internal names should be static/interned, and applications should be allowed to add their own FlowUi-relevant zones around work performed between Flow calls.

```cpp
#if FLOW_UI_DEV_MODE
    #define FLOWUI_DEV_ZONE(category, name) /* RAII scope */
    #define FLOWUI_DEV_ZONE_ENTITY(category, name, entity) /* RAII + identity */
    #define FLOWUI_DEV_COUNTER(category, name, value) /* sample */
    #define FLOWUI_DEV_MARK(category, name) /* instant */
#else
    #define FLOWUI_DEV_ZONE(category, name) ((void)0)
    #define FLOWUI_DEV_ZONE_ENTITY(category, name, entity) ((void)0)
    #define FLOWUI_DEV_COUNTER(category, name, value) ((void)0)
    #define FLOWUI_DEV_MARK(category, name) ((void)0)
#endif
```

The off form must not evaluate arguments. The dev form should check a cheap category bit before taking a clock reading. Categories should include at least `Frame`, `Element`, `Layout`, `Renderer`, `GPU`, `Storage`, `Resource`, `Text`, `Input`, `Action`, `Theme`, `Popup`, `Startup`, and `DevTool`.

Provide modes rather than one all-or-nothing fire hose:

| Mode | Default collection | Intended use |
|---|---|---|
| Summary | Frame phases, counters, aggregate definition cost | Always on in dev builds |
| Balanced | Per-frame spans, renderer/resource events, selected element detail | Normal diagnosis |
| Deep | Every element instance, allocations, verbose layout/text events | Short captures only |
| Startup | Initialization and first usable frame | Launch diagnosis |

The tool can switch runtime categories, but some especially expensive probes may require a `FLOW_UI_DEV_TRACE_LEVEL` rebuild. This is acceptable in developer mode and makes cost visible.

### 2.1.4 Define “frame time” before reporting it

The tool should show several related clocks instead of one ambiguous number:

- **Presentation interval:** time between presented/displayed frames, when the platform exposes it.
- **Application frame transaction:** `beginFrame` through completion of FlowUi's draw/present call.
- **CPU active time:** non-overlapping active work on the frame's critical CPU path.
- **CPU blocked time:** fence, acquire, present, locks, and other waits.
- **GPU execution:** timestamped queue work attributable to the frame.
- **End-to-end latency:** input sample to presented result, only when the required platform data exists.

CPU and GPU overlap. Adding all thread durations or adding CPU and GPU frame durations produces false totals. The report engine should distinguish inclusive duration, exclusive/self duration, wall-clock critical path, and utilization.

“CPU-bound”, “GPU-bound”, “presentation-bound”, and “mixed/unknown” should be findings with evidence and confidence:

- CPU-bound evidence: the CPU critical path exceeds the budget/GPU path and the GPU is repeatedly starved.
- GPU-bound evidence: timestamped GPU execution or queue backlog exceeds the CPU path and later causes frame-resource waits.
- Presentation-bound evidence: CPU/GPU work is below budget while acquire/present pacing dominates.
- Unknown: GPU timestamps unavailable, calibration poor, or the evidence conflicts.

The current `drawFrameMs` is useful CPU work, but must never be relabelled as GPU time.

### 2.1.5 Real GPU timing

Add a timestamp query pool per frame-in-flight/queue and write timestamps around the full UI GPU workload and major passes. Vulkan's timestamp command writes device timestamps into query pools at specified pipeline stages ([`vkCmdWriteTimestamp2`](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdWriteTimestamp2.html)). Read results asynchronously only after the owning frame is known complete; never stall the current frame just to display profiling data.

Start with coarse zones:

- full submitted frame;
- each viewport render pass;
- UI pass;
- uploads/transfers when applicable.

Do not issue a pair of GPU queries for every element or render run by default. Query availability and query count are finite, and timestamp instrumentation has cost. A selected-run or short deep-capture mode may add finer zones later.

Use `timestampPeriod` and valid timestamp bits correctly. If `VK_KHR_calibrated_timestamps` is supported, periodically correlate host and device domains and retain the maximum deviation; Vulkan explicitly requires callers to judge deviation and recalibrate over time ([`vkGetCalibratedTimestampsKHR`](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetCalibratedTimestampsKHR.html)). When calibration is absent, show duration on a GPU-relative track without pretending the CPU/GPU start alignment is exact.

### 2.1.6 What to time

#### Startup and first usable frame

Wrap the existing `App::Impl::init` sequence and `makeApplication` path. At minimum record:

- backend and window-system creation;
- Vulkan instance, physical-device choice, logical device, queues, and allocator/storage initialization;
- manager construction/attachment;
- window state, swapchain, and frames-in-flight;
- renderer pipelines, descriptor state, and fallback texture;
- font/image/icon managers and default FSEL icon/font registration;
- element resource preparation;
- first begin/build/layout/record/submit/present;
- total time to application object and total time to first presented UI.

Startup capture must begin before normal per-window dev state exists. This is one reason the session belongs to `App`, or to a tiny bootstrap recorder transferred into `App` during construction.

#### Frame lifecycle

Record explicit spans for input sampling/routing, staged theme mutations, storage frame begin/commit/cancel, UI begin, application UI build, element processing, Clay layout, resource preparation/uploads, render-command conversion, viewport callbacks, command recording, submission, presentation, transient reset, state garbage collection, and popup processing.

User code needs a clear `uiBuild` boundary. If FlowUi cannot infer the full boundary from the existing API, offer a public development-only RAII region. Otherwise “UI build” will mean only time inside element callbacks and omit the user's composition loop.

#### Elements

Measure two different things deliberately:

1. **Own invocation work:** state lookup, resource resolution, override/capture cost, interaction callbacks, `runLogic`, and `buildElement`/`constructElement` callback.
2. **Inclusive subtree work:** time until a construct-only element's child scope closes or the build callback returns, including descendants it emits.

Nested scopes allow exclusive time to subtract child scopes. Reports must label these meanings; a container that merely owns many expensive children should rank high by inclusive subtree cost but low by own callback cost.

Summary mode should aggregate by definition without retaining every invocation:

```cpp
struct ElementCostAggregate {
    uint64_t invocationCount;
    uint64_t totalOwnNs;
    uint64_t totalSubtreeNs;
    uint64_t maxOwnNs;
    QuantileSketch ownDistribution;
};
```

Balanced/deep mode can retain instance spans. High-cardinality dimensions—instance ID and source call site—must be opt-in, selected, or restricted to top offenders. Definition-level aggregation remains cheap and answers the common question.

The tool should present two leaderboards side by side:

- **Total impact:** total exclusive time and call count. This identifies the 10 μs element invoked a million times.
- **Per-call headroom:** maximum and high-percentile own time. This identifies the 1 ms element invoked once.

Neither replaces the other.

#### Layout, text, and rendering

Capture data that explains work, not just duration:

- Clay elements and render commands produced;
- visible vs culled elements and commands;
- layout depth, child count, overflow/clip/scroll counts, and floating elements;
- text measurement/layout calls, glyphs, line count, cache hits/misses, and bytes retained;
- render-command counts by type;
- UI instances, render runs, glyphs, images, and per-run instance count;
- run-break reasons such as texture, clip/scissor, pipeline, and viewport changes;
- bytes uploaded, buffer capacity/high-water/growth, and descriptor rebuild reason;
- viewport callback, record, resize, and pixel-area data;
- where practical, element ID to emitted Clay/render-command span and render-run membership.

This permits useful findings: “instance count is reasonable, but alternating textures create 4,000 one-instance runs” is much more actionable than “UI record took 3 ms.”

#### Actions and input

`ActionManager` already records definition/source metadata and invocation status in development builds. Extend that into events:

- invocation source, target ID/recipe, availability, status, and duration;
- discarded result and exception/error status;
- input event to routed element to action flow edges;
- focus/pointer-capture changes and click-consumption decisions;
- callback time by action and invocation count.

Never expose or serialize borrowed action resources. Existing `ActionDebugInfo` correctly avoids doing so.

### 2.1.7 Memory and resource observability

The default scope is **FlowUi-owned memory**, not every allocation in the process. Instrumenting global `new`/`delete` can be an optional deep mode because it requires recursion guards, stack capture policy, third-party filtering, and substantial overhead.

Build on `IStorageSystem` telemetry in three levels:

#### Frame snapshots

Sample the existing storage/resource/window statistics once per completed frame and derive deltas:

- live, reserved, committed, retired, and peak bytes;
- allocation/growth counts and their change this frame;
- upload bytes, binding cache rates, invalid-handle count;
- arena capacity, requested bytes, alignment/padding waste, high-water, and unused capacity;
- resource live/retired/pending slot states;
- per-window transient and binding data.

#### Lifetime events

Persistent pools and GPU resources should emit allocate/create, retire, destroy/free events containing size, alignment, memory/resource kind, interned tag/name, source, owner (manager/definition/instance when known), and lifetime ID. Connect CPU creation, upload, submission, retirement, and destruction using flow edges.

For arenas, do not invent individual deallocations: an arena reset is the deallocation event. Report requested bytes and allocation count during the epoch, then freed bytes at reset.

#### Derived memory reports

Distinguish:

- **footprint:** currently live/committed/reserved bytes;
- **net growth:** allocated minus freed over an interval;
- **churn:** allocated plus freed bytes/events over an interval;
- **headroom/waste:** reserved or committed minus live/requested;
- **lifetime:** frame count/time between create and destroy;
- **peak:** maximum within selected range;
- **shutdown survivors:** live allocations when their owner should have shut down.

Call shutdown survivors “leak candidates”, not confirmed leaks. App-lifetime managers and driver-owned allocations are legitimate until their defined destruction point.

### 2.1.8 Retention, percentiles, and captures

Use two histories:

1. A configurable frame-summary ring retaining compact samples for thousands of frames.
2. A byte-budgeted detailed event ring retaining only as much trace as configured.

The detailed recorder should support circular flight-recorder mode: continuously retain recent history, freeze the preceding window when a slow-frame/memory-growth trigger fires, and continue for a configured number of frames afterward. Perfetto and PIX both use circular buffers for hard-to-reproduce events; Perfetto specifically describes stopping a ring-buffer trace after a missed-frame trigger so the cause in the recent past survives ([Perfetto trace configuration](https://perfetto.dev/docs/concepts/config)), while PIX exposes sequential and circular timing captures and makes costly sources opt-in ([PIX Timing Captures](https://learn.microsoft.com/en-us/windows/win32/direct3dtools/pix/articles/timing-captures/pix-timing-captures)).

Triggers should include:

- frame/CPU/GPU/UI budget exceeded;
- percentile regression;
- storage budget exceeded or unexpected growth;
- invalid handle, element ID collision, dropped telemetry, or application marker;
- manual capture/pin.

For selected ranges show count, total, mean, median, P90, P95, P99, custom PX, maximum, standard deviation or median absolute deviation, budget misses, and rate. Always show the sample count and selected time/frame range. A P99 based on 20 samples must be visibly weak evidence.

For the bounded recent ring, exact quantiles over compact numeric samples are simple and correct. For long sessions use a mergeable fixed-footprint histogram. HDR Histogram is designed around bounded footprint, constant recording cost, and arbitrary percentile queries ([HDR Histogram](https://github.com/HdrHistogram/HdrHistogram)). A t-digest is an alternative for compact mergeable quantiles, but FlowUi does not need two sketches initially.

Retain the exact worst frames and allow one-click navigation from a statistic to the responsible frames. A percentile without exemplars is a report, not a diagnosis.

### 2.1.9 Semantic findings: turn data into answers

Add a `DevReportEngine` that computes findings from trace/counter data. A finding contains:

```cpp
struct DevFinding {
    FindingKind kind;
    Severity severity;
    float confidence;
    FrameRange evidenceRange;
    EntityRef primaryEntity;
    std::vector<EvidenceRef> evidence;
    std::string_view summary;
};
```

The engine should operate on a hierarchy and report percentages of the relevant parent:

```text
Frame wall time
├── CPU critical path
│   ├── blocked/waiting
│   └── active work
│       ├── UI build
│       │   └── element definitions / instances
│       ├── Clay layout
│       └── command recording
├── GPU queue work (overlapping, not additive)
│   ├── viewport passes
│   └── UI pass
└── presentation pacing
```

If UI build is 5% of a frame, the UI panel may still identify its internal top element, but the top-level finding should not imply that optimizing it will fix frame pacing. Conversely, an element taking only 10 μs per call may dominate UI build through count. Reports should explicitly say both:

> `Row` contributes 61% of UI-build own time across 84,200 calls (10.4 μs P95). `Chart` has the slowest individual call (1.1 ms P95) but contributes 7% of UI-build time.

Useful rule families include:

- CPU/GPU/presentation classification with confidence;
- excessive fence wait or frame-resource backpressure;
- UI build/layout/recording dominance;
- high-count vs high-cost element offenders;
- deep or unstable layout, clipping/overflow explosions;
- render-run fragmentation and descriptor/buffer churn;
- text measurement/cache problems;
- allocation churn, unbounded growth, arena waste, or delayed GPU retirement;
- input/action latency, disabled/unbound actions, discarded results;
- telemetry quality problems and profiler self-overhead.

Findings should cite measurements and link to the exact frames, spans, instances, resources, or source locations. Thresholds and frame budgets are user-configurable. Recommendations must be phrased as evidence-based candidates, not universal truths.

### 2.1.10 Measurement overhead and trust

Development mode permits cost, but an unquantified observer can still send a programmer in the wrong direction.

- Put all tool-owned work on a `DevTool` category/track.
- Show an **observed frame** (the application as currently running) and an **application-only estimate** that excludes explicitly delimited developer tool work where subtraction is valid.
- Prefer the separate tool window so opening the panel does not resize the inspected window.
- Measure recorder CPU time, records emitted, bytes written, dropped records, GPU query use, and UI tool cost.
- Mark estimates and unavailable metrics; do not silently fill with zero.
- Allow capture without rendering the tool every frame.

The separate view cannot remove all perturbation: shared device work and scheduling still interact. The UI must call this an estimate, and serious regressions should be confirmed in a release-like build or an external profiler.

### 2.1.11 Trace storage and external interoperability

Instrumentation and statistical sampling solve different problems. FlowUi's first-party recorder should prioritize instrumented semantic zones because it can name definitions, instances, resources, and lifecycle phases precisely. A future optional CPU sampler can attach call stacks to slow ranges without placing a scope in every user function, but it is platform-specific and statistical; it should appear as another data source, not replace semantic spans. Allocation call stacks should likewise be a deep-capture option with deduplication and sampling. For whole-process instruction, lock, context-switch, cache, and driver analysis, launch/export integration with VTune, Tracy, Perfetto, or platform tools is a better investment than rebuilding an operating-system profiler inside FlowUi.

Use a versioned chunked `.flowtrace` format containing session/build/platform metadata, intern tables, frame indexes, records, and optional summaries. Chunks make partial recovery and streaming possible. Source paths and debug names may be sensitive; export should offer redaction and should not copy source contents by default.

Provide JSON/CSV summary export for CI and an optional Perfetto conversion path. Perfetto separates low-overhead recording from powerful trace processing/queries ([PerfettoSQL](https://perfetto.dev/docs/analysis/perfetto-sql-getting-started)); FlowUi can borrow that separation and let advanced users open a converted trace without making Perfetto a runtime dependency. Tracy remains a useful external cross-check because it already spans CPU/GPU frames and allocation tracking ([Tracy](https://github.com/wolfpld/tracy)). VTune's separate top-down total and self-cost/bottom-up hotspot views reinforce the need for both contribution and per-scope analysis ([VTune Hotspots](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/basic-hotspots-analysis.html)).

FlowUi's built-in reports remain valuable because external tools do not understand Flow definition/instance identity, Clay layout decisions, render-run causes, or override provenance unless FlowUi emits that semantics.

---

## 2.2 Tooling: inspect, alter, and persist

### 2.2.1 One inspection snapshot, not several unrelated captures

Replace the flat placeholder with a retained `DevInspectionFrame` assembled after layout and render-command generation. Keep the efficient pre-order representation, but add parent/child ranges and cross-stage correlations:

- definition, instance, source, authored keys, debug label;
- declaration/authored parameters and final effective parameters;
- state/resource handles and safe reflected snapshots;
- parent, children, depth, and owning window;
- computed bounding box and content/padding/border regions;
- layout sizing rules, resolved sizes, gaps/alignment/direction;
- clip/scissor/scroll state, floating attachment/z-order, visibility/culling;
- interaction state, focus, pointer capture, and hit-test order;
- emitted Clay/render-command range and render-run attribution;
- warnings, timings, allocations, actions, and changes associated with the node.

Capture the tree during build as today, then resolve computed geometry after Clay layout. This avoids trying to inspect a future bounding box during element creation.

Pinning a frame freezes its inspection snapshot and trace references. Otherwise transient elements or a moving popup disappear before the programmer can examine them.

### 2.2.2 Picking and overlays

Chrome DevTools succeeds because viewport picking, the tree, computed values, event listeners, and source are correlated rather than separate tools ([Chrome Elements panel](https://developer.chrome.com/docs/devtools/elements)). FlowUi should support:

- inspect-mode hover and click in the application window;
- tree hover highlighting and tree selection;
- locked selection while the application continues;
- breadcrumbs through parent definitions and constructed Clay nodes;
- cycling overlapping candidates in actual hit-test/z/clip order;
- rulers, guides, coordinates, and distance between selected elements;
- optional freeze of input, animations/time, or a selected popup while inspecting.

The overlay should be a dedicated development render lane composited after application UI. It should not insert Clay nodes into the application tree: doing so changes layout, hit testing, command counts, element identity, and the performance being diagnosed.

Overlay modes:

- border, padding, content, and clipped regions in distinct colors;
- outer spacing contributed by the parent's gap/alignment, labelled as such rather than inventing a CSS-like margin that Clay does not own;
- all child bounds or direct child bounds;
- layout direction, gaps, alignment guides, min/max and resolved sizing;
- text baselines, line boxes, glyph bounds, caret/selection;
- scissor/clip and scroll viewport/content extents;
- floating anchor/attachment points and popup placement adjustment;
- pointer hit target, hover/press/capture/focus;
- render-run boundaries or overdraw proxies where meaningful.

The tooltip should show identity, definition, size/position, source, key active constraints, and warnings. The inspector should explain resolved layout in plain language—“240 px because `FIXED(240)`” or “grew to available width, then clipped by parent”—rather than expose only a rectangle.

### 2.2.3 Correct and explicit override semantics

First fix invocation order. Effective parameter values must be resolved before any hook that receives `params`:

```text
authored parameters
  -> baked definition/instance patch
  -> live definition override
  -> live instance override
  -> temporary preview override
  -> validate
  -> interaction hooks / runLogic / build or construct
  -> capture effective value and provenance
```

The property inspector shows **Authored**, **Effective**, and **Source** for every field, with reset controls for each layer. A programmer must be able to tell whether a value came from code, theme, baked patch, definition override, exact instance override, or temporary preview.

Definition and instance scopes should remain distinct:

- definition scope changes every invocation of one element definition;
- instance scope changes one stable `FlowElementID`/`GlobalFlowID` in one window or an explicitly global identity;
- preview scope affects only the isolated preview scenario;
- multi-selection can create a reviewed batch of exact overrides, not an implicit selector language.

Exact instance persistence requires stable explicit/keyed identity. When the selected instance has an automatic or order-derived identity, the tool should allow a temporary override but warn that it cannot be safely baked until the identity is made stable. Multiple instances from one source line are not a runtime problem; they are a source-persistence and identity problem.

Do not initially build a CSS-like arbitrary selector system. Definition and exact stable-instance scopes cover the important cases and are predictable. Key-family or source-location selectors may be added later with clearly displayed match counts.

### 2.2.4 Parameters, state, and resources are not equally editable

The current UI treats these categories similarly, but their runtime semantics differ:

- **Parameters** are values for the current invocation. A validated copy can be safely patched before callbacks.
- **State** is persistent mutable behavior. Editing it is a command against an existing record, may violate invariants, and may race with callbacks.
- **Resources** are app-wide definition data with construction/destruction contracts. Mutating raw bytes can invalidate handles or invariants shared by every instance.

Recommended policy:

| Data | Default tool capability | Required mechanism |
|---|---|---|
| Parameters | View and edit | copy, validate, apply before hooks |
| State | View; edit only opted-in fields | frame-boundary transaction or registered mutation command |
| Resources | View metadata; rebuild/mutate only through opt-in adapter | definition resource editor/rebuild callback |

Remove non-functional generic state/resource override controls until these paths exist. Never raw-write arbitrary reflected bytes merely because a member pointer is known.

### 2.2.5 Reflection becomes an editor schema

Keep compile-time macros/concepts, but evolve descriptors from “field name plus byte operations” into a versioned schema:

```cpp
struct DevFieldSchema {
    StableFieldId id;
    DevValueKind kind;
    DevFieldFlags flags;       // editable, bakeable, sensitive, advanced...
    DevEditorHint editor;      // number, color, spacing, resource, action...
    DevConstraints constraints; // min/max/step/unit/enum labels
    CaptureFn capture;
    ValidateFn validate;
    ApplyFn apply;
    SerializeFn serialize;
};
```

Support signed/unsigned widths, floating values, enums with any ordinary underlying width, strings, colors, IDs/keys, optionals, nested reflected structs, and explicitly registered small containers. Pointer-like values, borrowed spans, callbacks, and opaque handles are view-only unless a semantic editor adapter is registered.

Additional metadata should include:

- display name, category, description, unit, bounds, step, precision;
- hidden/advanced/read-only/sensitive/runtime-only/bakeable flags;
- validator and error message;
- reset/default provider;
- theme token, resource kind, or action-slot semantics;
- schema version and migration aliases for renamed fields.

Action values must serialize stable action IDs/recipes and arguments, never closure bytes. Resource values must serialize stable resource keys or construction descriptors, never addresses.

Static registration should be audited for linker dead-stripping and initialization order. Prefer descriptors referenced by the element registration path or an explicit generated registry over relying solely on translation-unit side effects.

### 2.2.6 Changes are transactions

All edits feed a `DevChangeSet`:

- atomic apply at a safe frame boundary;
- validation before commit;
- undo/redo and grouped edits;
- clear distinction among temporary, saved, baked, and source-promoted;
- diff by window/definition/instance/theme/action;
- reset one field, one scope, or all changes;
- atomic file save, schema/build identity, and recovery from an interrupted save.

This prevents a slider drag from producing hundreds of permanent entries and lets a programmer review exactly what will enter the build.

### 2.2.7 Persistence: three options and one recommended default

#### Option A: improve the current textual updater

The dev tool could directly launch the current updater after export, removing the manual command.

**Advantages:** small change; edits familiar source initializers.

**Problems:** line/column data becomes stale; formatting and comments complicate matching; macros, templates, generated sources, helper functions, computed values, conditional construction, aggregate temporaries, and multiple instances at one call site remain ambiguous. A text patch cannot represent exact per-instance changes when the authored expression is shared.

This should be a compatibility path, not the new foundation.

#### Option B: AST-assisted source rewriting

A separate Clang LibTooling/Transformer executable can consume `compile_commands.json`, find the typed call expression, and emit a reviewed patch. Clang Transformer is explicitly designed for typed C++ source-to-source rules ([Clang Transformer](https://clang.llvm.org/docs/ClangTransformerTutorial.html)).

**Advantages:** much safer than line-based text matching; understands types and syntax; excellent for “promote this definition default to source.”

**Problems:** brings a Clang toolchain dependency; macros and generated code are still difficult; a tool works per translation unit; it cannot invent a clean expression for every runtime-derived value; exact loop instances still have no unique source expression.

Keep this optional and out-of-process. Do not link Clang into the application or core FlowUi runtime.

#### Option C: generated typed override artifact — recommended

The tool saves a versioned manifest and FlowUi's build integration automatically generates typed C++:

```text
.flowui/changes/active.flowchanges
    -> flowui-dev-generate
    -> build/.flowui/generated/FlowUiBakedChanges.cpp
    -> linked into the application
```

CMake directly supports custom commands whose outputs become compiled sources, rerunning the generator when its inputs change ([CMake generated files](https://cmake.org/cmake/help/latest/guide/tutorial/Custom%20Commands%20and%20Generated%20Files.html)). The developer never manually compiles or calls an updater.

Generated output should contain typed tables/functions per definition, stable exact-instance cases, theme initializers, and supported action bindings. Production does not parse JSON and does not contain `DevValue` or reflection.

```cpp
// Illustrative generated code.
void applyBaked(NumberInput<float>::Parameters& p, ElementInstanceKey key) noexcept {
    p.step = 0.25f; // definition-level change
    if (key == ElementInstanceKey::fromStable("sidebar.opacity")) {
        p.max = 1.0f;
    }
}
```

**Advantages:** deterministic, reviewable artifact; represents instance-specific changes; no runtime parser; naturally integrated with the next build; can be omitted wholesale.

**Problems:** generated files must participate in build/reload; stable identity is mandatory; an exact-instance dispatch has some irreducible comparison/branch cost; generated code can drift when schemas change and therefore needs diagnostics/migration.

This is the default persistence model. “Promote to source” remains an optional operation for values that belong in authored C++.

### 2.2.8 Performance promises for baked changes

The tool should promise what can be measured:

- no dynamic allocation, JSON, reflection, strings, or hash-map lookup in production;
- typed generated values and calls;
- definition-level patches eligible for inlining/LTO;
- exact-instance patches implemented as compact generated dispatch;
- a benchmark and assembly/trace regression gate for the generated layer.

It cannot guarantee universal bit-for-bit or cycle-for-cycle equality with arbitrary hand-written C++. An instance-specific value inherently needs either authored wiring or an identity decision somewhere. For strict zero-dispatch equivalence, promote the change into the call site's parameters/theme or generate a specialized definition selected directly by authored code.

This honest boundary preserves trust while still making baked output comparable in normal use.

### 2.2.9 Live reload boundaries

Supported reflected values can update immediately in memory and can be reloaded from a watched manifest. Themes can be staged at frame boundaries using their existing mutation mechanism.

Arbitrary new C++ behavior, resource types, or captured lambdas require compilation. Dynamic-library code reload is a separate high-risk project involving ABI, state migration, and lifetime safety; it should not be smuggled into this upgrade. The tool can invoke the configured build command and reconnect/relaunch while preserving the workspace, selection, and capture, but should call that **build and reload**, not universal hot reload.

### 2.2.10 Input, state, and scenario controls

To diagnose transient behavior, add development-only controls at existing semantic boundaries:

- pause/resume UI time and step a frame;
- record/replay `FrameInput`, window size/DPI, and relevant time values;
- emulate hover/press/focus only through the input/focus managers, not by mutating element state bytes;
- inspect pointer-capture and popup dismissal/consumption decisions;
- set viewport/window presets;
- invoke registered non-destructive actions with explicit confirmation policy;
- register deterministic scenario setup/reset functions.

Input recording should state what it does not capture. External application state, threads, networking, and nondeterministic resource callbacks require application-provided scenario hooks or will prevent deterministic replay.

### 2.2.11 Element library viewer

Registration metadata alone is insufficient to instantiate arbitrary elements. Parameters may contain spans, pointers, action calls, resource keys, or mandatory application context. Add an optional development preview descriptor:

```cpp
struct DevElementPreview {
    std::string_view name;
    PreviewBuildFn build;
    PreviewResetFn reset;
    std::span<const PreviewScenario> scenarios;
    PreviewRequirements requirements;
};
```

FSEL should ship previews for every standard element. Application elements opt in. Scenarios provide valid sample parameters/resources and named states such as default, hovered, focused, disabled, empty, overflow, and error. The viewer renders them in an isolated ID/state namespace with mock-safe actions, selectable theme/DPI/window size, and no accidental access to production state.

Useful matrices include theme variants, DPI scales, sizing constraints, interaction states, and localization/text lengths. The preview descriptor is also a natural entry point for screenshot/golden tests later, though tests are not the primary design here.

### 2.2.12 Theme tooling

`ThemeManager` already owns typed named variants and frame-boundary mutations. Add development enumeration/snapshot APIs and register reflected theme schemas. The tool can then:

- list theme types and variants;
- clone/rename a variant and select it per preview/window;
- edit tokens with validation and provenance;
- compare variants and find unused/high-impact tokens;
- preview a theme across the element library;
- save to the change manifest and bake a typed theme initializer.

Theme editing should prefer tokens over thousands of instance overrides. The change UI can suggest “promote to theme” when many selected fields share the same authored theme source.

### 2.2.13 Action tooling and visual wiring

FlowUi already has stable `AppActionID`, development names/source, invocation counts/status, and stateless UI action recipe IDs. Build on those rather than introducing a parallel callback registry.

Visual binding requires both sides to declare semantics:

- an element field is registered as an **action slot** with accepted action kind/signature and event meaning;
- an app action or UI recipe has a stable ID/name, availability, argument/result schema, source, and safety metadata;
- resources/arguments are supplied by a registered binding adapter or explicit constants, not discovered by serializing captured references.

Dragging an action onto a compatible slot creates a typed change recipe. Incompatible actions are disabled with an explanation. The tool can invoke actions marked safe for preview; destructive or externally visible actions require confirmation and should be disabled by default in isolated previews.

Generated bindings can become direct typed calls/thunks after their initial element/action identity dispatch. Ephemeral lambdas with captured local state cannot be safely persisted; the tool should ask the programmer to promote them to a named recipe or app action.

### 2.2.14 Build-time development support

Compilation integration should do more than generate baked values. In developer builds it should also:

- emit a stable build/schema fingerprint used to reject stale manifests and traces;
- validate duplicate definition, field, preview, theme, and action IDs as early as possible;
- produce or verify an explicit registry catalog so dead-stripped translation units do not silently disappear from tooling;
- expose the compile database path to the optional AST/source-reveal helper;
- generate a human-readable build report listing enabled trace categories, registered schemas/previews, baked changes, unresolved entries, and what will be excluded from production;
- make generation dependencies explicit so changing a manifest, schema, or generator reliably rebuilds the typed artifact.

Compile-time errors remain C++ compiler errors. The tool may parse and link configured build output to a change or source location, but should not create a second incompatible diagnostic language.

---

## 2.3 Interface: where the DX becomes real

### 2.3.1 Default host and layout

Use a separate companion window by default. FlowUi already supports multiple windows, and this avoids shrinking the inspected UI, consuming its clicks, or putting the profiler timeline inside the frame it is trying to diagnose. The application window receives only a transparent dev overlay and pick-input interception while inspect mode is active.

Keep an in-app docked mode for single-window backends and quick use. Both hosts consume the same models.

Recommended workspace:

```text
┌ Toolbar: Inspect | Pause | Record | Mode | Window | Budget | Search | Save/Bake ┐
├ Navigator ───────┬ Main view: live overview / timeline / library canvas ┬ Inspector ┤
│ Elements         │                                                       │ Authored  │
│ Definitions      │                                                       │ Computed  │
│ Resources        │                                                       │ Changes   │
│ Themes           │                                                       │ Actions   │
│ Actions          │                                                       │ Warnings  │
├──────────────────┴───────────────────────────────────────────────────────┴───────────┤
│ Drawer: Findings | Diagnostics | Memory | Changes | Trace details/logs              │
└───────────────────────────────────────────────────────────────────────────────────────┘
```

This is a workspace, not a single permanent layout. Panels should dock, hide, persist size, and have keyboard navigation. A command palette makes rarely used features discoverable without filling the screen with buttons.

The tool itself should be rebuilt using the current element system and FSEL. That dogfoods the public path and removes the parallel legacy widget stack. Mark its definitions as developer-internal, but keep its own performance lane available when diagnosing the tool.

### 2.3.2 Home and health overview

On opening, show a one-screen answer rather than raw tables:

- frame pacing chart and selected frame budget;
- CPU/GPU/presentation classification plus confidence/evidence;
- average/median/P90/P95/P99/custom/max for the selected range;
- UI build/layout/record/GPU and wait contributions;
- memory footprint, churn, peak, and budget status;
- top total-impact element and slowest-per-call element;
- run/instance ratio, text/cache health, and resource warnings;
- current capture mode, recorder overhead, and dropped events;
- prioritized findings.

Every card is a navigation link. Clicking “UI build 42% of CPU active time” opens the relevant frame range and element contribution view.

### 2.3.3 Profiler surface

The profiler combines:

- frame-time strip with budget lines, hitch markers, and pinned frames;
- zoomable/pannable CPU thread, GPU queue, counter, allocation, and flow tracks;
- selected-range statistics with editable percentile;
- top-down hierarchy with total/inclusive and self/exclusive duration;
- bottom-up/hotspot aggregation;
- dual element rankings for total impact and per-call headroom;
- comparison against another frame/range/session/baseline;
- correlated source, instance, render runs, resources, and findings.

Selecting a span highlights its element in the application and hierarchy when still present. Selecting an element filters its build spans, actions, allocations, Clay commands, and runs. Selection is a shared concept across tools, not duplicated per panel.

Show overlapping work as overlapping tracks. Percentage bars should state their denominator (`of UI build`, `of CPU active`, `of frame wall interval`). This small wording rule prevents a large class of misleading reports.

### 2.3.4 Element inspector

The inspector has four primary tabs:

1. **Layout:** box model, resolved sizing, constraints, parent/children, clip/scroll/floating details.
2. **Properties:** authored and effective values, provenance, validation, and scoped editing.
3. **Behavior:** interaction state, action slots/bindings, focus/pointer capture, popup/input decisions.
4. **Performance:** own/subtree timing, allocations, render commands/runs, history, and linked findings.

Hovering the box-model regions highlights them in the application, following the proven viewport/tree relationship in Chrome's CSS tooling ([Chrome CSS reference](https://developer.chrome.com/docs/devtools/css/reference)). Editing a value previews immediately; the change header always shows scope (`temporary`, `this instance`, `definition`, `theme`) before commit.

Source reveal uses a configurable editor command/URI and validates the file/build identity. It should open the element invocation or registered definition/field source, not guess silently when metadata is stale.

### 2.3.5 Findings and diagnostics are different

Keep two feeds:

- **Findings** are derived performance/usage conclusions with evidence and confidence.
- **Diagnostics** are correctness facts: ID collisions, invalid resource handles, unregistered fields, override schema mismatch, dropped telemetry, failed resource construction, Clay errors, popup anchor loss, or action invocation failure.

Both support severity, search/filter, source/element navigation, suppression with reason, and occurrence history. Do not hide repeated diagnostics entirely; aggregate count and show first/latest frame.

### 2.3.6 Memory and resources

The memory workspace should offer:

- stacked timeline by memory/resource class;
- live/reserved/committed/retired and budget lines;
- allocation/free/churn chart;
- lifetime table by resource/owner/tag/source;
- arena high-water/waste/growth;
- GPU retirement lag and submission-serial flows;
- current resource inventory and failed/invalid states;
- compare two frames and show what appeared, disappeared, or grew.

Default to semantic FlowUi resources. A future global allocator view is clearly marked as a different, high-overhead capture.

### 2.3.7 Elements, themes, and actions as authoring tools

The **Library** view pages/searches registered previews and supports scenario, theme, DPI, and size matrices. Missing preview requirements are explained, not crashed through.

The **Themes** view edits registered typed variants and shows a live preview matrix. Changes can be staged, reverted, or baked.

The **Actions** view lists app actions and UI recipes, binding/availability/source/invocation data, and compatible element slots. Drag/drop is a convenience over an explicit typed binding transaction. A generated-code preview should always be available so visual wiring never becomes mysterious configuration.

These tools reduce C++ ceremony but do not conceal the underlying model. FlowUi programmers are still expected to understand Clay sizing/layout and Flow element identity; the tool uses the same names and concepts as the public APIs.

### 2.3.8 Change review and build integration

The Changes drawer groups edits into:

- parameters by definition/instance;
- themes;
- action bindings;
- preview/scenario settings that are not application changes.

Each entry shows old/new value, scope, identity stability, source, validation, and production cost class. Operations:

- apply/revert/undo/redo;
- save live workspace;
- bake typed artifact;
- optionally promote supported changes to source through a reviewed AST patch;
- copy an equivalent C++ snippet;
- run configured build/reload;
- compare behavior/performance before and after.

Saving source-like changes should require explicit workspace permission, mirroring the sensible boundary used by browser developer workspaces ([Chrome DevTools Workspaces](https://developer.chrome.com/docs/devtools/workspaces)). Generated build-directory output needs no source mutation; committing a manifest or promoted patch is the programmer's choice.

### 2.3.9 Offline and automated use

The same UI should open `.flowtrace` files with intervention controls disabled. Offline mode is essential for startup, crashes, CI artifacts, and reports from another machine. A trace records build/schema IDs so the viewer can warn when local symbols/registries differ.

Add a headless reporter after the core metrics stabilize:

```text
flowui-dev-report capture.flowtrace \
    --budget frame.p95=16.7ms \
    --budget ui-build.p99=2ms \
    --budget storage.live=64MiB
```

CI should run deterministic application-owned scenarios, export machine-readable results, compare compatible hardware/configuration baselines, and retain the worst trace. Do not compare GPU timings across unrelated devices as though they were interchangeable.

### 2.3.10 Accessibility and robustness of the tool itself

The development tool is still a UI and should support keyboard-only navigation, focus visibility, scalable text/DPI, color-blind-safe palettes, textual alternatives to charts, and searchable tables. When the inspected application fails to build a frame, the companion window should remain usable where backend state permits.

Trace processing and report generation should be incremental or background work. A large capture must not freeze the inspected application simply because a table is sorting.

---

## 3. Programmer journey with the upgraded system

### 3.1 Define the application

The programmer enables `FLOW_UI_DEV_MODE`, defines normal Flow elements, registers editable schemas where desired, and uses stable IDs for instances whose changes may be persisted. FSEL definitions arrive with schemas and preview scenarios. Application elements work without metadata, but appear read-only/limited until registered.

Actions remain normal `AppActionID`/`UiAction` recipes. A programmer adds action-slot metadata only if visual binding is desired. Themes remain typed theme structs and variants.

### 3.2 Compile and run

`makeApplication` starts a bootstrap trace before expensive initialization. Once the first window exists, the companion tool window opens according to `DevToolsConfig`. The application code does not launch a separate updater or profiler server.

The tool immediately shows startup time, first-frame time, current health, memory, diagnostics, and a rolling frame history. Summary recording is already active, so a hitch immediately before opening the profiler is still present.

### 3.3 Inspect and shape

The programmer presses the inspect shortcut and points at the UI. The overlay shows the hit region and box model; clicking locks the element. The tree, computed layout explanation, properties, actions, performance, and source all follow the same selection.

They change padding for one stable instance, then switch the change scope to the definition and see every match highlighted before committing. A theme-backed color is edited at its theme token instead of creating 200 overrides. Undo and compare are immediate.

### 3.4 Diagnose a slow frame

The overview reports that the application is likely CPU-bound with high confidence and that UI build is 63% of CPU active time. The programmer selects the P99 frame. Total-impact ranking shows a cheap `Row` definition called 80,000 times; per-call ranking independently shows a costly chart. Clicking `Row` highlights its source instances and shows their call count. Another finding reveals one render run per instance due to alternating textures.

If GPU time is the issue, actual delayed timestamp results appear on the GPU track. If timestamps or calibration are unavailable, the tool says so instead of deriving a false GPU duration from submit/present calls.

### 3.5 Persist deliberately

The Changes drawer reports which edits have stable identity and can be baked. The programmer clicks **Bake**. The manifest is saved atomically; the next normal CMake build runs the generator and compiles typed output automatically. No external manual updater step exists.

For a definition default that clearly belongs in authored code, **Promote to source** opens an AST-generated patch or copies a C++ snippet. Ambiguous macro/loop/computed expressions remain in the generated layer or require manual authoring; the tool never guesses destructively.

### 3.6 Validate and ship

The programmer reruns a saved scenario and compares ranges/baseline. The production build disables developer mode, removing the tool, live registries, tracing, and reflection. Only the approved generated typed artifact or promoted source remains. A build report lists exactly what crossed the boundary.

---

## 4. Key trade-offs and decisions

| Question | Recommendation | Why | Cost/limit |
|---|---|---|---|
| One manager or `UiManager` extension? | App-scoped `DevSession`, per-window probes | Startup/shared storage/GPU are app-wide; layout/input are per-window | New coordination layer |
| Raw counters or trace? | Uniform spans/counters/flows/allocations plus derived reports | Supports timeline and future metrics without giant structs | Requires intern tables and retention design |
| Record everything continuously? | Summary always-on; byte-bounded detailed ring; triggered deep capture | Preserves recent cause and controls overhead | Deep details may not exist before enabled/triggered |
| GPU time from host calls? | Vulkan timestamp queries, delayed readback | Measures device execution | Hardware support, finite queries, calibration uncertainty |
| Definition or instance timing? | Definition aggregates always; instances in selected/deep capture | Controls cardinality while supporting drill-down | Not every historical instance has detail |
| Average or percentile? | Exact recent samples plus long-session histogram; custom PX and exemplars | Captures spikes and remains bounded | Quantiles need sample-count context |
| Edit state/resources like params? | No; semantic mutation/rebuild adapters only | Preserves invariants/lifetimes | More registration work for advanced editors |
| Persist by source patch? | Generated typed artifact by default; AST promotion optional | Handles exact instances and avoids brittle textual rewrite | Stable IDs and generated dispatch required |
| Promise hand-written performance? | Guarantee production exclusions and typed/no-allocation path; benchmark it | Honest and enforceable | Exact instance selection cannot always be free |
| Docked or separate tools? | Separate companion by default; docked fallback | Avoids resizing and click interference | Still shares process/device resources |
| Build the Interface from legacy dev elements? | Migrate to current element system and FSEL | Dogfoods supported APIs and removes duplication | Migration work precedes feature expansion |
| Replace external profilers? | No; export/interoperate while owning Flow semantics | External tools provide system depth | Built-in profiler stays FlowUi-local by design |

---

## 5. Recommended internal boundaries

Avoid a monolithic “DevManager.” The following responsibilities can initially share implementation files, but their interfaces should remain separate:

```text
DevSession (App-owned coordinator)
├── DevTraceRecorder       hot event ingestion, categories, buffers, export
├── DevFrameCorrelator     CPU/GPU/window/frame alignment
├── DevInspectionModel     retained element/layout/render snapshots, selection
├── DevOverrideStore       live effective-value layers and validation
├── DevChangeSet           transactions, undo/redo, save/bake state
├── DevReportEngine        statistics, hierarchy, findings, comparisons
├── DevOverlayRenderer     post-application highlights and guides
└── DevInterfaceHost       companion/docked/offline UI consumers
```

Managers publish through narrow adapters rather than depending on tool widgets. For example, storage emits storage events and snapshots; it does not know how a memory chart is drawn. The renderer publishes query results and element/run correlations; it does not own the profiler timeline.

The development schema/registry is shared by inspection and generation, but generation consumes a serialized versioned schema/manifest rather than reaching into a running process during the build.

---

## 6. Implementation plan

### Phase 0 — make the current development contract truthful

1. Move parameter override resolution before interaction hooks and build/construct.
2. Hide or disable generic state/resource editing until semantic application paths exist; add real snapshot capture where safe.
3. Document and enforce override precedence and instance identity stability.
4. Introduce app-scoped `DevSession` ownership and per-window registration without changing the visible tool yet.
5. Migrate the current debug panel to the current element system/FSEL or place it behind the new consumer interface so legacy widgets stop defining backend architecture.

This phase prevents new profiler/editor work from encoding existing inconsistencies.

### Phase 1 — telemetry foundation and useful frame history

1. Implement interned event metadata, thread-local producers, category masks, bounded buffers, and recorder-overhead counters.
2. Convert current `PerformanceDiagnostics` instrumentation into frame spans/counters while preserving a compatibility summary.
3. Add startup/makeApplication, manager, storage snapshot/delta, action, renderer-count, and element-definition aggregate instrumentation.
4. Expose compact frame history, exact range statistics, custom percentile, worst-frame navigation, capture modes, and triggers.
5. Define and write the versioned `.flowtrace` format.

At the end of this phase the existing tool can already offer a meaningful health page and frame chart.

### Phase 2 — inspection model, picker, and overlay

1. Correlate captured Flow/Clay nodes with post-layout bounds, clipping, interaction, render commands, and runs.
2. Add retained/pinned inspection frames and shared selection.
3. Add application-window reverse hit testing and dedicated overlay rendering.
4. Implement box/layout/text/clip/floating/input overlay modes and source reveal.
5. Build the new Layout/Properties/Behavior/Performance inspector.

This delivers the Chrome-DevTools-like UI diagnosis loop before attempting visual authoring breadth.

### Phase 3 — complete profiler and reporting

1. Add Vulkan timestamp query pools, delayed readback, supported calibration, and GPU tracks.
2. Add detailed element instance spans, exclusive/inclusive accounting, render-run reasons, text/cache metrics, allocation/resource lifetime events, and flow correlations.
3. Implement overview, timeline, top-down/bottom-up views, dual element rankings, memory workspace, range comparison, and findings engine.
4. Separate observed/application/dev-tool cost and surface telemetry quality.
5. Add optional Perfetto conversion to validate traces against a mature viewer.

### Phase 4 — safe editing and automatic persistence

1. Introduce schema v2 constraints/adapters, authored/effective/provenance values, stable IDs, and field migration diagnostics.
2. Implement frame-boundary change transactions, validation, undo/redo, and atomic manifests.
3. Add semantic state mutation and resource rebuild adapters.
4. Build the typed change generator and CMake custom-command integration.
5. Add reviewed AST-assisted source promotion as an optional tool, not a runtime dependency.
6. Establish production binary/performance gates proving that unapproved dev machinery is absent.

### Phase 5 — authoring workspaces and automation

1. Add element preview descriptors, FSEL previews, isolated library viewer, and scenario matrices.
2. Add theme registry introspection, editing, comparison, preview, and typed baking.
3. Add action-slot schemas, compatible visual binding, invocation diagnostics, and generated typed bindings.
4. Add input/scenario record/replay, offline viewer, headless reports, and baseline comparison.
5. Consider a local remote-consumer protocol only after in-process/offline models are stable.

### Acceptance principles across every phase

- A metric must state what clock, interval, owner, and denominator it represents.
- A derived finding must link to evidence and expose uncertainty.
- Every hot-path feature must have a disabled/category-off cost and a measured enabled cost.
- An editor must show scope and provenance before applying a change.
- A persisted instance change must use stable identity or refuse to claim durability.
- Production builds must contain only explicitly approved typed output.
- The companion tool must remain useful when a frame is paused, failed, or opened offline.

---

## Final recommendation

The upgrade should begin as an observability and correctness project, not as a visual editor project. A trustworthy trace, stable correlated inspection snapshot, and explicit override semantics are the load-bearing pieces. Once those exist, the profiler, hierarchy, overlay, property editor, element library, theme designer, and action binder become different views and consumers of one coherent model.

The most consequential DX choices are:

1. retain recent information before the programmer knows they need it;
2. explain costs hierarchically and distinguish total contribution from per-call cost;
3. correlate every view through stable definition/instance/window/frame/resource identities;
4. make live changes reversible and scoped;
5. bake approved changes into typed build output automatically;
6. be explicit where measurement or persistence cannot be exact.

That gives FlowUi something more valuable than a collection of debug widgets: a development environment in which authored C++, runtime behavior, layout, rendering, resources, and performance remain visibly connected from the first application construction call to the production build.
