---
topic_id: workflow.memory_growth
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: agentic
source_selectors: ["agentic.evidence_contract.primary"]
---

# Investigate retained growth and capacity

1. Declare fixture, warmup, repeat count and steady-state/cold-start boundaries. Record resource/input identity and source units.
2. Capture bounded memory samples/events with epochs, interval weighting and cutoff coverage. Separate live logical bytes from backing/reusable/retired storage and observer allocations.
3. Compare the same repeated scenario before/after drain points. Identify persistent growth, churn and delayed GPU retirement separately.
4. Change source/capacity only where measured evidence supports it. Review through existing capacity/baking contracts and preserve workload correctness.
5. Replay the high-water workload, validate coverage and compare weighted source metrics. Stop when the explicit growth/capacity objective passes without eviction/reset ambiguity or correctness regression.
