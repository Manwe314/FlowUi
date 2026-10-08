# Performance capture implementation

Implemented from [PerfromanceDXupgrade.md](PerfromanceDXupgrade.md).

Performance now opens with an empty investigation prompt. Main Capture commits the Inspector settings, suspends developer interface work, and arms or starts recording on a clean application tick. Manual reopening stops before the reopening tick. Duration and first budget-event completion seal the snapshot and return to Performance. A fresh shortcut press is required after arming; reopening while armed cancels without replacing the previous snapshot.

The [application-owned controller](include/devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp) handles recording, event tails, finalization, cancellation and reopening. The existing timing ring retains one generation, reuses nested storage across captures, and reports overwritten prefixes. Rolling statistics continue independently when retention is disabled. Timeline consumers borrow sealed records through a read lease and build presentation indexes rather than duplicate raw timing history. Diagnostics reference capture generations and report unavailable timing evidence explicitly; memory and error monitoring continue independently.

## Choices made during implementation

- **Window lifetime:** suspend and hide the existing developer window, preserving its identity, element states and rendering resources. Root interface state is also owned outside the drawable element. This avoids destroying every tab's child state and recreating GPU resources; the tradeoff is that suspended window resources remain allocated. Inspect overlays, picking and UI replay are suspended during capture as well.
- **Layout:** capture configuration sits above sample investigation in the Inspector. Only settings relevant to the selected start/end mode are shown. The contextual header retains the CPU detail selector beside Main Capture. Capture identity, duration, wrap warnings and quality information appear above the Workbench controls. A budget-event marker is drawn when its time remains inside the retained interval.
- **Defaults:** immediate start, five-second duration, Ctrl+Shift+F8 for shortcut start, 16.6 ms budget and a 0.5-second event tail. The shortcut editor uses named keys and modifier combinations. Budget values are entered numerically, with 16.6/33.3 ms guidance in the label.
- **Budget metric:** application-tick cadence is the default. A named application window's completed CPU frame duration can be selected instead. Removing that window stops the capture with an explanatory message. Investigation filters do not affect this metric.
- **Finalization:** wait nonblockingly for eligible CPU scopes and submitted GPU measurements, bounded to two seconds. A timeout seals with an incomplete warning. Automatic return waits for sealing; manual reopening during recording also waits for this bounded finalization.
- **Input bounds:** duration and event tail are limited to 24 hours; the Inspector budget is limited to 60,000 ms. Invalid values and a conflict with the panel-toggle chord are rejected before suspension.
- **Replacement:** a new generation clears sample-specific drilldown cards and selection while preserving investigation preferences. Canceling an armed request preserves the existing generation.

## Executed validation

The following 11 CTest tests passed, with command exit status 0:

- `flowui.dev.performance_capture`
- `flowui.dev.performance_capture_interaction`
- `flowui.dev.timeline`
- `flowui.dev.timeline_layout`
- `flowui.dev.timeline_interaction`
- `flowui.dev.performance_selection`
- `flowui.dev.errors`
- `flowui.dev.memory`
- `flowui.dev.inspect_interaction_0`
- `flowui.dev.inspect_interaction_1`
- `flowui.dev.inspect_overlay_menu`

Coverage includes retention and storage reuse, idle rolling statistics, capture boundaries, fresh shortcut edges, budget thresholds/tails, bounded finalization, borrowed timeline details, window suspension/state retention, and capture-aware diagnostic references. The GUI tests exercised the available Vulkan/display environment.

The developer-disabled library also built successfully in `Build-performance-production`, with baked changes disabled for that configuration. The existing baked overrides contain developer-specific schema; the main `Build` configuration retains its existing settings.

Additional element-state API/GC and Inspect subtree targets could not compile with this environment's GCC 12: the unchanged [registration code](include/managers/structs/ElementManagerStructs.hpp#L96) triggers an immediate-function diagnostic for `elementDebugName()`. Those tests are not reported as passing. Sanitizer runs, all compiled timing-level variants and numerical overhead profiling remain unverified; the design report's full matrix is not claimed complete.
