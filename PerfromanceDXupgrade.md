# Performance developer experience upgrade

## Purpose and target behavior

The Performance Workbench becomes an investigator for one sealed capture. Opening Performance before the first capture displays **“Capture a snapshot to investigate.”** The Inspector always exposes capture configuration. The contextual controls contain **Main Capture**, which closes the developer interface, arms or starts capture, and eventually returns to Performance with an immutable snapshot. Timeline navigation, minimap, filtering, tracks, clustering, and drilldown remain investigation tools. Follow, Freeze, and Refresh snapshot disappear.

This is a design and implementation report, not an implemented change. It is based on the current working tree, including existing uncommitted timeline changes. Those changes must be preserved during implementation.

The important architectural split is:

- **Collection and rolling statistics** continue for the application lifetime.
- **Detailed retention** writes into the existing timing report ring only during capture and its bounded finalization.
- **Investigation** borrows the sealed ring and holds derived indexes and presentation state, without owning duplicate timing records.
- **Developer-interface closure** suspends the interface session; it does not end monitoring or discard tab state.

## What the code does today

### Workbench and timeline

[DevPerformanceWorkbench.cpp](src/devSystems/devInterface/Performance/Workbench/DevPerformanceWorkbench.cpp#L11) pauses on interaction, applies commands, checks reporting mutation sequence, and refreshes at roughly 100 ms intervals while following. It obtains `capture_snapshot()`, moves copied reports and descriptors into `DevTimelineState`, then invokes `extract_timeline()`.

[DevTimelineData.hpp](include/devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp#L39) defines an owned `TimelineSnapshot`; `DevTimelineState` additionally owns `retained_reports` and descriptors. [extract_timeline](src/devSystems/devInterface/Performance/Workbench/DevTimelineData.cpp#L13) copies CPU/GPU values and source strings into `TimelineBlockSlice`, constructs synthetic tick groups, reconstructs hierarchy, and computes frame metrics. It also temporarily copies unaligned GPU records. Removing only `retained_reports` would therefore leave another substantial record projection and would not meet the direct-read goal.

[DevWorkbenchHeader.cpp](src/devSystems/devInterface/Performance/Workbench/DevWorkbenchHeader.cpp#L39) supplies Follow/Freeze, transport, Spike, zoom, domain selection, minimap, and Refresh snapshot. Previous/Next/Spike remain useful within a sealed capture. Space currently toggles pause; remove that binding rather than assigning an unrelated capture action to it.

[DevTimelineLayout.hpp](include/devSystems/devInterface/Performance/Workbench/DevTimelineLayout.hpp) describes derived display geometry. [DevTimelineViewport.hpp](include/devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp) owns window rendering resources and submission-aligned input. These are valid caches, not snapshot owners. Keep their clipping, upload retirement, picking, and submission guarantees. They must be rebuilt after window recreation and reject commands from an older capture generation.

[DevPerformanceInspector.cpp](src/devSystems/devInterface/Performance/Inspector/DevPerformanceInspector.cpp) currently shows sample details and track controls. Add capture configuration above sample details; it must work when no sample exists. [DevPerformanceSelector.cpp](src/devSystems/devInterface/Performance/Selector/DevPerformanceSelector.cpp) currently uses live window/thread metadata and reporting descriptors. Investigation should instead use capture metadata, including windows or threads that no longer exist.

There is a small naming difference from the proposal: the current contextual placeholder is `Frame: Latest ▾`, at [DevContentHeader.cpp](src/devSystems/devInterface/Permanents/Elements/DevContentHeader.cpp#L317), rather than a literal Frame Summary button. Replace that placeholder with Main Capture. The adjacent CPU reporting selector is functional and must be integrated with the capture settings rather than accidentally removed.

### Timing collection and reporting

[DevTimingReporting.hpp](include/devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp) owns the retained reports and exposes copying observer APIs. [DevTimingReporting.cpp](src/devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.cpp#L164) currently:

- Stores reports in `ring[app_tick % capacity]` and assumes a continuous app-tick interval.
- Publishes intervening ticks through `publishThrough()`, including empty ticks.
- Reuses nested vectors through `clearReport()`, preserving their capacities.
- Drains CPU records, GPU records, and element aggregates on every consumption.
- Appends rolling samples only after finding a retained report. This must change; otherwise disabling retention also disables rolling statistics.
- Copies descriptor and track tables on every consumption.
- Uses a shared mutex, so returning an ordinary unlocked span into mutable reports would be unsafe.

The existing rolling series already provides a bounded duration ring and running sum per zone/domain. Exact percentiles are calculated on query. Reuse this algorithm initially; do not replace exact rolling semantics with a lifetime average or approximate histogram without making that a separate choice.

[PublicStructs.hpp](include/FlowUi/PublicStructs.hpp#L525) defaults to 4096 retained app ticks, 2048 rolling samples per series, and a minimum retention capacity of 20 times maximum frames in flight. This capacity is **ticks**, not seconds, records, or bytes. The nested report vectors can grow independently, so bounded tick count does not imply a strict memory bound.

[DevTimingRecorder.cpp](src/devSystems/devMonitoringAndReporting/timing/DevTimingRecorder.cpp#L197) records identities, parentage, timestamps, and context into producer storage. Its drain transfers records to a vector. Keep producer queues: workers still need safe publication to the reporting owner. “One snapshot buffer” removes redundant retained history, not the producer queues or Vulkan query slots.

[DevGpuTiming.hpp](include/devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp) carries app-tick, window/frame, device, queue, submission, zone index, and calibration identities. GPU records arrive when submitted slots resolve, potentially several ticks later. [resolveCompleted](src/devSystems/devMonitoringAndReporting/timing/DevGpuTiming.cpp#L156) operates on completed slots; FlowUi calls it during fence processing and window graphics draining. Switching recording off is not proof that previously submitted measurements are resolved.

### Entire monitoring/reporting subsystem: consequences

[DevMonitoringAndReporting.cpp](src/devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.cpp) owns CPU timing, GPU timing, memory, errors, and their reporting objects. The capture controller belongs here or alongside these application-owned services, not in a drawable element.

| Area | Existing responsibility | Effect of this redesign |
|---|---|---|
| Timing zones, recorder, descriptors, quality | RAII/manual scopes, producer queues, context, detailed records, overhead/loss counters | Continue collection and draining; separate rolling updates from capture retention; add generation attribution and finalization support. |
| GPU timing | Query pools, submission identity, availability, calibration, completed record drain | Resolve already submitted capture work after admission stops; retain only eligible generation/tick results before sealing. |
| Memory recorder, external scope, probes, sources | Operations, storage/container accounting, manager and environment probes, source/tuning metadata | Continue current-state accounting. Do not gate these operations on Main Capture or erase the Memory tab session. |
| Memory reporting | Current/session peaks, segment/event rings, independent memory captures, capacity recommendations | Remains independently owned. Its history is a different data domain, not a redundant timing snapshot buffer. |
| Error monitoring, recorder, stack provider | Occurrences, evidence, breadcrumbs, deferred state snapshots, stacks, emergency/fatal reporting | Remains active while the developer interface is closed. Error state snapshots are not timing snapshot copies and must remain. |
| Error reporting and advice | Groups, timing/memory correlation, triggered captures, retained evidence, remediation advice | Timing correlation becomes capture-aware. Remove pinned copies of timing reports under the strict single timing-snapshot policy; preserve independently owned error and memory evidence. |

[DevMemoryReporting.cpp](src/devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.cpp#L282) drains operations, samples probes/checkpoints, maintains current values and peaks, and retains segments/events. Its `beginCapture()`/`endCapture()` describe memory capture intervals; do not repurpose them as Main Capture.

[DevErrorReporting.cpp](src/devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.cpp#L142) correlates errors by `appTickReport()`. Its triggered capture processing also copies reports returned by `appTickRange()` into `DevErrorTriggeredCapture::timingTicks`. This is a hidden second timing-history owner. Under the proposed policy, change those timing fields to capture-generation/range references plus explicit availability status. Outside Main Capture, timing context is `NotCaptured`; after replacement it is `Evicted`/unavailable. Diagnostics cannot promise automatic pre-error timing history while timing retention is disabled. Advice and tests must describe that limitation accurately. Keep error/memory triggered evidence independently; one active **Performance timing snapshot** does not mean one error occurrence or one memory capture.

### Application and interface lifetime

[DevInterface root](include/devSystems/devInterface/Permanents/Elements/DevInterface.hpp#L32) uses `windowLifetime()`. [DevInterface backend](src/devSystems/devInterface/Permanents/Backend/DevInterface.cpp#L55) toggles by destroying and recreating a window. [ElementStatePolicy.hpp](include/managers/structs/ElementStatePolicy.hpp) currently has only Transient and WindowLifetime; **there is no existing appLifetime policy**. [ElementStorageController::destroyWindow](src/managers/ElementStorageController.cpp#L421) releases the whole window partition. A global element ID alone therefore does not preserve state across destruction.

[FlowUi.cpp](src/FlowUi.cpp#L1341) consumes previous work, advances `appTick`, notes a boundary, and polls input. [App::drawFrame](src/FlowUi.cpp#L2672) currently synchronizes DevInterface **after drawing the main window**, then dispatches managed windows. That is too late to make a newly detected manual-open request exclude all current-tick main-window work unless admission is finalized retrospectively.

Window destruction drains graphics and releases renderer, timeline controller, element, UI, and swapchain resources. Window-close requests are also handled by managed dispatch, separately from the backend toggle. Capture orchestration must handle both paths and applications using explicit per-window frame APIs.

## Proposed user experience and settings

### Inspector configuration

Always show a Capture section with the following values. These defaults are proposals, not existing behavior.

| Setting | Values | Proposed default and semantics |
|---|---|---|
| Start | Immediately / Shortcut | Immediately; means first complete clean tick after developer-interface closure. |
| Start shortcut | Key and modifiers, Press edge | Ctrl+Shift+F8, validated against the configured panel-toggle chord; configurable before arming. |
| End | Manual reopen / Frame budget event / Duration | Duration, 5.0 seconds. Exactly one automatic end mode is active. Manual reopen always cancels or stops. |
| Budget | Positive milliseconds; presets | 16.6 ms, 33.3 ms; 16.6 ms default. Strict `duration > budget`. |
| Budget source | Application tick cadence / Specific application window CPU frame | Application tick cadence by default. Explicit labels prevent confusing CPU frame work with delivery cadence or GPU duration. |
| After event | Nonnegative seconds | 0.5 seconds; zero includes the triggering completed interval and then stops. |
| Duration | Positive finite seconds | 5.0 seconds measured from actual recording start, not from arming. |
| CPU detail | Available compiled levels | Preserve current runtime level; warn in the configuration when detailed zones cannot be recorded. |
| Capacity information | Read-only effective tick capacity | Show tick capacity and wrapping policy. Capacity changes are separate from routine capture. |

Use existing FSEL combo/radio/numeric-input elements. Store numeric UI values in suitable binding types, then validate and convert to nanoseconds with checked bounds. Reject NaN, infinity, invalid chords, overflow, zero/negative duration, and negative tail before closing. Do not assume the fixed shortcut default is available on every host; registration/conflict failure is a visible configuration error.

Configuration is edited in persistent interface state. Pressing Main Capture commits a validated **immutable settings copy** to the controller. Later UI edits cannot change an armed/running capture. View filters never change what is recorded or which budget event is tested.

### Lifecycle as seen by the user

1. First opening: empty-state message, configuration, and Main Capture; no timeline controller surfaces.
2. Main Capture: queue a request. After input/action processing, close the interface at a safe point. Closure work is excluded from capture.
3. Immediate mode: start on the next clean tick. Shortcut mode: remain armed with no ring writes until a fresh shortcut press; a held key does not trigger.
4. Manual opening while armed: disarm, reopen, retain the previous sealed snapshot if one existed. Arming alone does not invalidate it.
5. Actual start: invalidate the previous snapshot and all commands/views referencing it; begin a new generation using the same storage.
6. Manual opening while recording or collecting the event tail: stop admission before UI reconstruction; exclude the tick containing the reopen request, finalize, and return to Performance.
7. Automatic end: stop at a complete interval, finalize, reopen Performance, and display the snapshot. Other tabs' saved data remains intact.
8. Ordinary close/reopen while investigating: keep the same snapshot, viewport, cards, filters, and selected sample.

Manual mode may run indefinitely, retaining the newest capacity-sized suffix. Budget mode may also run indefinitely if no event occurs; do not secretly add a timeout. Window-creation failure preserves the sealed snapshot and provides a retryable open request/status.

## Ownership and persistent state

### Choose external session ownership

Use the existing backend `devSystems::DevInterface` as the lifetime owner of a `std::unique_ptr<DevInterfaceState>`, constructed lazily or during initialization. Add an out-of-line destructor if the state is forward-declared. Pass a non-owning `DevInterfaceState* interface_state` through root parameters and use that state in all root helpers. The root stops owning substantive session state. This avoids broad changes to storage lifetime and global-ID semantics.

Expose an application-owned request path to the capture backend: an internal App accessor or explicit controller pointer in contextual-control parameters. UI actions queue commands; they never call `destroyWindow()` while building/interacting with the window being destroyed. Follow the existing `ActionCall` pattern used in DevContentHeader.

Persistence must extend beyond the root. Inspect selector/workbench/type editors and Clay code generation have their own window-lifetime states. Audit these declarations and move meaningful session data into persistent tab-session structs, keyed by stable inspected object/definition identity. Save expansion, selection, editor buffers, viewport/scroll offsets, and code-generation options. Rehydrate them on build. Ephemeral hover, pressed state, focus ownership, Clay IDs, GPU handles, and font/frame resources remain window-owned. A root-only change does **not** satisfy preservation of all tabs.

Keep `selectedWindowId` and performance scopes if the object still exists. If an inspected application object was destroyed during capture, show unavailable and safely clear invalid references. Capture scope metadata still allows historical timing investigation. Recreated developer-window IDs are never historical application identities.

An alternative is a real app-lifetime state policy plus a storage partition keyed independently of WindowId. That would require changes to registration, lookup, destruction, ID collision rules, and migration tests. It is a larger API change and is not the recommended first implementation.

## Capture controller and boundary contract

### State and API shape

Add `DevPerformanceCapture.hpp/.cpp` under monitoring/reporting, with no dependency on drawable elements. Own it through `DevMonitoringAndReporting`. Keep DevInterface responsible for window operations. The controller emits a close/open request that the App safe point consumes.

Proposed types:

- `PerformanceCaptureStartMode { Immediate, Shortcut }`.
- `PerformanceCaptureEndMode { Manual, BudgetEvent, Duration }`.
- `PerformanceCapturePhase { Idle, ClosingInterface, Armed, Recording, PostEvent, Finalizing, Sealed }`.
- `PerformanceCaptureStopReason { ManualOpen, DurationElapsed, BudgetEvent, Shutdown, Failure }`.
- `PerformanceCaptureSettings`: chord, budget source/window identity, duration/budget/tail nanoseconds, start/end enums and selected timing detail.
- `PerformanceCaptureStatus`: generation, phase, immutable settings, actual start/end/event timestamps, requested/retained tick range, dropped-prefix count, completion quality, stop reason, and reopen error.

Representative recoverable operations return `Status` or `Result<T>`: `request_capture(settings)`, `request_interface_open()`, `acquire_sealed_capture(generation)`. Lifecycle callbacks such as `on_tick_boundary(...)`, `on_completed_window_frame(...)`, and `on_submission_resolved(...)` are nonthrowing and update explicit failure/quality state. Observers carry `[[nodiscard]]` and `noexcept` where appropriate. Implement this state machine against an injected monotonic clock and event sink so tests do not need sleeps or Vulkan.

### Precise transition rules

| Phase | Event | Result |
|---|---|---|
| Idle/Sealed | Valid Main Capture request | ClosingInterface; previous snapshot remains readable until actual start. |
| ClosingInterface | Successful safe-point closure | Armed or pending immediate start at next clean boundary. |
| ClosingInterface | Closure failure | Return to previous state; visible error; no recording. |
| Armed | Fresh valid shortcut press | Start at next clean boundary; new generation. |
| Armed | Any interface-open request | Disarm; open UI; no new snapshot. |
| Recording | First eligible over-budget interval | Save event timestamp/identity; enter PostEvent or stop immediately for zero tail. |
| Recording/PostEvent | Manual open | Stop at start of reopen tick; Finalizing. |
| Recording | Duration reached | Finish eligible complete interval; Finalizing. |
| PostEvent | Event-tail deadline reached | Finish eligible complete interval; Finalizing. |
| Finalizing | Eligible producers/submissions resolved or bounded timeout | Seal; request Performance reopening. |
| Any phase | Application shutdown | Disarm or stop/finalize without reopening. |

The first budget event wins. Subsequent spikes do not extend the tail. Reopen/cancel has priority over a shortcut-start event processed in the same tick. Repeated Main Capture requests cannot queue multiple sessions. Preserve a separate generation counter from UI presentation revision.

### Integrate at application safe points

Place orchestration in the shared polling/frame lifecycle, not exclusively in `App::drawFrame()`. Both convenience and explicit `pollEvents()` + per-window frame usage must advance it exactly once per app tick. Factor DevInterface synchronize into explicit pending close/open processing; avoid running a second conflicting toggle path after the main draw.

Define capture membership using a half-open clean-tick interval `[first_app_tick, end_app_tick_exclusive)`. At the boundary, close the previous cadence interval, drain producers, and evaluate completed measurements. Poll/process input, resolve panel-open/start intent, and perform window transitions before admitting the next application interval.

The current boundary is noted before input polling, so implementation must choose and consistently enforce a revised order. Recommended: preserve the initial cadence timestamp, process input/open intent before application drawing, then commit whether the tick is admissible. A tick with developer-interface destruction, creation, or UI work is ineligible. Do not simply filter `TimingCategory::DevTool`: lifecycle waits and application timing from that tick can also be contaminated.

If shortcut dispatch currently occurs during per-window begin/draw rather than during polling, add a small platform-input capture-intent path before application work, or hold current-tick admission provisionally until intent is resolved. Do not claim an early hook exists without implementing it. Track `dev_interface_worked_this_tick` as a fallback guard and drop that provisional tick on late open detection.

Full intervals have unavoidable quantization: duration/tail may overshoot the deadline by the last admitted interval, and irregular polling can increase overshoot. Show actual duration. Manual reopening excludes the entire reopen tick. Maintain boundary timestamps independently of retained slots so the last captured interval closes without publishing a new ineligible slot.

For budget events, application cadence is `boundary_end_ns - boundary_start_ns`. Window mode uses completed `kWindowFrameTotal` records for the configured application window. Require completed/noncanceled valid-clock measurements; never trigger on an open boundary, GPU-local time, a derived CPU envelope, or a view-filtered subset. Event-tail time starts at the measurement's end timestamp, not delayed ingestion time. If that deadline has already elapsed when detected, stop at the next safe boundary and report detection delay.

## Ring retention without snapshot copies

### Replace app-tick modulo publication

Keep the existing `std::vector<TimingAppTickReport>` and nested capacity reuse. Add capture ownership metadata and write positions. Use a capture-local sequence for physical placement; app ticks may have arbitrarily long gaps between captures.

For capacity `capacity`, track `next_write_slot`, current snapshot `head`, `count`, `total_admitted_ticks`, and `generation`:

1. At actual capture start, reset logical count/head and increment generation; do not clear every physical slot or reset producer queues.
2. For each eligible new tick, reuse `ring[next_write_slot]`, clearing its logical contents with `clearReport()`.
3. If `count < capacity`, increment count. Otherwise advance head and increment dropped-prefix count.
4. Advance `next_write_slot = (next_write_slot + 1) % capacity`.
5. Resolve logical tick position as `(head + offset) % capacity`; validate stored app-tick and generation before ingesting delayed records.
6. At stop, fix the range. At seal, freeze all retained report contents and capture metadata.

Maintain a bounded tick-to-slot lookup for delayed ingestion. During a contiguous capture a checked arithmetic offset into the retained suffix suffices; support provisional exclusion explicitly rather than assuming every application tick occupies a slot. A sorted contiguous vector of tick/slot entries is another option. Do not walk all uncaptured ticks with `publishThrough()`. An old-generation delayed record can never create slots in the new generation.

Capture metadata includes head/count **and** generation, retained app-tick range, requested start/end, actual time range, dropped-prefix count, event identity, quality deltas, settings, and stop reason. Head/count alone cannot validate stale selections or explain loss. Crossing the physical array end is not itself lost history; warn when current-generation data is overwritten. Distinguish a physical wrap from overwritten captured ticks.

Reuse nested vectors and mark unused windows/frames unoccupied. Direct consumers must skip unoccupied entries; current `publicSnapshot()` hides them by copying, but a borrowed reader will see reusable storage. Do not use clear/shrink-to-fit on the whole ring, descriptor registry, or rolling table for every new capture. Reusing vectors avoids routine deallocation; new workload peaks can still allocate, and the UI must not promise allocation-free capture on first use.

Capacity changes can invalidate borrowed spans. Queue/report capacity changes while a snapshot is sealed, armed, recording, or finalizing; apply them only after explicitly releasing the generation for replacement, before recording. Keep effective frames-in-flight capacity calculation and show the actual capacity. A future byte budget would be a separate retention feature.

### Immutable borrowed reads

Introduce a move-only `TimingCaptureReadView` acquired only for a sealed generation. It exposes up to two `std::span<const TimingAppTickReport>` segments for chronological ring traversal plus capture metadata and immutable descriptor/window/thread metadata. Implement a scoped shared-lock guard or equivalent owner-managed lease; mutations/resizing require an exclusive lock. Do not hold this lease across UI frames or in pending callbacks. Restrict traversal to the reported count and occupied records.

At build/layout/Inspector time, acquire a view, validate generation, use it, and release it. Main Capture only queues a mutation; exclusive changes occur after frame readers release views. Persistent commands hold identities/indexes plus generation, never raw pointers or spans. Window renderer submitted hit geometry can outlive a CPU view; make it resolve through a generation check at input time.

Metadata must remain valid independently of live objects. Keep a generation-owned descriptor/source-label table and window/thread display metadata; a bounded metadata copy at sealing is acceptable because it is not duplicated timing history. If descriptor storage already guarantees app-lifetime immutable strings, borrowing is possible; current record/source `string_view` fields need their lifetime checked before assuming this. Live registry growth must not reallocate storage underlying a sealed view.

### Timeline indexing instead of owned record slices

Replace `TimelineSnapshot` with `TimelineCaptureIndex` and an accessor layer. A real-sample entry contains a compact `TimingSampleRef`: generation, logical tick offset, window/frame positions, record position, and CPU/GPU domain. Synthetic tick groups and GPU-local groups contain only derived intervals/identities. Keep contiguous vectors for parent indices, child offsets/children, sorted frame indexes, frame metrics, lanes, cluster members, and visible geometry.

Build the hierarchy once per sealed generation. CPU identity remains track + invocation; GPU identity remains device + queue + submission + zone index, with existing fallback behavior for unavailable zone indices. Validate synchronous containment and cycles as today. Retain context records needed for ancestry even when hidden by selection.

Accessors resolve duration, start, exclusive time, entity, flags, stages, calibration and source location from the borrowed ring/descriptor view. Use descriptor-indexed cached display labels rather than copying source/name strings per sample. Uncalibrated GPU rows continue to use submission-local/device time and never pretend to have host alignment. Avoid the temporary `unaligned_records` record copy: collect sample references instead.

Refactor `timeline_lanes`, `timeline_major_lanes`, `cluster_timeline`, `build_timeline_layout`, `timeline_item_detail`, the Inspector, strip and drilldown card to consume those accessors/indexes. Display geometry and formatted hovered text can be owned because they are derived output. This is a larger coherent change than deleting one report vector, but it is necessary to meet the requested direct-read contract.

Build stable sample ordering for a generation. On scope/domain/category changes, update visibility and lane/layout caches while preserving sample identity and valid drilldown roots. New capture invalidates cards/inspected sample and queued commands; preserve track preferences, split heights, filters and user-selected zoom defaults. Initialize viewport origin/range from the new capture rather than leaving timestamps from the previous capture.

Remove `paused`, `refresh_requested`, refresh clock/sequence polling and interaction-induced pause logic. Keep `snapshot_revision` as a presentation/cache revision and use a distinct `capture_generation` for data identity. Previous/Next and Spike use captured frame metrics only. Blank filtering shows “No samples match this scope,” distinct from “Capture a snapshot to investigate.” A capture with intervals but no instrumented zones remains a valid cadence snapshot with detail-unavailable status.

## Rolling statistics and low idle overhead

Split ingestion in this order:

1. Drain completed CPU/GPU records and element aggregates into reusable scratch vectors.
2. Update rolling duration series for every valid completed sample, regardless of capture membership or ring eviction.
3. Update metadata only when descriptor/track registration revision changes.
4. If recording/finalizing, route eligible records into capture slots; otherwise discard detailed records after rolling processing.
5. Publish quality/status counters without changing sealed report contents.

Separate `not_retained_by_policy`, `late_after_eviction`, `late_after_seal`, stale-generation records, and producer overflow. Disabled retention is not data loss. Rolling values should include late resolved GPU samples once, even if their detailed tick was overwritten. Keep current invalid-sample policy unless tests demonstrate a required correction; document which samples enter statistics.

Reuse `RollingSeries` bounded sample storage and running sum. Compute exact min/max/percentiles only when requested, with a cached result revision where beneficial. Keep sorted percentile scratch buffers out of the producer hot path. Prefer dense zone/domain indexes for hot series data, with a lookup table on first descriptor registration; reserve outside loops. Descriptor registration and first series use may allocate, but steady-state ingestion should reuse storage.

Add `drain_completed_records_into(destination)` overloads to CPU/GPU services and an element-aggregate equivalent rather than repeatedly returning fresh vectors. Keep existing copying APIs only where needed for compatibility; Performance must not call them. Audit callers before deleting them.

Do not turn `cpuLevel` off to disable ring writing: that would lose the rolling inputs. Producer-side aggregate-only collection may be a later optimization, but must preserve exact sample-window percentiles, detail-level suppression, parent accounting, cross-thread publication and quality. The first implementation can retain producer recording/draining while eliminating idle report retention and snapshot extraction; measure before expanding scope.

Memory/error monitoring still contributes overhead. Closing DevInterface removes its build/render work but does not make a development build identical to production. Expose configured monitoring levels and actual capture quality; avoid automatically changing unrelated monitoring settings or losing their state.

## Finalization and delayed data

Stop **admitting new ticks** separately from **sealing retained data**. In Finalizing, allow only completed records belonging to admitted, still-retained ticks of the current generation. Continue rolling updates for everything else. Do not publish new capture slots.

Track submitted GPU work by generation and tick in frame slots/submission metadata. Mark a submission resolved on success or failure, including zero usable queries, canceled submission, and window destruction. A pending count must reach zero through every path. Add a fence-aware safe-point sweep for admitted submitted slots; reuse existing completion processing and never read unresolved queries as complete.

Workers require a similarly explicit completion/flush contract. A drain can see only records published so far, and an active scope can end long after its captured tick. Finalization should wait for known admitted frame tasks and bounded producer flush acknowledgments, not assume one drain is a barrier. Do not block normal rendering while awaiting indefinite user scopes.

Proposed finalization bound: 2 seconds of monotonic time, configurable internally for tests. This is separate from requested capture duration and event tail. After the bound, seal retained data with “Incomplete: pending CPU/GPU measurements” and counts; late records then update rolling statistics only. Application polling must continue to advance finalization. If the application stops polling, reopening cannot happen until it resumes.

Reopen after sealing for the simple immutable investigation contract. Manual reopen has priority over event tails but may briefly await bounded finalization. If instant manual UI return later becomes necessary, show “Finalizing capture…” with no timeline reads until sealed; do not expose mutating reports as a sealed snapshot.

For scopes spanning a boundary, admit detail by original tick/generation attribution and mark cross-boundary/incomplete quality. Do not silently truncate a measured duration and present it as the original measurement. Capture-wide viewport endpoints come from admitted cadence boundaries; overflowing scope intervals can be clipped in display while the Inspector reports their actual measured range.

## Status and loss reporting

Put a compact capture-status strip on Performance, above the timeline:

- Generation, stop reason, requested/actual duration and retained time interval.
- Retained ticks versus total admitted ticks and effective capacity.
- “Capture wrapped: oldest N captured ticks overwritten; showing newest retained interval” when dropped-prefix count is positive.
- Event source, threshold, measured value and marker when budget-triggered.
- Detail level, producer/ingestion loss, unresolved submissions, missing hierarchy, and GPU calibration limitations.

If a budget event was overwritten before the tail ended, retain its small metadata/marker identity and say the triggering interval is outside retained history. Do not claim its full samples remain available. Quality for a generation uses baseline/end counters or per-generation attribution rather than showing lifetime loss as new capture loss.

## Detailed implementation plan

Each step has an acceptance gate; the final target includes every step.

### 1. Characterize lifecycle and freeze the contracts

Inspect current input/shortcut dispatch timing, explicit-frame callers, worker completion, and GPU slot resolution paths. Add deterministic failing tests for manual-open tick exclusion, shortcut disarm, uncaptured app-tick gaps, and repeated capture. Record baseline idle/capture allocation and reporting costs with fixed workloads. Preserve current uncommitted files; do not revert or reformat unrelated code.

Gate: exact start/end membership, budget metric, finalization, and snapshot replacement semantics are executable test cases.

### 2. Preserve interface sessions

Change backend header/implementation, root parameters/build helpers, and tab-session owners to external lifetime. Save meaningful child session state and restore scroll/editor state. Add queued close/open requests distinct from ordinary toggle requests. Ensure renderer/Clay resources still die with their window.

Gate: capture close/reopen and ordinary close/reopen preserve Performance and other tab data; destroyed inspected objects are handled safely; application teardown destroys persistent session state once.

### 3. Separate statistics from retained reports

Change `DevTimingReporting::consumeThrough()` so rolling updates precede retention checks. Add reusable drain overloads/scratch buffers, metadata revisions, policy-aware status, and tests. Gate both `note_tick_boundary()` and all record paths; idle must not publish or revise capture reports.

Gate: idle rolling statistics advance while ring content/hash and generation remain unchanged; late/evicted records contribute once.

### 4. Implement capture ring and read views

Replace global-tick modulo assumptions with reusable capture-local positions and generation checks. Implement head/count/range bookkeeping, occupied-slot iteration, read-view leases and capacity-change rules. Reuse nested vectors. Add compact metadata ownership.

Gate: two captures separated by huge tick gaps retain only their own records without gap-filling, stale writes or history copies; wrapped traversal matches chronological reference output.

### 5. Implement the controller

Add settings/status/state-machine files and ownership/accessors in `DevMonitoringAndReporting`; register new sources in CMake. Validate settings through Result/Status, use injected clock, and cover transition priority and failure rollback. Freeze settings on request.

Gate: every state/event transition has deterministic coverage, including cancellation, zero tail, retry, and shutdown.

### 6. Integrate App safe points and finalization

Change `FlowUi.cpp`, backend synchronize, input intent handling, worker flush acknowledgments, and GPU slot metadata/resolution. Ensure one controller update per tick across convenience/explicit APIs. Exclude closure/open/UI ticks. Track admitted pending submissions and bounded finalization without global device-idle stalls introduced for capture sealing.

Gate: captured timelines contain neither DevInterface work nor contaminated lifecycle ticks; late eligible GPU data arrives before seal; no report mutates after seal.

### 7. Convert timeline consumers to direct reads

Change `DevTimelineData.hpp/.cpp`, layout, viewport, controls, strip, card, Inspector and Selector. Replace owned reports/real-sample slices with references and derived indexes. Preserve hierarchy, GPU local picker, clustering, lane preferences and submit-aligned picking. Remove Follow/Freeze/Refresh and interaction pause behavior.

Gate: investigation matches current geometry/identity expectations, no raw record histories are copied into UI state, and filter changes keep valid selections/cards stable.

### 8. Connect the capture UI

Replace contextual placeholder with Main Capture and its action. Add Inspector start/end/chord/budget/tail/duration settings and clear validation errors. Add empty state/status strip, event marker, wrapping and incomplete-detail messages. Automatic/manual stop selects Performance on reopen; armed cancellation preserves previous investigation.

Gate: all configured paths work through real UI input, including key edge handling, main-window focus, narrow layouts and failed window recreation.

### 9. Update Diagnostics correlation and advice

Replace triggered-capture timing report copies with generation/range references, using brief borrowed queries. Preserve independent errors/memory evidence. Update availability transitions and advice to distinguish not captured, overwritten, pending, incomplete and replaced. Avoid reporting-lock inversion: never call error/memory callbacks while holding the timing exclusive lock.

Gate: errors remain recorded with interface closed; Diagnostics never shows unrelated old-generation data or promises unavailable automatic timing history.

### 10. Verify overhead and finish migration

Run the verification matrix below, remove obsolete transport/data fields and UI copy call sites, document final semantics and measured limits, and format only changed C++ files. Inspect exported headers/source lists and dev-disabled compilation. Re-run checks only after relevant changes/failures.

Gate: all relevant commands return zero; before/after costs and any remaining first-use allocations are recorded. The feature is complete only when persistence, capture control, direct reads, and quality reporting all work together.

## Verification matrix

Tests below are proposed unless explicitly described as existing. No build or test has been run for this report.

| Area | Scenario | Required evidence |
|---|---|---|
| Initial UI | Open Performance with no capture | Empty message and configuration; no minimap/macro/card surfaces or history extraction. |
| Persistence | Configure every tab; close/reopen and capture-return | Tab models, editor buffers, search, split extents, track preferences, valid selections/viewport survive. Window GPU resources recreate safely. |
| Immediate start | Press Main Capture during DevInterface tick | Closure tick excluded; first retained tick starts at next clean boundary. |
| Shortcut arming | Press shortcut before arming, hold during close, then release/repress | No pre-arm or held-key start; exactly one fresh post-arm edge starts. |
| Shortcut scope | Main/secondary window focus, text field, chord conflict | Defined focus policy and registration errors; toggle wins over start in same tick. |
| Armed reopen | Reopen before start with and without old capture | Disarmed; no ring writes; previous snapshot remains intact. |
| Manual stop | Reopen while recording/tail; late shortcut dispatch | Entire reopen tick excluded; reopen lands on Performance; no UI frame enters snapshot. |
| Duration | Fake clock just below/equal/above deadline | Actual start anchors timer; complete-interval stop and reported overshoot match contract. |
| Budget boundary | Duration equal threshold, 1 ns above, canceled/anomalous record | Strict exceed only; valid complete configured metric; no false trigger from view filters. |
| Budget tail | Zero/positive tail, repeated spikes, delayed detection | Trigger included; first event wins; deadlines use event end; no repeated extension. |
| Multiple windows | Different CPU frame times; remove target window | Explicit chosen source; independent scope filters; target removal yields visible stop/failure policy rather than switching sources silently. |
| Ring sizes | Capacity 1, small capacity, exactly capacity, capacity+1, multiple wraps | Correct head/count/order; dropped-prefix count excludes overwriting old generations. |
| Capture replacement | Large uncaptured tick gap, canceled arming, second actual start | No gap fill; storage retained; generation increments only at replacement; stale commands rejected. |
| Occupancy | Different window/frame populations on successive uses | No phantom windows/frames from reusable unoccupied entries. |
| Borrow safety | Reader active during requested replacement/resize | No mutation until lease released; no persistent spans; sanitizers report no invalid access. |
| Idle statistics | Known CPU/GPU durations while no recording | Exact bounded-window count/mean/min/max/percentiles; sealed ring remains byte/logically unchanged. |
| Late statistics | Evicted or previous-generation delayed records | One rolling contribution, correct policy counters, zero new capture slots. |
| GPU finalization | Results resolve before stop, after stop, fail, timeout, window destroyed | Eligible records included before seal; unresolved counts terminate on all paths; sealed data immutable. |
| CPU finalization | Worker completion, held manual scope, detach/flush failure | Bounded completion; no deadlock; incomplete quality visible; original attribution respected. |
| Event overwritten | Tail exceeds retention capacity | Event metadata retained; missing trigger samples disclosed; marker never dereferences evicted record. |
| Timeline regression | Hierarchy/cycles, overlapped lanes, micro clusters, GPU local time | Same investigation behavior; parent/child and identities valid through filtering and drilldown. |
| Submitted picking | Old submitted geometry followed by new generation | Old hit/hover/action discarded; correct current sample selected. |
| Errors/memory | Error before/during/after capture; memory activity during closure | Current/peak memory and errors continue; timing availability truthful; no pinned timing copies. |
| Failure paths | Invalid settings, close/open failure, allocation/ingestion error, shutdown | No partial armed state or infinite finalization; sealed generation survives reopen failure; no shutdown reopening. |
| API paths | Convenience begin/draw and explicit polling/per-window dispatch | Exactly one advance per tick; same membership and reopening semantics. |
| Build variants | Dev off; CPU timing levels 0/1/2/3; GPU unsupported; memory/error levels | Valid builds and appropriate unavailable-detail UI; no dev-only symbols leak into production. |
| Performance | Warmed fixed workload, no capture / recording / sealed investigation | Idle has zero capture-slot publications/history projections; stable-storage reuse; capture/analysis allocations and latency measured separately. |

### Concrete verification commands

Use the repository's Ninja build and preserve existing configuration choices. For a new build directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target flowui_dev_timeline_tests flowui_dev_timeline_layout_tests flowui_dev_timeline_interaction_tests flowui_dev_performance_selection_tests flowui_dev_error_tests flowui_dev_memory_tests -j
ctest --test-dir build -R '^flowui\.dev\.(timeline|timeline_layout|timeline_interaction|performance_selection|errors|memory)$' --output-on-failure
```

Confirm dev-mode/test options in the actual CMake cache; those targets are conditional. Do not overwrite an existing build directory configured with another generator. Add proposed targets `flowui_dev_performance_capture_tests`, `flowui_dev_timing_capture_retention_tests`, and `flowui_dev_performance_capture_interaction_tests`, with corresponding `flowui.dev.performance_capture`, `flowui.dev.timing_capture_retention`, and `flowui.dev.performance_capture_interaction` CTest names. Build and run them explicitly after registration.

Run relevant existing element-state/ID and Inspect integration tests after session migration. Use separate build directories for dev-disabled and compiled detail variants. Run AddressSanitizer/UndefinedBehaviorSanitizer for borrowed views and replacement, and ThreadSanitizer where supported for producer publication/finalization. GUI integration requires a functioning display/Vulkan environment; a skipped GUI run is not a passing result.

For profiling, warm up fixed representative workloads, compare equal-duration idle runs, then repeat capture/replace cycles at equal capacity and detail level. Record CPU reporting time, allocations/bytes, ring publications, raw-history copy count, peak retained capacity, and viewport upload/layout counters. Require no per-tick idle capture publication and no UI-owned raw-history copies. Establish numerical latency/allocation thresholds from baseline measurements rather than inventing them in this report. First-use allocation and increasing workload peaks are measured separately from warmed reuse.

Documentation validation for this report: verify all local link targets exist and review the requested behavior against the plan/matrix. Runtime verification begins with implementation; this report makes no runtime pass claim.
