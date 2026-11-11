# Agentic verification suites

Keep runtime evidence, screenshots, logs, coverage reports and verification matrices
(including Markdown reports) under `.flowui/agentic/`. The `docs/` tree is reserved
for authored Markdown documentation and generated Doxygen output. Use
`.flowui/agentic/verification/<suite>/` for harness exports and
`.flowui/agentic/v1/` for the versioned runtime store.

Verification groups registered scenarios, finite pointer/keyboard plans, assertions,
and selected screenshots into one sequential job. CLI, authenticated local IPC,
and MCP use the same five operations. See the
[request schema](../include/devSystems/devAgenticWorkflow/schemas/v1/VerificationRequest.schema.json),
[report schema](../include/devSystems/devAgenticWorkflow/schemas/v1/VerificationReport.schema.json),
and [runtime setup](agentic-workflow.md).

| Operation | MCP tool | Behavior |
| --- | --- | --- |
| `verification.plan` | `flowui_verification_plan` | Validate and resolve workloads against a live instance without taking control. |
| `verification.run` | `flowui_verification_run` | Admit one suite and optionally await its report. Mutates application input/state. |
| `verification.status` | `flowui_verification_query` | Read counters and phase; optionally wait for a sequence change. |
| `verification.report` | `flowui_verification_query` | Read a terminal report, including after the application exits. |
| `verification.cancel` | `flowui_verification_cancel` | Request input release and bounded fixture cleanup. |

Enable `FLOW_UI_DEV_MODE` and `AGENTIC_WORKFLOW` when building. The application
must enable the workflow and local attachment; interaction cases additionally
require `input_control_enabled`, and image checkpoints require
`visual_capture_enabled`. Initial suite execution supports windowed workloads.
Independent application processes can execute independently; one instance has
one control owner.

Save a request such as the following as `suite.json`. These scenario and custom
target identities belong to the FSEL demo; applications register their own.

```json
{
  "schema": "FlowUi.Agentic.VerificationRequest",
  "schema_version": "1.0",
  "suite_id": "fsel.smoke",
  "suite_revision": "1",
  "idempotency_key": "fsel-smoke-attempt-001",
  "wait_timeout_ms": 20000,
  "execution_timeout_ns": "30000000000",
  "failure_policy": "stop_suite",
  "cases": [
    {
      "case_id": "gallery",
      "workload": {
        "kind": "registered_scenario",
        "scenario_id": "demo.gallery.steady",
        "revision": "1",
        "fixture_id": "gallery-default-v1",
        "seed": "17"
      },
      "checkpoints": [
        {
          "checkpoint_id": "gallery-visible",
          "trigger": "case_verified",
          "evidence": "image_and_tree"
        }
      ]
    },
    {
      "case_id": "gallery-button",
      "workload": {
        "kind": "interaction_plan",
        "plan": {
          "schema": "FlowUi.Agentic.InteractionPlan",
          "schema_version": "1.0",
          "mode": "common_controls",
          "window_id": "1",
          "steps": [
            {
              "operation": "click",
              "target": {"custom_id": "demo.primary_button"},
              "expect": ["target_hovered", "target_pressed", "target_released"]
            }
          ]
        }
      }
    }
  ],
  "evidence": {"policy": "checkpoints_and_failure"},
  "limits": {
    "maximum_cases": 16,
    "maximum_images": "4",
    "maximum_snapshot_bytes": "33554432",
    "maximum_retained_bytes": "67108864",
    "maximum_report_bytes": "65536"
  }
}
```

Select the exact run returned by `instances`. Both `--request-file PATH` and
`--request JSON` accept suite documents. The request may instead carry `instance`;
a conflicting surrounding selection is rejected.

```sh
flowui-agent instances --json
flowui-agent verification plan --instance RUN_ID --request-file suite.json --json
flowui-agent verification run --instance RUN_ID --request-file suite.json --require-pass --json
flowui-agent verification status --instance RUN_ID --job JOB_ID --after-sequence 1 --wait-timeout-ms 20000 --json
flowui-agent verification report --instance RUN_ID --job JOB_ID --limit 1 --json
flowui-agent verification cancel --instance RUN_ID --job JOB_ID --json
```

Add `--store PATH` when using a store outside the workspace default. MCP sends
`{"operation":"verification.run","arguments":{"instance":"RUN_ID","request":{...}}}`
to the corresponding tool. No suite accepts shell commands, executable paths,
embedded scripts, or unrestricted filesystem readers.

Run waits default to twenty seconds and may be reduced to zero. A wait timeout
returns `operation_status: accepted` and the same job identity; execution
continues. Status accepts an unsigned decimal-string `after_sequence` and an
integer `wait_timeout_ms` from zero through 20000. Status contains progress
rather than image or tree payloads. CLI IPC and adapter subprocess timeouts allow
a five-second transport margin; configure the MCP client above twenty-five
seconds for maximum waits. Disconnect does not cancel verification. Use the
cancel operation explicitly.

A successful command can contain a failing verification verdict. With
`--require-pass` or `require_pass: true`, terminal `fail` returns exit code 7,
`inconclusive` returns 6, and `pass` returns 0. Accepted jobs still return 0.
Required assertion mismatches fail; missing required observations/checkpoints,
interrupted execution, or unsuccessful cleanup prevent a pass. Delivered clicks
alone do not establish passing coverage. Failed cleanup marks the instance dirty
and blocks further suite admission until restart.

Cases reuse existing scenario setup/step/verify/cleanup callbacks or the existing
interaction-plan language. Scenario revisions, fixture identities, and optional
digests are checked again at admission. `stop_suite` stops after a failed case;
`stop_case` and `continue_cases` advance only when cleanup establishes a usable
fixture. Checkpoints support `case_verified` and `case_failed` with `image`,
`tree`, or `image_and_tree`; they are required by default. Failure-triggered
pixels are labeled as a later eligible frame, and historical failure pixels are
unavailable. Read originals through the report's capture/image/artifact selectors
using the existing artifact tools.

Scenario seeds execute exactly as normalized, including zero. Use the discovered
fixture seed when its setup requires a particular value.

Use `evidence.policy: assertions_only` and omit checkpoints for frequent behavior
checks without image readback. Visual suites use `checkpoints_and_failure` and
explicit checkpoints. Content flags default false for tree text, stacks, native
messages, and interaction outcomes. A later request cannot reconstruct evidence
omitted during execution.

The suite has separate snapshot, image-count, retained-byte, and report budgets.
Cases publish sequentially and release heavy runtime payloads after publication.
The default limits shown above permit at most 16 cases; the hard case ceiling is
64. Live runtime reservations may impose lower image/snapshot ceilings. Report
pages accept `limit` from 1 through 64 and `max_bytes` from 4096 through 1048576.
Pagination preserves aggregate verdict and coverage and returns a cursor bound
to the canonical report digest; changed or mismatched cursors fail. Timing and
resource fields identify unavailable measurements with null values or explicit
limitations; owned bytes do not imply measured RSS or GPU allocations.
The request's `report.detail` selects `summary`, `failures` (default), or `all`
for the run response. Explicit report queries recover detailed canonical records
within the same pagination and byte limits.

Within one live instance, the same idempotency key and normalized execution digest
identify one admission. Different execution content conflicts. Wait duration and
report preferences do not change execution identity. A deliberate rerun needs a
new key. Compact admission receipts remain for 24 hours, with a hard capacity of
4096; full capacity rejects new admission. Runtime suite/report history retains
32 entries. Evicted runtime results return `result_expired` while their receipt
survives, retaining `job_id`, `run_id`, and a `next` report selector for recovery. These receipts are process-local and
do not promise deduplication after restart or outside the advertised horizon.

Reports persist under `verification/JOB_ID.json` inside the selected run directory.
The store resolves its timestamp-prefixed directory through `run.json` identity.
Reports remain readable with `verification report --instance RUN_ID --job JOB_ID`
after the endpoint disappears or runtime history expires. The reader validates
schema, job/run identity, store confinement, and response bounds. Durable reports
and capture artifacts are independent of runtime history eviction. Explicit prune
preserves report-referenced captures and captures tagged with suite ownership;
malformed retained reports block deletion rather than risking dangling evidence.

Publication maintains the advisory capture index by inserting the newly verified
manifest under the store lock. The incremental path limits index JSON to 16 MiB;
missing, malformed, or incompatible caches trigger authoritative repair. Unsafe,
ambiguous, or busy cache updates are deferred without failing the sealed capture.
Explicit index repair still scans manifests, and capture reads/listing continue
to resolve authoritative evidence independently of the cache.

Automatic disk retention, suite contact-sheet attachments, indexed capture
lookup, verification caching, and generation-checked `store.prune.plan` /
`store.prune.apply` remain future work. Existing explicit CLI `store.prune` is
separate administration. Requesting an optional contact sheet records a
limitation and preserves original image selectors. RAM bounds do not impose an
automatic total store-size ceiling.

The [live CLI suite](../tests/AgenticWorkflowTests/AgenticVerificationLiveTests.py)
exercises FSEL interactions, the developer Catalogue window, selected image/tree
evidence, idempotent recovery, cancellation, pagination, and repeated admission.
The [live MCP suite](../tests/AgenticWorkflowTests/AgenticLiveMcpTests.py) exercises
the official SDK and every advertised operation. Execute tests in an environment
with the required display/renderer and optional pinned MCP dependencies; their
source alone is not evidence that a run passed.
