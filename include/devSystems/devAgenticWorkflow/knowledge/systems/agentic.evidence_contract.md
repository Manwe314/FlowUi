---
topic_id: agentic.evidence_contract
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: agentic
source_selectors: ["agentic.evidence_contract.primary"]
---

# Agentic evidence, coverage and command contracts

Begin with capabilities and an exact run/window/scenario. The service is dormant until explicitly configured. Collection advances at normal App safe points and publication runs from owned snapshots. The CLI and MCP adapter use one typed command registry; MCP is an external process, not the App endpoint.

A sealed manifest is the inventory authority. Verify schema, safe relative artifact paths, byte counts and SHA-256 before querying. The rebuildable index cannot invalidate sealed evidence. Staging files are incomplete. Only requested artifacts exist, and every section states complete, partial, not requested, disabled, unsupported or failed coverage.

All new uint64 identities, byte counts, timestamps and nanosecond durations are decimal JSON strings. Exact joins use run/window/frame/submission/occurrence identity. Temporal proximity is a candidate, not causation. Unknown or evicted targets remain unresolved with their selectors.

Use capture plan before capture start, then job status. Archive queries read existing artifacts, with bounded cursors tied to manifest/query/schema digests. Live job and interaction queries report bounded current state without manufacturing frames. Objective failure is a valid analysis result; incomplete coverage is inconclusive. Performance, visual and interaction verification use reproducible corresponding passes.

Source: [FlowUi::devSystems::agentic::AgenticWorkflowService](include/devSystems/devAgenticWorkflow/AgenticService.hpp).
