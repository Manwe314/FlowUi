# FlowUi DevOverlay — Implementation Design

## 1. Purpose and subsystem boundaries

This report specifies the design and implementation of **`DevOverlayService`**, the visual inspection and overlay engine of the **`DevTooling`** subsystem in FlowUi (referenced in `DevTooling.md` Section 12).

`DevSystems` is structured into three canonical developer subsystems:

1. **`DevMonitoringAndReporting`** — read-only telemetry, retained evidence, profilers, memory accounting, error tracking, and statistics *(Status: Closed)*.
2. **`DevTooling`** — actionable development functionality: validated edits, runtime intervention, tree capture, schema registry, overrides, baking, and visual overlays *(Status: Active)*.
3. **`DevInterface`** — the developer user interface (FSEL/tool window) through which a developer inspects data, selects elements, and triggers tools *(Status: Future)*.

### 1.1 The Core Requirement: A Renderer-Only Sidecar

Chrome DevTools, Figma, Xcode View Debugger, and Safari Web Inspector rely on non-intrusive visual overlays to render bounding boxes, padding regions, child gaps, rulers, typography baselines, and hit-test targets.

In FlowUi, **visual overlays MUST NOT be constructed as Clay elements in the application layout tree**. Doing so would:
- alter the application's Clay element counts, layout geometry, child indices, and scroll extents;
- pollute hit testing and input routing;
- distort measured frame timing, memory allocations, and render-command diagnostics.

Instead, `DevOverlayService` is a **Renderer-Only Sidecar Pass**. Clay handles application UI layout and emits standard UI render commands. `DevOverlayService` reads post-layout geometry from `DevTreeCapture` snapshots, generates lightweight overlay primitives (`UiInstance`), and injects them directly into the renderer pipeline to draw **after all application UI** with absolute top-level priority.

```text
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           DEVELOPER SELECTION & PICKING                         │
│                                                                                 │
│  DevInterface (or Input Controller) ──► DevOverlaySelectionSpec                 │
│                                          • Primary Target (Selected FlowNode)   │
│                                          • Secondary Target (Hover/Compare)     │
│                                          • Mode Flags (BoxModel, Rulers, etc.)  │
└──────────────────────────────────────────┬──────────────────────────────────────┘
                                           │
                                           ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                                   DEV TOOLING                                   │
│                                                                                 │
│   DevTreeCapture Snapshot ─────┐                                                │
│   (Post-layout geometry, clip, ├─► DevOverlayService                            │
│    padding, gaps, text bounds) │   • Resolves overlay geometry & colors         │
│                                │   • Generates DevOverlayCommandBuffer          │
│   DevSchemaRegistry / Overrides┘   • Converts to UiInstance primitives          │
└──────────────────────────────────────────┬──────────────────────────────────────┘
                                           │
                                           ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                                 RENDERER SIDECAR                                │
│                                                                                 │
│   Vk_UiRenderer Pass 1: Application UI Render Commands (Clay)                   │
│   Vk_UiRenderer Pass 2: Caret & Text Selection Highlights                       │
│   Vk_UiRenderer Pass 3: DevOverlay Sidecar Pass (Always Top Priority) ◄── HERE   │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Selection & target contract

`DevOverlayService` strictly separates **selection input** from **overlay command generation**:

- **`DevInterface` (or Picking Controller)** owns pointer interaction, element picking, tree hover states, and selection logic.
- **`DevOverlayService`** takes a read-only selection specification (`DevOverlaySelectionSpec`) and generates render commands. It does not perform mouse picking or state tracking.

### 2.1 Selection Specification Data Model

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <span>
#include <vector>

#include "FlowUi/ElementID.hpp"
#include "internal/ElementInstanceKey.hpp"

namespace FlowUi::devMode {

enum class DevOverlayModeFlags : std::uint32_t {
	None                 = 0,
	BoxModel             = 1u << 0u, // Content, Padding, Border, Parent Gap
	RulersAndDistance    = 1u << 1u, // Cross-hairs, distance vectors & labels
	TreeHierarchy        = 1u << 2u, // Parent/child bounds color-coded by depth/sibling
	Typography           = 1u << 3u, // Text baselines, cap-height, unwrapped extent
	ScissorAndClip       = 1u << 4u, // Scissor bounds, scroll content overflow
	RenderRunDiagnostics = 1u << 5u, // Render-run breaks, texture splits, overdraw
	Default              = BoxModel | RulersAndDistance,
};

inline constexpr DevOverlayModeFlags operator|(DevOverlayModeFlags a, DevOverlayModeFlags b) noexcept {
	return static_cast<DevOverlayModeFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline constexpr bool operator&(DevOverlayModeFlags a, DevOverlayModeFlags b) noexcept {
	return (static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)) != 0;
}

struct DevOverlayTargetSpec {
	std::uint32_t flowNodeIndex = UINT32_MAX;
	FlowDefinitionID definition{};
	detail::element::ElementInstanceKey instanceKey{};
	bool isValid() const noexcept { return flowNodeIndex != UINT32_MAX; }
};

struct DevOverlaySelectionSpec {
	DevOverlayTargetSpec primaryTarget{};   // Currently selected element
	DevOverlayTargetSpec secondaryTarget{}; // Currently hovered or distance-compared element
	DevOverlayModeFlags modeFlags = DevOverlayModeFlags::Default;
	float uiScaleFactor = 1.0f;
};

} // namespace FlowUi::devMode

#endif // FLOW_UI_DEV_MODE
```

---

## 3. Architecture & renderer sidecar pipeline

### 3.1 Harmonization with existing Caret & Selection Sidecars

FlowUi's Vulkan renderer (`Vk_UiRenderer`) already processes non-Clay render commands for text input fields (`InputFieldFrameOverrides`). Carets and text selection highlight boxes are inserted directly into `Vk_UiRenderer` instance buffers as rect primitives (`UiInstance`).

`DevOverlayService` uses the exact same `UiInstance` primitive infrastructure.

`Vk_UiRenderer` instance primitives support three draw modes (`UiType`):
1. **`UiType::Solid`**: Solid filled rectangles, stroked border rectangles, rounded corner rectangles, 1-pixel screen-aligned lines.
2. **`UiType::Msdf`**: High-resolution, DPI-independent text labels using the application's shared MSDF font catalog.
3. **`UiType::Textured`**: Texture quads for asset, image, icon, and font atlas previews.

### 3.2 Top-Level Draw Priority (Bypassing Scissor & Z-Index)

Application UI elements are subject to layout scissor rectangles (`Clay_ClipElementConfig`) and z-index sorting. 

Dev overlays **must ignore application UI clipping and z-index rules**. If an element is clipped or partially off-screen, the overlay (such as padding indicators or distance rulers) must remain visible over the entire window.

`DevOverlayService` achieves this by generating its runs with:
- a **Full-Window Scissor Rect** (`(0, 0, windowWidth, windowHeight)`), overriding application clip stacks;
- **Final Render Order**: Executed as the last `UiRun` batch in `Vk_UiRenderer`, drawing directly on top of the frame framebuffer.

### 3.3 Command Buffer Architecture

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string_view>
#include <vector>

#include "Ui/Vk_UiRenderer.hpp"

namespace FlowUi::devMode {

enum class DevOverlayPrimitiveKind : std::uint8_t {
	FilledRect,
	StrokedRect,
	Line,
	TextLabel,
	TextureQuad,
};

struct DevOverlayPrimitive {
	DevOverlayPrimitiveKind kind = DevOverlayPrimitiveKind::FilledRect;
	RectF bounds{};
	std::uint32_t colorRGBA = 0xFFFFFFFF;
	float cornerRadius[4]{0.0f, 0.0f, 0.0f, 0.0f};
	float borderWidth[4]{0.0f, 0.0f, 0.0f, 0.0f};
	std::string textLabel{};
	float textSizePoints = 11.0f;
	std::uint32_t textureIndex = 0;
};

struct DevOverlayCommandBuffer {
	std::vector<DevOverlayPrimitive> primitives{};
	std::vector<UiInstance> instances{};
	std::vector<UiRun> runs{};

	void clear() noexcept {
		primitives.clear();
		instances.clear();
		runs.clear();
	}
};

} // namespace FlowUi::devMode

#endif // FLOW_UI_DEV_MODE
```

---

## 4. DX Visual overlay modes & visual packaging

This section details the visual choices, DX benefits, and color palettes for each visual tool mode in `DevOverlayService`.

```text
┌───────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                   DevOverlay Visual Tool Modes                                        │
├───────────────────────────────────┬───────────────────────────────────┬───────────────────────────────┤
│ Mode 1: Box Model (Figma/Chrome)  │ Mode 2: Rulers & Distance         │ Mode 3: Tree Hierarchy        │
│ • Content Box (Blue fill 15%)     │ • Axis Guides (Red/Cyan 1px)      │ • Color-coded depth outlines  │
│ • Padding Box (Green fill 25%)    │ • Distance Vectors (Arrows)       │ • Sibling index palette       │
│ • Border Box (Orange stroke)      │ • Pixel Distance Text Badges      │ • Child container bounds      │
│ • Parent Gap (Purple hash pattern)│ • Viewport Edge Projections       │ • Parent-child connectors     │
├───────────────────────────────────┼───────────────────────────────────┼───────────────────────────────┤
│ Mode 4: Typography & Glyph        │ Mode 5: Clip & Viewport           │ Mode 6: Render-Run Overdraw   │
│ • Baseline Guide (Pink 1px)       │ • Scissor Boundary (Dashed Red)   │ • Pipeline split markers      │
│ • Cap-Height & Descender Lines    │ • Scroll Content Extents          │ • Texture atlas batch breaks  │
│ • Unwrapped Text Box vs Clipped   │ • Hidden Overflow Regions         │ • Overdraw heatmap overlay    │
└───────────────────────────────────┴───────────────────────────────────┴───────────────────────────────┘
```

### 4.1 Mode 1: Chrome / Figma Box Model Overlay

The Box Model visualizer breaks down the selected element's layout into four distinct geometric regions:

```text
 ┌─────────────────────────────────────────────────────────────┐
 │ Parent Gap / Margin Region (Purple Stripe: 0x9B51E040)      │
 │  ┌───────────────────────────────────────────────────────┐  │
 │  │ Border Box (Solid Orange Stroke: 0xF2994AEE, 1.5px)   │  │
 │  │  ┌─────────────────────────────────────────────────┐  │  │
 │  │  │ Padding Box (Semi-Translucent Green: 0x27AE6040)│  │  │
 │  │  │  ┌──────────────────────────────────────────┐  │  │  │
 │  │  │  │ Content Box (Cyan/Blue Tint: 0x2F80ED30) │  │  │  │
 │  │  │  │ W: 320 px  x  H: 180 px                │  │  │  │
 │  │  │  └──────────────────────────────────────────┘  │  │  │
 │  │  └─────────────────────────────────────────────────┘  │  │
 │  └───────────────────────────────────────────────────────┘  │
 └─────────────────────────────────────────────────────────────┘
```

#### Color Palette Standard (Figma & Industry Compatible)
- **Content Area**: `0x2F80ED30` (Cyan-Blue, 20% opacity fill).
- **Padding Area**: `0x27AE6040` (Emerald Green, 25% opacity fill).
- **Border Box**: `0xF2994AEE` (Solid Amber/Orange, 1.5px stroke).
- **Parent Gap / Alignment**: `0x9B51E040` (Purple, 25% opacity stripe fill).

#### Dimension Badge Rendering
An MSDF text badge is rendered at the top-left or center of the content box showing exact resolved pixel dimensions:
$$\text{"320 px} \times \text{180 px"}$$

---

### 4.2 Mode 2: Rulers & Distance Measurement Mode

When a developer selects a **Primary Target** and hovers a **Secondary Target** (or compares against the parent viewport), `DevOverlayService` calculates relative distance vectors.

```text
    ┌─────────────────────────┐
    │ Primary Element         │
    │ (Selected)              │
    └────────────┬────────────┘
                 │
                 │ ◄─── Distance Vector: "48 px" (MSDF Badge)
                 │
    ┌────────────┴────────────┐
    │ Secondary Element       │
    │ (Hovered Target)        │
    └─────────────────────────┘
```

#### Calculated Rulers & Distance Indicators
1. **Axis Projection Guides**: 1-pixel screen-aligned guide lines extending from element edges to viewport boundaries.
2. **Gap Vector Lines**: Solid 1-pixel vector lines with arrow heads connecting nearest bounding box edges between primary and secondary targets.
3. **Distance Badges**: High-contrast MSDF text badges anchored at the midpoint of distance vectors displaying pixel gap distances (`"48 px"`).
4. **Alignment Guides**: Highlighted dotted lines when edges align along X or Y axes (center alignment, left/right snap).

---

### 4.3 Mode 3: Tree Hierarchy & Child Bounds Mode

To visualize layout containment without cluttering the screen, `DevOverlayService` assigns a deterministic color palette to child elements based on **tree depth** and **sibling index**.

#### Hash-Based Depth Palette

```cpp
constexpr std::uint32_t DepthPalette[] = {
    0xEB5757FF, // Depth 0: Red
    0xF2994AFF, // Depth 1: Orange
    0xF2C94CFF, // Depth 2: Yellow
    0x27AE60FF, // Depth 3: Green
    0x2F80EDFF, // Depth 4: Blue
    0x9B51E0FF, // Depth 5: Purple
};

inline std::uint32_t colorForDepthAndSibling(std::uint32_t depth, std::uint32_t siblingIndex) noexcept {
    const std::uint32_t baseColor = DepthPalette[depth % 6];
    // Slightly shift hue/brightness per sibling
    return baseColor ^ ((siblingIndex * 0x1F1F1F00) & 0x00FFFFFF);
}
```

#### Visual Features
- **Child Outlines**: Thin 1-pixel colored borders around all descendants of the selected element.
- **Parent Container Boundary**: Dashed line around the direct parent container.
- **Hierarchy Badges**: Small depth badges (`"L2: #card_3"`) showing tree depth and debug labels.

---

### 4.4 Mode 4: Typography & Glyph Geometry Mode

For text elements, inspecting standard bounding boxes is insufficient. Developers need to verify baseline alignment, line heights, and font metrics.

```text
 ── Cap Height ────────────────────────────────────────── (Cyan 1px)
    FlowUi MSDF Text Line Box
 ── Baseline ──────────────────────────────────────────── (Solid Pink 1.5px)
 ── Descender ─────────────────────────────────────────── (Yellow 1px)
```

#### Typography Overlay Metrics
- **Baseline Guide**: Solid 1.5-pixel Hot-Pink line (`0xFF007AFF`) passing through text baseline.
- **Cap-Height Line**: Cyan dotted line (`0x00C7BEFF`) marking uppercase letter tops.
- **Descender Line**: Yellow dotted line (`0xFFCC00FF`) marking bottom of descenders (e.g. 'g', 'p', 'y').
- **Unwrapped Text Bounds**: Translucent outline showing full unwrapped text width versus current layout clipped box width.

---

### 4.5 Mode 5: Clip Bounds & Scroll Viewport Overlay

When layout elements overflow or scroll:
- **Clip Boundary**: Red dashed stroke (`0xEB5757FF`) around active Clay clip containers.
- **Scroll Content Extent**: Translucent hatched rectangle showing total unclipped content size inside scroll viewports.
- **Hidden Overflow Indicator**: Red warning badge showing hidden pixel overflow (`"Overflow: -140px Y"`).

---

### 4.6 Mode 6: Render-Run Break & Overdraw Diagnostics

Integrates with `Vk_UiRenderer` telemetry:
- **Pipeline Break Indicators**: Colored markers at element positions where texture swaps, scissor updates, or pipeline changes force `Vk_UiRenderer` to break batching.
- **Overdraw Heatmap**: Multi-pass alpha accumulation revealing UI areas drawn multiple times.

---

## 5. Detailed component & C++ code specifications

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <span>
#include <vector>

#include "devSystems/devTooling/overlay/DevOverlayTypes.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

namespace FlowUi::devMode {

class DevOverlayService {
public:
	DevOverlayService() = default;
	~DevOverlayService() = default;

	// Build overlay primitive commands based on current selection spec and captured tree snapshot
	void generateOverlayCommands(
		const DevOverlaySelectionSpec& selection,
		const DevTreeSnapshot& treeSnapshot,
		const DevSchemaGeneration& schemaGen,
		DevOverlayCommandBuffer& outCommandBuffer) noexcept;

	// Convert abstract overlay primitives into Vk_UiRenderer UiInstance & UiRun streams
	void buildUiRendererInstances(
		const DevOverlayCommandBuffer& commandBuffer,
		float windowWidth,
		float windowHeight,
		std::vector<UiInstance>& outInstances,
		std::vector<UiRun>& outRuns) noexcept;

private:
	void buildBoxModelOverlay(
		const DevClayNode& clayNode,
		const DevFlowNode& flowNode,
		float scale,
		DevOverlayCommandBuffer& out) noexcept;

	void buildDistanceRulers(
		const DevClayNode& primary,
		const DevClayNode& secondary,
		float scale,
		DevOverlayCommandBuffer& out) noexcept;

	void buildTypographyOverlay(
		const DevClayNode& clayNode,
		float scale,
		DevOverlayCommandBuffer& out) noexcept;
};

} // namespace FlowUi::devMode

#endif // FLOW_UI_DEV_MODE
```

---

## 6. Implementation strategies & trade-off analysis

### 6.1 Strategy 1: Overlay Primitive Generation Architecture

| Option | Mechanism | Pros | Cons | Recommendation |
|---|---|---|---|---|
| **A: Shader-Driven Procedural Overlay** | Render a single full-screen quad and compute borders, padding, and rulers procedurally inside a custom Vulkan fragment shader using element coordinate uniforms. | Zero CPU primitive generation cost; smooth antialiased GPU rendering. | Requires custom Vulkan pipeline, descriptor sets, uniform buffers, and complex GPU fragment math for text badges. | **Reject.** Excessive renderer complexity. |
| **B: Sidecar `UiInstance` Primitive Pipeline** | Generate abstract `DevOverlayPrimitive` structs on CPU, then convert into standard `UiInstance` (Solid & MSDF) primitives executed by `Vk_UiRenderer`. | Reuses 100% of existing Vulkan pipelines, font catalog, and descriptor sets. Zero new shaders required. | Small CPU time ($< 10 \;\mu\text{s}$) to emit primitives into vector buffers. | **Recommended.** Cleanest integration and minimal code bloat. |

---

### 6.2 Strategy 2: Modular Tool Packaging vs Single Monolithic Overlay

| Option | Mechanism | Pros | Cons | Recommendation |
|---|---|---|---|---|
| **A: Monolithic All-In-One Overlay** | Always render box model, rulers, hierarchy, and text guides simultaneously for the selected element. | Single code path. | Visual clutter; overwhelming on small elements. | **Reject.** Unusable for dense UIs. |
| **B: Mode-Masked Modular Tool Overlay (`DevOverlayModeFlags`)** | Separate visual overlays into flags (`BoxModel`, `RulersAndDistance`, `TreeHierarchy`, `Typography`). `DevInterface` toggles flags independently. | Extremely clean DX; developer sees only relevant diagnostic data. | Requires modular builder functions per mode. | **Recommended.** Flexible and industry-standard. |

---

## 7. Step-by-step implementation plan

```text
┌──────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                   DevOverlay Implementation Roadmap                                  │
├───────────────────────────────────┬──────────────────────────────────┬───────────────────────────────┤
│ Stage 1: Data Models & Buffers    │ Stage 2: Box Model & Primitives  │ Stage 3: Rulers & Distance    │
│ • DevOverlayTypes.hpp             │ • Content, padding, border rects │ • Distance vector math        │
│ • DevOverlayCommandBuffer.hpp     │ • MSDF dimension text badges     │ • Axis guide projection lines │
│ • Selection spec data structures  │ • UiInstance conversion logic    │ • Alignment snap guides       │
├───────────────────────────────────┼──────────────────────────────────┼───────────────────────────────┤
│ Stage 4: Typography & Tree Bounds │ Stage 5: Renderer Pass Injection │ Stage 6: Definition of Done   │
│ • Baseline & cap-height lines     │ • Vk_UiRenderer sidecar pass     │ • Verification test suite     │
│ • Hash-based depth color palette  │ • Top-level draw priority        │ • Zero production overhead    │
│ • Clip & overflow rects           │ • Scissor override handling      │ • Memory & timing audit       │
└───────────────────────────────────┴──────────────────────────────────┴───────────────────────────────┘
```

### Stage 1: Data Models & Buffers
- Create `include/devSystems/devTooling/overlay/DevOverlayTypes.hpp` and `DevOverlayCommandBuffer.hpp`.
- Define `DevOverlaySelectionSpec`, `DevOverlayModeFlags`, `DevOverlayPrimitive`, and `DevOverlayCommandBuffer`.

### Stage 2: Box Model Visualizer & Primitive Converter
- Implement `DevOverlayService::buildBoxModelOverlay(...)` in `src/devSystems/devTooling/overlay/DevOverlayService.cpp`.
- Query `DevClayNode` padding, border, and content bounds from `DevTreeCapture`.
- Emit solid translucent rectangles for content (`0x2F80ED30`), padding (`0x27AE6040`), and stroked border (`0xF2994AEE`).
- Emit MSDF text primitive for dimension label (`"320 x 180"`).
- Implement `DevOverlayService::buildUiRendererInstances(...)` to convert primitives into `UiInstance` and `UiRun` streams.

### Stage 3: Rulers & Distance Vectors
- Implement `DevOverlayService::buildDistanceRulers(...)`.
- Calculate gap vectors between `primaryTarget` and `secondaryTarget`.
- Emit 1-pixel screen-aligned guide lines and distance text badges (`"48 px"`).

### Stage 4: Typography & Tree Hierarchy Overlays
- Implement baseline (`0xFF007AFF`), cap-height, and descender line emission for text elements.
- Implement hash-based depth color palette for tree containment visualization (`buildTreeHierarchyOverlay`).

### Stage 5: Renderer Pass Injection
- In `Vk_UiRenderer`, add `renderDevOverlaySidecarPass(const PreparedUiFrame& prepared, DevOverlayCommandBuffer& overlayBuffer)`.
- Ensure overlay runs are submitted last with a full-screen scissor rectangle to guarantee top-level draw priority.

### Stage 6: Definition of Done & Verification
- Verify overlays render on top of all application UI without distorting application Clay layout or input routing.
- Verify zero memory leaks or unhandled growth in overlay vector buffers across frames.
- Verify zero CPU/GPU overhead when developer mode is disabled (`FLOW_UI_DEV_MODE == 0`).

---

## 8. Production boundary (`FLOW_UI_DEV_MODE == 0`)

When developer mode is disabled (`FLOW_UI_DEV_MODE == 0`):

1. `DevOverlayService`, `DevOverlayCommandBuffer`, and `DevOverlaySelectionSpec` are omitted from compilation.
2. `Vk_UiRenderer` sidecar overlay pass calls expand to `((void)0)`.
3. Absolute zero CPU memory, GPU memory, or rendering overhead in production builds.
