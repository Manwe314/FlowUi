---
topic_id: changes.review_and_baking
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: tooling
source_selectors: ["changes.review_and_baking.primary"]
---

# Revision-checked review and baking

Temporary overrides support bounded review. Durable baking uses `.flowui/changes/active.flowchanges`, the generator and rebuild. Agentic evidence lives separately under `.flowui/agentic/v1`. Capture manifests describe observations and are never accepted as change instructions.

A review names typed targets and constraints with expected schema/theme/override revisions. Applying it requires the selected live run and a unique operation identity. Idempotent retries return the original result; reusing a key with different arguments fails. All application mutations occur at a safe point.

Source editing remains the coding agent's workspace operation. A dirty digest identifies a patch without reconstructing it; reproduction needs the actual revision, explicitly included diff and resource/fixture inputs. Library and application repositories have distinct source identities.

Use `changes preview`, inspect the exact review and then `changes apply`. Rebuild, replay matching scenarios and collect fresh captures. Evidence store pruning preserves baselines/references and never deletes baked changes.

Source: [FlowUi::devSystems::tooling::DevBakePipeline](include/devSystems/devTooling/bake/DevBakePipeline.hpp).
