---
topic_id: timing.interpretation
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: monitoring
source_selectors: ["timing.interpretation.primary"]
---

# Cadence, scoped cost and GPU clocks

Window present-call cadence is `(N-1) * 1e9 / (last_present_ns-first_present_ns)` for one continuous valid segment. Aggregate segment elapsed durations and interval counts, not reciprocal frame FPS. App ticks, window starts, CPU frame scopes and display timing answer different questions.

CPU scope totals can be inclusive or exclusive. Parent and child durations must not be summed as independent work. Separate work, waits and unmeasured gaps. A large zone is a candidate, not proof of the bottleneck. GPU work overlaps CPU work; uncalibrated GPU timestamps cannot be placed on the CPU timeline by proximity. Preserve period, valid bits, calibration and submission identity.

Performance verification disables optional UI/overlay/tree/image work before warmup, then measures the same workload in an optimized build. Images collected simultaneously make the pass diagnostic. Writer activity can contend for CPU and disk even on a separate thread. Do not subtract observer cost to invent uninstrumented FPS.

Inspect `query performance`, quality and native timing artifacts. `objective check` requires independent passes, coverage and a strict comparison operator. Above 40 means greater than 40, not greater than or equal.

Source: [FlowUi::devSystems::TimingCategory](include/devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp).
