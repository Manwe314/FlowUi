---
topic_id: workflow.performance_improvement
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: agentic
source_selectors: ["agentic.evidence_contract.primary"]
---

# Improve a measured frame-rate regression

1. Discover the exact run/window, optimized build, capabilities, fixture and scenario. Resolve FPS to window present-call cadence and record the unavailable display-timing limitation.
2. Plan and collect performance diagnosis. Inspect coverage, work/wait/GPU contributors and source selectors. Unmeasured business logic needs a custom scope or external profiler, not a guessed bottleneck.
3. Make one targeted source/configuration change and record its patch/build identity. Preserve required rows, rendering, interactions and fixture settings.
4. Rebuild the same optimized profile, run focused correctness checks, then collect independent clean verification passes after declared warmup. Collect corresponding visual and diagnostic evidence separately.
5. Compare workload/hardware/build/instrumentation validity before evaluating strict objectives. Stop when all explicit gates pass. If coverage or comparability fails, measure again; if throughput misses, inspect the new bottleneck and iterate.
