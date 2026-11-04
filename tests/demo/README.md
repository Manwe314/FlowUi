# FSEL Immediate Gallery

This executable is an interactive gallery for the current Flow Standard Element
Library primitives. It uses the public `createElement()` surface, so each card
also serves as a concrete usage example.

Build and run from the repository root:

```sh
cmake --build build --target flowui_fsel_demo -j4
./build/tests/flowui_fsel_demo
```

The target is available when both `FLOWUI_BUILD_TESTS` and `COMPILE_FSELI` are
enabled. For the agentic controls, configure with
`-DFLOW_UI_DEV_MODE=ON -DAGENTIC_WORKFLOW=ON -DFLOW_UI_DEV_CAPTURE_CLAY=ON`, and
build `flowui-agent` alongside the demo.

## Start an explicitly controlled demo

```sh
./build/tests/flowui_fsel_demo --agentic-store .flowui/agentic/v1 \
  --agentic-visual --agentic-input --agentic-actions
```

Run the following commands from another terminal while the ordinary demo loop
continues. Discover the run, then set `RUN_ID` to that exact returned `run_id`:

```sh
./build/flowui-agent instances --store .flowui/agentic/v1 --json
./build/flowui-agent interaction capabilities --instance "$RUN_ID" --json
./build/flowui-agent interaction targets --instance "$RUN_ID" --window-id 1 --limit 100 --json
./build/flowui-agent action list --instance "$RUN_ID" --json
```

`--agentic-store` enables local attachment. Each additional flag opts into a
separate capability:

| Flag | Effect |
| --- | --- |
| `--agentic-visual` | Reserve transfer-source support before window creation |
| `--agentic-input` | Permit frame-bound replacement/recording; publish owned aliases for actual controls |
| `--agentic-actions` | Permit the two explicitly exported application actions below |
| `--agentic-changes PATH` | Use an explicit runtime changes manifest, useful for isolated preview/apply verification |

Without an explicit store or launch handoff, the service stays dormant. Omit
visual/input/action flags for collection without those capabilities. Input
control uses synthetic effective focus and pointer state inside FlowUi; it does
not move the desktop cursor or claim OS foreground focus.

## Target an actual control

[AgenticInteractions.hpp](AgenticInteractions.hpp) publishes these aliases from
the latest completed Flow/Clay layout. Hidden, clipped-away, missing or ambiguous controls
have no alias. Aliases match the authored scoped path and definition identity;
an optional alias cannot stop the application when a layout changes. Geometry/identity changes advance the alias revision. The
observation adapter reads the real primary hover/press/hold/release lists.

| Custom target | Actual demo control |
| --- | --- |
| `demo.navigate` | Header navigation between Gallery and Writing Studio |
| `demo.reset_control` | Header Reset demo button |
| `demo.primary_button` | Gallery Built button, which increments the activation counter |
| `demo.quick_note` | Gallery quick-note text field |
| `demo.editor_body` | Writing Studio document text area |

Raw Clay ID/name and Flow instance/debug-name selectors also work directly;
these aliases are an application example, not a required wrapper or widget
catalogue. Discover the current IDs/revisions before selecting a target.

The demo starts in Gallery. Compile and execute the supplied finite click plan:

```sh
./build/flowui-agent interaction plan --plan-file tests/demo/agentic-click.json --json
./build/flowui-agent interaction start --instance "$RUN_ID" \
  --plan-file tests/demo/agentic-click.json --idempotency-key demo-gallery-click-1 --json
```

Set `JOB_ID` to the accepted `job_id`, then inspect execution and publication:

```sh
./build/flowui-agent job status --instance "$RUN_ID" --job "$JOB_ID" --json
./build/flowui-agent interaction query --instance "$RUN_ID" --job "$JOB_ID" --json
./build/flowui-agent interaction query --instance "$RUN_ID" --job "$JOB_ID" --kind observations --json
```

The recipe has one preparatory up frame, one down frame, two held frames and one
up frame. Its four target assertions use observed membership. Callback outcomes
can appear during later construction; the demo adapter additionally copies the
actual activation/document-change counters. Copied labels and application outcomes
are redacted by default; an explicit capture content policy can retain them.
Overlapping/ancestor memberships do
not prove exclusive event consumption. The ordinary popup/input-field policies
still apply. Use a fresh idempotency key for a new gesture; reuse the same key
and request to recover an uncertain start.

## Invoke declared application actions

These actions operate on the real demo model and theme manager. They are a
separate path from pointer/key interaction.

| Export, revision `1` | Contract |
| --- | --- |
| `demo.reset` | No arguments. Dispatch a bound `AppAction` that resets `DemoState`, matching Reset demo. Completion proves dispatch; it does not assert presentation. |
| `demo.configure` | Typed object with one or more of `page` (`gallery`/`writing`), `theme` (`agentic-default`/`agentic-ocean`), `editor_soft_wrap` (boolean). Reject unknown fields/types before mutation. Completion waits for a subsequent completed layout, then reports observed page/theme/wrap and its exact window frame. |

```sh
./build/flowui-agent action invoke --instance "$RUN_ID" --action-id demo.configure \
  --revision 1 --arguments '{"page":"writing","theme":"agentic-ocean","editor_soft_wrap":false}' \
  --capture '{"duration_ns":"10000000000","sections":{"performance":"omit","runtime_diagnostics":"omit"},"content":{"include_interaction_outcomes":true}}' \
  --idempotency-key demo-writing-ocean-1 --json
./build/flowui-agent action invoke --instance "$RUN_ID" --action-id demo.reset \
  --revision 1 --arguments '{}' --idempotency-key demo-reset-1 --json
```

Wait for the first action job to finish before invoking the second: capture,
interaction, scenario and exported-action work share an execution lease. Poll
`job status` and `interaction query` using each returned job ID. A cancellation
cannot roll back model changes already dispatched. Existing catalogue entries
and bound UI recipes are not automatically exported.

For a bounded manual recording, start with an explicit frame limit, then stop
using its returned job ID:

```sh
./build/flowui-agent interaction record start --instance "$RUN_ID" \
  --window-id 1 --max-frames 240 --deadline-ns 10000000000 \
  --idempotency-key demo-record-1 --json
./build/flowui-agent interaction record stop --instance "$RUN_ID" --job "$JOB_ID" --json
```

Default recording omits text payloads and labels the resulting replay limitation.
Set plan `content.persistence` explicitly when submitting text, and add
`--include-text` when a recording should retain its effective text payloads.
`text.enter` follows normal Unicode text-focus routing; it does not model
OS IME composition or system clipboard paste.

## Reproduce fixed workloads

The [application scenarios](AgenticScenarios.hpp) register:

| Scenario | Actual demo workload |
| --- | --- |
| `demo.gallery.steady` | Default Gallery controls, fixed initial values and seed 17 |
| `demo.writing.large_document` | Writing Studio with 128 deterministic lines, retained document state and wrapping |
| `demo.theme_matrix` | Gallery/default, Writing/default, Gallery/ocean and Writing/ocean, cycling once per measured frame |

Setup waits for the real root element state, then saves and installs the fixture.
Assertions verify state and minimum step count; cleanup restores the previous
page, document, parameters and active theme. These scenarios perform application
state operations at safe points. Interaction assertions remain separate.

Use `visual_inspect`, at least four measured frames and at least four image slots
for `demo.theme_matrix`. Use the two fixed workloads for repeated performance
passes with matching scenario/fixture digests and build profiles. Theme variants
use reflected `FSELTheme` fields and do not save user preferences.

The supplied click plan includes an optional three-frame neutral settling step and generous fixture deadlines. Offset `0` remains valid; the settling step is an example choice for initial layout/resource work, not an execution requirement.

## Verify every MCP tool against the demo

The [live MCP suite](../AgenticWorkflowTests/AgenticLiveMcpTests.py) launches the demo and the adapter as separate processes, then calls every discovered tool operation through the official SDK's stdio client. It uses a private workspace, evidence store and changes manifest. Tool discovery is checked against the executed coverage, so an added operation requires a corresponding test.

Install the pinned optional dependencies in the Python environment used by CMake, then enable the integration test:

```sh
python3 -m pip install -r tools/flowui-agent-mcp/requirements.lock
cmake -S . -B Build -DAGENTIC_WORKFLOW=ON -DFLOW_UI_DEV_MODE=ON \
  -DFLOW_UI_DEV_CAPTURE_CLAY=ON -DFLOW_UI_ENABLE_BAKED_CHANGES=OFF \
  -DFLOWUI_AGENTIC_MCP_LIVE_TESTS=ON
cmake --build Build --target flowui-agent flowui_fsel_demo -j 2
ctest --test-dir Build -R '^flowui\.agentic\.live_mcp$' --output-on-failure
```

This requires a working desktop and Vulkan device. The optional test fails when its dependencies or capabilities are unavailable. Its coverage report distinguishes successful tool execution, expected validation failures, and evidence verdicts such as a performance objective being inconclusive with Vulkan validation enabled.
