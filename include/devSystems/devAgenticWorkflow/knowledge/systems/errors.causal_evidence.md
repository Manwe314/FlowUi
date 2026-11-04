---
topic_id: errors.causal_evidence
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: monitoring
source_selectors: ["errors.causal_evidence.primary"]
---

# Runtime occurrences and causal records

Compiler diagnostics, runtime FlowUI errors, Vulkan/native logs and test failures are separate producer domains. A runtime occurrence contains causal steps; propagated steps are not independent root failures. Group by code/site and cause chain with build-aware source fingerprints.

Occurrences retain tick/window/frame/submission context and timing/memory relations with state and confidence. Exported target resolution can be outside capture, evicted, pending or disabled even when the native relation was available. Preserve the selector and confidence instead of fabricating a match.

No retained occurrences is a bounded observation. Claiming a targeted issue is absent requires enabled producers, retained coverage and the same exercised scenario. A path not replayed is not re-exercised, not fixed. Late steps and occurrences beginning before the interval remain labeled context when they overlap it.

Use `query diagnostics --domain runtime`, `ingest build` and the diagnostic cleanup recipe. Fatal capsules and launch logs can survive some failures; arbitrary corruption or abrupt termination cannot guarantee a sealed manifest.

Source: [FlowUi::devSystems::DevErrorContext](include/devSystems/devMonitoringAndReporting/errors/DevErrorTypes.hpp).
