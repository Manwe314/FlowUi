---
topic_id: memory.capacity_and_lifetime
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: monitoring
source_selectors: ["memory.capacity_and_lifetime.primary"]
---

# Memory capacity, samples and retirement

Logical live bytes differ from backing allocation, reusable capacity and retired resources awaiting GPU completion. A larger backing footprint can be reusable capacity rather than a leak. Observe source identity, units, counts, retirement and the fixture across warmup and repeated steady-state intervals.

History samples are segments with duration. Clip the tail to the capture cutoff and weight values by interval length. Ring generations and epochs define continuity; do not subtract counters across resets. Oldest/newest bounds do not prove that there are no internal gaps. Preserve drop, eviction, pending and uncertainty metadata.

A history freeze pauses retention and must not span measurement or file output. Copy bounded chunks into owned storage, validate generation/cutoff, release the read, then serialize. Capture readback/snapshot memory is observer overhead and remains part of process/GPU totals.

Use `query memory --kind samples` and `--kind events`, compare the same scenario, and inspect capacity recommendations through the existing review pipeline. A capacity change must preserve behavior and be backed by exercised high-water evidence.

Source: [FlowUi::devSystems::DevMemoryReporting](include/devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp).
