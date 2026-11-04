---
topic_id: interaction.execution
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: ["interaction"]
owning_subsystem: devAgenticWorkflow
source_selectors: ["interaction.execution.primary"]
---

# Frame-bound input control

Enable App input control independently of collection and local attachment. Query `interaction capabilities` for current limits, observations and target support. One App owns all windows in a process; replay, recording, scenarios and exported actions share execution ownership. Transport only queues work. The application still polls and builds its normal frames.

`interaction plan` validates and compiles finite declarative data offline. Raw rows or explicit spans replace the complete effective input state; gaps, duplicate addresses, unknown key slots and nonfinite coordinates fail validation. Text and scroll apply once per span, while held buttons and keys persist only as declared. Window-logical coordinates pass through the App's usual scale conversion once. No shell, scripts or archive callbacks execute.

Relative offset `0` is the first eligible selected input frame after activation and the reported baseline. Startup frames 1–2 may not represent a settled layout; starting at offset `2` or `3` can help, but is optional and never a validity rule. Cover any earlier interval with explicit neutral/hold spans. Exact addresses use run/window/window-frame identity and reject passed addresses; they never silently shift to another frame. App ticks and window input frames are different axes.

Resolve a target from current revisioned geometry, then prepare a hover frame before down. Clipping and layout-to-window transforms matter; screenshots use separate framebuffer coordinates. A raw Clay target needs no Flow wrapper. Ambiguous, stale or hidden geometry fails rather than selecting the first match. During a held gesture, its declared pointer path does not follow a moving target implicitly.

Delivery, completed layout, observed hover/press/hold/release, assertion and application completion are separate facts. Callbacks can consume the previous completed interaction snapshot on the next frame. Popup suppression remains normal behavior. Primary-button membership does not prove exclusive routing; unsupported button/focus assertions remain explicit limitations.

`interaction start` returns a job; poll `job status` and `interaction query`, or cancel by exact instance/job ID. Cleanup neutralizes synthetic held state and quarantines physically held keys/buttons until release. Cancellation cannot undo an already delivered callback. Recording observes effective boundary input, not lost native events. Text is omitted unless explicitly requested; redacted payloads are non-replayable, and submitted text requires a persistence policy.

Plan `content.persistence` controls text retention, while `volatile` omits the stored source-plan payload. A retained plan preserves authored target selector literals for replay. Copied live target labels and application outcomes are separate capture content: `include_interaction_labels` and `include_interaction_outcomes` default false. Explicitly request them only when evidence needs their contents; execution success must remain distinguishable from unavailable/redacted outcome payloads.

Application/scenario callbacks can start a child with `start_interaction` and poll the owned `query_interaction` result on later safe points. Execution completion is independent of the parent capture's sealed/publication phase. Setup and declared verification/cleanup children use normal frames outside measurement; no callback recursively advances the App. Registered action completion adapters and scenario predicates supply application-specific readiness beyond the built-in interaction facts.

Source: [interaction DTOs and compiler](include/devSystems/devAgenticWorkflow/AgenticInteraction.hpp).

Relative workload digests exclude run binding and retry identity so equivalent replays can be compared across runs. Exact-address plans retain their run scope. Artifact SHA-256 covers the complete stored bytes and is distinct from the semantic workload digest. Recording hashes original input text even when the retained artifact redacts that text.
