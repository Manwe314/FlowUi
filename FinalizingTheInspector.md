# Finalizing the Performance Inspector

## Purpose and design decision

Replace the performance inspector's flat stream of controls and text with three clearly bounded sections, in this order: **Capture Settings**, **Rolling Stats**, and **Selected Zone Details**. The panel should help answer three questions: How will I record? Where does time go across the application? Why is this particular invocation expensive?

The main optimization experience must remain useful below a conventional frame budget. A consistent 8 ms application can still have a clear path toward 4 ms. Therefore, expose typical cost, repeated-call cost, recorded self time, and distribution alongside spikes. Red is a status signal, not the organizing principle of the panel.

This is a design and implementation proposal against the current working tree. It does not implement the UI.

## Current implementation and reusable foundations

- [Performance inspector](src/devSystems/devInterface/Performance/Inspector/DevPerformanceInspector.cpp) has one vertically clipped, padded root. Capture controls, sample investigation, identifiers, source text, navigation buttons, and major-track controls share that root. There is no Rolling Stats section today.
- [Inspector state](include/devSystems/devInterface/Performance/Inspector/DevPerformanceInspector.hpp) currently stores window labels and combo options. Keep those lifetimes safe when restructuring the controls.
- [Inspect inspector](src/devSystems/devInterface/Inspect/Inspector/DevInspectInspector.cpp) provides the visual precedent: a bounded panel, distinct title and identity surfaces, and structured content. Its percent-sized root explicitly avoids a grow-sized panel expanding beyond the column.
- [DevInterface theme](include/devSystems/devInterface/Permanents/Backend/DevTheme.hpp) already supplies depth surfaces, borders, teal and blue accents, and semantic status colors. Use these tokens rather than a new palette.
- [Capture controller](include/devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp) already supports immediate/shortcut start and manual/budget-event/duration end. Capture policy is independent of investigation filters.
- [Timing reporting](include/devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp) provides retained tick/frame records, quality status, and per-descriptor CPU/GPU rolling statistics. Existing `TimingRollingStatistics` includes count, min, max, mean, and configured percentiles; it does not provide application cadence distributions, self-time rankings, or scoped invocation comparisons.
- [Timeline data](include/devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp) supplies sample identity, hierarchy, recorded CPU exclusive duration, domain/clock information, and revision-checked commands. Selected measured details are non-owning views into the sealed capture.

## Visual language and layout contract

Use compact developer-tool typography and restrained surfaces. Modern here means readable hierarchy, aligned values, clear interaction states, and deliberate spacing.

| Element | Proposed properties |
| --- | --- |
| Panel | `kDepth1Panel`; bounded to the inspector column; vertical layout |
| Panel title | Reuse the Inspect title treatment; 14 px text; 12 px horizontal inset |
| Major section header | 30 px high; `kDepth2Ink`; 12 px title; optional trailing summary and disclosure |
| Section content | 10–12 px inset; 8 px group gaps; 4–6 px related-row gaps |
| Subsection heading | 11–12 px, medium emphasis; subtle divider rather than another large card |
| Field/control | 28 px high; persistent external label; unit visible beside numeric value |
| Body/value | 12 px; primary values `kTextCanvas`; labels `kTextSecondary` |
| Secondary metadata | 11 px; use muted color only for low-priority information, never essential values |
| Borders/corners | 1 px theme hairlines; 3 px corners on small inset surfaces |
| Status | Text or icon plus color; teal/blue for emphasis, amber/red for actual conditions |

Align numeric columns to the right. Use stable numeric widths or tabular digits when the existing font supports them. Avoid long narrative sentences in the normal view; explain terms through tooltips or a disclosure. Never concatenate several unrelated measurements into one text node.

Design for a 320–420 px inspector column. Above 360 px, compact pairs of fields can share a row. Below that, stack those pairs and show metric tiles in one column. At very narrow widths, wrap label/value rows instead of clipping values. Tables show fewer columns with row expansion for the remainder. No horizontal scrolling in the primary experience.

## Complete element hierarchy

Names below describe semantic UI nodes. Promote nodes with their own state, repeated behavior, or complex rendering into Flow elements; simple wrappers can remain Clay declarations. Stable local names are required in either case.

```text
performance_inspector_root
├── panel_title                         "Inspector"
└── panel_scroll_body
    ├── capture_settings_section
    │   ├── section_header
    │   └── section_content
    │       ├── capture_policy_fields
    │       │   ├── start_mode_row
    │       │   ├── shortcut_row         conditional
    │       │   ├── end_mode_row
    │       │   ├── duration_row         conditional
    │       │   └── budget_fields        conditional
    │       │       ├── budget_source_row
    │       │       └── threshold_tail_row
    │       ├── capture_policy_summary
    │       ├── capture_validation       conditional
    │       └── capture_information      collapsed disclosure
    ├── rolling_stats_section
    │   ├── section_header
    │   └── section_content
    │       ├── rolling_context_row
    │       ├── analysis_target_row
    │       ├── timing_summary_grid
    │       ├── cadence_sparkline
    │       ├── timing_distribution
    │       ├── optimization_opportunities
    │       │   ├── ranking_controls
    │       │   ├── ranked_zone_rows
    │       │   └── show_more_control
    │       └── timing_quality_notice    conditional
    └── selected_zone_section
        ├── section_header
        └── section_content
            ├── empty_selection         mutually exclusive with details
            └── selected_zone_content
                ├── zone_identity
                ├── measurement_notice conditional
                ├── cost_summary_grid
                ├── navigation_actions
                ├── invocation_comparison
                ├── hierarchy_costs
                │   ├── parent_row
                │   ├── child_cost_summary
                │   ├── ranked_child_rows
                │   └── show_more_control
                ├── source_location
                ├── timing_context
                └── technical_details collapsed disclosure
                    ├── record_identity
                    ├── absolute_timestamps
                    ├── raw_quality
                    └── gpu_metadata   GPU only
```

### Layer 0: outermost parent

`performance_inspector_root` uses percent width and height bounded by the existing column, top-to-bottom layout, zero outer padding, and `kDepth1Panel`. Its first child is the fixed panel title; its second child is a grow-sized scroll viewport with a zero minimum height. Do not put scrolling on a content-sized root.

### Layer 1: panel content and its three children

`panel_scroll_body` owns the single primary vertical scrollbar. Its three children are the major sections above, with fit-content heights and full available width. Use section borders and surfaces to separate them, not oversized blank gaps. Headers scroll with content; the panel title stays visible. Avoid nested scroll areas for child tables.

All three sections start expanded. Capture Settings and Rolling Stats may be collapsed; their headers retain a short policy or timing summary. Selected Zone Details stays expanded by default and can be collapsed explicitly. Expansion state persists across selection changes and captures. A small panel may require scrolling, but collapsing the first two sections should make room for investigation without changing data.

Every major section has exactly two direct children: a header and a content container. The header places disclosure/title first and compact contextual text second. Only the disclosure/header interaction toggles expansion; buttons embedded in a header must not toggle it accidentally.

## Section 1: Capture Settings

### Header and content order

Header: **Capture Settings**, with a compact summary such as `Immediate · 5 s`. The content's first child is the policy field group, followed by one-line policy help, validation feedback, and a collapsed information disclosure.

| Ordered field | Layout and behavior |
| --- | --- |
| Start | Label + combo: `Immediately` / `Shortcut` |
| Shortcut | Only for shortcut mode; modifiers and key side by side, stacked at narrow widths |
| Stop | Label + combo: `Manual reopen` / `Budget event` / `Duration` |
| Duration | Duration mode only; numeric input with `s` suffix |
| Budget source | Budget mode only; application tick cadence or a named application window |
| Threshold / After event | Budget mode only; paired numeric inputs with `ms` and `s` suffixes |

Labels belong outside combos, so the selected option need not repeat `Start:` or `End:`. Hidden mode-specific values persist when the user switches modes. Do not show a large shortcut key/modifier list until that mode needs it.

Keep existing ranges: duration 0.001–86400 s, budget 0.001–60000 ms, tail 0–86400 s. Reject non-finite values. Validation appears adjacent to the relevant field, with a short section-level error for controller failures. A disappeared budget window requires explicit correction; never silently select a different source.

The existing **Main Capture** action remains the capture entry point. Settings describe the next recording; the immutable settings associated with a completed recording remain available in capture information. Use one shared validation path so the panel and Main Capture cannot disagree. Do not add a duplicate start button to this panel.

The summary explains the consequence: `Main Capture closes the interface and records for 5 s`, or `Main Capture closes the interface and arms Ctrl + Shift + F8`. In manual mode: `Reopen the interface to stop`. This is short visible help; detailed lifecycle explanations belong in the disclosure.

`capture_information` contains capacity, retained/overwritten ticks, completed-capture stop reason, pending measurements, and incomplete/failure state. Show a compact warning outside the disclosure if truncation or failure materially affects the investigation. Capacity is not a capture duration guarantee.

### Keep capture and analysis targets separate

The capture budget controls when recording stops. The Rolling Stats analysis target controls comparison and highlighting only. A user may capture using a 16.6 ms trigger and investigate a 4 ms optimization goal. Neither setting should silently update the other.

## Section 2: Rolling Stats

### Data context before numbers

The first content child, `rolling_context_row`, states the dataset and coverage: `Retained capture · 842 complete ticks · 7.1 s`. When a capture is sealed, the values are frozen and labeled accordingly. They must not appear live. Empty state: `Record a capture to see application timing statistics`.

For this design, “rolling” means the newest bounded sequence of complete retained application ticks; a sealed capture freezes that sequence. Provide a compact range combo (`Last 120 ticks`, `Last 600 ticks`, `All retained`) and disclose actual coverage. Do not present the existing per-descriptor rolling history as if it had the same window: it has independently bounded invocation samples and may include a different population.

These statistics cover the application and exclude DevInterface work. Selector category, zone, thread, and timeline zoom filters do not change them. A labeled window-source selector may switch the cadence summary to a particular application window; it is an explicit analysis control and does not alter capture settings. The zone ranking stays application-wide and separates CPU/GPU domains.

`analysis_target_row` follows: **Target** numeric input, `ms`, initially 16.6, freely editable to 4 or another positive finite value. Show `Analysis only` in help. Persist it for the session. Compare the target against the chosen cadence/frame metric, not against the sum of overlapping zone durations.

### Summary, distribution, and trend

`timing_summary_grid` has four compact tiles, in reading order:

1. **Typical**: median cadence/frame time; mean as secondary text.
2. **Tail**: P95; P99 as secondary text.
3. **Worst**: maximum; sample location/reveal action when valid.
4. **Against target**: percentage above target and median headroom or overrun in ms.

The trend follows the tiles: a modest 40–48 px sparkline with a target reference line, a labeled range, and hover values. The distribution follows it as aligned rows for minimum, median, mean, P95, P99, maximum, and valid sample count. Always identify whether the metric is application tick cadence, CPU frame duration, or another supported metric. Do not label a recorded CPU envelope as a frame measurement.

Use nearest-rank percentiles consistently: sorted sample at `max(ceil(p × count), 1) - 1`. Missing boundaries and incomplete intervals are excluded from cadence percentiles and counted in the coverage notice. One sample is valid but marked `Limited sample`; P99 on a small population must not imply strong statistical confidence.

Color threshold comparisons without hiding below-target opportunities: red for above target, amber for 80–100% of target, neutral/teal below 80%. Include the numeric ratio and label. Changing the target recomputes comparisons and line placement, not captured measurements.

### Optimization opportunities

This subsection answers “Where can I shave time?” even when every tick meets the target. Its first child contains a CPU/GPU domain switch and ranking combo. CPU defaults to **Total recorded self time**, with alternatives **Total inclusive time**, **Mean per call**, **P95 per call**, and **Call count**. GPU defaults to **Total zone duration**; do not offer CPU self time for GPU samples.

Each ranked zone row contains a human-readable zone name, category, primary ranked duration, and compact secondary text such as `0.12 ms/call · 18 calls/tick`. A subtle normalized bar communicates relative cost within this ranking, not budget status. Display five rows initially and expand with `Show all (N)` in the main scroll body.

The row's expanded details expose total duration, mean/P95 invocation duration, invocation count, calls per complete tick, and share of the comparable measured population. Row activation selects a representative valid invocation (largest eligible duration, deterministic tie-break) and reveals it in the timeline. State that action in the tooltip; a descriptor aggregate is not itself a recorded invocation. Do not silently change the selector filters.

For CPU, summing eligible recorded exclusive durations by descriptor helps identify repeated small costs without double-counting synchronous children. This remains measured elapsed time and may include waits or uninstrumented work. Inclusive totals deliberately double-count nesting; label them accordingly. Split wait-role costs from work in row metadata and provide a work/wait filter without treating work as proven CPU execution time.

For GPU, overlapping queues and nested GPU zones prevent duration totals from being application wall time or device utilization. Keep device/queue context visible on expansion; never add CPU and GPU durations into a single “total application time”. Category breakdowns, if shown, must use the same explicitly labeled domain and aggregation rules.

`timing_quality_notice` is the final child. Show a concise warning with count and affected metric when missing, truncated, dropped, canceled, or anomalous records undermine a conclusion. Detailed counters can expand here. Keep this about timing integrity, not memory or unrelated diagnostics.

## Section 3: Selected Zone Details

### Header and selection contract

Header: **Selected Zone Details**, with CPU/GPU or derived-group context. Selecting an invocation in either the major timeline or any minor card updates the same inspector. Empty state is one concise instruction: `Select a zone in a timeline to inspect its cost and context`.

Resolve selection using capture generation and snapshot revision before accessing any non-owning details. New capture, eviction, or snapshot replacement clears an invalid selection and shows `The selected sample is no longer retained`. A clustered timeline item must resolve to an individual sample through the existing selection interaction; never silently combine several records into one invocation detail view.

Preserve section disclosures when selection changes. If the inspector is scrolled deep into technical details, selecting a different zone returns the scroll position to this section's identity block so the new identity and cost are visible. Do not force-open sections the user explicitly collapsed.

### Ordered children of selected_zone_content

**1. `zone_identity`.** Zone name wraps to two lines, with full name available on hover/expansion. Beneath it are compact CPU/GPU, category, and human-readable role badges. Add named window and thread/queue context; use numeric fallback only when no friendly identity exists. Do not lead with descriptor or invocation IDs.

**2. `measurement_notice`.** Prominent only when needed: incomplete, canceled, clock anomaly, missing hierarchy, truncated detail, or GPU local-clock restrictions. Synthetic ticks say `Derived application tick grouping`. These notices precede computed comparisons so questionable data cannot appear authoritative.

**3. `cost_summary_grid`.** Lead with useful numbers absent from timeline geometry:

| Metric | Definition / availability |
| --- | --- |
| Elapsed | Selected inclusive duration; all real samples |
| Recorded self | Recorded exclusive duration; real CPU samples only |
| Self share | Exclusive / inclusive duration when inclusive is positive and reliable |
| Parent share | Selected inclusive / valid synchronous parent duration; same compatible hierarchy only |

CPU helper tooltip: “Elapsed minus recorded synchronous direct-child time; includes waits and uninstrumented work.” Self time is a measurement opportunity, not a guarantee of removable CPU work. Zero denominators show `—`. GPU substitutes a contextual metric such as comparable submission-relative offset; it never fabricates exclusive duration. Derived tick groups show interval duration and recorded coverage, without per-invocation self metrics.

**4. `navigation_actions`.** Compact, labeled actions: `Focus in minor`, `Reveal in major`, and `Inspect parent` when available. Reuse existing command semantics, but make the selected sample identity explicit for reveal so it cannot act on an unrelated card or frame. Disable reveal with an explanatory tooltip for a GPU-local sample that cannot map to the major CPU clock. Wrap buttons at narrow widths; no unlabeled icon-only controls.

**5. `invocation_comparison`.** Title **Compared with other calls**. Population: same descriptor and domain, within the selected capture/range, selected window and CPU thread or GPU device/queue context. Show that population label and N before results. Present median, P95, maximum, selected/median ratio, and percentile rank. Rank uses the proportion of eligible calls with duration less than or equal to the selection; define ties consistently. Add `Largest call` navigation when a different valid sample exists. If the selected call lies outside the analysis range, say so and retain its own elapsed value without giving it a population rank.

This supplies information not easily read from track geometry: whether this invocation is typical, unusually slow, or part of a repeated expensive pattern. Exclude invalid samples from statistical comparisons and show the exclusion count. With no comparable population, render a short unavailable state instead of zeros.

**6. `hierarchy_costs`.** Title **Where this call spent time**. First child is a named parent row with `Inspect parent`. Second child summarizes recorded self and synchronous child coverage for eligible CPU samples. Third child lists direct children ranked by cost: child name, elapsed, and percentage of selected duration, with inspect action. Show five initially; `Show all (N)` reveals the rest, replacing the current silent 16-child cutoff.

Only present an additive self/child breakdown when hierarchy and exclusive timing are reliable. For overlap, missing children, or inconsistent accounting, show individual child durations with a warning and suppress the additive bar. GPU lists instrumented child zones without claiming their sum accounts for parent duration. Asynchronous CPU work is not a synchronous child deduction. Do not infer removable cost from a large child duration.

**7. `source_location`.** Title **Source**. Show function first and `filename:line` second; wrap long functions, expose the complete path on expansion or copy. Add copy actions using established DevInterface controls. Offer editor navigation only if an existing supported integration is available; never present a dead link or button.

**8. `timing_context`.** Compact aligned rows: app tick, named window/frame, track, offset from a valid containing root, and overlap within that root. Relative time is preferred over raw nanosecond timestamps. Root-relative values are shown only for compatible clocks; identify the root explicitly rather than assuming the last minor card is relevant to every selected sample.

**9. `technical_details`.** Collapsed by default and last in the section. Contains descriptor/invocation/entity IDs, recorded parent identity, frame/tick raw identity, absolute start/end ns, raw flags alongside decoded labels, and GPU submission/zone indices, device/queue identities, timestamp ticks/period/valid bits, stages, calibration ID and deviation. Use named key/value rows and copyable full values. Raw IDs may wrap; never truncate the copied value. Keep this accessible through scrolling and disclosure, not deleted.

### Formatting rules

- Store/calculations stay in ns. Display short durations in µs below 1 ms and ms at/above 1 ms, with enough precision to distinguish micro-optimizations. Use up to three decimals and show `<0.001 µs` for a positive duration below display precision; actual zero is `0 µs`.
- Use a consistent unit across directly compared table columns. Tooltips or technical rows provide exact ns. Relative offsets retain an explicit sign.
- Percentages use one decimal; counts are integers with readable grouping; ratios use two decimals. Display unavailable as `—` with a reason, never as zero.
- Translate roles and quality flags into names. Do not expose numeric enums as the primary label.
- Titles and functions wrap; aligned value rows can become stacked rows at narrow widths. Every primary data row remains readable without overlap or horizontal scrolling.

## Track controls and interaction ownership

Move **Major tracks** to the major timeline toolbar, where hide/pin/reorder actions affect the visible tracks. Its expandable settings area belongs to the workbench, not a fourth inspector section or an unrelated footer inside Selected Zone Details. Preserve all existing hide, pin, show, and ordering behavior and semantic track keys.

Capture settings remain session/controller-owned. Analysis target/range, disclosures, and ranking choice are inspector/session presentation state. Timeline selection/actions remain in `DevTimelineState`; do not create a competing selected-zone state. Keyboard navigation, focus, combo popups, input editing, and clipboard actions should follow FSEL and existing DevInterface conventions. Ensure popups survive the panel's clipping contract and mouse-wheel input goes to the correct scroll owner.

## Data preparation and performance

Build one derived analysis snapshot for the retained capture/range, shared by rolling and selected comparisons. Use contiguous vectors and compact descriptor-to-aggregate indexes; reserve outside loops. Keep chronological metrics for trends and separately sorted duration indexes for percentiles and largest-call lookup. Avoid copying raw timing records into another history.

Cache by capture generation, reporting mutation/revision, analysis range, and explicit summary source. Selection changes need only resolve the chosen sample and comparable group. Target changes update threshold counts/headroom without rebuilding hierarchy. UI disclosure changes do not recalculate aggregates. Sorting a ranking changes indexes, not measurement ownership.

Resolve original sample details under a valid `TimingCaptureReadView` lease within the UI frame. Never persist its spans or string views after the lease expires, and do not acquire another reporting lock while holding it. Owned presentation labels/aggregates may be cached; raw sample references require generation validation and a fresh lease. Extend reporting with a coherent read API only if necessary to obtain live retained data without conflicting locks; avoid stitching together mismatched snapshots.

For cadence, use complete boundary intervals and existing trustworthy frame milestones. For exclusive CPU aggregates, use recorded values and quality/hierarchy eligibility. For comparisons, keep domain and clock identity separate. GPU-local durations remain useful for within-queue comparisons even when CPU-clock placement is unavailable.

## Implementation plan

### 1. Establish the analysis contract and focused helpers

Define a small derived inspector analysis model beside the performance inspector, with documented metric definitions, eligibility rules, population identity, and cache keys. Implement duration/ratio formatting, nearest-rank percentiles, cadence extraction, descriptor aggregates, and comparable-call lookup as pure helpers. Add focused tests for repeated small calls, nested exclusive versus inclusive cost, missing boundaries, zero denominators, invalid records, GPU queue separation, percentile ties, and target changes to 4 ms.

Deliverable: complete calculations with validated semantics before visual wiring. Reuse existing timing/timeline data; do not change capture policy or add dependencies.

### 2. Create the bounded panel and section shell

Refactor [DevPerformanceInspector](include/devSystems/devInterface/Performance/Inspector/DevPerformanceInspector.hpp) into the bounded title + scroll body + three sections. Add a local section/header, metric tile, and key/value row vocabulary using the theme. Keep reusable helpers within Performance unless another actual consumer justifies shared infrastructure. Wire disclosures and stable element identities. Verify the percentage root, child minimum height, scroll offsets, wrapping, and clipping in the existing column.

Deliverable: all three real sections using current data and honest empty states; no placeholder metrics.

### 3. Pack capture settings and unify validation

Lay out existing FSEL combos/number inputs with external labels and conditional compact rows. Extract shared validation for Main Capture and panel feedback; preserve current shortcut semantics and limits. Add policy summary and capture information disclosure. Distinguish upcoming policy from recorded settings. Test mode switching, invalid/non-finite inputs, disappearing windows, and unchanged capture lifecycle behavior.

### 4. Implement Rolling Stats

Wire the derived snapshot to range/source controls, analysis target, metric tiles, distribution, sparkline, quality notices, and ranked opportunities. Implement expanded rows and representative-sample navigation with revision checking. Verify that selector filters and zoom leave application statistics unchanged; source/range changes must update labeled coverage. Confirm sealed statistics remain frozen and that a 4 ms target highlights opportunities below 16.6 ms.

### 5. Implement Selected Zone Details

Replace the text dump with the ordered identity, notices, costs, actions, comparison, hierarchy, source, context, and technical disclosure. Decode roles/flags. Remove the silent child limit through explicit expansion. Handle real CPU, calibrated GPU, local-clock GPU, synthetic ticks, absent source, missing parent, and stale selection. Ensure reveal/focus commands address the selected invocation and valid generation.

### 6. Relocate track management

Move existing track-control rendering to the major workbench controls without altering semantic track preferences. Reuse the current actions and revision guards. Verify hide/show, pin/unpin, ordering, and reachability when no zone is selected.

### 7. Validate the complete experience

Use existing [capture interaction tests](tests/DevSystemsTests/DevPerformanceCaptureInteractionTests.cpp), [capture controller tests](tests/DevSystemsTests/DevPerformanceCaptureTests.cpp), [timeline interaction tests](tests/DevSystemsTests/DevTimelineInteractionTests.cpp), and [timeline data tests](tests/DevSystemsTests/DevTimelineTests.cpp) as the starting points. Inspect registered target/filter names in [tests/CMakeLists.txt](tests/CMakeLists.txt) before running narrow builds/tests.

Format only changed C++ files. Configure with the repository's documented Debug/export-commands setup if needed, build the actual affected targets, and execute focused CTest filters. Record exit codes before claiming success. No dependency upgrades or unrelated API cleanup.

Exercise the UI at approximately 320, 380, and 480 px widths, plus a short panel height. Verify long names/paths, large IDs, hundreds of children, empty captures, one-sample captures, stale generations, dropped records, and independent CPU/GPU clocks. Capture screenshots using the repository's existing preview/debug facilities and compare the section treatment with Inspect. Measure repeated idle builds to confirm cached analysis avoids per-frame history sorting.

### Acceptance criteria

- Exactly three major inspector sections with consistent headers and content areas; major-track controls are available in the timeline workbench.
- Capture controls are compact, labeled, conditional, and still drive the existing capture lifecycle.
- Rolling Stats exposes typical/tail/worst timing and ranked repeated costs; optimizing toward 4 ms is useful without waiting for a red spike.
- Dataset, sample coverage, domain, metric definition, and measurement limitations are visible where needed.
- A selected zone leads with identity, self/inclusive cost, and comparison; large IDs and raw timestamps are secondary and fully accessible.
- Major and minor selections update the same details, and stale records cannot leave misleading values or dangling views.
- No overlapping text, inaccessible primary values, hidden child truncation, or dead controls; narrow widths remain usable.
- Existing capture/timeline behaviors and focused calculation/interaction checks pass after implementation, with actual commands and exit codes recorded.
