---
topic_id: layout.flow_and_clay
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: tooling
source_selectors: ["layout.flow_and_clay.primary"]
---

# Flow ownership and Clay layout

Flow nodes identify authored element instances. Clay nodes represent layout implementation details; one Flow element can own multiple Clay nodes and should own at least one. A raw Clay node is not necessarily an independently configurable Flow component. Tree records carry parent/child indices and ownership, bounds, clips and generation.

Geometry becomes meaningful after layout. Bounds, clipping and floating placement must be interpreted in the owning window frame, logical/physical scale and viewport transform. Published tree buffers are reused; asynchronous exporters copy selected generations into owned records. No live Clay pointer or lease may cross into writer I/O.

An image/tree join requires the producer-supplied run/window/frame and tree generation. A UI runtime ordinal is a separate observation. A screenshot alone cannot prove keyboard focus, navigation, hover or persistence. Pair appearance evidence with scenario assertions.

Inspect the selected subtree and its owning source site before reading an entire UI module. `query visual --kind frames` lists image keys; `artifact read` retrieves a selected inventoried tree.

Source: [FlowUi::devSystems::tooling::DevTreeSnapshot](include/devSystems/devTooling/tree/DevTreeTypes.hpp).
