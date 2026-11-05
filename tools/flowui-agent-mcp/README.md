# FlowUi Agentic MCP adapter

This optional package maps MCP stdio tools to `flowui-agent` structured JSON commands. The C++ registry owns collection, artifact validation, correlation and objective formulas. FlowUi applications have no Python dependency.

Install the selected Python environment and the package, with `flowui-agent` on `PATH`:

```sh
python3 -m pip install -r tools/flowui-agent-mcp/requirements.lock
python3 -m pip install --no-deps tools/flowui-agent-mcp
flowui-agent setup --client codex --print --json
```

The generated project entry selects the workspace explicitly. Register `flowui-agent-mcp --workspace <project>` with the client's stdio MCP manager. `--cli` can select an installed CLI executable. Normal setup prints configuration and never changes client settings. App attachment also requires explicit runtime configuration and an exact `instance` run ID. Codex configuration uses project `.codex/config.toml` and the `mcp_servers.flowui` table with command/args, following the [official OpenAI documentation](https://learn.chatgpt.com/docs/extend/mcp).

Use one FlowUi `App` per process; multiple windows share that App's run. Independent app instances run in separate processes. See [runtime activation](../../docs/agentic-workflow.md) for explicit configuration and launch handoff precedence. The feature remains dormant by default, and the application must enable visual resource support before construction to offer screenshots.

The adapter uses the [official Python SDK low-level server](https://py.sdk.modelcontextprotocol.io/advanced/low-level-server/) and [stdio transport](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports). SDK versions are pinned separately from the FlowUi command and artifact schema versions. It validates typed tool arguments and CLI result envelopes, bounds subprocess stdout/stderr, and returns the same envelope as text and structured content. Objective `verdict: fail` remains a normal tool result. Execution failures set `isError`.

`flowui_changes_apply` is separate from preview/read tools. Store deletion and unrestricted process launch are CLI administration operations and are absent from MCP discovery. Image retrieval returns at most 4 MiB of PNG content; metadata-only clients can use `include_image: false` and retain the original artifact locator. Temporary derivative paths do not become general file-reading APIs.

`flowui_interaction` maps capabilities, revisioned targets, offline planning, execution, recording and evidence queries to the same typed CLI registry. `flowui_action` lists explicitly exported application actions or invokes one with its exact revision and typed application arguments. Their execution operations are mutations, including clicks that appear to navigate. The App must separately enable `input_control_enabled` and/or `action_control_enabled`; collection or attachment alone grants neither permission. No tool accepts executable scripts, foreign callbacks or arbitrary artifact paths.

Submit a finite source `plan`, or select a retained plan with `plan_artifact: {capture, artifact, sha256}`. Stored plans pass current integrity, redaction and compiler checks before execution; redacted text cannot be reconstructed. Start calls require an idempotency key and return job identities. Use `flowui_job` for recovery/cancellation and `interaction.query` for retained `plan`, `inputs`, `observations` and `results`. A successful dispatch, an observed interaction, a completed application action and a complete evidence capture are separate outcomes.

To replay a relative plan in another run, explicitly set `rebind_run: true`. The CLI verifies the selected immutable source and forwards its capture/artifact/checksum/manifest provenance; the service binds the relative input clock to the current run. Exact window-frame addresses cannot be rebound. Source provenance remains distinct from the new execution's frame identities. Target discovery supports a bounded first page or one explicit target selector; it does not advertise a cursor.

Plan `content.persistence` selects retained text or redacted text; `volatile` omits the stored source-plan payload. A retained plan preserves its authored target selector literals so it can be resolved for replay. Copied live labels and application outcome payloads separately require optional capture `content.include_interaction_labels` and `content.include_interaction_outcomes`; both default false. Recording text additionally requires `include_text: true`.

Relative input offset `0` is valid after the declared baseline. Startup frames 1–2 may be unrepresentative; optionally begin a gesture at offset `2` or `3` with explicit earlier neutral spans. This is guidance, not an enforced minimum. Coordinates are window-logical input units, not screenshot pixels. The ordinary App loop controls frame progress.

An interrupted tool call with an exact instance/job identity hands cancellation to the same `job.cancel` command under a two-second bound. The service retires submitted work normally. If a capture-start response has not delivered a job ID, the adapter cannot select an unknown job: retry the same idempotency key to recover its identity, then poll or cancel explicitly.

`flowui_verification_plan` validates a versioned `FlowUi.Agentic.VerificationRequest` in `arguments.request`, resolves its registered scenarios or embedded finite interaction plans, and estimates its evidence budget. `flowui_verification_run` admits the sequential suite using a required execution idempotency key. Requests can select `instance` inside the document or in the surrounding arguments; conflicting selections are rejected. The equivalent CLI accepts `verification run --request-file suite.json` or `--request JSON`.

Run waits are bounded to twenty seconds. An unfinished suite returns its accepted job identity so `flowui_verification_query` can wait for progress with `verification.status`, `after_sequence` and `wait_timeout_ms`, or retrieve `verification.report` with bounded pagination. Status carries counters and identity rather than image payloads. Both CLI IPC and adapter waits reserve five extra seconds for transport, even when the adapter's general timeout is shorter. Configure the MCP client's timeout above twenty-five seconds for the maximum wait.

The report separates execution success from verification verdict: a completed command may report `fail` or `inconclusive`. `require_pass: true` maps a terminal `fail` verdict to exit code 7 and `inconclusive` to exit code 6 while retaining the report. Image checkpoints return evidence selectors for the existing artifact tool. Contact sheets are reported as unavailable when the service cannot provide them. Assertions-only requests collect no image readback.

Disconnecting or interrupting a verification call preserves admitted execution. Retry the same suite/key to recover admission; use `flowui_verification_cancel` explicitly to stop new steps, release input and attempt fixture cleanup. Run and cancel are separate mutating tools, while plan and query are read-only. Store pruning remains a CLI administration operation until its generation-checked maintenance contract is implemented.

Run boundary tests without a GPU:

```sh
python3 -m unittest discover -s tools/flowui-agent-mcp/tests -v
FLOWUI_AGENT_CLI="$PWD/build/flowui-agent" \
  python3 -m unittest discover -s tools/flowui-agent-mcp/tests -v
```

The SDK-dependent tests report skips when the optional package is absent. Those skips are not evidence of protocol execution. Client-specific IDE behavior and Windows transport must be tested in those environments before being advertised as verified.
