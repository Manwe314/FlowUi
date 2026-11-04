---
topic_id: resources.textures_fonts_icons
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: resources
source_selectors: ["resources.textures_fonts_icons.primary"]
---

# Resource registration and frame-safe lifetime

Register resources with stable keys and declared ownership. Loading, baking and registration are different phases. Fonts influence layout; missing or changed font inputs can invalidate a geometry or pixel comparison. Record resource pack and fixture identities with captures.

Images sampled by an in-flight submission cannot be destroyed immediately after application replacement. Frame-safe retirement follows the owning device/fence lifecycle. A viewport texture can be produced in an earlier frame than the window that samples it; export that relationship explicitly.

Startup diagnostics retain initialization instead of hiding it behind warmup. Cold resource loading, pipeline preparation and steady-state rendering need separate profiles. Imported external logs retain producer identity and source labels.

Inspect source dictionaries and resource catalogue entries scoped to the affected component. Use the startup failure recipe when no window exists, and the viewport topic for image layouts and readback. Missing provenance is unknown, not inferred from the executable directory.

Source: [FlowUi::ResourceConfig](include/FlowUi/Resources.hpp).
