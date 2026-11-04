---
topic_id: runtime.frame_lifecycle
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: runtime
source_selectors: ["runtime.frame_lifecycle.primary"]
---

# Frame lifecycle and identity

Call shared polling once for an application cadence, then complete each successful window begin/end/draw triplet. UI authoring belongs between begin and end. End publishes layout; draw records commands and submits. Polling retires completed work and consumes timing, memory, then diagnostics before the next tick. A transport thread enqueues commands and cannot render a frame or mutate platform state.

An app tick can include multiple windows. A window frame is assigned by App; the frame slot is a reusable GPU resource index. Submission serials identify actual submissions. A canceled frame may consume a window frame identity and never submit. Keep run, tick, window, frame, device, queue and submission fields separate. Never infer joins from counters or timestamps.

Start-to-start cadence includes work and waiting outside the CPU frame scope. Successful present-call cadence observes calls, not physical display. A stopped polling loop is unresponsive; an IPC worker cannot manufacture progress. Retrieve frame relations first, then inspect only the implicated lifecycle phase.

Useful commands: `status`, `query performance`, `artifact read --artifact frame_relations`. Invariants are exercised by frame contract and agentic identity tests.

Source: [FlowUi::App](include/FlowUi/App.hpp).
