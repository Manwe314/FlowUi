# Agentic Workflow frame evidence

Build with `FLOW_UI_DEV_MODE=ON` and `AGENTIC_WORKFLOW=ON`. The generated public
`FLOW_UI_AGENTIC_WORKFLOW` value controls the API, renderer hooks, and installed
package. Runtime collection requires explicit application configuration; building
the feature alone creates no evidence directory and starts no endpoint.

Configure the service before constructing the application:

```cpp
FlowUi::AppConfig configuration{};
#if FLOW_UI_AGENTIC_WORKFLOW
configuration.agentic_workflow.enabled = true;
configuration.agentic_workflow.store_path = ".flowui/agentic/v1";
configuration.agentic_workflow.application_id = "my_application";
configuration.agentic_workflow.local_attachment_enabled = true;
configuration.agentic_workflow.visual_capture_enabled = true;
configuration.agentic_workflow.input_control_enabled = true;
configuration.agentic_workflow.action_control_enabled = true;
#endif
auto application = FlowUi::makeApplication(configuration);
```

FlowUi supports one `App` per process, with multiple application windows. Run
independent application instances in separate processes and select their exact
run IDs with `--instance`. The service observes the application's normal polling
loop; it cannot produce frames while that loop is stopped.

An explicit `flowui-agent launch` passes `FLOWUI_AGENTIC_STORE`,
`FLOWUI_AGENTIC_RUN_ID`, and `FLOWUI_AGENTIC_ENDPOINT_ENABLE` once at startup.
Leave `enabled` unset and `store_path` empty to accept that handoff. Explicit
configuration takes precedence; `enabled = false` keeps the service dormant and
`local_attachment_enabled = false` disables attachment. Visual resource support
requires the application's own `visual_capture_enabled` opt-in before creation.

Input replacement and exported application actions have separate runtime opt-ins:
`input_control_enabled` and `action_control_enabled`. Enable each only when the
application should expose that execution path. Planning works offline through
`flowui-agent interaction plan --plan-file plan.json`; execution, target discovery
and recording select a live run. Input plans are finite declarative data. They
replace assessed input at the normal window input boundary, before the App's
usual coordinate conversion and UI consumers. The App continues to own polling,
layout, rendering, action dispatch and cleanup.

Relative offset `0` is the first valid selected input frame after the declared
baseline. Startup frames 1–2 can be unrepresentative; optionally begin at offset
`2` or `3` with explicit neutral input covering earlier frames. This is guidance,
not a minimum accepted offset. Exact-address schedules use run/window/window-frame
keys. Input delivery, completed interaction observations, callback completion and
presentation remain separate facts. See the [input-control knowledge topic](../include/devSystems/devAgenticWorkflow/knowledge/systems/interaction.execution.md)
and [replay workflow](../include/devSystems/devAgenticWorkflow/knowledge/workflows/interaction_replay.md).

Enable visual resource support before creating application windows and viewports.
The application-owned service selects a bounded visual capture at its safe point.
The renderer receives the service's exact run, capture, app tick, window frame,
tree generation, and theme revision when that frame starts. Successful submission
adds the real device, queue, and submission serial. A reusable GPU frame slot is
never used as an evidence identity.

The [frame producer](../include/devSystems/devAgenticWorkflow/frameCapture/AgenticFrameCapture.hpp)
records transfer commands and restores the image layout. After the normal frame
fence signals, it invalidates noncoherent memory and converts the completed
readback into a bounded, owned RGB payload. The service receives that payload
through a nonthrowing sink. It does not receive Vulkan allocations. Unsubmitted
or canceled records produce no completed image. Submitted buffers remain alive
until their fences retire, including during cancellation and shutdown.

Viewport content has its own revision and committed producer tick/frame/submission.
A load-only pass with no callback can sample content produced by an earlier frame;
the image record retains that producer alongside the current consumer key. New
producer identities commit only after successful submission, including frames
outside a selected capture. Failed device drains discard untrusted readback bytes.

The [image contract](../include/devSystems/devAgenticWorkflow/AgenticImage.hpp)
provides PPM source encoding, portable PNG derivatives, physical-pixel crops,
contact sheets, and equal-size pixel comparisons. It records the original image
dimensions and crop offset so callers can preserve logical-to-physical/DPI
transforms. Image bytes and metadata are published only as inventoried artifacts
of an immutable capture. The renderer performs no filesystem operations.

Supported readback conversions are RGBA8, BGRA8, and RGBA16 half float. Alpha is
discarded. Half-float RGB is clamped to `[0, 1]` before RGB8 conversion, explicitly
recorded as `linear-clamped-rgb8`; this is unsuitable for an HDR color-accuracy
claim. Window captures retain the selected Vulkan color-space metadata.
Viewport images with no declared color space retain `unknown`. PNG encoding
does not recover discarded alpha or clipped dynamic range.

Readback reserves GPU-to-CPU bytes and reports failures instead of waiting for
disk output or allocating without a limit. Fence-completion copying still costs
CPU time and memory bandwidth. Use `visual_inspect` for appearance and a separate
`performance_verify` pass for clean cadence acceptance. A screenshot establishes
appearance. Interaction observations and scenario assertions establish their
declared input, focus and application outcomes separately.

The [renderer tests](../tests/RendererTests/AgenticFrameCaptureTests.cpp) exercise
fence ownership, two windows, canceled frames, submission gaps, resized targets,
reused slots, and RGB conversion. The
[image tests](../tests/AgenticWorkflowTests/AgenticImageTests.cpp) run without a GPU
and verify codecs, budgets, crops, differences, and contact sheets.
