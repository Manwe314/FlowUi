---
topic_id: themes.variants_and_overrides
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: tooling
source_selectors: ["themes.variants_and_overrides.primary"]
---

# Typed theme variants and durable changes

ThemeManager owns typed theme schemas, named variants, active selection and staged mutations. Reflected fields carry types and constraints. Authored values, effective values with overrides, and temporary preview values are distinct observations. Export schema identity and exact revisions with all values.

A configurable application theme reuses these contracts. Persisting the user's chosen variant across restart is application settings work; a preview is not persistence. New fields or structural UI changes require source editing when no reflected field exists. Existing runtime overrides continue to apply while optional inspection copies are disabled.

Preview and apply select a typed review transaction, exact target run and expected schema/theme/override revisions. Stale revisions reject the operation. Baking writes the distinct `.flowchanges` contract; a capture manifest is never executable change input.

Use `query visual --kind themes`, `changes preview`, `changes apply` and matching state captures. Record light/dark, narrow/wide, selected/hover/focus and empty/loading/error states supported by the app. Appearance and interaction acceptance remain separate.

Theme queries paginate individual reflected fields rather than returning an entire multi-megabyte theme export. Each row keeps the field's `path`, identity, constraints and authored/effective/startup values together with its variant identity and revisions. Use a small `limit`, follow the returned cursor with the same filters, or select exact `variant` and `path` values in `filter`. The inventoried Themes artifact retains the complete nested variant/field document. Built-in input overlay styles are serialized as their actual caret, color and blink settings even though generic editing treats that type as opaque.

Source: [FlowUi::ThemeManager](include/managers/ThemeManager.hpp).
