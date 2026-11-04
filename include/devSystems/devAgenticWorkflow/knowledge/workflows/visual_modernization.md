---
topic_id: workflow.visual_modernization
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: agentic
source_selectors: ["agentic.evidence_contract.primary"]
---

# Modernize an application surface with typed themes

1. Resolve the surface through app semantics or named Flow nodes. Record fixture, current PNG, scoped tree, active theme, schema/revisions and source sites.
2. Inspect element/layout/theme contracts. Propose concrete hierarchy, spacing, typography, density and feedback states; visual quality requires review rather than a numeric manifest verdict.
3. Add application theme types/variants and a user-facing configuration path. Reuse ThemeManager staged updates. Persist settings across restart when required and assert persistence separately from preview.
4. Use revision-checked preview/apply for existing reflected fields; edit source for structural changes. Preserve the original fixture and interactions.
5. Capture matching light/dark, compact/wide, scroll/selection, hover/focus and supported empty/loading/error states. Compare image, geometry and token evidence; test keyboard/focus behavior and representative performance. Stop when the requested state matrix, behavior and visual review are covered.
