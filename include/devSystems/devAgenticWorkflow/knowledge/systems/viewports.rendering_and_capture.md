---
topic_id: viewports.rendering_and_capture
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: renderer
source_selectors: ["viewports.rendering_and_capture.primary"]
---

# Viewport production and visual evidence

Window readback and offscreen viewport readback share the actual App frame context. Record before submission, attach submission/device/queue only after queue submission succeeds, retire after the fence completes. A canceled or unsubmitted command buffer yields no completed image.

Mapped GPU buffers require noncoherent invalidation where appropriate. Convert/copy to bounded CPU-owned RGB bytes, then enqueue writer work; GPU allocator/device destruction stays with the renderer owner. Capture requires transfer-source capability and supported formats, with layouts restored.

PPM preserves simple RGB source bytes. PNG is a portable derivative. Alpha discard, half-float clamping, color space and unknown transforms remain declared limitations; encoding a PNG cannot recover discarded fidelity. Crops use physical coordinates and retain the transform to the original image.

Frame slots are reusable resources, not image identity. Prior viewport-image revisions sampled by a window need an explicit producer-to-consumer relation. `artifact image` exports an inventoried image/crop; visual comparison supplies pixel evidence without replacing product review.

Source: [FlowUi::ViewPortManager](include/managers/ViewPortManager.hpp).
