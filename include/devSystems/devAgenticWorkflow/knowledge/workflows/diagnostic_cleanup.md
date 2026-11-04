---
topic_id: workflow.diagnostic_cleanup
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: agentic
source_selectors: ["agentic.evidence_contract.primary"]
---

# Resolve build and runtime diagnostics with replay

1. Identify producer domains: compiler/test, FlowUI runtime, Vulkan validation and native logs. Ingest build evidence separately; never assign it runtime occurrence IDs.
2. Group runtime code/site/cause chains without counting propagated steps as new root failures. Preserve confidence, loss, retained coverage and source context.
3. Retrieve the relevant semantic contract and implicated source only. Edit the cause, rebuild and run focused tests; preserve warning flags and enabled diagnostic producers.
4. Replay the same fixture/scenario and collect fresh diagnostics. Diff resolved, still observed, newly observed and not re-exercised groups.
5. Stop when the relevant build/test checks pass and targeted issues are absent with adequate exercised coverage. An unexercised path remains unknown; no retained issues cannot establish every application path is clean.
