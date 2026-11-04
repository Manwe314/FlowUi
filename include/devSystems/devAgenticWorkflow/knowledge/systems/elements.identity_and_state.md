---
topic_id: elements.identity_and_state
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: tooling
source_selectors: ["elements.identity_and_state.primary"]
---

# Element definitions, instances and persistent state

A Flow element definition describes reusable behavior. An invocation identifies an instance whose state persists across frames. Parameters belong to the current build; per-instance state belongs to the stable instance key. Repeated rows need stable keys derived from their semantic records rather than transient array positions.

Resource registration, element identity and active overrides are required application behavior. Optional inspection snapshots are separate. Turning off tree/value capture for a performance verification pass must not turn off state progression or committed element/theme changes.

When a table repeats expensive preparation, first correlate timing scopes with definition and instance IDs. Large inclusive parent scopes include their children. An optimization that drops required rows changes the workload and cannot satisfy the same scenario. Preserve visible range, fixture count, selected item and interaction assertions when introducing virtualization.

Use `context read --topic layout.flow_and_clay` for ownership and `query visual` for retained geometry. Source selectors refer to the matching library revision; application-owned row builders require their own registered source sites.

Source: [FlowUi::FlowElementID](include/FlowUi/ElementID.hpp).
