---
topic_id: workflow.startup_failure
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: agentic
source_selectors: ["agentic.evidence_contract.primary"]
---

# Investigate initialization and resource failures

1. Launch an explicitly selected executable with structured argv and working directory. Preserve bounded stdout/stderr by source. Do not concatenate shell commands.
2. Use startup_diagnose with no warmup exclusion. Inspect pre-window status, build/resource identity, fatal evidence and initialization diagnostics.
3. Resolve source/contract selectors for the earliest relevant cause. An unsealed staging directory is incomplete evidence after a crash.
4. Fix and rebuild the implicated source/resource configuration. Replay the same cold fixture and environment in the appropriate diagnostic build.
5. Stop when initialization and relevant tests succeed and the targeted error is absent on the replayed path. Runtime evidence cannot guarantee collection after arbitrary memory corruption.
