# FlowUi DevTooling — Initial Architecture and Capability Report

## 1. Purpose and development status

This report begins the design of the second FlowUi developer subsystem:
**`DevTooling`**.

The three canonical developer systems are now:

1. **`DevMonitoringAndReporting`** — read-only capture, retained evidence,
   statistics, correlation, and human-readable reports.
2. **`DevTooling`** — actionable development functionality: validated edits,
   runtime intervention, execution control, overlays, asset catalogues, and the
   production change pipeline.
3. **`DevInterface`** — the FSEL/custom-element user interface through which a
   developer inspects data and operates DevTooling.

`DevTooling` was previously called `DevFunctionality`. The old presentation
shell called `DevTooling` is now conceptually `DevInterface`; the source-tree
rename has not yet happened. This report uses only the canonical names.

This is an initial architecture report, not the final implementation plan. Its
purpose is to inventory the current code, define the responsibility boundary,
split the large domain into understandable functional parts, identify viable
options, and establish the questions that focused follow-up designs must settle.

The central design rule is:

> Monitoring tells us what happened. DevTooling safely changes what will happen
> next. DevInterface lets a developer request and understand those operations.

DevTooling is therefore not merely “the backend of a property panel.” It is the
application-owned authority for every development operation that can alter live
FlowUi behavior or produce an approved production input.

---

## 2. Scope and boundaries

## 2.1 What DevTooling owns

DevTooling owns the write-side workflows behind:

- reflecting editable user and library types;
- resolving effective element parameters;
- temporary, definition-wide, and exact-instance overrides;
- validation, transactions, undo/redo, and change provenance;
- converting approved live edits into durable typed production changes;
- semantic state, resource, theme, and action mutations;
- development-only interaction forcing and input simulation;
- cooperative pause, frame stepping, and supported execution checkpoints;
- application-window picking and post-layout overlay injection;
- current resource catalogues and safe development actions such as reload or
  rebuild;
- isolated element previews and deterministic development scenarios;
- commands which connect the future DevInterface to normal FlowUi managers
  without letting widgets mutate manager internals directly.

Some output from DevTooling intentionally survives into production: an approved,
generated, typed change artifact. The live registry, editor values, histories,
debug strings, command queue, and Interface do not.

## 2.2 What DevTooling does not own

DevTooling does not own:

- timing, memory, or error telemetry;
- historical event retention and statistical reports;
- the visual composition of the developer window;
- arbitrary application memory editing;
- arbitrary C++ source-level stepping;
- a replacement for GDB, Valgrind, RenderDoc, or system profilers;
- unrestricted callback invocation, network replay, or application-state
  serialization;
- a second general-purpose UI/layout engine.

It may consume immutable snapshots and identities from
`DevMonitoringAndReporting`, but it must never edit a monitoring record. It may
provide models and commands to `DevInterface`, but it must not depend on FSEL
widgets or presentation state.

## 2.3 Current state versus history

The current `DevRuntime` captures an element tree, parameter snapshots, and live
overrides per `UiManager`. The upgraded system should be application-owned, with
per-window adapters, because changes, themes, actions, assets, persistence, and
execution control cross window boundaries. Window identity remains part of any
target where the same semantic element identity can appear in more than one
window.

The application is intended to be a singleton, but the design should still keep
ownership explicit: one `App` owns one DevTooling session; a global static
registry may contribute immutable schemas, but must not become the mutable
session itself.

---

## 3. Repository baseline

## 3.1 Strong foundations to preserve

The current developer implementation already provides useful foundations:

- `DevRegistry` records element definitions, parameter/state/resource struct
  types, fields, enums, and capture/apply functions.
- registration macros compile out when `FLOW_UI_DEV_MODE` is disabled;
- `DevRuntime` distinguishes definition-level and exact-instance parameter
  overrides;
- element capture records definition identity, instance identity, source
  location, authored keys, and display metadata;
- exact runtime targeting uses the numeric `ElementInstanceKey`, not the debug
  label;
- runtime edits can be exported, and the standalone updater can patch a subset
  of definition defaults and element construction expressions;
- normal element creation already passes through a centralized invocation path,
  which is the correct place to resolve authored, baked, and live parameter
  layers;
- interaction, input-field, popup, theme, action, image, icon, font, storage, and
  renderer systems already have semantic manager boundaries that DevTooling can
  adapt rather than bypass.

These are not prototypes to discard. Stable definition/instance identity,
typed element knowledge, and one invocation gate are the load-bearing parts of
the new system.

## 3.2 Immediate correctness and maturity gaps

The current implementation also exposes several gaps that should shape the
first stages:

1. Development parameter overrides are currently applied after interaction
   hooks. Hooks can observe authored parameters while build/construct observes
   effective parameters.
2. State and resource override maps exist, but the normal state/resource paths
   do not generally apply or populate them. Generic UI controls currently imply
   a capability the runtime does not safely provide.
3. Reflection is a list of field names, type hashes, raw member-pointer bytes,
   and hard-coded conversion functions. It is not yet an editor schema.
4. Enums are restricted to `uint8_t`; nested user structs, optionals, containers,
   constraints, units, resource keys, action slots, and semantic adapters are
   absent or special-cased.
5. Type and field hashes alone do not form a durable persistence schema. Rename,
   migration, version, and build compatibility are not represented.
6. Registry discovery depends heavily on static registration and must be audited
   for initialization order and linker dead stripping.
7. The source updater depends on source position and recognizable call syntax.
   It cannot represent two durable values for instances produced by one loop or
   one nested build expression.
8. Change export is a manual path, not an atomic reviewed workflow integrated
   into the application build.
9. The legacy developer panel is built inside the application's Clay tree. It
   affects layout and measured work and should not dictate the new backend.
10. Resource managers provide useful individual lookups, but not uniform,
    immutable catalogue snapshots suitable for an Interface.

The first DevTooling work should make existing promises truthful before adding
new breadth.

---

## 4. Recommended architecture

DevTooling should be one App-owned facade with several narrow internal services,
not a collection of unrelated public managers and not one giant mutable
`DevRuntime`.

```text
App
└── DevTooling                         write-side authority
    ├── DevSchemaRegistry              types, fields, elements, adapters
    ├── DevOverrideEngine              effective parameter layers
    ├── DevChangeSet                   transactions, validation, undo/redo
    ├── DevBakePipeline                manifest and typed generation input
    ├── DevMutationService             state/resource/theme/action commands
    ├── DevInteractionController       visual forcing and input simulation
    ├── DevExecutionController         pause and stepping
    ├── DevOverlayService              picking and overlay command sidecar
    ├── DevAssetCatalogue              icons, images, fonts, themes, actions
    └── DevScenarioService             previews and deterministic replay
         │
         ├── reads immutable inspection/report models
         │   from DevMonitoringAndReporting
         ├── calls narrow manager adapters at safe points
         └── exposes query models and commands to DevInterface
```

These are semantic boundaries, not a requirement for ten heap-allocated manager
objects. Early implementations may group closely related services. Keeping their
interfaces distinct prevents the registry, overlay renderer, change history,
and execution controller from becoming inseparable later.

## 4.1 One command path

DevInterface should not receive mutable manager references. It should submit
typed commands to DevTooling:

```cpp
DevCommandResult DevTooling::submit(const DevCommand& command);
DevToolingSnapshot DevTooling::snapshot() const;
```

The exact surface can be more strongly typed than one variant, but all mutation
requests should share these semantics:

1. resolve the target and show its scope;
2. validate against the current schema and manager state;
3. stage the operation in a transaction;
4. apply it only at a declared safe point;
5. return an explicit applied/rejected/deferred result;
6. record provenance and an undo operation when reversal is supported;
7. report failures through the frozen ErrorContract and Dev error monitoring.

Commands that need GPU/resource lifetime work may complete asynchronously. The
Interface should receive a command ID and status rather than block or retain raw
manager pointers.

## 4.2 Safe application points

Ordinary live edits should be applied between frame transactions, after prior
submissions have been accounted for and before affected elements are invoked
again. Resource rebuilds may require retirement over later submission serials;
“accepted” therefore does not always mean “GPU destruction completed.”

No DevTooling command should mutate:

- a parameter object while an element callback is using it;
- persistent state concurrently with its logic callback;
- a resource while the renderer or an in-flight submission owns it;
- the element tree or overlay buffers from a foreign thread.

The initial system can require all command submission and application on the
App platform thread. A future remote Interface can enqueue POD/owned commands
through a bounded thread-safe ingress without changing manager ownership.

---

## 5. Schema and reflection 2.0

The registry should mature from a field copier into the shared schema used by
inspection, validation, editing, manifest serialization, and code generation.

## 5.1 Stable schema identity

Each registered entity needs explicit durable identity:

- `DevTypeId` and `DevSchemaVersion`;
- `DevFieldId`, with migration aliases for renamed fields;
- `FlowDefinitionID` for element definitions;
- stable enum value IDs/numeric values;
- a build/schema fingerprint used to reject incompatible change manifests.

Compiler-derived type tokens and source metadata remain valuable diagnostics,
but should not be the only durable key. A source line is provenance, not
identity.

## 5.2 Field schema

A mature field descriptor should describe both data and permitted behavior:

```cpp
struct DevFieldSchema {
    DevFieldId id;
    DevTypeId ownerType;
    DevTypeId valueType;
    DevValueKind kind;
    DevFieldFlags flags;
    DevEditorHint editor;
    DevConstraints constraints;
    DevApplyPolicy applyPolicy;
    DevPersistencePolicy persistence;
    DevCaptureFn capture;
    DevValidateFn validate;
    DevApplyFn apply;
    DevResetFn reset;
};
```

Useful metadata includes:

- display name, description, category, ordering, advanced/hidden status;
- minimum, maximum, step, precision, unit, scale, and normalization rules;
- read-only, sensitive, runtime-only, preview-only, and bakeable flags;
- enum and bit-flag choices independent of underlying integer width;
- nested structs and explicitly bounded arrays/small containers;
- optional values and variants with known alternatives;
- resource-key, theme-token, element-ID, and action-slot semantics;
- default/reset provider and validation explanation;
- whether an edit is a direct parameter copy, frame-boundary state command, or
  resource rebuild operation.

Pointer-like values, spans, callbacks, opaque handles, and borrowed data should
be view-only unless a semantic adapter explicitly owns their edit lifecycle.
Callbacks and captured lambdas must never be persisted as bytes.

## 5.3 Type adapter model

The present duplicated type-specific capture/apply logic should converge on a
registered type-adapter model. Built-in adapters cover scalar C++ values, common
Clay values, strings, colors, IDs, and resource keys. Users can register:

- a reflected aggregate adapter;
- a custom scalar/editor adapter;
- a semantic state mutation adapter;
- a resource construction/rebuild adapter;
- an action-slot or recipe adapter.

This preserves the convenience of macros for normal structs while allowing
types with invariants to participate safely. Registration should generate typed
trampolines rather than make raw member-pointer byte storage the public
foundation.

## 5.4 Arbitrary user structs

User-defined parameter structs should support recursive reflection. Registration
must be able to express nested fields without requiring the developer to flatten
every Clay or application type by hand. A useful target shape is:

```cpp
FLOWUI_DEV_SCHEMA(CardParameters, 2,
    FLOWUI_DEV_FIELD(title),
    FLOWUI_DEV_FIELD(layout, .category = "Layout"),
    FLOWUI_DEV_FIELD(accent,
        .editor = DevEditorHint::Color,
        .persistence = DevPersistencePolicy::Bakeable));
```

The exact macro syntax should be designed separately. The important contract is
that the same typed registration drives live capture, editor validation, change
serialization, and generated production application.

## 5.5 Registration reliability

Static side-effect registrars are convenient but fragile in static libraries.
Preferred options are:

1. descriptors referenced by the registered element definition;
2. an explicit generated catalogue linked from App initialization;
3. static registrars retained only as a compatibility/convenience path with
   duplicate-ID diagnostics.

The build should emit a catalogue/fingerprint report so a missing schema is
detected before a developer assumes a field is editable.

---

## 6. Parameter editing and effective values

Parameter editing is the first-class capability and should be completed before
generic state or resource editing.

## 6.1 One effective-parameter pipeline

Every callback which receives parameters must see the same effective object:

```text
authored/default parameters
  -> baked definition override
  -> baked exact-instance override
  -> live definition override
  -> live exact-instance override
  -> temporary preview override
  -> field/type validation and normalization
  -> interaction hooks
  -> runLogic
  -> buildElement / constructElement
  -> effective-value capture with provenance
```

This corrects the current hook/build mismatch. The snapshot should distinguish:

- authored value;
- effective value;
- the winning layer;
- all shadowed override layers;
- validation/normalization performed;
- whether the instance identity is durable enough to bake.

## 6.2 Scopes

Initial edit scopes should remain deliberately small and predictable:

- **preview** — isolated scenario only;
- **exact instance** — one stable instance in one window or an explicitly global
  identity;
- **definition** — every invocation of one definition;
- **reviewed batch** — an explicit list of exact targets.

An arbitrary CSS-like selector language is not recommended initially. It is easy
to create changes whose match set silently expands after code changes. Future
key-family or source selectors may be added only if the Interface always shows
the current match count and the manifest records the matching policy.

## 6.3 Impact preview

Before a definition edit is committed, DevTooling should return all currently
known affected instances so the Interface can highlight them. This closes a
common DX trap: turning an intended one-instance adjustment into an application-
wide change.

---

## 7. Change transactions

All write operations should feed a `DevChangeSet`, including parameter edits,
theme changes, supported state commands, resource rebuilds, and later action
bindings.

A transaction should provide:

- validation before commit;
- atomic application at a safe point;
- grouped edits for slider drags and multi-field changes;
- undo/redo where the adapter can supply a valid inverse;
- explicit temporary, saved, baked, and source-promoted states;
- dirty state and last-save result;
- diff by definition, instance, window, theme, action, or resource;
- authoring and effective-value provenance;
- conflict detection when code/schema/resource generations change;
- atomic manifest writing and interrupted-save recovery.

Continuous controls should preview freely but coalesce into one reviewed change
when interaction ends. Undo is a semantic inverse transaction, not a blind copy
of raw memory.

The change model should also support branches or named working sets later, but a
single active working set with checkpoints is enough for the first version.

---

## 8. Durable production changes

Durable instance-specific changes are the hardest and most valuable part of the
system. They cannot be solved reliably by adding more patterns to the existing
text updater.

## 8.1 Why the current source updater cannot be the foundation

Changing this expression is straightforward:

```cpp
ui.createElement(kCard, "settings")
    .setParameters(CardParameters{.padding = 12});
```

But one expression in a loop may produce many semantic instances:

```cpp
for (const Row& row : rows) {
    ui.createElement(kRow, Indexed("row", row.id))
        .setParameters(makeRowParameters(row));
}
```

There is no separate source initializer to patch for one `row.id`. Rewriting the
call either changes every row or must invent application logic and storage that
the author never requested. Nested build callbacks have the same limitation.

Source position is also unstable under formatting, macros, helpers, generated
code, and ordinary refactoring. Source rewriting remains useful only when the
developer explicitly promotes a simple change into authored code.

## 8.2 Recommended default: a versioned change manifest and typed generator

DevTooling should atomically save approved edits into a versioned manifest. A
build-time FlowUi generator validates that manifest against the registered
schema and emits typed C++ which is compiled into the application:

```text
.flowui/changes/active.flowchanges
    -> flowui-dev-generate
    -> build/.flowui/generated/FlowUiBakedChanges.cpp
    -> normal application link
```

Each manifest entry should contain at least:

- manifest, generator, and schema versions;
- build/schema fingerprint;
- definition ID;
- target scope and stable instance selector when applicable;
- stable field ID/path and value type;
- a typed value, never a formatted C++ fragment as the source of truth;
- source/authoring provenance for review;
- optional migration aliases and applicability diagnostics;
- explicit status: active, disabled, unresolved, or rejected.

Production code must not contain JSON parsing, `DevValue`, the live registry,
editor strings, or hash maps. Generated functions/tables apply typed values at
the centralized element invocation boundary.

## 8.3 Stable instance identity

Exact-instance persistence works only when the instance has a stable semantic
identity. A source line plus loop index is not sufficient. Good identities are:

- an explicit `GlobalFlowID`;
- an authored stable key derived from application domain identity;
- a stable hierarchical identity composed from a stable parent and authored
  child key.

Automatically/order-derived identities may be used for a live temporary edit,
but DevTooling should mark them **not bakeable**. The UI should explain why and
offer the authored call site where a stable key can be added. It must never claim
that an unstable exact-instance edit will persist reliably.

For the same semantic identity in multiple windows, the manifest must state
whether the target is global or includes `WindowId`/a stable window key. This
decision cannot be inferred silently.

## 8.4 Precedence in development and production

The baked artifact participates in the same effective-value pipeline in both
build modes:

```text
authored -> baked definition -> baked instance -> live definition
         -> live instance -> preview
```

The live layers compile out of production. A development build therefore shows
the exact value production would receive before additional live experimentation.

## 8.5 Performance contract

The generated layer should guarantee:

- no allocation, parsing, reflection, strings, or dynamic registry lookup in
  production;
- typed values and typed application functions;
- definition changes grouped per definition and eligible for inlining/LTO;
- exact-instance dispatch grouped per definition using compact sorted tables,
  switches, or generated comparisons;
- measurable benchmark and binary-inspection gates.

An exact-instance value has an irreducible identity decision unless application
code selects a specialized definition directly. The system should not promise
universal zero-cost equivalence with hand-authored code.

For a singleton definition whose complete override and construction inputs are
compile-time values, the generator may emit `constexpr` data or a specialized
definition path. `consteval` is only valid when the entire operation is required
and able to execute at compile time; a runtime-created element cannot generally
be “consteval-ed.” The practical optimization is typed constexpr data, inlining,
dead-code elimination, and no runtime reflection.

## 8.6 Source promotion

Two optional promotion paths remain useful:

- a reviewed snippet/patch for straightforward definition defaults or unique
  construction sites;
- a future Clang-based typed rewrite tool for safer source-aware promotion.

Ambiguous loop, macro, helper, computed, or nested cases should stay in the typed
artifact or require manual authoring. DevTooling should never guess and rewrite
application semantics destructively.

---

## 9. State, resources, themes, and actions

These categories must not reuse parameter editing merely because their fields
can be reflected.

## 9.1 Persistent element state

State is owned across frames and may have invariants. The default capability is
inspection only. Editable state requires an opt-in semantic adapter which can:

- snapshot an owned value;
- validate a requested transition;
- apply it at a frame boundary;
- describe whether it is temporary or bakeable;
- provide an inverse when undo is supported;
- reject edits while the instance is absent, busy, or incompatible.

Examples include setting a slider value, resetting a text field, or selecting a
tab through the same semantic operation normal code would use. Arbitrary writes
to reflected state memory are forbidden.

## 9.2 Resources

Resources have construction, sharing, GPU upload, retirement, and failure
semantics. An editable resource field therefore needs a rebuild/replace adapter,
not a member-pointer write. Useful commands include:

- reload an image or SVG source;
- rebuild a font or icon atlas variant;
- evict a selected cache entry;
- replace a stable resource key;
- rebuild definition resources from a validated construction descriptor.

The command result should distinguish accepted, rebuilding, applied, retired,
and failed states.

## 9.3 Themes

Themes should be edited at their typed token/variant origin whenever possible.
DevTooling should enumerate registered variants, stage token changes at the
ThemeManager's safe mutation boundary, preview their impact, and bake typed theme
initializers. If many instance fields share one theme source, the tool should
suggest promoting the change to that token rather than retaining hundreds of
instance overrides.

## 9.4 Actions

Action tooling should build on stable `AppActionID` and registered UI action
recipes. An editable action slot needs a declared signature/event meaning;
candidate actions need compatible argument/result schemas and safety metadata.

DevTooling may support explicit invocation of actions marked preview-safe.
Externally visible or destructive actions should be disabled by default and
require a policy/confirmation from DevInterface. Captured lambdas and raw
callback addresses are neither editable nor persistable.

---

## 10. Interaction forcing and input control

“Always hovered” and “locked pressed” contain two different operations which
must be explicit:

1. **visual forcing** changes the pseudo-state used to choose appearance;
2. **behavior simulation** injects a semantic input transition and may execute
   hooks/actions.

A developer inspecting button styling usually wants the first and must not
accidentally submit a purchase, delete data, or open a network request.

Recommended forceable visual states include:

- hovered;
- pressed/held;
- focused;
- disabled;
- selected/checked where the element exposes that visual semantic;
- text caret and selection visualization;
- popup/anchor visualization without automatic action invocation.

Visual forcing should be an ephemeral exact-instance layer with clear priority
and a one-click clear operation. It should enter the element's effective
interaction view through a narrow `DevInteractionController`, not mutate Clay's
global pointer arrays or persistent application state invisibly.

Behavior simulation should instead use the normal input/focus/action boundaries:

- press, hold, release, click, key, text, focus, blur;
- pointer position and capture policy;
- input field caret/selection requests;
- popup outside-press/dismiss sequences.

Every behavior simulation must show the intended target and whether actions may
run. Input-driven edits belong in transactions only when they produce an
explicit editable value; ordinary application side effects are not automatically
undoable.

---

## 11. Freeze and stepping

DevTooling should offer cooperative execution control, not arbitrary thread
suspension.

## 11.1 Frame pause state machine

A useful initial state machine is:

```text
Running
  -> PauseRequested
  -> PausedAtFrameBoundary
  -> StepOneFrameRequested
  -> RunningOneFrame
  -> PausedAtFrameBoundary
```

Pause is honored only at an App-owned quiescent point where:

- no frame-local arena data is half-constructed;
- no manager lock or Vulkan recording transaction must remain held;
- resource retirement and platform maintenance have a defined policy;
- the pinned inspection snapshot is complete.

The application process and DevInterface must remain responsive while UI time is
paused. Platform events may still be pumped into a separate queue; the developer
chooses whether a stepped frame consumes frozen, live, or replayed input.

## 11.2 What “step one frame” means

The step contract should state whether it advances:

- application delta time;
- animations/timers;
- one window or every active window;
- queued input;
- resource uploads and retirement;
- action callbacks;
- presentation.

Recommended default: one complete normal App frame transaction using a frozen
input snapshot and a configurable fixed delta. Multi-window stepping needs an
explicit ordered frame group rather than assuming all applications draw every
window equally.

## 11.3 Sub-frame stepping

The current FlowUi build path is synchronous C++. It cannot safely suspend in an
arbitrary element callback and later resume the stack without a debugger,
coroutines, or a major lifecycle rewrite. Three distinct features should not be
confused:

1. **recorded-event stepping** — walk through already captured timing/error/
   breadcrumb events while execution remains paused; safe and valuable;
2. **coarse live checkpoints** — stop between App-owned phases such as input,
   element build, layout completion, preparation, recording, submit, and
   presentation, but only where the phase boundary is resumable;
3. **arbitrary source stepping** — debugger territory and out of scope.

Implement recorded-event stepping first, full-frame stepping second, and only
add coarse live checkpoints after each boundary has an explicit lifetime and
rollback audit. Never pause while user code is on the stack merely to emulate a
debugger.

## 11.4 Break conditions

Later, DevTooling can pause at the next safe boundary when a condition observed
by Monitoring occurs:

- a selected error code/site;
- a frame exceeding a budget;
- a selected element invocation;
- a resource allocation/growth threshold;
- an explicit developer watch condition.

Monitoring detects and records the condition; DevTooling owns the actionable
pause request.

---

## 12. Picking and the renderer-only overlay sidecar

The overlay must not create a second Clay tree or inject debug elements into the
application tree. Doing so would change the layout, input, IDs, render-command
counts, and performance being inspected.

## 12.1 Data flow

The proposed path is:

```text
captured Flow/Clay identity
  + post-layout geometry/clip/z/input snapshot
  + DevInterface-selected overlay mode
  -> DevOverlayService
  -> bounded DevOverlayCommandBuffer
  -> renderer dev sidecar pass after application UI
```

The renderer already handles rectangle, text, and texture work. The new hook is
a separate command ingestion lane which reuses existing instance generation and
pipelines where possible. It is “renderer-only” in the sense that it requires no
overlay layout tree and ideally no new shader family; it still needs a narrow
renderer bridge and a well-defined final ordering pass.

The overlay command vocabulary should initially contain:

- filled/stroked rectangle;
- line/ruler/guide;
- text label;
- texture quad for atlas/resource previews;
- optional polyline when layout traces require it.

## 12.2 Overlay modes

Useful modes include:

- element border, content, padding, and clipped regions;
- parent-owned gap and alignment regions, labelled accurately rather than called
  CSS margin;
- direct-child/all-child bounds;
- min/max/resolved sizing guides;
- scroll viewport and content extents;
- floating anchor and popup attachment/adjustment;
- pointer hit box, capture, hover, press, focus, and forced state;
- text line boxes, baselines, glyph bounds, caret, and selection;
- render-run boundaries and texture changes;
- attached identity, source, dimensions, constraints, and warning text.

Overlay modes are configured by DevInterface, but DevTooling resolves targets
and builds commands. The sidecar must never participate in application hit
testing.

## 12.3 Picking

Picking uses the retained post-layout geometry and actual clip/z/hit-test order.
It should support:

- hover candidate;
- click to lock selection;
- cycling overlapping candidates;
- selecting from Interface and highlighting in the application;
- pinning the inspected frame while the live application continues;
- source/definition/instance breadcrumbs.

The geometry snapshot belongs to read-only inspection/Monitoring. The selection,
pick request, and overlay injection are DevTooling operations.

## 12.4 Coordinate and lifetime requirements

Every overlay command needs window identity, frame generation, logical-to-
framebuffer scale, clip rectangle, and ordering. A command referencing stale
frame geometry must be rejected or rendered explicitly as a pinned historical
snapshot; it must never silently target a new instance that reused an ID.

---

## 13. Resource and definition catalogues

DevTooling should expose immutable, generation-tagged catalogues rather than let
DevInterface traverse private manager containers.

## 13.1 Icon catalogue

Useful icon entries include:

- stable resource key and source provenance;
- SVG/document status;
- cached size variants;
- atlas page, rectangle, UVs, generation, and last-use information;
- request/actual raster size;
- preview `TextureRef`;
- supported reload, evict, or prewarm commands.

The Interface can render an atlas page through its existing texture reference
and place metadata overlays over its regions. Raw GPU readback is not required
for the initial browser.

## 13.2 Image catalogue

Useful image entries include key, source path/provenance, dimensions, format,
texture generation, load/fallback state, and preview reference. Safe commands
include reload, replace source, and remove/restore where the manager can provide
valid lifetime handling.

## 13.3 Font catalogue

Useful font entries include family/name, variant, weight/style, stable font ID,
metrics, glyph coverage, fallback relationship, atlas layer/page, generation,
and a preview string. Later views can show glyph rectangles and missing-glyph
coverage without exposing mutable atlas internals.

## 13.4 Other catalogues

The same snapshot pattern should cover:

- element definitions and preview descriptors;
- theme types, variants, and tokens;
- actions, recipes, signatures, safety, and bindings;
- registered resources and lifecycle state;
- viewports and render targets when useful for previews.

Catalogue enumeration is current-state inspection, not historical telemetry.
DevTooling may cache an immutable generation snapshot for Interface stability;
Monitoring remains responsible for usage history, allocation events, and
statistics.

---

## 14. Additional DX capabilities to consider

The existing FSEL demo suggests several useful capabilities beyond a property
editor.

## 14.1 Theme-token provenance and promotion

Buttons, text fields, sliders, popups, and layout cards often repeat visual
values through nested override structs. DevTooling should identify a common
theme/token origin, preview the impact across all instances, and let a developer
promote a repeated field edit into one theme change.

## 14.2 Element isolation and state matrices

Registered preview descriptors can build one definition in an isolated ID/state
namespace. Scenarios can cover default, hovered, pressed, focused, disabled,
empty, overflow, error, theme, DPI, and localization variants. FSEL should ship
preview scenarios; application elements opt in with setup/reset functions.

This is safer than trying to instantiate arbitrary definitions from reflection:
parameters may contain actions, spans, resource keys, or required application
context.

## 14.3 Input and scenario recording

Record/replay of `FrameInput`, window dimensions/DPI, and controlled delta time
would make transient text-field, slider, popup, drag, and focus behavior
repeatable. The system must state that external threads, networking, clocks, and
application state are not deterministic unless the application registers
scenario setup/reset adapters.

## 14.4 Responsive and environment presets

Allow a preview or window to use named size, scale, DPI, input, reduced-motion,
and localization presets. These are controlled execution inputs, not fake
post-layout scaling.

## 14.5 Action and binding inspection

Show which action recipe is bound to each semantic event, its source, signature,
availability, recent result, and whether it is safe to invoke. Typed visual
binding can be added later as a change transaction with generated production
output.

## 14.6 Focus, hit-target, and popup diagnostics

The demo relies on input fields, shortcuts, stable popup anchors, pointer-follow
surfaces, and nested interaction. Useful actionable helpers include:

- request/clear focus through the owning manager;
- reveal current tab/focus order;
- highlight pointer targets and overlapping capture candidates;
- hold a popup visual open for inspection without firing its actions;
- replay outside-press/dismiss policy;
- show anchor, placement, clipping, and fallback adjustments.

## 14.7 Current-frame correctness audits

On demand, DevTooling can analyze the current inspection snapshot for:

- clipped or zero-sized interactive elements;
- undersized hit targets;
- suspicious contrast where colors are known;
- text overflow/missing glyphs;
- duplicate/unstable identities;
- focusable elements hidden by clipping;
- definition edits with unexpectedly large impact.

The analysis result is read-only evidence and may use Monitoring report models;
the “highlight/fix/promote” actions belong to DevTooling. These should be
evidence-based checks, not universal styling rules.

## 14.8 Watches and safe breakpoints

A developer may register watches such as “pause when this popup appears,” “pause
after this action,” or “highlight every instance whose effective padding exceeds
32.” Watches resolve through stable schemas and identities and pause only at a
safe boundary.

## 14.9 Build and reload assistance

DevTooling may save a manifest, invoke a user-configured build/relaunch helper,
and restore workspace/selection after reconnect. This is build and reload, not
arbitrary C++ hot reload. Dynamic-library ABI and live-state migration are a
separate project.

## 14.10 Capabilities best left outside the first system

The following are useful but should not expand the initial core:

- arbitrary application variable editors;
- remote network tooling protocol;
- dynamic C++ code reload;
- screenshot/golden test orchestration;
- IDE process launching and source editor integration;
- accessibility standards certification;
- unrestricted scripting against manager internals.

DevTooling can later provide stable command/query hooks for external helpers
without owning all of them.

---

## 15. Configuration

The current `DevToolsConfig` mixes panel presentation, capture, and override file
options. With the canonical split, configuration should eventually become:

```text
AppConfig.dev
├── monitoringAndReporting   capture levels, retention, report policies
├── tooling                  editing, persistence, execution, overlay policies
└── interface                companion/docked UI and shortcuts
```

Candidate DevTooling configuration includes:

- enable live mutation separately from read-only inspection;
- manifest path and autosave policy;
- generated artifact/build integration mode;
- maximum undo/change/overlay command capacity;
- default edit scope and impact-preview requirement;
- whether behavior simulation and action invocation are permitted;
- pause input/time policies and fixed step delta;
- resource rebuild/reload permissions;
- source promotion helper configuration;
- schema mismatch policy: reject by default, explicit migration only;
- production baked-change feature switch.

Panel width, panel shortcut, companion window placement, and internal Interface
element visibility belong to `DevInterfaceConfig`, not DevTooling.

Defaults should be conservative: parameter previews enabled, persistent mutation
reviewed, action invocation disabled, unstable-instance baking rejected, and
source rewriting never automatic.

---

## 16. Safety, errors, and performance

## 16.1 Error integration

DevTooling does not create a second error system. Failed commands produce the
frozen `FlowUiError` contract and flow through the App's configured
`ErrorObserver`; DEV monitoring enriches them with command, target, schema,
transaction, and source breadcrumbs.

A rejected edit is normally a recoverable command result, not an exception or
fatal termination. Internal invariant failure, corrupt generated schema, or an
unsafe lifecycle violation may use the normal ErrorPolicy resolution.

## 16.2 Bounded development work

Even in DEV mode, hot paths should remain explicit and measurable:

- parameter resolution performs no per-field allocation during element invoke;
- immutable schemas are registered once;
- override stores are pre-grouped/indexed by definition and instance;
- overlay command buffers are bounded and report truncation;
- catalogue snapshots rebuild only when their manager generation changes;
- continuous edits coalesce rather than append unbounded transactions;
- deep validation/generation occurs off the invocation hot path;
- DevMonitoring records DevTooling's own CPU and memory cost separately.

Production builds without baked changes should contain none of the live tooling
paths. Production builds with baked changes contain only the generated typed
application layer and its identity dispatch.

## 16.3 Capability security

The in-process first version can trust the local developer, but commands should
still carry capability metadata. This prepares for a future companion or remote
transport without exposing arbitrary memory/function calls. Sensitive reflected
fields should default to hidden/redacted and never enter manifests without
explicit opt-in.

---

## 17. Proposed development stages

These stages are an initial decomposition. Each major area should receive a
focused design report before implementation where its contract is still open.

### Stage 0 — canonical names and truthful baseline

1. Rename the old `DevFunctionality` shell/domain to `DevTooling` and the old
   presentation `DevTooling` shell to `DevInterface`.
2. Inventory the legacy `devMode` implementation under the new boundaries.
3. Move effective parameter resolution before every parameter-consuming hook.
4. Hide or remove non-functional generic state/resource editing promises.
5. Document current identity, precedence, DEV-off, and per-window ownership.

### Stage 1 — schema and parameter foundation

1. Design stable type/field IDs, schema versions, migration aliases, and build
   fingerprinting.
2. Replace duplicated hard-coded conversions with type adapters.
3. Add constraints, nested structs, broad enum support, validation, and
   provenance.
4. Move live override ownership into one App-owned DevTooling session with
   per-window targets.
5. Implement the single effective-parameter pipeline and regression tests.

### Stage 2 — transactional live editing

1. Introduce typed commands and safe-point application.
2. Add `DevChangeSet`, coalescing, undo/redo, impact preview, and atomic saves.
3. Expose authored/effective/layered values through an Interface-neutral model.
4. Mark instance identities as temporary or bakeable with explicit reasons.

### Stage 3 — production persistence

1. Define the versioned change manifest.
2. Implement schema validation and stale-entry diagnostics.
3. Generate typed definition and exact-instance applications.
4. Integrate generation into CMake and produce a human-readable build report.
5. Add production exclusion, binary, benchmark, and behavior-equivalence tests.
6. Retain source updater support only as a compatibility/promotion path.

### Stage 4 — semantic mutations and interaction controls

1. Define state mutation and resource rebuild adapter contracts.
2. Add visual-state forcing separately from behavior simulation.
3. Wire input-field focus/caret, popup, and selected FSEL state operations through
   their normal managers.
4. Add theme mutation and action-slot schemas.
5. Make unsupported raw state/resource fields visibly read-only.

### Stage 5 — inspection selection, picking, and overlays

1. Consume post-layout geometry/clip/z/input snapshots.
2. Add shared selection, pinning, reverse hit testing, and overlap cycling.
3. Define the bounded renderer sidecar command buffer and final overlay pass.
4. Add box, spacing, text/caret, clip, popup, and interaction modes.
5. Measure and expose overlay cost separately from application rendering.

### Stage 6 — execution control

1. Define App quiescent points and the pause state machine.
2. Implement pause/resume and one-frame stepping with explicit input/time policy.
3. Add recorded-event stepping using Monitoring data.
4. Audit and optionally add coarse library-owned live checkpoints.
5. Add safe-boundary watches triggered by errors, timing, memory, or identity.

### Stage 7 — catalogues, previews, and DX workflows

1. Add immutable manager catalogue adapters for icons, images, fonts, themes,
   actions, resources, and element definitions.
2. Add preview descriptors and isolated FSEL/application scenarios.
3. Add input/scenario record and replay.
4. Add theme/action workflows and current-frame correctness audits.
5. Hand stable query/command models to DevInterface.

### Stage 8 — Interface readiness and cleanup

1. Remove backend dependencies on legacy `devFlowElements` widgets.
2. Verify every operation can be driven headlessly without DevInterface.
3. Freeze DevTooling's public/internal consumer contract.
4. Build the new FSEL-based DevInterface as a consumer rather than a privileged
   manager peer.

---

## 18. Focused follow-up reports

Before broad implementation, the following reports would turn this initial map
into concrete contracts:

1. **`DevToolingSchemaAndOverrides.md`** — schema v2, value representation,
   adapters, invocation order, scopes, provenance, validation, and transactions.
2. **`DevToolingPersistence.md`** — stable identity, manifest format, generator,
   CMake integration, source promotion, migrations, and performance gates.
3. **`DevToolingMutationAndInteraction.md`** — state/resource/theme/action
   adapters, visual forcing, behavior simulation, and safety.
4. **`DevToolingOverlayAndPicking.md`** — geometry contract, command sidecar,
   renderer integration, coordinate spaces, picking, and overlays.
5. **`DevToolingExecutionControl.md`** — pause lifecycle, frame groups, time/input
   policies, recorded versus live stepping, and watches.
6. **`DevToolingCataloguesAndPreviews.md`** — manager snapshots, asset browsers,
   isolated elements, scenario inputs, and reload operations.

The first two are the most load-bearing because every later DevInterface edit
depends on stable schema, identity, scope, and persistence semantics.

---

## 19. Decisions recommended now

The following decisions are already supported strongly enough by the current
code and development goals to treat as the initial direction:

1. DevTooling is one App-owned write-side authority with narrow internal
   services, not one public manager per feature.
2. DevInterface consumes commands and immutable models; it never mutates manager
   storage directly.
3. Parameter editing is completed first and all callbacks receive one effective
   parameter value.
4. Reflection becomes a stable, versioned editor schema with semantic adapters.
5. Generic state/resource byte editing is rejected; those paths require explicit
   mutation/rebuild adapters.
6. Exact-instance live overrides remain, but only stable authored identities may
   be baked.
7. A generated typed artifact is the default production persistence mechanism.
   Source promotion is optional and reviewed.
8. Visual state forcing never invokes behavior implicitly; behavior simulation
   is a separate explicit command.
9. Pause occurs only at App-owned safe points. Recorded-event and frame stepping
   precede any coarse live sub-frame stepping.
10. Overlays use a post-application renderer sidecar and do not create a second
    Clay tree.
11. Asset/definition browsers receive immutable manager catalogue snapshots and
    use semantic commands for changes.
12. DEV-off builds remove all live Tooling machinery; only explicitly approved
    typed generated changes may remain.

## 20. Open design choices

Focused reports still need to decide:

- the exact schema declaration syntax and whether an explicit generated registry
  replaces all static registration;
- the durable representation of hierarchical instance identity and stable
  window identity;
- the manifest encoding and migration policy;
- whether generated exact-instance lookup uses per-definition switches, sorted
  tables, or generated perfect hashing based on entry count;
- whether live changes are shared across all windows by default or require an
  explicit window scope;
- which state/resource adapters FSEL guarantees in its first release;
- the exact App frame boundary at which commands and pause requests commit;
- whether the overlay buffer is translated into existing Clay-compatible render
  commands or enters a separate renderer-native command type;
- which catalogue fields can be exposed without coupling the schema to Vulkan
  implementation details;
- which build/reload operations FlowUi owns versus delegates to a configured
  external helper.

These choices do not weaken the overall architecture. They define the next
design work.

---

## Final assessment

DevTooling has a clear semantic split even though its features are broad. Its
core is not overlays, pausing, or an asset browser individually. Its core is a
**validated command and change system built on stable FlowUi identities and typed
schemas**. Runtime overrides, persistent production changes, semantic state
operations, forced interactions, execution control, renderer overlays, and
resource catalogues are specialized users of that same authority.

The current Dev Registry and exact-instance override store are valuable seeds,
but they should be matured rather than expanded in place. The decisive upgrade
is to make edits transactional and identity-aware, then connect them to a typed
generated production artifact. That removes prop drilling for approved instance
values without pretending source expressions can identify loop-created
instances, and it preserves a small, optimizable production path.

With that foundation, DevInterface can later be ambitious without becoming
privileged or fragile: it will simply present immutable evidence and submit
well-defined DevTooling commands.
