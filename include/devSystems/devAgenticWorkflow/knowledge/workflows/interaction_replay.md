---
topic_id: workflow.interaction_replay
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: ["interaction"]
owning_subsystem: devAgenticWorkflow
source_selectors: ["interaction.execution.primary"]
---

# Exercise an interaction and inspect its evidence

Discover the selected App's input-control capability and available target/observation domains. Resolve one unambiguous window/Clay/Flow/custom target and record its revision. Use coordinates only with an explicit window-logical policy. Exported actions offer a separate application-level path; invoke them only when direct action dispatch is the behavior being tested.

Compile the finite plan with `flowui-agent interaction plan --plan-file plan.json --json`. Offset `0` is valid after the reported baseline. Early startup frames 1–2 can be unrepresentative; optionally cover them with neutral input and begin the gesture at offset `2` or `3`. This is a recipe choice, not a runtime restriction.

Submit the same source plan with `interaction start --instance RUN --plan-file plan.json --idempotency-key CASE`. Optional capture configuration joins measurement/visual evidence to that job. Recover an uncertain start using the same idempotency key; do not submit a second mutation. Poll `job status --instance RUN --job JOB` and read `interaction query --instance RUN --job JOB`. Select the exact instance on every live command. Input is applied only while the application progresses its ordinary loop.

Inspect retained `plan`, `inputs`, `observations` and `results` with `interaction query --capture CAPTURE --kind KIND`. Compare scheduled/consumed/observed/asserted/retained counts independently. A delivered down/up pair without a completed observation is not a proven click; an observed click without an application completion receipt is not a proven save/load. Keep performance acceptance separate from interaction assertions and evidence completeness.

Replay retained source with `interaction start --plan-artifact '{"capture":"CAPTURE","artifact":"interaction_plan","sha256":"HEX_DIGEST"}'` and select the live instance/idempotency key. For a different run, add `--rebind-run` explicitly. Only relative input-frame schedules can be rebound; exact run/window/frame addresses retain their original scope. The command backend verifies the inventoried checksum and preserves source provenance before current runtime validation. Redacted text is not replayable.

Cancel with `job cancel` when needed and inspect cleanup/handoff status. To reproduce manually assessed input, start a bounded recorder with an explicit window, maximum frames and deadline, then stop it by job ID. Default text omission is a stated replay limitation, not an empty string invented as original input.

Source: [shared command registry](include/devSystems/devAgenticWorkflow/AgenticCommands.hpp), [interaction contracts](include/devSystems/devAgenticWorkflow/AgenticInteraction.hpp).
