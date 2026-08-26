# FlowUi DevOverrideEngine — Implementation Design

## 1. Purpose and subsystem boundaries

This report specifies the design and implementation of **`DevOverrideEngine`**, the operational live-editing engine of the **`DevTooling`** subsystem in FlowUi.

`DevSystems` is structured into three canonical developer subsystems:

1. **`DevMonitoringAndReporting`** — read-only telemetry, retained evidence, profilers, memory accounting, error tracking, and statistics *(Status: Closed)*.
2. **`DevTooling`** — actionable development functionality: validated edits, runtime intervention, tree capture, schema registry, overrides, and change pipelines *(Status: Active)*.
3. **`DevInterface`** — the developer user interface (FSEL/tool window) through which a developer inspects data, executes actions, and alters live properties *(Status: Future)*.

Within `DevTooling`, previous reports established:
- **`DevTreeCapture`** — captures correlated Flow and Clay element forests, layout geometry, clip boundaries, and node ownership.
- **`DevSchemaRegistry`** — provides compile-time field/struct declarations (ADL `consteval` + macros), ingests descriptors into immutable `DevSchemaGeneration` generations, and equips types/fields with typed operational trampolines (`DevTypeOps`, `DevFieldOps`).

`DevOverrideEngine` is the direct functional continuation of `DevSchemaRegistry`. It consumes published schema generations and operational field trampolines to manage runtime overrides. Its primary responsibilities are:

- maintaining a validated memory store of active parameter, theme, state, and resource overrides;
- applying stored overrides to element parameter objects during frame construction;
- capturing post-logic active values for inspection by `DevInterface`;
- staging, validating, and committing live edits submitted by `DevInterface` via atomic frame-boundary transactions (`DevChangeSet`);
- self-monitoring its own timing and memory footprint via `DevTiming` and `DevMemory`.

This design requires **no backward compatibility** with legacy developer runtime paths.

---

## 2. Core architectural decisions

The architecture is built upon the following principles:

1. **Complete separation of Override Application (`DevOverrideApply`) and Value Observation (`DevOverrideCapture`).**
   - The engine does not mix override storage with live value capture. Override memory stores only explicit runtime modifications. Live observation captures actual post-logic effective values per frame.
2. **Strict execution ordering for override application.**
   - Overrides are applied *after* authored code mutates parameters via `.setParameters(...)` / `.mergeParameters(...)`, but *before* interaction callbacks (`onPressed`, `onHeld`, `onHovered`), `runLogic()`, and `buildElement()` / `constructElement()` execute.
3. **Observation captured post-logic.**
   - Active field values are recorded during tree capture finalization *after* interaction callbacks and element logic have completed.
4. **Schema-driven validation and application.**
   - Edits are validated against the active `DevSchemaGeneration` before staging. Applying values to parameter objects uses typed trampolines (`DevFieldOps::applyReplacementToDraft`), preserving C++ memory safety without raw byte manipulation or dynamic `reinterpret_cast`.
5. **Atomic frame-boundary transactions.**
   - All runtime edits from `DevInterface` are enqueued into a thread-safe `DevChangeSet` and applied at a safe frame boundary between frame transactions.
6. **Zero production cost.**
   - Guarded by `FLOW_UI_DEV_MODE`. When disabled (`FLOW_UI_DEV_MODE == 0`), all override stores, tables, transaction queues, and invocation hooks compile out completely.

---

## 3. Execution ordering: The Apply vs. Capture separation

### 3.1 The timing problem

During an element's lifecycle within a frame, parameters undergo mutations from multiple sources:

```text
[ Authored Construction ]
       │  ui.createElement(kButton, "ok").setParameters(ButtonParams{.label = "OK"})
       ▼
[ Dev Override Application ]  <── MUST OCCUR HERE
       │  Applies retained live/baked overrides to draft parameter object
       ▼
[ Interaction Callbacks ]
       │  onPressed(), onHeld(), onHovered() read effective parameters and mutate state
       ▼
[ Element Logic & Build ]
       │  runLogic(), buildElement() / constructElement() execute using effective parameters
       ▼
[ Post-Build Completion ]   <── ACTIVE OBSERVATION CAPTURE
```

If override application and value capture were combined into a single "capture-and-override" step, two failure modes would occur:

1. **Applying overrides too early (before `.setParameters()`):**
   - The authored `.setParameters(...)` call would immediately overwrite any dev override set by the developer.
2. **Applying overrides too late (after interaction callbacks):**
   - Interaction hooks (`onPressed`, `onHeld`) would execute using authored parameter values instead of dev-overridden effective values, causing visual/behavioral divergence between interaction handling and rendering.
3. **Overwriting override memory with post-interaction mutations:**
   - If override memory updated its stored values from post-interaction parameters, manual developer overrides would be overwritten whenever interaction hooks dynamically modified parameter values.
4. **"First-time capture only" fragility:**
   - If the system only captured parameters on the very first frame an element was drawn, subsequent dynamic logic changes, state transitions, or code-driven parameter updates would never be visible in `DevInterface`.

### 3.2 The dual-surface solution

To resolve these conflicts, `DevOverrideEngine` is divided into two decoupled components:

```text
               ┌─────────────────────────────────────────────────────────┐
               │                      DevInterface                       │
               └──────────────┬──────────────────────────▲───────────────┘
                              │ Submits                  │ Queries Active
                              │ DevOverrideCommand       │ Values & Overrides
                              ▼                          │
┌────────────────────────────────────────────────────────┼──────────────┐
│ DevOverrideEngine                                      │              │
│                                                        │              │
│  ┌──────────────────────────────────────────┐          │              │
│  │ DevOverrideApply (Retained Memory Store) │          │              │
│  └────────────────────┬─────────────────────┘          │              │
│                       │ Mutates params before hooks    │              │
│                       ▼                                │              │
│  ┌─────────────────────────────────────────────────────┴───────────┐  │
│  │                    Element Invocation Pipeline                  │  │
│  │  1. Authored .setParameters() / .mergeParameters()              │  │
│  │  2. DevOverrideApply::applyOverrides()  <── (In-place mutation)  │  │
│  │  3. Interaction Callbacks (onPressed, onHeld, onHovered)        │  │
│  │  4. Element Logic & buildElement() / constructElement()         │  │
│  └────────────────────┬────────────────────────────────────────────┘  │
│                       │                                               │
│                       │ Reads active post-logic values                │
│                       ▼                                               │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │ DevOverrideCapture (Active Observation Surface)                 │  │
│  └─────────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────────┘
```

1. **`DevOverrideApply` (Override Storage & Application)**:
   - Owns the retained memory store containing explicitly authored developer overrides (definition overrides, exact-instance overrides, baked patches, ephemeral previews).
   - Invoked during element construction immediately after `.setParameters()` / `.mergeParameters()` complete.
   - If an active override exists for the target element and field, it invokes `DevFieldOps::applyReplacementToDraft` to mutate the draft parameters in-place *before* interaction hooks run.
   - Never modifies its stored override values based on frame execution results.

2. **`DevOverrideCapture` (Observation Surface)**:
   - Captures active, post-logic effective parameter values during tree capture finalization (*after* `buildElement` / `constructElement` finish).
   - Stores active values in a double-buffered snapshot accessible by `DevInterface`.
   - Never writes into `DevOverrideApply`'s memory store.

3. **`DevInterface` Display Logic**:
   - To render property editor fields for a selected element instance, `DevInterface` queries **`DevOverrideCapture`** to obtain the active observed values.
   - `DevInterface` simultaneously queries **`DevOverrideApply`** to check if an active override entry exists for each field path.
   - If an override entry exists in `DevOverrideApply`, `DevInterface` displays the field with an **"Overridden"** indicator (and exposes a "Reset to Authored" control).
   - If no override entry exists, `DevInterface` displays the active observed value cleanly without an override badge.
   - This eliminates the need for "first-time capture only" semantics while ensuring code logic never clobbers manual overrides.

---

## 4. Layered precedence and target scopes

### 4.1 Target scopes

Overrides target elements at three distinct scopes:

1. **Definition Scope (`FlowDefinitionID`)**:
   - Targets all live instances of a specific element definition (e.g., `app.card` or `fsel.button`).
   - Useful for global design tuning across the entire UI.
2. **Exact-Instance Scope (`ElementInstanceKey`)**:
   - Targets a single stable instance of an element in a window using its `ElementInstanceKey` or `GlobalFlowID`.
   - Requires a stable semantic key. If an instance identity is order-derived or unstable, the edit is allowed ephemerally but flagged as **not bakeable**.
3. **Ephemeral Preview Scope**:
   - Ephemeral, single-frame or scenario-scoped overrides used during interactive drag operations (sliders, color pickers) before committing a transaction.

### 4.2 Layered evaluation pipeline

When an element is invoked, `DevOverrideApply` evaluates active layers in ascending precedence order. Higher layers overwrite lower layers:

```text
1. Authored Parameters      (set via C++ code in .setParameters / .mergeParameters)
   └── 2. Baked Definition  (compiled from .flowchanges manifest)
       └── 3. Baked Instance    (compiled exact-instance patch)
           └── 4. Live Definition   (active DevInterface definition edit)
               └── 5. Live Instance     (active DevInterface instance edit)
                   └── 6. Ephemeral Preview (active interactive drag preview)
```

`DevOverrideApply` evaluates this pipeline for each reflected field in the element's parameter schema. If no override exists at layers 2–6, the authored value remains unchanged.

---

## 5. Implementation strategies and trade-off analysis

### 5.1 Strategy 1: Override Storage Representation

How overridden field values are stored in memory within `DevOverrideApply`.

| Strategy | Mechanism | Pros | Cons | Recommendation |
|---|---|---|---|---|
| **A: Generic String / JSON Map** | Store override values as text or JSON trees keyed by string paths (`"card.padding.top" -> "12"`). | Flexible, easily serializable, simple debugging. | High memory overhead, continuous string allocations, slow parsing/conversions on every frame invocation. | **Reject.** Violates performance contracts. |
| **B: Individual `DevOwnedValue` Heap Allocations** | Store each overridden field as an individual heap-allocated type-erased `DevOwnedValue` object in a hash map keyed by `(Target, DevFieldId)`. | Strongly typed, safe destruction via `DevTypeOps::destroyOwned`, straightforward field-level insertion/removal. | Heap fragmentation for many overrides, pointer indirection per field lookup. | **Viable alternative.** Good for modest override counts. |
| **C: Schema-Backed Coalesced Block Store (`DevOverrideArenaStore`)** | Store override values in contiguous arena memory blocks per target scope. Small values (<= 16 bytes: floats, ints, colors, enums) stored inline in flat POD arrays; large values (strings, sequences) stored in a block arena. | Zero per-field heap allocations during frame apply pass, cache-friendly linear iteration, minimal memory fragmentation. | Requires custom arena management and alignment handling for dynamic strings. | **Recommended.** Best performance and footprint characteristics. |

**Final Choice:** **Strategy C (Coalesced Block Store with Inline Small-Value Buffers).**

---

### 5.2 Strategy 2: Override Lookup & Field Application Dispatch

How `DevOverrideApply` identifies and applies overrides to an element's parameter struct during invocation.

| Strategy | Mechanism | Pros | Cons | Recommendation |
|---|---|---|---|---|
| **A: Dynamic Recursive Schema Traversal** | Traverse `DevSchemaGeneration` field-by-field on every element invocation, looking up each field in the override store. | Requires no pre-compiled tables; handles dynamic schema updates automatically. | High CPU overhead per element invocation due to recursive schema tree walking. | **Reject.** Unacceptable frame overhead. |
| **B: Pre-Compiled Definition Override Tables (`DevDefinitionOverrideTable`)** | Upon `DevSchemaGeneration` publication, pre-compile flat lookup tables for each registered `FlowDefinitionID`. The table contains direct field offsets, `DevFieldOps` function pointers, and override flags. | $O(1)$ definition lookup, array-indexed field evaluation, zero string comparisons, zero allocation during invocation. | Must be invalidated and rebuilt whenever `DevSchemaGeneration` publishes a new generation. | **Recommended.** Maximum invocation performance. |

**Final Choice:** **Strategy B (Pre-Compiled Definition Override Tables).**

---

### 5.3 Strategy 3: Transaction Ingestion & Mutability Model

How edits from `DevInterface` enter `DevOverrideEngine`.

| Strategy | Mechanism | Pros | Cons | Recommendation |
|---|---|---|---|---|
| **A: Direct In-Place State Mutation** | `DevInterface` widgets directly mutate `DevOverrideApply`'s internal maps on the UI thread as user actions occur. | Instant update, simple implementation. | Race conditions if UI and element rendering occur concurrently; unvalidated partial edits during slider drags. | **Reject.** Unsafe and unpredictable. |
| **B: Frame-Boundary Transaction Queue (`DevChangeSet`)** | `DevInterface` submits typed `DevOverrideCommand`s to a lock-free queue. `DevOverrideEngine` processes and validates commands at a declared frame boundary, applying mutations atomically to `DevOverrideApply`. | Thread-safe, supports multi-field transactions, enables undo/redo, prevents invalid/partial states, cleanly separates UI from runtime. | Single-frame latency between edit submission and application (imperceptible to human user). | **Recommended.** Safe, predictable, and clean. |

**Final Choice:** **Strategy B (Frame-Boundary Transaction Queue `DevChangeSet`).**

---

### 5.4 Strategy 4: Active Observation Capture Pipeline

How `DevOverrideCapture` records active post-logic values.

| Strategy | Mechanism | Pros | Cons | Recommendation |
|---|---|---|---|---|
| **A: Full Parameter Struct Deep Copy** | Copy the full parameter object byte-for-byte into heap memory for every element on every frame. | Simple implementation. | High allocation overhead, captures unreflected padding/bytes, requires type-specific copy constructors for all user structs. | **Reject.** Wasteful and unsafe for non-POD structs. |
| **B: Schema-Driven Compact Value Serialization** | Use `DevTypeOps::capture` and `DevFieldOps::captureMember` to write active field values into double-buffered flat string/value scratch buffers associated with `DevTreeCapture` nodes. | Schema-validated, skips unreflected/read-only data, double-buffered (zero lock contention during rendering), reusable buffer capacity across frames. | Requires integration with `DevTreeCapture` frame lifecycle. | **Recommended.** Efficient, clean, and correlated with tree inspection. |

**Final Choice:** **Strategy B (Schema-Driven Compact Value Serialization).**

---

## 6. Detailed data model and storage specifications

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "FlowUi/ElementID.hpp"
#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"
#include "internal/ElementInstanceKey.hpp"

namespace FlowUi::devMode {

// ============================================================================
// Identity & Scope Types
// ============================================================================

enum class DevOverrideScope : std::uint8_t {
	Definition,
	ExactInstance,
	EphemeralPreview,
};

enum class DevOverrideLayer : std::uint8_t {
	BakedDefinition  = 1,
	BakedInstance    = 2,
	LiveDefinition   = 3,
	LiveInstance     = 4,
	EphemeralPreview = 5,
};

struct DevOverrideTarget {
	FlowDefinitionID definition{};
	detail::element::ElementInstanceKey instance{};
	DevOverrideScope scope = DevOverrideScope::Definition;

	friend constexpr bool operator==(const DevOverrideTarget&, const DevOverrideTarget&) = default;
};

struct DevOverrideFieldKey {
	DevTypeId ownerType = 0;
	DevFieldId fieldId = 0;
	DevFieldIndex fieldIndex{};

	friend constexpr bool operator==(const DevOverrideFieldKey&, const DevOverrideFieldKey&) = default;
};

// ============================================================================
// Small-Buffer Storage for Override Values
// ============================================================================

struct alignas(8) DevSmallValueBuffer {
	static constexpr std::size_t InlineCapacity = 24;
	std::uint8_t bytes[InlineCapacity]{};
	std::uint8_t size = 0;
	bool isHeapAllocated = false;
	void* heapPayload = nullptr;
};

struct DevOverrideValue {
	DevTypeId typeId = 0;
	DevEditorKind editorKind = DevEditorKind::None;
	DevSmallValueBuffer data{};
	const DevTypeOps* ops = nullptr;

	[[nodiscard]] bool isValid() const noexcept { return typeId != 0 && ops != nullptr; }
};

// ============================================================================
// Retained Override Entry
// ============================================================================

struct DevOverrideEntry {
	DevOverrideTarget target{};
	DevOverrideFieldKey fieldKey{};
	DevOverrideLayer layer = DevOverrideLayer::LiveDefinition;
	DevOverrideValue value{};
	std::uint64_t transactionSerial = 0;
	bool isBakeable = true;
};

// ============================================================================
// Pre-Compiled Definition Lookup Table
// ============================================================================

struct DevCompiledFieldOverride {
	DevFieldId fieldId = 0;
	DevFieldIndex fieldIndex{};
	DevFieldOps ops{};
	std::uint32_t activeOverrideMask = 0; // Bitmask of active layers
	DevOverrideValue activeValues[6]{};   // Values indexed by DevOverrideLayer
};

struct DevDefinitionOverrideTable {
	FlowDefinitionID definition{};
	DevTypeId parametersType = 0;
	std::vector<DevCompiledFieldOverride> fields{};
	bool hasActiveOverrides = false;
};

// ============================================================================
// Active Observation Data Model (DevOverrideCapture)
// ============================================================================

struct DevCapturedFieldSnapshot {
	DevFieldId fieldId = 0;
	DevFieldIndex fieldIndex{};
	DevStringRef fieldName{};
	DevEditorKind editorKind = DevEditorKind::None;
	DevEditCapability editCapability = DevEditCapability::Editable;
	DevSmallValueBuffer capturedValue{};
	bool isOverridden = false;
	DevOverrideLayer winningLayer = DevOverrideLayer::LiveDefinition;
};

struct DevActiveElementSnapshot {
	FlowDefinitionID definition{};
	detail::element::ElementInstanceKey instance{};
	std::uint32_t flowNodeIndex = 0;
	std::vector<DevCapturedFieldSnapshot> fields{};
};

} // namespace FlowUi::devMode

#endif // FLOW_UI_DEV_MODE
```

---

## 7. Engine component specifications

### 7.1 `DevOverrideApply`

`DevOverrideApply` manages the active override store and executes the in-place parameter draft mutation pass during element invocation.

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <unordered_map>
#include <vector>

#include "FlowUi/ElementID.hpp"
#include "devSystems/devTooling/override/DevOverrideTypes.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

namespace FlowUi::devMode {

class DevOverrideApply {
public:
	DevOverrideApply() = default;
	~DevOverrideApply() = default;

	// Invalidate pre-compiled tables when DevSchemaGeneration updates.
	void onSchemaGenerationPublished(const DevSchemaGeneration& generation) noexcept;

	// Set or clear an override entry in retained memory.
	DevValueOperationStatus setOverride(const DevOverrideEntry& entry) noexcept;
	DevValueOperationStatus clearOverride(const DevOverrideTarget& target, DevFieldId fieldId) noexcept;
	void clearAllOverrides() noexcept;

	// Invoked by ElementInvocation after .setParameters() / .mergeParameters()
	// Mutates draftParams in-place before interaction hooks run.
	template <typename Element>
	void applyOverrides(
		FlowDefinitionID definition,
		detail::element::ElementInstanceKey instanceKey,
		void* draftParams) noexcept {
		if (!hasActiveOverrides_) return;

		const auto tableIt = compiledTables_.find(definition);
		if (tableIt == compiledTables_.end() || !tableIt->second.hasActiveOverrides) {
			return;
		}

		const DevDefinitionOverrideTable& table = tableIt->second;
		DevApplyContext applyCtx{};

		for (const DevCompiledFieldOverride& field : table.fields) {
			if (field.activeOverrideMask == 0) continue;

			// Resolve highest-priority active layer
			const DevOverrideValue* winningValue = nullptr;
			for (int layer = static_cast<int>(DevOverrideLayer::EphemeralPreview);
				 layer >= static_cast<int>(DevOverrideLayer::BakedDefinition);
				 --layer) {
				if (field.activeOverrideMask & (1u << layer)) {
					winningValue = &field.activeValues[layer];
					break;
				}
			}

			if (winningValue && winningValue->isValid() && field.ops.applyReplacementToDraft) {
				DevValueView view{winningValue->typeId, &winningValue->data};
				(void)field.ops.applyReplacementToDraft(draftParams, view, applyCtx);
			}
		}
	}

	[[nodiscard]] bool hasActiveOverrides() const noexcept { return hasActiveOverrides_; }
	[[nodiscard]] std::size_t activeOverrideCount() const noexcept { return totalActiveOverrides_; }

private:
	std::unordered_map<FlowDefinitionID, DevDefinitionOverrideTable> compiledTables_{};
	std::vector<DevOverrideEntry> rawOverrides_{};
	bool hasActiveOverrides_ = false;
	std::size_t totalActiveOverrides_ = 0;
};

} // namespace FlowUi::devMode

#endif // FLOW_UI_DEV_MODE
```

---

### 7.2 `DevOverrideCapture`

`DevOverrideCapture` records active effective parameter values after interaction callbacks and `buildElement` / `constructElement` have finished.

```cpp
#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <vector>
#include "devSystems/devTooling/override/DevOverrideTypes.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

namespace FlowUi::devMode {

class DevOverrideCapture {
public:
	DevOverrideCapture() = default;

	void beginFrame() noexcept;
	void endFrame() noexcept;

	// Captures active post-logic effective values for one element instance.
	void captureElementActiveValues(
		FlowDefinitionID definition,
		detail::element::ElementInstanceKey instanceKey,
		std::uint32_t flowNodeIndex,
		const void* effectiveParams,
		const DevSchemaGeneration& schemaGen,
		const DevOverrideApply& overrideStore) noexcept;

	[[nodiscard]] const std::vector<DevActiveElementSnapshot>& publishedSnapshots() const noexcept {
		return publishedSnapshots_;
	}

private:
	std::vector<DevActiveElementSnapshot> buildingSnapshots_{};
	std::vector<DevActiveElementSnapshot> publishedSnapshots_{};
};

} // namespace FlowUi::devMode

#endif // FLOW_UI_DEV_MODE
```

---

## 8. DevInterface ingestion & transaction interface (`DevChangeSet`)

`DevInterface` communicates with `DevOverrideEngine` through a transaction queue. Widgets must never mutate override stores directly.

### 8.1 Command vocabulary

```cpp
enum class DevCommandKind : std::uint8_t {
	SetFieldOverride,
	ClearFieldOverride,
	ResetDefinitionOverrides,
	ResetInstanceOverrides,
	ClearAllOverrides,
	BeginBatchDrag,
	UpdateBatchDrag,
	EndBatchDrag,
};

struct DevOverrideCommand {
	DevCommandKind kind = DevCommandKind::SetFieldOverride;
	DevOverrideTarget target{};
	DevOverrideFieldKey fieldKey{};
	DevOverrideValue value{};
	std::uint64_t transactionId = 0;
};

struct DevCommandResult {
	std::uint64_t transactionId = 0;
	bool applied = false;
	DevValueOperationStatus status = DevValueOperationStatus::Success;
	DevStringRef errorMessage{};
};
```

### 8.2 Ingestion workflow

1. **Submission**: `DevInterface` submits a `DevOverrideCommand` via `DevTooling::submitCommand(cmd)`.
2. **Staging**: Commands enter a thread-safe staging queue (`DevChangeSet`).
3. **Frame-Boundary Commit**: At the start of the next frame transaction (before `beginFrame`), `DevOverrideEngine` dequeues commands, validates them against `DevSchemaGeneration`, and updates `DevOverrideApply`'s compiled lookup tables.
4. **Coalescing**: Interactive drag sequences (`BeginBatchDrag` -> `UpdateBatchDrag` -> `EndBatchDrag`) coalesce intermediate edits into a single undoable transaction once the drag finishes.

---

## 9. Integration with existing DevTooling components

### 9.1 Integration with `DevSchemaRegistry`

- **Schema Generation Fingerprinting**: Every published `DevSchemaGeneration` carries a unique `DevSchemaGenerationId`. `DevOverrideApply` binds its pre-compiled lookup tables to this ID. If a dynamic schema change occurs (e.g. during dev hot-rebuild), `DevOverrideApply` invalidates and regenerates its tables.
- **Field Operations**: Field application delegates to `DevFieldOps::applyReplacementToDraft`. Value capture delegates to `DevTypeOps::capture`.

### 9.2 Integration with `DevTreeCapture`

- **Node Correlation**: During `DevTreeCapture`'s `endFlow()` finalization, `DevOverrideCapture` receives the finalized `flowNodeIndex` and pairs it with the captured `DevActiveElementSnapshot`.
- **Identity Consistency**: `DevOverrideTarget` uses the exact same `ElementInstanceKey` generated by `DevTreeCapture`, guaranteeing that selected nodes in `DevInterface`'s tree view map directly to their active parameters and override states.

---

## 10. Integration with DevMonitoring (`DevTiming` and `DevMemory`)

`DevOverrideEngine` integrates with `DevMonitoringAndReporting` to record performance and memory metrics.

### 10.1 `DevTiming` Integration

Timing spans are placed around override application and capture passes using `FLOWUI_DEV_ZONE`:

```cpp
void DevOverrideApply::applyOverridesErased(...) {
    FLOWUI_DEV_ZONE(::FlowUi::devMode::DevZoneCategory::DevTool, "DevOverrideApply::apply");
    // Execution...
}

void DevOverrideCapture::captureElementActiveValues(...) {
    FLOWUI_DEV_ZONE(::FlowUi::devMode::DevZoneCategory::DevTool, "DevOverrideCapture::capture");
    // Execution...
}
```

Key timing metrics tracked:
- `overrideApplyCpuNs` — total CPU duration spent applying overrides across all element invocations in the frame.
- `activeCaptureCpuNs` — total CPU duration spent capturing active post-logic field values into observation snapshots.
- `transactionCommitCpuNs` — duration spent committing pending `DevChangeSet` transactions at the frame boundary.

---

### 10.2 `DevMemory` Integration

`DevOverrideEngine` implements a memory probe callback to report its heap and arena allocations into `DevMemoryRecorder`:

```cpp
void DevOverrideEngine::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
    sink.addSample({
        .category = devSystems::MemoryCategory::DevTooling,
        .name = "DevOverrideApply.StoreCapacityBytes",
        .bytes = storeCapacityBytes(),
    });
    sink.addSample({
        .category = devSystems::MemoryCategory::DevTooling,
        .name = "DevOverrideApply.ActiveOverrideBytes",
        .bytes = activeOverrideBytes(),
    });
    sink.addSample({
        .category = devSystems::MemoryCategory::DevTooling,
        .name = "DevOverrideCapture.SnapshotBufferBytes",
        .bytes = snapshotBufferBytes(),
    });
}
```

Key memory metrics tracked:
- `retainedOverrideCount` — total active override entries stored.
- `overrideStoreCapacityBytes` — allocated storage capacity of `DevOverrideApply`.
- `observationSnapshotBytes` — double-buffered memory used by `DevOverrideCapture`.

---

## 11. Dev self-monitoring and metric reporting

To ensure `DevTooling` does not degrade application performance, `DevOverrideEngine` reports its own operational statistics back into `DevMonitoringAndReporting`:

```cpp
struct DevOverrideStats {
	std::uint64_t frameNumber = 0;
	std::uint32_t activeDefinitionOverrides = 0;
	std::uint32_t activeInstanceOverrides = 0;
	std::uint32_t elementsOverriddenThisFrame = 0;
	std::uint64_t applyCpuNs = 0;
	std::uint64_t captureCpuNs = 0;
	std::size_t memoryFootprintBytes = 0;
	std::uint32_t rejectedTransactionsCount = 0;
};
```

This statistics capsule is published at the end of each frame and exposed to `DevReportEngine`. If `applyCpuNs` exceeds a configured threshold (e.g. > 100 μs) or if memory capacity grows abnormally, `DevReportEngine` issues a developer finding highlighting the overhead.

---

## 12. Production boundary (`FLOW_UI_DEV_MODE == 0`)

When developer mode is disabled (`FLOW_UI_DEV_MODE == 0`):

1. `DevOverrideEngine`, `DevOverrideApply`, `DevOverrideCapture`, and `DevChangeSet` classes are omitted from compilation.
2. `ElementInvocation`'s call to `applyOverrides(...)` expands to `((void)0)`.
3. `DevTreeCapture`'s call to `captureElementActiveValues(...)` expands to `((void)0)`.
4. No override tables, value buffers, strings, or transaction queues exist in the compiled binary.
5. Absolute zero CPU and memory overhead in production builds.
