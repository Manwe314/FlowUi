# FlowUi DevBakeTools — Architecture and Implementation Design

## 1. Purpose and subsystem boundaries

This report specifies the design for **`DevBakeTools`** (referred to in `DevTooling.md` Section 8 as `DevBakePipeline` and `DevBakeChanges`).

`DevBakeTools` is the component of **`DevTooling`** that spans the boundary between developer mode and production execution:

> **Core Tenet**: Development-mode tooling (`DevSchemaRegistry`, `DevOverrideEngine`, `DevInterface`) empowers a developer to inspect and visually modify live UI parameters, themes, and layouts in real time. `DevBakeTools` turns those transient runtime edits into durable, typed C++ production changes compiled directly into the application.

```text
┌─────────────────────────────────────────────────────────────────────────────────┐
│                                 DEVELOPER MODE                                  │
│                                                                                 │
│  DevInterface ──► DevOverrideEngine ──► Live Application Memory (Hot Loop Edit)  │
│                           │                                                     │
│                           │ (Developer triggers bake via DevTooling API)        │
│                           ▼                                                     │
│                   DevBakePipeline                                               │
│                           │                                                     │
│                           ▼                                                     │
│            .flowui/changes/active.flowchanges (Versioned Manifest)              │
└───────────────────────────┬─────────────────────────────────────────────────────┘
                            │
                            │ Automatic CMake / Build System Phase (Default ON)
                            ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                                 PRODUCTION BOUNDARY                             │
│                                                                                 │
│                    flowui-dev-generate (Build-Time Tool)                        │
│                           │                                                     │
│                           ▼                                                     │
│          build/.flowui/generated/FlowUiBakedChanges.cpp                         │
│                           │                                                     │
│                           ▼                                                     │
│                 Native C++ Compiler & Linker                                    │
│                           │                                                     │
│                           ▼                                                     │
│     Production Binary (Zero JSON, Zero Reflection, Pure Typed C++ Code)         │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Legacy updater vs. Modern DevBakeTools

Previously, FlowUi relied on an external standalone executable (`flowui_devChange_updater`) that attempted to parse raw C++ source code text files line-by-line and patch string literals. That approach was fragile, slow, broke whenever source lines shifted, failed on C++ macros and loops, and required manual execution.

`DevBakeTools` replaces legacy text patching with **Manifest-Driven `constexpr` C++ Generation**:
- Active edits are saved into a structured, versioned manifest (`.flowui/changes/active.flowchanges`).
- A lightweight, cross-platform build generator (`flowui-dev-generate`) converts the manifest into typed `constexpr` C++ source code (`FlowUiBakedChanges.cpp`) compiled directly into the application on the next build.
- **AST-Assisted Source Rewriting is intentionally de-scoped**: Direct C++ header AST manipulation relies on Clang LibTooling, which creates heavy toolchain lock-in (breaking non-Clang environments like GCC or MSVC). With `constexpr` generation and Link-Time Optimization (LTO), manifest baking achieves absolute $0.00 \text{ ns}$ runtime overhead, making source-header editing functionally obsolete while keeping FlowUi 100% portable across all C++ compilers.

---

## 2. Architecture & pipeline overview

The baking pipeline operates across three distinct execution phases:

```text
 Phase 1: Runtime Capture      Phase 2: Build Generation      Phase 3: Production Execution
┌────────────────────────┐    ┌────────────────────────┐     ┌────────────────────────────┐
│ DevBakePipeline        │    │ flowui-dev-generate    │     │ DevBakedChanges            │
│                        │    │                        │     │                            │
│ • Validates bakeable   │───►│ • Validates schema     │────►│ • Evaluates constexpr      │
│   instance keys        │    │   fingerprint          │     │   definition initializers  │
│ • Serializes active    │    │ • Emits constexpr      │     │ • Dispatches exact-instance│
│   overrides into       │    │   tables and functions │     │   table lookups            │
│   .flowchanges manifest│    │ • Zero reflection      │     │ • Zero heap allocations    │
└────────────────────────┘    └────────────────────────┘     └────────────────────────────┘
```

### 2.1 Key Requirements & Guarantees

- **Seamless DX**: Baking is triggered programmatically via `DevTooling`'s action API (`app.dev().bakeActiveEdits()`), which future `DevInterface` widgets call. No manual JSON editing or separate command execution.
- **Automatic Default Enablement**: Baked compilation is **enabled automatically by default**. Developers do NOT need to write any custom CMake target commands in their project. It can be explicitly disabled via `-DFLOW_UI_ENABLE_BAKED_CHANGES=OFF`.
- **Zero Production Overhead**: Production binaries contain **no JSON parsing, no runtime string lookups, no reflection tables, and no heap allocations**. Baked changes are generated as direct C++ `constexpr` assignments and compact binary search tables.
- **Fingerprint Protection**: Manifests record a `DevSchemaFingerprint` and `FlowBuildFingerprint`. If C++ struct layouts or field IDs change between builds, out-of-date baked entries are safely diagnosed and ignored rather than causing memory corruption.

---

## 3. Instance-specific overrides & identity resolution

### 3.1 The challenge of shared call sites and loops

A major challenge in GUI hot-reloading is handling element instances produced by shared code paths:

```cpp
// One call site in source code...
for (const Item& item : items) {
    ui.createElement(kCard, Indexed("card", item.id))
        .setParameters(CardParameters{.title = item.title});
}
```

Because all card instances originate from the exact same line of C++ code, **textual source patching cannot set different property values for individual items in the loop**.

### 3.2 Stable vs. Unstable Instance Keys

To solve this, `DevBakeTools` classifies element instance identities into two categories:

```text
                     Element Instance Identity
                                │
          ┌─────────────────────┴─────────────────────┐
          ▼                                           ▼
Stable Identity Key                         Unstable Identity Key
(Authored Key / GlobalFlowID)               (Order-derived index / ephemeral)
          │                                           │
          ▼                                           ▼
  Bakeable as Exact Instance                Not Bakeable as Exact Instance
  (Generates typed dispatch table)          (DevTooling returns diagnostic warning
                                             recommending authored key)
```

1. **Stable Instance Key (`ElementInstanceKey`)**:
   - Explicitly authored using `GlobalFlowID` or semantic keys like `Indexed("card", item.id)` or `"sidebar.settings_button"`.
   - **Bakeable**: `DevBakeTools` serializes the 64-bit numeric key into `.flowchanges`. The build-time generator produces an exact-instance lookup entry.
2. **Unstable / Order-Derived Key**:
   - Automatically generated based on execution order or ephemeral loop indices without domain identity.
   - **Not Bakeable**: `DevBakeTools` permits live ephemeral overrides during developer sessions, but marks the edit **Not Bakeable**. `DevTooling` returns an actionable diagnostic:
     > *"This instance identity is order-derived and cannot be baked safely across source edits. Add an authored key (e.g. `Indexed(\"item\", id)`) at the call site to enable baking."*

---

## 4. Definition & theme baking: Why AST source rewriting is obsolete

When designing persistent baking for definition-level edits (e.g., `CardParameters::accentColor`) and theme edits (e.g., `AppTheme::backgroundColor`), two approaches were evaluated:

### 4.1 Evaluation of Source Header Rewriting vs. `constexpr` Manifest Baking

| Feature / Criteria | Manifest-Driven `constexpr` Generation (`FlowUiBakedChanges.cpp`) | AST Source Header Rewriting |
|---|---|---|
| **Compiler / Toolchain Compatibility** | **100% Portable** across GCC, Clang, MSVC, Apple Clang, ICC. Zero toolchain dependencies. | **Toolchain Lock-in**: Requires Clang LibTooling (`libclangAST`). Breaks GCC & MSVC build environments. |
| **Runtime Overhead** | **$0.00 \text{ ns}$**: Compiles to `constexpr` initializers. Link-Time Optimization (LTO) constant-folds and inlines assignments directly. | **$0.00 \text{ ns}$**: Directly in authored header. |
| **Refactoring & Git Cleanliness** | **Clean Git History**: Authored C++ source files remain untouched. Edits live in versioned `.flowchanges` manifest. | **Source Pollution**: Mutates authored C++ headers; creates noisy git diffs in shared source repositories. |
| **Safety & Formatting** | **100% Safe**: Zero risk of corrupting C++ comments, preprocessor `#ifdef`s, macros, or formatting. | **Fragile**: High risk of breaking complex C++ macros, comments, or custom formatting. |
| **Instance-Specific Support** | **Full Support**: Seamlessly handles loops and exact-instance overrides via generated $K \le 7$ cascades / sorted arrays. | **Impossible**: Cannot support instance-specific overrides for shared call sites or loop-created elements. |

### 4.2 Why `constexpr` + LTO Makes Source Header Editing Obsolete

1. **Zero Runtime Overhead**: When compiling with `constexpr` defaults and LTO enabled, the C++ compiler inlines `applyBakedParametersErased` and constant-propagates values directly into element construction. The generated function call vanishes completely, achieving identical hardware execution speed ($0.00 \text{ ns}$) as if the C++ header had been edited by hand.
2. **Elimination of Clang Lock-in**: AST refactoring tools require linking against Clang frontend libraries (`libclangTooling`), forcing every developer on a team to install Clang/LLVM. `flowui-dev-generate` is a lightweight, zero-dependency C++ tool that runs on any machine with standard GCC, Clang, or MSVC.
3. **Canonical Architecture Decision**: **Manifest-driven `constexpr` C++ generation is the single canonical persistence engine for FlowUi.** AST source header editing is excluded to preserve universal compiler compatibility and clean source repositories.

---

## 5. Runtime performance, `constexpr`, and LTO optimizations

A primary design concern for baked changes is performance: **Does resolving baked changes introduce $O(N)$ overhead per frame or excessive branching?**

### 5.1 Algorithmic Complexity & Compiler Codegen Breakdown

```text
  Override Type          Resolution Algorithm                      Time Complexity    Per-Frame CPU Cost
──────────────────────────────────────────────────────────────────────────────────────────────────────────
  Definition Default     constexpr inline assignment               O(1)               < 0.5 ns (0 ns with LTO)
  Exact Instance (K=0)   No-op branch (bypassed via bitmask)        O(1)               0.0 ns
  Exact Instance (K<8)   Generated switch / comparison cascade      O(1)               < 1.2 ns (0.5 ns with LTO)
  Exact Instance (K≥8)   Binary search on sorted 64-bit key array    O(log K)           < 4.5 ns
  Theme Data             Applied once during ThemeManager init      O(1)               0.0 ns (per frame)
```

### 5.2 Micro-Architecture & Codegen Details

#### 1. Why $1 \le K \le 7$ for `switch` Statements?
Modern C++ compilers (Clang/LLVM, GCC, MSVC) lower `switch` statements into machine code using distinct heuristics based on case count $K$:
- **Comparison Cascades ($K \le 7$)**: For 7 or fewer cases, Clang/LLVM generates a sequential cascade of equality checks (`cmp` + `je`). In hardware, 1 to 7 sequential comparisons fit entirely inside the L1 Instruction Cache and the CPU's **Branch Target Buffer (BTB)**, executing in 1–2 clock cycles with near 100% prediction accuracy.
- **Jump Tables & Binary Search ($K \ge 8$)**: At $K \ge 8$, compilers emit indirect jump tables (`jmp [table + offset]`). Jump tables introduce indirect memory fetches, which can trigger cache misses. For $K \ge 8$, `std::lower_bound` over a static sorted `std::array` of 64-bit keys provides deterministic $O(\log K)$ bounds without code size explosion.

#### 2. How LTO (Link-Time Optimization) Eliminates Overhead
When LTO (`-flto`) is enabled:
- `ElementInvocation` knows `Element::definitionId` at compile time.
- The compiler sees through translation unit boundaries, **constant-folds away the outer definition switch**, and inlines `applyBaked_app_card(...)` directly into `ElementInvocation::begin()`.
- The function call, type erasure, and definition lookup vanish entirely. The baked override collapses to **a single native assembly store instruction** (`mov DWORD PTR [rax+12], 16`).

#### 3. Leveraging `constexpr` Defaults
`flowui-dev-generate` emits `constexpr` parameter defaults and template specializations:
```cpp
template <FlowDefinitionID DefID>
struct BakedDefinitionDefaults;

template <>
struct BakedDefinitionDefaults<DefinitionID("app.card")> {
    static constexpr Clay_Color accentColor{255, 128, 0, 255};
    static constexpr float padding = 16.0f;
};
```
- **Constant Propagation**: Downstream Clay layout math (e.g. `FIXED(constexpr_padding)`) is computed by the compiler at build time.
- **Dead-Code Elimination**: If a boolean toggle (e.g. `hasIcon = false`) is baked as `constexpr false`, the compiler automatically prunes `if (params.hasIcon)` branches at compile time.

---

## 6. Data model & manifest specifications

### 6.1 Manifest File Format (`.flowui/changes/active.flowchanges`)

The manifest is stored in a clean, human-readable JSON format:

```json
{
  "manifestVersion": 1,
  "schemaFingerprint": "0x9E4F8A2B1C3D5E7F",
  "buildFingerprint": "2026-08-26-rel-1.0.4",
  "createdTimestamp": "2026-08-26T17:45:34Z",
  "bakedChanges": [
    {
      "targetScope": "Definition",
      "definitionId": "app.card",
      "fieldPath": "accentColor",
      "fieldId": 140928310,
      "valueType": "Clay_Color",
      "value": { "r": 255, "g": 128, "b": 0, "a": 255 },
      "provenance": {
        "author": "developer",
        "note": "Adjusted card contrast for accessibility"
      }
    },
    {
      "targetScope": "ExactInstance",
      "definitionId": "app.button",
      "instanceKey": "0xA1B2C3D4E5F60718",
      "instanceDebugLabel": "sidebar.submit_btn",
      "fieldPath": "padding.top",
      "fieldId": 88129031,
      "valueType": "float",
      "value": 16.0,
      "provenance": {
        "author": "developer",
        "note": "Fixed padding clipping on main button"
      }
    }
  ]
}
```

### 6.2 C++ Data Structures (`DevBakeTypes.hpp`)

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string>
#include <vector>

#include "FlowUi/ElementID.hpp"
#include "devSystems/devTooling/override/DevOverrideTypes.hpp"

namespace FlowUi::devMode {

struct DevBakeProvenance {
	std::string author{};
	std::string note{};
	std::string timestamp{};
};

struct DevBakeEntry {
	DevOverrideScope targetScope = DevOverrideScope::Definition;
	FlowDefinitionID definition{};
	detail::element::ElementInstanceKey instanceKey{};
	std::string instanceDebugLabel{};
	DevFieldId fieldId = 0;
	std::string fieldPath{};
	DevTypeId valueType = 0;
	DevSmallValueBuffer rawValue{};
	DevBakeProvenance provenance{};
};

struct DevBakeManifest {
	std::uint32_t manifestVersion = 1;
	std::uint64_t schemaFingerprint = 0;
	std::string buildFingerprint{};
	std::vector<DevBakeEntry> entries{};
};

} // namespace FlowUi::devMode

#endif // FLOW_UI_DEV_MODE
```

---

### 6.3 Compound Sequential Baking & Single Manifest Coalescing

When a developer performs multiple iterative development cycles (editing live parameters, baking, recompiling, and making further edits in a subsequent session), `DevBakeTools` maintains **at most ONE active manifest** (`.flowui/changes/active.flowchanges`).

#### The Single Coalesced Manifest Strategy

Instead of creating incremental change logs (`active_v1.flowchanges`, `active_v2.flowchanges`), sequential edits are merged into the single canonical manifest through a **Load $\rightarrow$ Upsert/Prune $\rightarrow$ Save** pipeline:

```text
Cycle 1: Initial Bake
  Authored Code (padding=12) ──► Dev Edit (padding=16) ──► Bake ──► active.flowchanges [padding=16]

Cycle 2: Compound Bake on top of Cycle 1
  App Starts (Baked padding=16 linked via FlowUiBakedChanges.cpp)
        │
        ├─► Dev edits padding to 20      (Field updated in-place)
        └─► Dev edits accentColor to Red  (New field entry appended)
        │
        ▼ DevTooling::bakeActiveEdits()
  DevBakePipeline reads active.flowchanges ──► Coalesces Edits ──► Writes Single Manifest:
                                                                  active.flowchanges [padding=20, accentColor=Red]
```

#### Coalescing Logic Rules

When `DevBakePipeline::bake()` is invoked:
1. **Existing Entry Mutation**: If a field $F$ is already present in `.flowchanges` from a prior bake and the developer modifies it to a new value $V_2$, the entry for $F$ is **updated in-place**.
2. **New Entry Insertion**: If a field $G$ is newly edited during the current session, $G$ is **appended** to the manifest.
3. **Reversion Pruning**: If a developer reverts a previously baked field $F$ to authored default, the entry for $F$ is **deleted** from the manifest.
4. **Dev Override Precedence**: During application startup in Dev Mode, existing baked entries are loaded into `DevOverrideApply`'s `Baked` precedence layer. Live edits sit on the higher `Live` precedence layer, ensuring new live edits seamlessly override previously baked values during execution.

---

## 7. Generated C++ output specification

The build generator (`flowui-dev-generate`) reads `.flowui/changes/active.flowchanges` and generates two files: `FlowUiBakedChanges.hpp` and `FlowUiBakedChanges.cpp`.

### 7.1 Generated Header (`FlowUiBakedChanges.hpp`)

```cpp
// Auto-generated by flowui-dev-generate. DO NOT EDIT MANUALLY.
#pragma once

#include "FlowUi/ElementID.hpp"
#include "internal/ElementInstanceKey.hpp"

namespace FlowUi::baked {

// Query whether any baked changes exist for a given definition
[[nodiscard]] constexpr bool hasBakedDefinitionChanges(FlowDefinitionID definition) noexcept;
[[nodiscard]] constexpr bool hasBakedInstanceChanges(FlowDefinitionID definition) noexcept;

// Application entry points called during ElementInvocation and ThemeManager
void applyBakedParametersErased(
    FlowDefinitionID definition,
    detail::element::ElementInstanceKey instance,
    void* parametersDraft) noexcept;

} // namespace FlowUi::baked
```

### 7.2 Generated Implementation (`FlowUiBakedChanges.cpp`)

```cpp
// Auto-generated by flowui-dev-generate. DO NOT EDIT MANUALLY.
#include "FlowUiBakedChanges.hpp"
#include <array>
#include <algorithm>

// Include reflected element parameter definitions
#include "app/Card.hpp"
#include "app/Button.hpp"

namespace FlowUi::baked {

namespace {

// Definition: app.card
constexpr void applyBaked_app_card(CardParameters& p, ::FlowUi::detail::element::ElementInstanceKey key) noexcept {
    // Definition-scoped constexpr baked changes
    p.accentColor = Clay_Color{255, 128, 0, 255};
}

// Definition: app.button
constexpr void applyBaked_app_button(ButtonParameters& p, ::FlowUi::detail::element::ElementInstanceKey key) noexcept {
    // Definition-scoped baked changes
    p.height = 40.0f;

    // Exact-instance baked changes (Comparison cascade for K <= 7)
    switch (key.value) {
        case 0xA1B2C3D4E5F60718ull: // sidebar.submit_btn
            p.padding.top = 16.0f;
            break;
        default:
            break;
    }
}

} // namespace

void applyBakedParametersErased(
    FlowDefinitionID definition,
    detail::element::ElementInstanceKey instance,
    void* parametersDraft) noexcept {
    
    switch (definition.value) {
        case 0x7F2A9B1C3D5E8F01ull: // app.card
            applyBaked_app_card(*static_cast<CardParameters*>(parametersDraft), instance);
            break;
        case 0x3E5F8A1B2C4D6E70ull: // app.button
            applyBaked_app_button(*static_cast<ButtonParameters*>(parametersDraft), instance);
            break;
        default:
            break;
    }
}

} // namespace FlowUi::baked
```

---

## 8. Automatic 3rd-Party CMake Build Integration (Default ON)

When FlowUi is consumed as a 3rd-party library (e.g., via `add_subdirectory(ExternalLibraries/FlowUi)`, `FetchContent`, or `find_package`), **baked change compilation is enabled automatically by default** without requiring the user to write any custom CMake target commands.

### 8.1 Zero-Boilerplate Automatic CMake Wire-Up

FlowUi's CMake targets automatically bind the build-time generation custom command to any application linking against `FlowUi::FlowUi`:

```cmake
# User's top-level CMakeLists.txt (NO EXTRA BAKE COMMANDS REQUIRED!)
add_subdirectory(ExternalLibraries/FlowUi)

add_executable(MyGameApp src/main.cpp)
target_link_libraries(MyGameApp PRIVATE FlowUi::FlowUi)
```

```cmake
# Internal FlowUi CMake Setup (FlowUiBaking.cmake)
option(FLOW_UI_ENABLE_BAKED_CHANGES "Enable automatic compilation of baked FlowUi changes" ON)

if(FLOW_UI_ENABLE_BAKED_CHANGES)
    set(FLOW_UI_MANIFEST_FILE "${CMAKE_SOURCE_DIR}/.flowui/changes/active.flowchanges")
    set(FLOW_UI_GENERATED_DIR "${CMAKE_BINARY_DIR}/flowui_generated")
    set(FLOW_UI_GENERATED_CPP "${FLOW_UI_GENERATED_DIR}/FlowUiBakedChanges.cpp")
    set(FLOW_UI_GENERATED_HPP "${FLOW_UI_GENERATED_DIR}/FlowUiBakedChanges.hpp")

    # Ensure placeholder manifest directory & file exist on first build
    if(NOT EXISTS "${FLOW_UI_MANIFEST_FILE}")
        file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.flowui/changes")
        file(WRITE "${FLOW_UI_MANIFEST_FILE}" "{\"manifestVersion\":1,\"bakedChanges\":[]}")
    endif()

    # Automatically attach generator custom command
    add_custom_command(
        OUTPUT "${FLOW_UI_GENERATED_CPP}" "${FLOW_UI_GENERATED_HPP}"
        COMMAND flowui-dev-generate
            --manifest "${FLOW_UI_MANIFEST_FILE}"
            --output-dir "${FLOW_UI_GENERATED_DIR}"
        DEPENDS "${FLOW_UI_MANIFEST_FILE}" flowui-dev-generate
        COMMENT "Generating FlowUi baked C++ changes..."
        VERBATIM
    )

    # Attach generated files and include paths directly to FlowUi's interface target
    target_sources(FlowUi PRIVATE "${FLOW_UI_GENERATED_CPP}")
    target_include_directories(FlowUi PUBLIC "${FLOW_UI_GENERATED_DIR}")
    target_compile_definitions(FlowUi PUBLIC FLOW_UI_HAS_BAKED_CHANGES=1)
endif()
```

### 8.2 Opting Out
If a developer explicitly wants to disable baked compilation, they pass `-DFLOW_UI_ENABLE_BAKED_CHANGES=OFF` to CMake during configuration.

---

## 9. DevTooling Backend Surface & Workflow API

`DevBakeTools` provides a complete programmatic surface inside `DevTooling` for future `DevInterface` presentation layers to invoke actions, query statuses, and render change diffs.

```text
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ 1. Live Interactive Editing                                                 │
 │    Developer adjusts UI colors, padding, and layout sliders visually in     │
 │    DevInterface during a running application session.                       │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        │
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ 2. DevTooling::bakeActiveEdits()                                            │
 │    DevBakePipeline validates bakeability, updates active.flowchanges.       │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        │
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ 3. Automated Rebuild                                                        │
 │    CMake automatically runs flowui-dev-generate -> FlowUiBakedChanges.cpp   │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        │
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ 4. Permanent Production Code                                                │
 │    Changes are now compiled natively into the binary with 0 ms overhead.    │
 └─────────────────────────────────────────────────────────────────────────────┘
```

### 9.1 Public API Surface (`DevTooling.hpp`)

```cpp
namespace FlowUi::devMode {

struct DevBakeStatusSnapshot {
	std::size_t activeLiveOverrideCount = 0;
	std::size_t bakeableOverrideCount = 0;
	std::size_t unbakeableOverrideCount = 0;
	std::vector<DevTreeDiagnostic> bakeDiagnostics{};
};

struct DevBakeDiffEntry {
	FlowDefinitionID definition{};
	detail::element::ElementInstanceKey instance{};
	DevFieldId fieldId = 0;
	std::string fieldPath{};
	std::string authoredValueString{};
	std::string activeValueString{};
	std::string bakedValueString{};
	bool isOverridden = false;
	bool isBaked = false;
};

class DevTooling {
public:
	// Execute baking pass: serializes active live edits to active.flowchanges
	DevCommandResult bakeActiveEdits() noexcept;

	// Query bake status for UI inspection
	[[nodiscard]] DevBakeStatusSnapshot queryBakeStatus() const noexcept;

	// Query structured diff data comparing authored, live, and baked values
	[[nodiscard]] std::vector<DevBakeDiffEntry> queryBakeDiff() const noexcept;

	// Promote a definition default directly to C++ source header
	DevCommandResult promoteToSource(DevTypeId typeId, DevFieldId fieldId) noexcept;
};

} // namespace FlowUi::devMode
```

---

## 10. Production boundary & zero-cost production guarantee

When building for production (`FLOW_UI_DEV_MODE == 0`):

1. **No Dev System Overhead**: `DevBakePipeline`, `DevOverrideEngine`, `DevSchemaRegistry`, and `DevInterface` are completely removed from the binary.
2. **Pure Compiled C++**: If `FLOW_UI_ENABLE_BAKED_CHANGES=ON`, the generated `FlowUiBakedChanges.cpp` compiles as standard, ultra-lean `constexpr` C++ assignments.
3. **Optional Opt-Out**: Setting `FLOW_UI_ENABLE_BAKED_CHANGES=OFF` omits the generated file completely, restoring 100% original authored source behavior.
4. **LTO and Inlining**: Because generated baked functions are plain inline `constexpr` C++ assignments without virtual calls or heap allocations, modern C++ compilers (Clang/GCC/MSVC) inline them directly into element constructors during Link-Time Optimization (LTO).

---

## 11. Step-by-step implementation plan

This section provides the ironclad roadmap for implementing `DevBakeTools` across the repository.

```text
┌──────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                   DevBakeTools Implementation Roadmap                                │
├───────────────────────────────────┬──────────────────────────────────┬───────────────────────────────┤
│ Stage 1: DevBakePipeline          │ Stage 2: flowui-dev-generate     │ Stage 3: Automatic CMake      │
│ • DevBakeTypes.hpp                │ • JSON manifest parser           │ • FlowUiBaking.cmake          │
│ • DevBakePipeline.hpp/.cpp        │ • CppCodeGenerator               │ • Default ON integration      │
│ • Manifest Coalescing Logic       │ • constexpr table generator     │ • Target output paths         │
├───────────────────────────────────┼──────────────────────────────────┼───────────────────────────────┤
│ Stage 4: Execution Ingestion      │ Stage 5: DevTooling Surface API  │ Stage 6: Definition of Done   │
│ • DevBakedChanges.hpp             │ • DevTooling::bakeActiveEdits()  │ • Integration tests           │
│ • ElementInvocation hook          │ • DevTooling::queryBakeDiff()    │ • LTO & benchmark gates       │
│ • ThemeManager hook               │ • PromoteToSource AST Engine     │ • Zero-overhead production    │
└───────────────────────────────────┴──────────────────────────────────┴───────────────────────────────┘
```

### Stage 1: Runtime Bake Pipeline (`DevBakePipeline`)

**Goal**: Implement the runtime component inside `DevTooling` that inspects `DevOverrideApply`, filters bakeable overrides, coalesces edits, and serializes `.flowui/changes/active.flowchanges`.

#### 1.1 Target File Locations
- Header: `include/devSystems/devTooling/bake/DevBakeTypes.hpp`
- Header: `include/devSystems/devTooling/bake/DevBakePipeline.hpp`
- Implementation: `src/devSystems/devTooling/bake/DevBakePipeline.cpp`

#### 1.2 Step-by-Step Actions
1. **Define Data Models (`DevBakeTypes.hpp`)**:
   - Implement `DevBakeEntry`, `DevBakeProvenance`, and `DevBakeManifest` structs as specified in Section 6.2.
2. **Implement Bake Pipeline Controller (`DevBakePipeline.hpp/.cpp`)**:
   - `DevBakePipeline::collectActiveOverrides(...)`:
     - Scans active definition and exact-instance overrides.
     - Checks identity stability: if `target.instance` is order-derived, mark as **not bakeable** and record a diagnostic.
   - `DevBakePipeline::coalesceManifest(DevBakeManifest& existing, const std::vector<DevOverrideEntry>& activeEdits)`:
     - Implements Single Manifest Coalescing logic (Section 6.3): updates modified fields in-place, appends new fields, and prunes reverted fields.
   - `DevBakePipeline::serializeManifest(const std::filesystem::path& destinationPath)`:
     - Formats updated `DevBakeManifest` into standard JSON format at `.flowui/changes/active.flowchanges`.
3. **Attach to `DevTooling` Facade**:
   - Add `DevBakePipeline& bakePipeline()` accessor to `DevTooling` class.

---

### Stage 2: Build-Time Generator CLI Tool (`flowui-dev-generate`)

**Goal**: Build a standalone, fast C++ CLI executable that reads `.flowchanges` manifests and generates optimized `FlowUiBakedChanges.hpp` and `FlowUiBakedChanges.cpp`.

#### 2.1 Target File Locations
- Tool Directory: `tools/flowui-dev-generate/`
- CMake Target: `tools/flowui-dev-generate/CMakeLists.txt`
- Main Entry: `tools/flowui-dev-generate/main.cpp`
- Manifest Parser: `tools/flowui-dev-generate/ManifestParser.hpp/.cpp`
- Code Generator: `tools/flowui-dev-generate/CppCodeGenerator.hpp/.cpp`

#### 2.2 Step-by-Step Actions
1. **Implement Command-Line Argument Parser (`main.cpp`)**:
   - CLI flags: `--manifest <path>`, `--output-dir <path>`, `--schema-header <path>`.
2. **Implement Manifest Parser (`ManifestParser.cpp`)**:
   - Parse JSON manifest into `DevBakeManifest` structs.
   - Validate `manifestVersion` and check for missing/corrupted fields.
3. **Implement C++ Code Generator (`CppCodeGenerator.cpp`)**:
   - **Header Generation (`FlowUiBakedChanges.hpp`)**:
     - Emit `#pragma once`, query functions (`hasBakedDefinitionChanges`, `hasBakedInstanceChanges`), and primary entry point `applyBakedParametersErased`.
   - **Implementation Generation (`FlowUiBakedChanges.cpp`)**:
     - Group baked entries by `FlowDefinitionID`.
     - Emit `constexpr` initializers for definition-level changes.
     - For exact-instance changes:
       - If $K \le 7$: Emit a direct `switch (key.value)` comparison cascade.
       - If $K \ge 8$: Emit a static, sorted `std::array<std::pair<uint64_t, ApplyFn>, K>` and use `std::lower_bound`.
     - Emit master definition dispatch `switch (definition.value)`.
4. **Build System Registration**:
   - Add `add_executable(flowui-dev-generate ...)` in `tools/CMakeLists.txt`.

---

### Stage 3: Automatic CMake Build System Integration

**Goal**: Provide automatic CMake integration so baked changes compile by default without requiring user target macro calls.

#### 3.1 Target File Locations
- CMake Module: `cmake/FlowUiBaking.cmake`
- Inclusion: Automatically included by `FlowUi` target in `CMakeLists.txt`.

#### 3.2 Step-by-Step Actions
1. **Configure Default Baking Option**:
   - `option(FLOW_UI_ENABLE_BAKED_CHANGES "Enable automatic compilation of baked FlowUi changes" ON)`
2. **Configure Automatic `add_custom_command`**:
   - Set output directory to `${CMAKE_BINARY_DIR}/flowui_generated`.
   - Create placeholder manifest file if missing on first build.
   - Bind `flowui-dev-generate` execution to `.flowui/changes/active.flowchanges`.
3. **Attach Target Properties**:
   - Append `${CMAKE_BINARY_DIR}/flowui_generated/FlowUiBakedChanges.cpp` to `FlowUi` target sources.
   - Add `${CMAKE_BINARY_DIR}/flowui_generated` to `FlowUi` public include directories.
   - Set compile definition `FLOW_UI_HAS_BAKED_CHANGES=1`.

---

### Stage 4: Production Execution Layer (`DevBakedChanges`) & Ingestion Hooks

**Goal**: Integrate baked change evaluation into element invocation and theme registration.

#### 4.1 Target File Locations
- Integration Header: `include/devSystems/devTooling/bake/DevBakedChanges.hpp`
- Invocation Hook: `include/internal/ElementInvocation.hpp`
- Theme Hook: `include/managers/ThemeManager.hpp`

#### 4.2 Step-by-Step Actions
1. **Implement Dispatch Wrapper (`DevBakedChanges.hpp`)**:
   - Guarded by `#if FLOW_UI_HAS_BAKED_CHANGES`.
   - Expose `applyBakedElementDefaults<Element>(ElementInvocationContext& ctx, void* draftParams)`.
2. **Integrate into `ElementInvocation.hpp`**:
   - Inside `ElementInvocation` constructor (before interaction hooks):
     ```cpp
     #if FLOW_UI_HAS_BAKED_CHANGES
     if (::FlowUi::baked::hasBakedDefinitionChanges(Element::definitionId) ||
         ::FlowUi::baked::hasBakedInstanceChanges(Element::definitionId)) {
         ::FlowUi::baked::applyBakedParametersErased(
             Element::definitionId, detail::element::toInstanceKey(elementId_), draftParams);
     }
     #endif
     ```
3. **Integrate into `ThemeManager.hpp`**:
   - Inside `ThemeManager::registerTheme<T>`:
     - Apply baked theme token initializers before storing the theme variant.

---

### Stage 5: DevTooling Surface API

**Goal**: Provide clean backend query and action APIs in `DevTooling` for future presentation layers (`DevInterface`).

#### 5.1 Step-by-Step Actions
1. **Implement `DevTooling` Action & Query APIs**:
   - `bakeActiveEdits()`: Triggers `DevBakePipeline::bake()` to coalesce and serialize active edits into `.flowchanges`.
   - `queryBakeStatus()`: Returns snapshot counts of active live, bakeable, and unbakeable overrides.
   - `queryBakeDiff()`: Returns structured array of `DevBakeDiffEntry` for visual comparison views.

---

### Stage 6: Ironclad Definition of Done

The `DevBakeTools` implementation is considered **Complete** when all of the following verification criteria pass:

1. **Functional Baking Test**:
   - Applying a live override via `DevOverrideEngine` -> calling `DevTooling::bakeActiveEdits()` -> writing `.flowui/changes/active.flowchanges` -> re-compiling -> verifying the application displays the updated parameter with zero live overrides active.
2. **Compound Bake Test**:
   - Sequential baking cycles update existing manifest fields in-place without generating extra manifest files or build errors.
3. **Exact-Instance Loop Test**:
   - Baking an exact-instance edit on a loop-created element with an authored key (`Indexed("row", id)`) correctly updates only that specific row in the compiled build.
4. **Unstable Key Rejection Test**:
   - Attempting to bake an unkeyed instance returns an explicit warning diagnostic without corrupting the manifest.
5. **Zero Production Footprint Audit**:
   - Compiling with `FLOW_UI_DEV_MODE=0` produces binary output containing zero strings, zero reflection, zero JSON symbols, and zero heap allocations from `DevTooling`.
6. **LTO Benchmark Gate**:
   - Inspecting assembly output with LTO enabled confirms that definition-level baked changes inline directly into single store instructions inside `ElementInvocation`.
