# DevInterface Architecture & Visual Identity Specification

## Executive Summary

This document specifies the complete architectural blueprint and visual identity for **`DevInterface`**. It is designed as a single, top-level `DrawableFlowElement` that expands to fill the developer UI viewport (`grow(0)` in both dimensions) and serves as the singleton root for all developer tooling surfaces.

The visual identity embodies a dark, precision-engineered **Teal-Ocean** design language: calm, dense, technical, and optimized for long engineering sessions. Structure is created through subtle background depth shifts and strict 1 px hairlines.

All driving state lives inside a non-transient, persistent `DevInterfaceState` struct held within `StateOf<DevInterface>`.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. PERMANENT HEADER (L ➔ R) [Depth 1: #0d2230 | 1 px Border: #182f3c]       │
│ [Title] │ [Window Selector ▼] │ [Quick Report Badges] │ [General Controls] │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. MAIN CONTENT (T ➔ B)                                                     │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ 2a. Main Content Header                                                 │ │
│ │ [Tabs: Inspect | Perf | Mem | Diag | Changes | Cat] │ [Context Controls]│ │
│ ├─────────────────────────────────────────────────────────────────────────┤ │
│ │ 2b. Main Body Surface (3-Column Horizontal Flex Layout)                 │ │
│ │ ┌───────────────────┬─────────────────────────┬──────────────────────┐  │ │
│ │ │ Selector Area     │ Workbench Area          │ Inspector Area       │  │ │
│ │ │ [Depth 1: #0d2230]│ [Depth 0: #071A22]      │ [Depth 1: #0d2230]   │  │ │
│ │ └───────────────────┴─────────────────────────┴──────────────────────┘  │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────────────┤
│ 3. PERMANENT FOOTER (L ➔ R) [Depth 1: #0d2230 | 1 px Border: #182f3c]       │
│ [App State: Running] │ [Status: 0 Errors, 3 Changes] │ [Last Action Msg] │ [Exit Status] │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 1. Visual Identity & Design Tokens System

### 1.1 Color Palette Table

| Token Name | Hex Code | Role & Usage |
| :--- | :--- | :--- |
| **Keel** | `#071A22` | Deepest background floor ground (Depth 0: canvas, code surfaces, timeline base). |
| **Panel** | `#0d2230` | Persistent panel background (Depth 1: selector, inspector, header, footer). |
| **Ink** | `#182227` | Elevated surface (Depth 2: section headers, table headers, card backgrounds). |
| **Elevated Control**| `#1e2e38` | Interactive working surface (Depth 3: input fields, property cells, dropdowns). |
| **Hover** | `#243848` | Interactive hover state fill. |
| **Selected Row** | `#0e3654` | Row selection background (Signal Blue tint). |
| **Current** | `#18B8A6` | **Primary Brand Accent (Teal)**: Active tab underlines, focus borders, tree left indicators, toggle ON. |
| **Sea Glass** | `#73D5C5` | **Baked / Persisted Accent**: Cool secondary teal for baked overrides & chart sparklines. |
| **Signal Blue** | `#3288D8` | **Navigation Highlight**: Selection indicators and selected row background tinting. |
| **Signal Coral** | `#F2684A` | **Live Changes Accent**: Authored live overrides, change count badges, diff highlights. |
| **Canvas** | `#F5F8F7` | **Primary Text**: Headings, selected labels, active readable text (100% luminance). |
| **Secondary Text** | `#78b8c8` | **Field Labels & Metadata**: Column headers, unselected tree nodes (65% luminance). |
| **Muted Text** | `#3d6878` | **Low-Contrast Metadata**: Timestamps, IDs, memory addresses, tick counters. |
| **Green** | `#2ab870` | **Semantic Live State**: Live runtime pulse dot, healthy status, constructed state. |
| **Amber** | `#d4922a` | **Semantic Warnings**: Degraded data, layout diagnostics (hollow square badge). |
| **Red** | `#e05252` | **Semantic Errors**: Problem dots, error text, failure counters. |

---

### 1.2 Depth Layering & Structural Surfaces

Structure is established through subtle background depth level shifts rather than heavy drop shadows or card borders. All depth levels carry an ocean-teal tint (`#071A22` floor ground).

```
Depth 3: Elevated Controls  [#1e2e38]  Inputs, property cells, working surfaces
  ▲
Depth 2: Ink Surfaces       [#182227]  Section headers, table headers, inline cards
  ▲
Depth 1: Structural Panels  [#0d2230]  Selector column, Inspector column, Header, Footer
  ▲
Depth 0: Page Floor         [#071A22]  Keel canvas ground behind all panels & code space
```

---

### 1.3 Separator & Line Philosophy

* **1 px Hairlines Only**: Zero 2 px lines, zero drop shadows, zero glassmorphism, zero gradients.
* **Primary Hairline (`#182f3c`)**: Dark teal hairline used for structural column boundaries, main section breaks, and header/footer borders.
* **Subtle Hairline (`#182227`)**: Barely-there separator used inside dense table rows where visual weight must be minimal.
* **Visible Border (`#26485c`)**: Declares editability around interactive elements (inputs, dropdowns, buttons).

---

### 1.4 Typography Hierarchy

* **UI Font (`Inter`)**: Used for all standard labels, tab titles, headers, and UI controls.
* **Monospace Font (`JetBrains Mono`)**: Exclusively reserved for:
  * Technical IDs, addresses, and hashes
  * File paths and line/column numbers
  * Timing/duration values (`16.6 ms`, `42 µs`, `48.2 MB`)
  * Raw property values in editors
  * Frame and tick counters

---

### 1.5 Accent & Semantic Rules

1. **`Current` Teal (`#18B8A6`)**: Primary navigational spotlight. Accent-bordered buttons use `#071f2a` background (Keel soaked in Current).
2. **`Signal Coral` (`#F2684A`)**: Marks live unbaked overrides and active diffs (`◆`). Deliberate and notable, but not alarming.
3. **`Sea Glass` (`#73D5C5`)**: Marks settled, persisted, or baked overrides.
4. **`Signal Blue` (`#3288D8`)**: Reserved strictly for selected row highlights (`#0e3654`).
5. **Corner Radii**: 3–4 px maximum on interactive controls (buttons, input fields, badges). **0 px radius** on structural panels and column dividers.

---

## 2. Top-Level Layout Decomposition

The `DevInterface` root element is structured top-to-bottom into **3 First-Generation Children**:

### 2.1 First-Generation Child 1: Permanent Header (Left ➔ Right)
* **Visual Specs**: Height: 44 px | Background: Depth 1 `#0d2230` | Bottom Border: 1 px `#182f3c` | Padding: 0 16 px.
* **Layout Structure**:
  * **Title**: `Canvas` text (`#F5F8F7`), Inter SemiBold, branding mark.
  * **Window Selector**: Elevated control background (`#1e2e38`), border (`#26485c`), radius 4 px, dropdown arrow.
  * **Quick Report Data**: Monospace telemetry badges:
    * **FPS / Frame Time**: `#73D5C5` Sea Glass text.
    * **Memory**: `#78b8c8` Secondary text.
    * **Live Pulse**: `#2ab870` Green pulse dot.
  * **General Controls Area**: Overlay toggle, panel lock, settings gear, and close button.

### 2.2 First-Generation Child 2: Main Content (Top ➔ Bottom)
* **Visual Specs**: Sizing: `grow(0)` | Background: Depth 0 `#071A22`.
* **Main Content Header**:
  * **Section 1: Navigation Tabs**: 6 Tabs (`Inspect`, `Performance`, `Memory`, `Diagnostics`, `Changes`, `Catalogue`). Active tab text: `Canvas` (`#F5F8F7`) with 2 px `Current` (`#18B8A6`) bottom underline. Unselected tabs: `Secondary` (`#78b8c8`).
  * **Section 2: Contextual Controls Area**: Dynamically renders tab-specific filter inputs, search bars (`#1e2e38`), and action triggers.
* **Main Body (3 Conceptual Surfaces)**:
  * **`Selector Area` (Left Surface)**: Width: 280 px (resizable) | Background: Depth 1 `#0d2230` | Right Border: 1 px `#182f3c`. Holds tree navigation, entity lists, catalog index. Selected tree row: `#0e3654` with `Current` (`#18B8A6`) 2 px left border indicator.
  * **`Workbench Area` (Center Surface)**: Sizing: `grow(0)` | Background: Depth 0 `#071A22`. Holds primary visualizer (Clay element forest tree, timeline flamegraph, diff viewer).
  * **`Inspector Area` (Right Surface)**: Width: 320 px (resizable) | Background: Depth 1 `#0d2230` | Left Border: 1 px `#182f3c`. Holds property grid, parameter sliders, reflection values. Live overrides shown in `Signal Coral` (`#F2684A`).

### 2.3 First-Generation Child 3: Permanent Footer (Left ➔ Right)
* **Visual Specs**: Height: 32 px | Background: Depth 1 `#0d2230` | Top Border: 1 px `#182f3c` | Padding: 0 12 px.
* **Layout Structure**:
  * **Application Operational State**: `[● LIVE]` status badge (`#2ab870` Green).
  * **Interface Status Reports**: `#e05252` Red for errors count, `#F2684A` Signal Coral for unbaked changes count.
  * **Action Message Box**: `Muted` text (`#3d6878`) showing the log of the last undo/redo action.
  * **Exit Status Indicator**: `#F2684A` Signal Coral badge (`[⚠️ 3 Unbaked Changes]`) alerting the developer if unbaked modifications exist.

---

## 3. State & Orchestration Architecture

All driving data lives inside a non-transient, persistent `DevInterfaceState` struct held within `StateOf<DevInterface>`.

### 3.1 C++ Theme Constants (`include/devSystems/devInterface/DevTheme.hpp`)

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "clay.h"
#include "FlowUi/App.hpp"

namespace FlowUi::devSystems::theme {

// --- Depth Layering Backgrounds ---
inline const Clay_Color kDepth0_Keel     = Flow_Color("#071A22"); // Page ground floor
inline const Clay_Color kDepth1_Panel    = Flow_Color("#0d2230"); // Structural columns
inline const Clay_Color kDepth2_Ink      = Flow_Color("#182227"); // Elevated section headers
inline const Clay_Color kDepth3_Elevated = Flow_Color("#1e2e38"); // Working control surfaces
inline const Clay_Color kHover_Surface   = Flow_Color("#243848"); // Hover state fill
inline const Clay_Color kSelected_Row    = Flow_Color("#0e3654"); // Selection background

// --- 1 px Hairline Borders ---
inline const Clay_Color kBorder_Primary  = Flow_Color("#182f3c"); // Column boundaries
inline const Clay_Color kBorder_Subtle   = Flow_Color("#182227"); // Dense row separators
inline const Clay_Color kBorder_Visible  = Flow_Color("#26485c"); // Input/button borders

// --- Brand & Accent Colors ---
inline const Clay_Color kAccent_Current  = Flow_Color("#18B8A6"); // Primary teal brand accent
inline const Clay_Color kAccent_SeaGlass = Flow_Color("#73D5C5"); // Baked override accent
inline const Clay_Color kAccent_SigBlue  = Flow_Color("#3288D8"); // Selection highlight
inline const Clay_Color kAccent_SigCoral = Flow_Color("#F2684A"); // Live unbaked edits accent

// --- Typography Colors ---
inline const Clay_Color kText_Canvas    = Flow_Color("#F5F8F7"); // Primary text (100%)
inline const Clay_Color kText_Secondary = Flow_Color("#78b8c8"); // Labels & tags (65%)
inline const Clay_Color kText_Muted     = Flow_Color("#3d6878"); // Low-contrast metadata

// --- Semantic Status Colors ---
inline const Clay_Color kStatus_Green   = Flow_Color("#2ab870"); // Healthy/Live
inline const Clay_Color kStatus_Amber   = Flow_Color("#d4922a"); // Warning
inline const Clay_Color kStatus_Red     = Flow_Color("#e05252"); // Error

} // namespace FlowUi::devSystems::theme

#endif
```

---

### 3.2 Persistent State Struct (`DevInterfaceState.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "FlowUi/WindowId.hpp"
#include "FlowUi/ElementID.hpp"

namespace FlowUi::devSystems {

enum class DevTab : uint8_t {
    Inspect = 0,
    Performance = 1,
    Memory = 2,
    Diagnostics = 3,
    Changes = 4,
    Catalogue = 5,
};

enum class AppOpState : uint8_t {
    Running = 0,
    Paused = 1,
    FrameStepping = 2,
};

// Abstract Undo/Redo Command
struct IDevCommand {
    virtual ~IDevCommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    [[nodiscard]] virtual std::string description() const = 0;
};

// History Stack
struct DevCommandStack {
    std::vector<std::unique_ptr<IDevCommand>> undoStack;
    std::vector<std::unique_ptr<IDevCommand>> redoStack;
    std::vector<std::string> historyLog;

    void pushAndExecute(std::unique_ptr<IDevCommand> cmd) {
        cmd->execute();
        historyLog.push_back(cmd->description());
        undoStack.push_back(std::move(cmd));
        redoStack.clear();
    }
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    void undo() {
        if (!canUndo()) return;
        auto cmd = std::move(undoStack.back());
        undoStack.pop_back();
        cmd->undo();
        historyLog.push_back("Undo: " + cmd->description());
        redoStack.push_back(std::move(cmd));
    }
    void redo() {
        if (!canRedo()) return;
        auto cmd = std::move(redoStack.back());
        redoStack.pop_back();
        cmd->execute();
        historyLog.push_back("Redo: " + cmd->description());
        undoStack.push_back(std::move(cmd));
    }
};

struct DevInterfaceState {
    // --- Navigation & Target State ---
    DevTab activeTab = DevTab::Inspect;
    WindowId selectedWindowId = MainWindowId;
    FlowElementID selectedElementId{};
    std::string searchQuery{};

    // --- Application Control State ---
    AppOpState appState = AppOpState::Running;
    bool isOverlayEnabled = true;
    bool isPanelPinned = false;

    // --- Command History & Undo/Redo ---
    DevCommandStack commandStack{};
    bool hasUnbakedChanges = false;
    std::string lastActionMessage = "System initialized";

    // --- DevInterface Self-Monitoring ---
    uint64_t devUiFrameTimeUs = 0;
    size_t devUiMemoryBytes = 0;
    uint32_t activeErrorCount = 0;

    // --- Splitter Layout Dimensions ---
    float selectorWidth = 280.0f;
    float inspectorWidth = 320.0f;
};

} // namespace FlowUi::devSystems
```

---

## 4. Element Implementation (`DevInterface.hpp`)

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"
#include "managers/FlowUiElementBuilder.hpp"
#include "devSystems/devInterface/DevInterfaceState.hpp"
#include "devSystems/devInterface/DevTheme.hpp"

namespace FlowUi::devSystems::interface_elements {

struct DevInterface {
    using State = DevInterfaceState;
    using BuildContext = ElementBuildContext<DevInterface>;

    static constexpr FlowDefinitionID definitionId = DefinitionID("flowui.dev_interface.root");
    static constexpr std::string_view debugName = "Developer Interface";
    static constexpr bool isDevInternal = true;

    static void buildElement(BuildContext& ctx) {
        State& state = ctx.state();
        const auto startTime = std::chrono::high_resolution_clock::now();

        // Top-Level Root: grow(0) sizing, Keel ground background (#071A22)
        Clay_ElementDeclaration root{};
        root.layout.sizing = {
            .width = CLAY_SIZING_GROW(0),
            .height = CLAY_SIZING_GROW(0),
        };
        root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
        root.backgroundColor = theme::kDepth0_Keel;

        CLAY(ctx.clayID(), root) {
            // 1. Permanent Header (Depth 1: #0d2230)
            buildPermanentHeader(ctx, state);

            // 2. Main Content Area (Header + 3-Column Flex Body)
            buildMainContentArea(ctx, state);

            // 3. Permanent Footer (Depth 1: #0d2230)
            buildPermanentFooter(ctx, state);
        }

        const auto endTime = std::chrono::high_resolution_clock::now();
        state.devUiFrameTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    }

private:
    static void buildPermanentHeader(BuildContext& ctx, State& state);
    static void buildMainContentArea(BuildContext& ctx, State& state);
    static void buildPermanentFooter(BuildContext& ctx, State& state);
};

inline constexpr DevInterface kDevInterface{};
static_assert(DrawableFlowElement<DevInterface>);

} // namespace FlowUi::devSystems::interface_elements

#endif
```

---

## 5. Incremental Implementation Roadmap

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 1: Theme Constants & DevInterface Shell                                │
│   - Create DevTheme.hpp with Teal-Ocean palette & 1 px hairline tokens      │
│   - Create DevInterfaceState & DevInterface element shell                   │
│   - Wire grow(0) layout container with Keel floor ground (#071A22)           │
├─────────────────────────────────────────────────────────────────────────────┤
│ STEP 2: Permanent Header & Window Selector                                  │
│   - Implement Depth 1 (#0d2230) header bar with 1 px #182f3c bottom border  │
│   - Connect WindowId dropdown & JetBrains Mono telemetry badges             │
├─────────────────────────────────────────────────────────────────────────────┤
│ STEP 3: Tab Bar & 3-Column Surface Flex Layout                              │
│   - Build tab bar with Current (#18B8A6) active underline                   │
│   - Lay out Selector (Left #0d2230), Workbench (Center #071A22),            │
│     and Inspector (Right #0d2230) side-by-side surfaces                     │
├─────────────────────────────────────────────────────────────────────────────┤
│ STEP 4: Command Stack (Undo/Redo) & Footer Reporter                         │
│   - Integrate IDevCommand & DevCommandStack                                 │
│   - Connect Footer action log, Red error badge, and Coral unbaked badge     │
├─────────────────────────────────────────────────────────────────────────────┤
│ STEP 5: Specialized Tab Views Implementation                                │
│   - Populate Inspect, Performance, Memory, Diagnostics, Changes, Catalogue  │
└────────────────────────────────────────────────(────────────────────────────┘
```

---

## 6. Visual Identity Alignment Checklist

* [x] **Palette Tokens**: Keel (`#071A22`), Panel (`#0d2230`), Ink (`#182227`), Elevated (`#1e2e38`), Current (`#18B8A6`), Sea Glass (`#73D5C5`), Signal Blue (`#3288D8`), Signal Coral (`#F2684A`), Canvas (`#F5F8F7`).
* [x] **Depth Layering Strategy**: 4 Depth levels (Depth 0 Keel ➔ Depth 1 Panel ➔ Depth 2 Ink ➔ Depth 3 Elevated).
* [x] **1 px Hairlines**: All dividers strictly 1 px (`#182f3c` primary, `#182227` subtle, `#26485c` visible). Zero drop shadows or glassmorphism.
* [x] **Typography Rules**: Inter for UI text; JetBrains Mono for hashes, file paths, timings (ms/µs/MB), raw values, and tick counters.
* [x] **Accent Usage**: `Current` for active tab underlines & focus borders; `Signal Coral` for live unbaked edits; `Signal Blue` for row selection background (`#0e3654`); `Sea Glass` for persisted override badges.
* [x] **Radii**: 3–4 px max on interactive controls; 0 px radius on structural panels and columns.
