# FlowUi DevRegistry Tooling — Tree Capture and Schema Registration Design

## 1. Purpose

This report is the focused design for the first functional part of
`DevSchemaRegistry` and its immediately adjacent capture machinery. It follows
the direction established by `DevUpgradeConcepts.md` and `DevTooling.md`, but is
deliberately narrower than either report.

This work owns two related capabilities:

1. capture and retain the current UI structure for each FlowUi window;
2. register the element, parameter, and nested value schemas needed to describe
   what those captured Flow elements contain.

The primary hierarchy is the semantic **Flow element tree**. A second,
post-layout **Clay element tree** is also valuable because it contains the
actual layout containers, text nodes, floating roots, clips, and computed
geometry which the renderer consumes. They describe different relationships
and must not be collapsed into one compromised tree.

This report includes the value representation needed to capture registered
parameter objects because schema usefulness depends on it. It does not design:

- override precedence or live mutation;
- state/resource mutation commands;
- persistence, baking, or source promotion;
- picking and overlay rendering;
- the final property-panel widgets;
- long-term inspection history or `.flowtrace` serialization.

Those features will consume the models designed here, but should be specified
in later focused reports.

The central recommendation is:

> Store immutable schema separately from per-frame values, capture Flow and
> Clay as two correlated forests, and only advertise editability when a value
> has a schema and an editor contract capable of servicing it.

---

## 2. Repository baseline

## 2.1 Current Flow capture

`DevRuntime` currently rebuilds `ElementTreePlaceholder` once per window per
frame. `ElementBuilder::invoke` starts a captured Flow node, and an RAII scope
ends it after a draw callback. Constructed elements keep both the Flow scope and
capture scope open until `UiManager::drawConstructed()` or frame cleanup closes
the corresponding Clay element. This is a sound capture point and should be
preserved.

The current storage is a flat pre-order vector. Each node has a `depth`,
definition and instance identity, source location, authored names, registration
flags, and several owned `std::string` values. This is a useful prototype, but
it has important limitations:

- parent and child relationships must be reconstructed from depth every time;
- roots are only implied by `depth == 0`;
- capture begin/end operations are not paired by a token, so mismatch evidence
  is weak;
- every captured node duplicates strings;
- the mutable building frame is also the object exposed to consumers;
- no completion, truncation, or cancellation quality is represented;
- no Clay range or Clay-node correlation is retained;
- `appendCapturedClayElement` exists but has no normal call site;
- hidden internal developer nodes also hide the entire subtree rather than
  retaining a structural explanation of what was excluded.

The capture is currently window-owned through `UiManager::devRuntime()`. That is
correct for the producer because Flow and Clay frames are per-window, but the
completed snapshots should be published to the application-owned developer
session so a companion tool can query all windows uniformly.

## 2.2 Current schema registry

`DevRegistry` currently stores three vectors: structs, enums, and elements.
Registration is performed by translation-unit static side effects created by
`FLOWUI_DEV_REGISTER_STRUCT`, `FLOWUI_DEV_REGISTER_ENUM`, and
`FLOWUI_DEV_REGISTER_ELEMENT`.

The strongest parts to retain are:

- fields are declared with typed member pointers;
- typed capture/apply trampolines avoid raw offset arithmetic;
- element registration understands the current `FlowElement` concept and its
  nested `Parameters`, optional `State`, and optional `Resources` aliases;
- the complete feature compiles away when `FLOW_UI_DEV_MODE` is disabled;
- `FlowDefinitionID` already provides stable element-definition identity.

The present representation cannot be the long-term editor schema:

- type identity relies on compiler-derived type hashes/tokens;
- fields use a hash of the C++ member name with no version or rename aliases;
- enums are limited to a `uint8_t` underlying type;
- `DevValue` hard-codes several Clay composites rather than representing
  arbitrary nested data;
- nested registered application structs are not recursively captured;
- `std::string_view`, optionals, arrays, variants, resource keys, action calls,
  and most FSEL parameter shapes are unsupported;
- unsigned values above `INT64_MAX` are converted to `double` and can lose
  integer precision;
- function pointers and opaque values have no explicit safe policy;
- field descriptors describe byte operations, not constraints, grouping,
  editor support, or capture quality;
- duplicate registration returns early instead of verifying that two
  declarations are identical;
- static constructors are vulnerable to initialization order and linker dead
  stripping.

The repository documentation also contains examples of the retired
`ElementDefinition<...>` shape while the current element system uses empty tag
types with nested aliases, a stable `definitionId`, optional `debugName`, and
static callbacks. The upgraded element registry should be designed directly for
the current shape.

## 2.3 Clay already has the data, but not a public snapshot API

Clay's built-in debugger traverses `Clay_Context::layoutElementTreeRoots`,
`layoutElements`, child-index arrays, and the layout-element hash map. This
proves that Clay already owns the information needed for a complete layout
forest, including separate roots for floating elements.

However, `Clay_SetDebugModeEnabled(true)` is not a suitable data API. In the
vendored Clay version it:

- reduces the application's root width by `Clay__debugViewWidth`;
- appends Clay's own debug UI during layout completion;
- owns selection and collapsed-row presentation state;
- exposes no public function that returns the hierarchy it traverses.

Public `Clay_GetElementData()` only retrieves the final box for a known ID. It
cannot enumerate nodes, children, unnamed elements, text nodes, or floating
roots. Therefore “expose Clay's debugger data” is the right direction, but it
requires a small read-only bridge; merely enabling debug mode is not enough.

---

## 3. Decisions recommended now

1. `DevSchemaRegistry` owns immutable type and element descriptions. It does
   not own a mutable frame tree.
2. Each window owns a `DevTreeCapture` producer. Completed immutable snapshots
   are published to the application-owned DevTooling session.
3. Flow and Clay are stored as two forests in one `DevUiSnapshot`; neither is
   projected into the other's hierarchy at capture time.
4. The Flow forest records semantic composition and ownership. The Clay forest
   records actual layout containment and floating layout roots.
5. Popup and other floating trees are first-class Clay roots with explicit
   attachment and semantic-owner edges.
6. Clay data is exported after `Clay_EndLayout()` through a narrow, optional,
   read-only visitor implemented beside the vendored Clay implementation.
7. Flow-to-Clay ownership is primarily derived from Clay emission ordinal
   ranges recorded around Flow capture scopes. Exact Flow/Clay ID equality is a
   useful additional link, not the sole correlation mechanism.
8. Runtime trees use dense index-based storage, interned strings, explicit root
   lists, and parent/child ranges.
9. Schema macros define typed descriptors; they should not perform registration
   as a static-constructor side effect.
10. Element invocation automatically references its element descriptor. An
    explicit catalogue anchors elements which may not be invoked during the
    current run and provides build-time completeness.
11. Nested registered aggregates recursively produce object values. A type is
    not “arbitrarily supported” merely because its bytes can be copied.
12. Standard editable kinds have an Interface-neutral editor contract. Opaque,
    borrowed, callable, and pointer-like types are read-only, metadata-only, or
    omitted unless a semantic adapter explicitly supports them.
13. Unsupported fields are visible in registry diagnostics. The Interface must
    never silently present a disabled blank editor.
14. `FLOW_UI_DEV_MODE=OFF` removes descriptors, capture calls, strings, and the
    Clay inspection bridge.

---

## 4. Responsibility split

The feature should be implemented as three cooperating parts even if the first
version keeps them in a small number of files:

```text
App / DevTooling
├── DevSchemaRegistry                 application schema index
│   ├── type schemas
│   ├── enum schemas
│   ├── element schemas
│   ├── type adapters
│   └── validation + fingerprint
│
└── DevUiSnapshotStore               completed snapshots by window
    └── latest immutable snapshot generation

UiManager (one per window)
└── DevTreeCapture                    current-frame producer
    ├── Flow capture stack
    ├── parameter-value arena
    ├── Clay emission intervals
    ├── post-layout Clay visitor
    └── snapshot finalization/publish
```

The important ownership rule is that descriptors outlive all snapshots, while
captured values never point into a transient builder parameter object or Clay's
next-frame arena.

`DevTreeCapture` may resolve schemas while recording, but it stores stable IDs
or compact handles, not raw descriptor pointers which could be invalidated if a
late schema is discovered.

Schema strings and frame strings need different lifetime domains. Stable type
names, field names, descriptions, groups, and source locations can be interned
in the application registry. Runtime debug paths, dynamic authored labels,
captured text, and Clay ID strings must be copied into a bounded snapshot-local
string pool. Permanently interning a loop's dynamic labels in the App would turn
an inspection feature into an unbounded string leak.

---

## 5. One completed UI snapshot, two forests

The completed unit should be a per-window, per-frame value:

```cpp
struct DevUiSnapshot {
    WindowId window{};
    uint64_t frameId = 0;
    uint64_t generation = 0;
    DevSchemaGeneration schemaGeneration{};
    DevCaptureQuality quality{};

    DevFlowForest flow{};
    DevClayForest clay{};
    DevCapturedValueArena values{};
    DevFlowClayLinks links{};
};
```

This report recommends the term **forest** in APIs even though the Interface
will often display a synthetic “Window” row above it. A window can legitimately
contain several top-level Flow invocations and Clay itself explicitly has
multiple layout roots. Inventing a real root node obscures that fact and
complicates identity.

A synthetic window, “Application UI”, or “Floating roots” row is a view-model
choice. It should not consume a node index or masquerade as authored UI.

## 5.1 Snapshot quality

Every completed snapshot should say whether it is trustworthy:

```cpp
enum class DevCaptureCompletion : uint8_t {
    Complete,
    Canceled,
    UnbalancedFlowScopes,
    ClayUnavailable,
    Truncated,
    Failed,
};

struct DevCaptureQuality {
    DevCaptureCompletion completion{};
    uint32_t droppedFlowNodes = 0;
    uint32_t droppedClayNodes = 0;
    uint32_t droppedValues = 0;
    uint32_t schemaFailures = 0;
    bool containsLateSchemas = false;
    bool containsSyntheticClayNodes = false;
};
```

Capacity exhaustion must retain a valid prefix and count what was dropped. A
partial snapshot is useful if it is labelled; an apparently complete corrupted
tree is not.

---

## 6. Flow forest model

## 6.1 Recommended node shape

Use dense, frame-local indices for navigation and stable semantic IDs for
cross-frame identity:

```cpp
using DevFlowNodeIndex = uint32_t;
inline constexpr DevFlowNodeIndex InvalidFlowNode = UINT32_MAX;

struct DevFlowNode {
    DevFlowNodeIndex parent = InvalidFlowNode;
    uint32_t firstChild = 0;
    uint32_t childCount = 0;
    uint32_t depth = 0;
    uint32_t preorderEnd = 0;       // exclusive subtree end

    FlowDefinitionID definition{};
    detail::element::ElementInstanceKey instance{};
    DevTypeId paramsType{};
    DevValueNodeIndex paramsValue = InvalidValueNode;

    DevStringId debugName{};
    DevSourceLocationId source{};
    DevStringId authoredInstanceKey{};
    DevFlowNodeFlags flags{};

    uint32_t clayBeginOrdinal = 0;
    uint32_t clayEndOrdinal = 0;    // exclusive
};

struct DevFlowForest {
    std::vector<DevFlowNode> nodes;
    std::vector<DevFlowNodeIndex> children;
    std::vector<DevFlowNodeIndex> roots;
};
```

`firstChild`/`childCount` refer into a packed child-index array. This is slightly
larger than recovering children from depth, but it makes tree queries direct and
keeps sibling roots explicit. `preorderEnd` makes subtree scans and Interface
virtualization cheap.

An alternative is `firstChild` plus `nextSibling` embedded in every node. That
avoids a second vector and is convenient while building, but sibling traversal
is pointer-chasing and obtaining child counts is slower. The recommended
implementation can use temporary first/last-child links while recording and
pack the child array when finalizing.

## 6.2 Capture tokens, not only depth

Replace a bare depth stack with explicit tokens:

```cpp
struct DevFlowCaptureToken {
    DevFlowNodeIndex node = InvalidFlowNode;
    uint32_t stackGeneration = 0;
};

DevFlowCaptureToken beginFlowElement(const DevFlowBegin& begin);
void endFlowElement(DevFlowCaptureToken token);
```

The token makes these errors diagnosable:

- a constructed element closed out of order;
- a draw callback exited with an unmatched scope;
- frame cancellation abandoned open scopes;
- suppression caused an ancestor and descendant to disagree about depth.

The production `ElementBuilder` behavior does not change. In development mode,
its existing RAII scope keeps the token. `ConstructedElementFrame` keeps the
token for a constructed element and supplies it to `closeConstructedToDepth`.

## 6.3 Parent selection

On begin:

1. the parent is the top non-suppressed capture token;
2. if there is no parent, append the new node to `roots`;
3. append the node index to the parent's temporary child list;
4. record its start Clay emission ordinal;
5. capture its registered parameter value at the declared capture point;
6. push the token.

On end:

1. validate that the token is the stack top;
2. record its exclusive end Clay emission ordinal;
3. set `preorderEnd` after all descendants are known;
4. pop the token.

If error recovery must close several scopes, finalize each node with an
`AutoClosed` or `Canceled` flag rather than silently decrementing depth.

## 6.4 What the Flow hierarchy means

The Flow parent means **semantic invocation ownership**:

- a Flow element invoked while another Flow element's draw/callback scope is
  active is its semantic child;
- a Flow element created while a constructed Flow element remains open is its
  semantic child;
- a Flow element invoked directly by application UI construction with no active
  Flow scope is a Flow root;
- raw Clay declarations do not create Flow nodes.

This definition remains stable even when a child emits a floating Clay root.
It is therefore useful for finding the component responsible for a popup,
tooltip, menu, or other detached layout surface.

## 6.5 Internal developer UI

There are three viable policies:

| Policy | Benefit | Cost |
|---|---|---|
| Drop node and subtree | Lowest storage; matches current behavior | Hides why capture/timing changed and can unbalance intuitive hierarchy |
| Keep node with `Internal` flag | Complete evidence and easy filtering | Companion Interface must filter its own tree by default |
| Replace subtree with one excluded marker | Bounded and honest | Loses internal detail when diagnosing the tool itself |

The recommended default for the future separate companion Interface is to keep
internal nodes with an `InternalDev` flag and filter them in the consumer. For a
legacy in-application panel, use one excluded-subtree marker to prevent the
panel from recursively inspecting itself. Capture policy must not be encoded as
an ordinary parent-depth suppression boolean.

---

## 7. Clay forest exposure

## 7.1 Recommended bridge

Add an optional FlowUi-owned inspection bridge in `src/clay.cpp`, where the
implementation-private Clay types are already visible. Declare only public POD
views in a FlowUi internal header. The bridge walks Clay's existing root and
child arrays after `Clay_EndLayout()` and calls a visitor supplied by
`DevTreeCapture`.

Conceptually:

```cpp
struct Clay_DevRootView {
    uint32_t rootOrdinal;
    uint32_t layoutElementOrdinal;
    uint32_t attachmentParentId;
    uint32_t clipElementId;
    int16_t zIndex;
};

struct Clay_DevElementView {
    uint32_t layoutElementOrdinal;
    uint32_t clayId;
    uint32_t parentOrdinal;
    uint32_t depthWithinRoot;
    uint32_t childCount;
    Clay_BoundingBox boundingBox;
    Clay_Dimensions dimensions;
    Clay_Dimensions minDimensions;
    Clay_ElementDeclaration declaration;
    Clay_String idString;
    Clay_String text;
    Clay_TextElementConfig textConfig;
    uint32_t clipElementId;
    bool isText;
    bool exiting;
};

void visitClayLayoutForDev(
    Clay_Context*,
    const ClayDevVisitor&,
    void* userData) noexcept;
```

The exact C/C++ ABI can change. The required contract is:

- no mutation of the Clay context;
- no use of `Clay_SetDebugModeEnabled`;
- no allocation inside Clay;
- a stable declaration order/layout-element ordinal;
- explicit root records, including attachment parent, clip, and z-index;
- parent/child traversal for unnamed and text elements;
- final geometry retrieved from Clay's hash-map item;
- copied data valid after the next `Clay_BeginLayout` resets ephemeral storage.

The feature should be guarded by a Clay integration define such as
`FLOWUI_CLAY_DEV_SNAPSHOT` and only compiled when `FLOW_UI_DEV_MODE` is enabled.

## 7.2 Why a FlowUi-local bridge is recommended first

There are four practical options:

### Option A — FlowUi-local read-only visitor beside `CLAY_IMPLEMENTATION`

Pros:

- small, contained change;
- reuses Clay's authoritative arrays and exact debugger traversal semantics;
- exposes computed geometry, unnamed nodes, text nodes, clips, and roots;
- does not make FlowUi depend on Clay's built-in debug presentation.

Cons:

- must be reviewed when the vendored Clay version changes;
- is an integration extension rather than an upstream public API.

This is the recommended first implementation.

### Option B — contribute a generic snapshot/visitor API to Clay

Pros:

- clean upstream ownership;
- other Clay integrations could reuse it;
- less long-term fork maintenance if accepted.

Cons:

- upstream API design and release timing are outside FlowUi's control;
- a generic API may expose less detail than FlowUi needs.

FlowUi can design Option A so it is straightforward to upstream later.

### Option C — instrument every Clay open/configure/close macro

Pros:

- captures declaration-time ownership directly;
- can work before layout completion.

Cons:

- touches hot macro paths;
- duplicates Clay's tree construction;
- computed dimensions, bounding boxes, transition roots, and sorted z-order are
  still unavailable until later;
- raw calls to internal functions can bypass a macro-only hook.

Do not use this as the primary tree capture.

### Option D — reconstruct from render commands

Pros:

- requires no Clay internals;
- data is already returned by `Clay_EndLayout`.

Cons:

- non-rendering containers disappear;
- command order is not layout containment;
- one element can produce several commands;
- culled elements and much layout configuration are absent.

This is unsuitable for a layout inspector.

## 7.3 Clay node storage

The FlowUi-owned copy should use the same dense navigation strategy as the Flow
forest:

```cpp
struct DevClayRoot {
    DevClayNodeIndex node{};
    uint32_t attachmentParentClayId = 0;
    DevClayNodeIndex attachmentParent = InvalidClayNode;
    DevClayNodeIndex clipNode = InvalidClayNode;
    int16_t zIndex = 0;
    uint32_t declarationOrder = 0;
    uint32_t paintOrder = 0;
    DevClayRootFlags flags{};
};

struct DevClayNode {
    DevClayNodeIndex parent = InvalidClayNode;
    uint32_t firstChild = 0;
    uint32_t childCount = 0;
    uint32_t preorderEnd = 0;
    uint32_t clayId = 0;
    uint32_t declarationOrdinal = 0;
    DevStringId debugName{};
    DevClayNodeKind kind{};
    Clay_BoundingBox bounds{};
    Clay_Dimensions measured{};
    Clay_Dimensions minimum{};
    Clay_ElementDeclaration declaration{};
    DevStringId text{};
    Clay_TextElementConfig textConfig{};
    DevClayNodeFlags flags{};
};
```

Pointer-bearing declaration fields must not be persisted as meaningful values.
For example, image/custom/user pointers can be reduced to null/non-null and a
semantic resource handle if the renderer/manager can supply one. The completed
snapshot must never invite a consumer to dereference Clay arena pointers.

The full `Clay_ElementDeclaration` copy is convenient initially, but a later
compact representation may be preferable. The bridge API and snapshot storage
should therefore be separate types.

---

## 8. Sibling roots, popups, and floating subtrees

## 8.1 The same popup has two legitimate parents

Consider a combo box whose Flow callback builds a popup:

```text
Flow ownership                           Clay layout

ComboBox                                RootContainer
├── Trigger                             └── ... normal content ...
└── PopupContent                        PopupRoot [floating, z=...]
    └── Option                          └── Option rows
```

In the Flow forest, `PopupContent` belongs to `ComboBox`. In Clay, its floating
root is removed from normal child layout and appears in
`layoutElementTreeRoots`, with an attachment parent ID and z-index. Both facts
are important:

- Flow ownership answers “which component created this?”;
- Clay attachment answers “which box positions and clips this?”;
- Clay root/paint order answers “why is it above this other surface?”

The capture must not choose one and discard the other.

## 8.2 Explicit edge types

Store cross-tree and non-containment relations separately:

```cpp
enum class DevUiEdgeKind : uint8_t {
    EmittedByFlow,
    ExactIdentity,
    FloatingAttachment,
    ClipDependency,
};

struct DevFlowClayLink {
    DevFlowNodeIndex flow{};
    DevClayNodeIndex clay{};
    DevUiEdgeKind kind{};
};
```

For each `DevClayRoot`, resolve `attachmentParentClayId` to a Clay node index
after all nodes have been copied. A missing parent is preserved as an unresolved
edge and diagnostic; it is not silently attached to the main root.

The Interface can offer projections without changing storage:

- **Flow view:** popup under its semantic element;
- **Clay view:** popup under a “Floating roots” group, sorted by paint order;
- **Combined view:** Flow tree with emitted Clay descendants and a link badge
  showing the floating attachment target.

## 8.3 Other sibling roots

Clay roots may also represent tooltips, menus, drag previews, overlays authored
by the application, and synthetic transition-exit nodes. Root metadata should
therefore use neutral terms such as `Floating` and `SyntheticTransition`, not a
hard-coded `Popup` boolean.

Popup-specific identity can be added by `PopupManager` correlation later. The
tree storage should not depend on the popup manager to remain correct.

---

## 9. Correlating Flow emission with Clay nodes

## 9.1 Recommended emission-interval method

Every Clay layout element already has a stable ordinal within the current Clay
frame. Expose a development-only `clayLayoutElementCount()` from the local Clay
bridge.

At Flow begin, store the current count. At Flow end, store the new count. This
creates a half-open emission interval `[begin, end)` for each Flow node. Nested
Flow intervals naturally overlap. During finalization, assign each Clay node to
the **deepest** Flow interval containing its declaration ordinal.

Example:

```text
Card Flow node             [10, 24)
  Label Flow node          [12, 15)
  Button Flow node         [16, 22)

Clay ordinal 13 -> Label
Clay ordinal 18 -> Button
Clay ordinal 23 -> Card
```

This avoids adding an observer call to every Clay open operation and correctly
attributes raw `CLAY`, `CLAY_TEXT`, and nested Flow output.

For constructed Flow elements, the interval remains open until the constructed
Clay scope is closed, matching the current semantic capture behavior.

## 9.2 Exact identity links

When a Flow ID maps to the same numeric Clay ID as a captured Clay node, add an
`ExactIdentity` edge. This commonly identifies a component's root and is more
precise than an emission-owner link.

It must not be required because:

- draw-style elements may emit several Clay nodes;
- raw Clay roots can use unrelated IDs;
- text and anonymous elements use generated IDs;
- a Flow callback may intentionally emit no Clay node;
- 64-bit Flow identities are bridged into Clay's 32-bit ID space and collision
  diagnostics already exist.

## 9.3 Nodes created after Flow capture

Clay can create or retain synthetic transition nodes during layout completion.
Their declaration ordinal may sit outside all Flow intervals. Mark them
`Synthetic` and attempt a secondary identity link to the originating Clay ID.
Never attribute them to the last active Flow element merely because it is
convenient.

## 9.4 Complexity

A naive “test every Clay node against every Flow interval” implementation is
quadratic. Because Flow nodes are captured in pre-order and intervals are
properly nested when scopes are balanced, ownership can be resolved in one
linear scan with a stack of active intervals. Unbalanced intervals should first
be repaired/closed at frame finalization and marked incomplete.

---

## 10. Capture timing and publication

Recommended frame lifecycle:

```text
UiManager::beginFrame
  DevTreeCapture::begin(frameId)
  Clay_BeginLayout

application + Flow element construction
  begin/end Flow tokens
  capture registered params
  record Clay ordinal intervals

UiManager::endFrame
  close remaining constructed elements
  Clay_EndLayout
  visit Clay layout forest
  correlate Flow and Clay nodes
  finalize packed child ranges and quality
  publish immutable snapshot
  PopupManager::endFrame and normal cleanup
```

The Clay visit must occur after `Clay_EndLayout`, because boxes and root paint
order are final then, and before the next `Clay_BeginLayout`, because Clay's
ephemeral strings and arrays will be reset.

Frame cancellation calls `DevTreeCapture::cancel()`. It may publish a canceled
Flow-only snapshot for diagnostics, but must not replace the last complete live
snapshot unless the consumer explicitly requests failed frames.

## 10.1 Double buffering

Use a mutable building buffer and an immutable published buffer per window.
Finalization swaps them by generation. The companion Interface receives a
reference-counted or generation-checked read view and never observes vectors
while they are being appended.

Two initial storage options are viable:

| Option | Pros | Cons |
|---|---|---|
| Reused `std::vector` buffers | Simple; capacity retained; easy migration from current code | Heap-backed and can grow unexpectedly |
| Storage-system arena/typed buffers | Fits FlowUi capacity accounting and bounded behavior | More implementation work; publication lifetime must be explicit |

Start with reused vectors plus configured reserves and hard capture budgets.
Move to the storage system after the model stabilizes. Either implementation
must report reserved, used, peak, growth, and dropped counts to the already
designed development memory monitoring.

## 10.2 Retention boundary

This feature needs only the latest complete snapshot per window and optionally
the latest failed snapshot. Pinning historical frames belongs to the later
inspection/history design. To prepare for it, snapshots must be self-contained
and must reference schema entities by stable ID/generation rather than live
parameter or Clay pointers.

## 10.3 String ownership

Use two logical string tables even if both initially share one implementation:

- **schema strings** live with a schema generation and contain stable,
  low-cardinality registration metadata;
- **snapshot strings** live with one `DevUiSnapshot` and contain runtime labels,
  text copies, and Clay strings under per-frame byte/count limits.

Nodes must identify which domain an ID belongs to, either through distinct ID
types or an encoded domain bit. Snapshot publication and pinning can then move
or retain runtime strings without growing the application registry.

---

## 11. Schema registry 2.0

## 11.1 Stable identity and immutable generations

Use explicit stable IDs for types and fields:

```cpp
struct DevTypeId { uint64_t value = 0; };
struct DevFieldId { uint64_t value = 0; };
using DevSchemaVersion = uint32_t;

struct DevTypeSchema {
    DevTypeId id{};
    DevSchemaVersion version = 0;
    DevStringId cppName{};
    DevStringId displayName{};
    DevValueKind kind{};
    DevTypeFlags flags{};
    DevEditorContract editor{};
    uint32_t firstField = 0;
    uint32_t fieldCount = 0;
    DevCaptureTypeFn capture = nullptr;
};

struct DevFieldSchema {
    DevFieldId id{};
    DevTypeId owner{};
    DevTypeId valueType{};
    DevStringId cppName{};
    DevStringId displayName{};
    DevStringId description{};
    DevStringId group{};
    DevFieldFlags flags{};
    DevEditorHint editorHint{};
    DevConstraints constraints{};
    DevCaptureFieldFn capture = nullptr;
};
```

An explicit stable type key such as `"app.card-style"` is preferable to a
compiler type hash. For convenience, the field ID can default to a hash of
`type-id + member-name`. Renaming then requires an explicit new ID or alias:

```cpp
FLOWUI_DEV_FIELD(accentColor,
    FLOWUI_DEV_ID("accent"),
    FLOWUI_DEV_ALIAS("highlight"));
```

The C++ type token and source location remain useful diagnostics but are not
persistence identity.

The registry exposes immutable generations. Late-discovered schemas create a
new generation without invalidating prior ID-based snapshot references. A
canonical, ID-sorted description produces the schema fingerprint later used by
persistence and traces.

## 11.2 Element schema follows the current element shape

Element schemas should be derived from the `FlowElement` type:

```cpp
struct DevElementSchema {
    FlowDefinitionID definition{};
    DevStringId displayName{};
    DevStringId description{};
    DevStringId category{};
    DevTypeId paramsType{};
    DevTypeId stateType{};
    DevTypeId resourcesType{};
    DevSourceLocationId registrationSource{};
    DevElementFlags flags{};
};
```

`ParametersOf<Element>` is always available, including
`NoElementParameters`. `StateOf<Element>` and `ResourcesOf<Element>` are added
only when the concepts say they exist. `definitionId` is already stable. The
display-name default should be `Element::debugName` when provided, then the C++
type name—not the macro token alone.

Registration should also record whether the element is draw-style,
construct-style, stateful, resource-owning, has parts, or is internal developer
UI. These are derived facts and do not require user repetition.

Registration is not a prerequisite for structural visibility. Every valid
typed Flow invocation has a definition ID, instance ID, source, and C++ type
token and therefore appears in the Flow forest. If its parameter schema is not
available, `paramsValue` is absent and the node carries an
`UnregisteredParameters` coverage reason. This keeps application elements
inspectable while making the missing opt-in obvious.

## 11.3 Registration states

The application-scoped registry should have explicit states:

1. **Collecting built-ins** — standard scalar, string, enum, optional, Clay, ID,
   resource, and action adapters are installed.
2. **Collecting catalogue** — FSEL and application catalogues are added.
3. **Runtime discovery** — an invoked typed element can ensure its own schema is
   present.
4. **Validated generation** — IDs, recursion, adapters, and editor coverage are
   checked and a new immutable generation is published.

Runtime discovery is helpful for fast DX, but a production-quality developer
build should also contain an explicit catalogue. Otherwise a popup definition
does not appear in the library until the programmer happens to open it, and
linker dead stripping remains invisible.

---

## 12. Recursive value model

## 12.1 Replace hard-coded composite variants with a value tree

The schema is a graph of reusable types. A captured parameter object is a tree
of values conforming to that graph. Store it in a flat arena:

```cpp
enum class DevValueKind : uint8_t {
    Null,
    Bool,
    SignedInteger,
    UnsignedInteger,
    Floating,
    String,
    Enum,
    Object,
    Sequence,
    Optional,
    Variant,
    Semantic,
    Opaque,
};

struct DevCapturedValueNode {
    DevTypeId type{};
    DevFieldId field{};       // zero for the root or sequence item
    DevValueKind kind{};
    DevValueFlags flags{};
    uint32_t firstChild = 0;
    uint32_t childCount = 0;
    union {
        bool boolean;
        int64_t signedInteger;
        uint64_t unsignedInteger;
        double floating;
        DevStringId string;
        DevEnumValue enumValue;
        DevSemanticValue semantic;
    } payload{};
};
```

This immediately supports arbitrary nesting without adding `DevFloat4Value`,
`DevLayoutConfigValue`, and another top-level variant alternative for every new
struct. Clay types can be described by the same aggregate schemas as user
types, with semantic editor hints for color, sizing, padding, or alignment.

Each captured node has a status flag such as `Valid`, `Redacted`, `Truncated`,
`Unsupported`, `BorrowedCopy`, or `CaptureFailed`. This allows the Interface to
explain missing values.

## 12.2 Capture recursion

For a registered aggregate:

1. allocate an `Object` value node;
2. visit fields in schema order;
3. invoke the typed field accessor to obtain a safe const reference;
4. dispatch to the registered adapter for the field type;
5. append child values and field IDs;
6. stop at the configured depth/node/string budget and mark truncation.

Cycle detection belongs to schemas, not runtime pointer chasing. By-value C++
aggregates cannot contain an infinite by-value cycle, but adapters for pointers,
containers, or graph handles must never recurse without an explicit bounded
policy.

## 12.3 Capture time for parameters

The eventual effective-parameter pipeline must expose one explicit capture
point: after all authored/baked/live layers and validation, before any callback
which receives parameters. This fixes the current mismatch where interaction
hooks execute before development overrides.

This report does not design those layers. It only requires a call resembling:

```cpp
capture.captureParameters(flowToken, schemaFor<Parameters>(), params);
```

once the final callback-visible `params` object exists.

Capture should be policy-driven:

- **StructureOnly:** record tree and schema handles, no parameter values;
- **Selected:** capture full values for selected/watched instances and a small
  summary for the rest;
- **AllRegistered:** capture every supported registered field;
- **Deep:** include advanced and bounded container content.

`AllRegistered` is acceptable for small applications and compatibility, but
should not be the only mode. Large strings and nested FSEL parameter objects can
otherwise dominate developer-frame cost.

---

## 13. What “arbitrary type support” can honestly mean

C++ currently provides no general standard reflection that can enumerate every
member of an arbitrary type. Even if it did, arbitrary byte mutation would not
be safe. The useful target is therefore:

> Any type can participate if it is a supported generic shape, a registered
> aggregate of participating fields, or supplies a semantic adapter. Types
> without such a contract remain explicitly opaque.

## 13.1 Automatically supported generic shapes

The registry can provide type adapters for:

- all ordinary signed and unsigned integer widths;
- `float` and `double` with finite/non-finite status;
- `bool`;
- enums with any ordinary underlying integer width;
- owning strings and safely copied string views;
- registered aggregates;
- `std::optional<T>` when `T` is supported;
- `std::array<T, N>` and C arrays under a configured item budget;
- explicitly approved bounded vectors/sequences;
- `std::variant<Ts...>` when every exposed alternative is supported;
- common Clay value types described as nested schemas;
- stable FlowUi IDs, keys, and selected resource references through semantic
  adapters.

Generic `std::vector` capture should be opt-in or bounded. A property Interface
can service a fixed array, but editing a vector raises insertion, deletion,
capacity, validation, and persistence semantics outside simple field editing.

## 13.2 Borrowed strings and views

Many current element parameters use `std::string_view`. Capturing a copy for
display is safe if the copy is owned by the snapshot. Editing it is not a simple
member assignment because the destination view needs a stable backing store.

The schema should therefore initially describe `std::string_view` as:

- capture: copied string;
- view: supported;
- direct editing: unsupported;
- reason: `BorrowedLifetime`.

A later override store can own replacement text and supply a view with the
correct lifetime. The registry should not claim edit support before that
contract exists.

## 13.3 Callables and pointers

Function pointers, member-function pointers, captured lambdas, `std::function`,
raw pointers, borrowed spans, and user-data pointers should never be serialized
or exposed as editable numeric addresses.

Possible policies:

| Type | Default capture | Standard editor |
|---|---|---|
| Function/callback | presence + safe registered name if available | None |
| Raw pointer | null/non-null; optional semantic handle | None |
| Borrowed span | size and element type; content only by explicit bounded adapter | None or read-only list |
| `ActionCall` | stable action/recipe identity through action adapter | Action selector, later |
| Texture/resource ref | stable resource key and status through manager adapter | Resource selector, later |
| Opaque custom class | type name and `Unsupported` reason | None |

These fields can still be useful to inspection without pretending they are
ordinary values. If even metadata creates noise, the field registration can
mark them hidden or omitted.

## 13.4 Custom semantic adapters

An adapter is required when a type has invariants or a useful meaning different
from its members:

```cpp
template <>
struct FlowUi::devMode::DevTypeAdapter<Angle> {
    static constexpr auto schema() {
        return devScalar<Angle>(
            "app.angle", DevEditorHint::Number,
            constraints().min(-180.0).max(180.0).unit("deg"));
    }

    static bool capture(const Angle& value, DevValueWriter& out) {
        return out.floating(value.degrees());
    }
};
```

Mutation/validation functions can be added by the later override report. Tree
capture only needs the const capture contract.

---

## 14. Editor-serviceability contract

The schema registry must not construct Interface nodes or depend on FSEL. It
should publish enough semantic information for `DevInterface` to choose a
standard editor factory.

```cpp
enum class DevStandardEditor : uint8_t {
    None,
    Toggle,
    SignedNumber,
    UnsignedNumber,
    FloatingNumber,
    Text,
    EnumChoice,
    Flags,
    Color,
    Vector,
    Spacing,
    Sizing,
    ObjectGroup,
    OptionalGroup,
    Sequence,
    ResourceChoice,
    ActionChoice,
};

struct DevEditorContract {
    DevStandardEditor standard = DevStandardEditor::None;
    DevEditorAdapterId custom{};
    DevEditSupport support = DevEditSupport::ViewOnly;
    DevUnsupportedReason reason{};
};
```

The registry validation pass computes editor coverage for each reachable field:

- `EditableStandard` — a guaranteed standard Interface editor exists;
- `EditableCustom` — a named custom adapter is required;
- `ViewOnly` — capture and formatting exist, mutation does not;
- `MetadataOnly` — presence/type/status only;
- `Unsupported` — registration is incomplete or invalid;
- `Hidden` — intentionally excluded.

Nested groups require no special-case widget in the registry. The Interface
recursively walks `Object`, `Optional`, `Variant`, and `Sequence` schemas and
uses the leaf editor contracts. Group/category/order metadata affects
presentation but not value storage.

This approach addresses diminishing returns: unsupported functional fields do
not consume per-frame value storage beyond a small status node, while the
registry still explains why they cannot be edited.

---

## 15. Macro and registration DX

## 15.1 Recommended descriptor macros

Macros should generate typed descriptor specializations, not invoke a global
singleton during static initialization. A plausible user-facing shape is:

```cpp
enum class CardTone : uint16_t {
    Neutral,
    Warning,
    Success,
};

struct CardStyle {
    Clay_Padding padding{};
    Clay_Color accent{};
};

struct CardParameters {
    std::string_view title{};
    CardStyle style{};
    CardTone tone = CardTone::Neutral;
    ActionCall onActivate{};
};

struct Card {
    using Parameters = CardParameters;
    using BuildContext = ElementBuildContext<Card>;

    static constexpr FlowDefinitionID definitionId =
        DefinitionID("app.card");
    static constexpr std::string_view debugName = "Card";

    static void buildElement(BuildContext& context) {
        Clay_ElementDeclaration root{};
        root.layout.padding = context.params.style.padding;
        root.backgroundColor = context.params.style.accent;

        CLAY(context.clayID(), root) {
            CLAY_TEXT(
                context.uiManager.toClayString(context.params.title),
                CLAY_TEXT_CONFIG({}));
        }
    }
};

inline constexpr Card kCard{};
static_assert(FlowElement<Card>);

FLOWUI_DEV_ENUM_SCHEMA(
    CardTone,
    "app.card-tone",
    FLOWUI_DEV_ENUM_VALUE(CardTone::Neutral),
    FLOWUI_DEV_ENUM_VALUE(CardTone::Warning),
    FLOWUI_DEV_ENUM_VALUE(CardTone::Success));

FLOWUI_DEV_STRUCT_SCHEMA(
    CardStyle,
    "app.card-style",
    1,
    FLOWUI_DEV_FIELD(padding,
        FLOWUI_DEV_GROUP("Layout"),
        FLOWUI_DEV_EDITOR(Spacing)),
    FLOWUI_DEV_FIELD(accent,
        FLOWUI_DEV_GROUP("Appearance"),
        FLOWUI_DEV_EDITOR(Color)));

FLOWUI_DEV_STRUCT_SCHEMA(
    CardParameters,
    "app.card.params",
    2,
    FLOWUI_DEV_FIELD(title,
        FLOWUI_DEV_NAME("Title")),
    FLOWUI_DEV_FIELD(style),
    FLOWUI_DEV_FIELD(tone),
    FLOWUI_DEV_FIELD(onActivate,
        FLOWUI_DEV_SEMANTIC(ActionSlot),
        FLOWUI_DEV_VIEW_ONLY));

FLOWUI_DEV_ELEMENT_SCHEMA(
    Card,
    FLOWUI_DEV_NAME("Card"),
    FLOWUI_DEV_CATEGORY("Application/Cards"));
```

`FLOWUI_DEV_FIELD(member)` can expand inside a descriptor function containing
`using Self = CardParameters`, allowing it to form `&Self::member` without
repeating the owner type. The member type is deduced, and a typed accessor
trampoline is generated.

Element registration infers the parameter/state/resource schemas from the
current element aliases. The element macro supplies only presentation metadata
which cannot be inferred. If no element macro exists, `debugName` and type
metadata provide a valid minimal descriptor.

Nested capture works because the `style` field resolves `CardStyle`'s schema
recursively. No flattening or special composite `DevValue` is required.

The application continues to use the element normally:

```cpp
ui.createElement(kCard, "account-card")
    .setParameters(CardParameters{
        .title = "Account",
        .style = CardStyle{
            .padding = CLAY_PADDING_ALL(12),
            .accent = Flow_Color("#4567dfff"),
        },
        .tone = CardTone::Neutral,
        .onActivate = openAccountAction,
    })
    .draw();
```

In an `Inspect` capture, this produces one Flow node whose `paramsValue` is an
object. `style` is another object with `padding` and `accent` children; `tone`
is an enum choice; `title` is a copied, borrowed/view-only string; and
`onActivate` is a semantic metadata value until the later action editor is
implemented. User code does not manually push tree nodes or capture values.

## 15.2 Explicit catalogue

Applications and libraries should anchor their available definitions:

```cpp
FLOWUI_DEV_CATALOGUE(
    AppDevCatalogue,
    FLOWUI_DEV_ELEMENT(Card),
    FLOWUI_DEV_ELEMENT(SettingsPanel),
    FLOWUI_DEV_ELEMENT(AccountPopup));

auto app = makeApplication(config);
#if FLOW_UI_DEV_MODE
app.devTooling().schemas().addCatalogue(AppDevCatalogue);
#endif
```

FSEL should ship one complete catalogue. Catalogue traversal registers the
element and recursively registers parameter, state, resource, enum, and nested
aggregate schemas.

For elements omitted from a catalogue, the templated `ElementBuilder` path can
still call `ensureElementSchema<Element>()` in development mode. The registry
marks the schema `LateDiscovered` and the build report recommends adding it to a
catalogue.

A headless consumer can query the published result without depending on the
future Interface. The exact API can change, but the intended use is:

```cpp
#if FLOW_UI_DEV_MODE
if (auto snapshot = app.devTooling().uiSnapshots().latest(window)) {
    for (DevFlowNodeIndex root : snapshot->flow.roots) {
        inspectFlowSubtree(*snapshot, root);
    }
}
#endif
```

## 15.3 Registration approach options

### Option A — retain static side-effect registrars

Pros: least migration and concise use.

Cons: initialization order, duplicate declarations, linker dead stripping,
hidden global mutation, and difficult catalogue completeness.

Use only as a compatibility wrapper during migration.

### Option B — macro-generated descriptor specializations plus explicit catalogue

Pros: typed, no static initialization side effect, recursively discoverable,
easy to fingerprint, and reliable for static libraries.

Cons: applications must list their top-level elements; specialization placement
and macro diagnostics need care.

This is the recommended foundation.

### Option C — intrusive schema declared inside every struct/element

Pros: descriptor cannot be separated from its type and is naturally referenced.

Cons: modifies application data types, is awkward for third-party/Clay types,
and mixes developer-only concerns into normal definitions.

Allow an intrusive customization function as an advanced alternative, but do
not require it.

### Option D — source-scanned/generated catalogue

Pros: can remove the manual catalogue and produce excellent build reports.

Cons: requires a parser/generator and build integration before basic runtime
capture works.

Treat this as a later convenience built on the same descriptor API.

## 15.4 Compatibility layer

The existing macros can temporarily translate into schema-v2 descriptors:

- old struct names become provisional stable IDs with a migration warning;
- old enum schemas retain their numeric values but lift the `uint8_t`
  restriction;
- old element registration becomes element presentation metadata;
- unsupported old fields receive explicit coverage diagnostics;
- no new feature should depend on raw `memberPointerBytes`.

Compatibility must be time-bounded. Silent use of compiler type hashes as
durable IDs would undermine later persistence.

---

## 16. Internal implementation sketch

## 16.1 Typed schema construction

Prefer typed static data and trampolines:

```cpp
template <auto Member>
consteval auto devField(std::string_view id, DevFieldOptions options = {}) {
    using Traits = MemberPointerTraits<decltype(Member)>;
    using Owner = typename Traits::Owner;
    using Value = typename Traits::Value;

    return DevStaticFieldDescriptor{
        .stableId = id,
        .valueDescriptor = &devSchemaDescriptor<Value>(),
        .capture = +[](const void* owner, DevValueWriter& out) {
            const auto& typed = *static_cast<const Owner*>(owner);
            return captureDevValue(typed.*Member, out);
        },
        .options = options,
    };
}
```

The member pointer lives in the generated function type. It does not need to be
copied into a byte vector and reconstituted at runtime.

## 16.2 Recursive registry ingestion

When adding an element:

1. validate the `FlowElement` concept;
2. build the element descriptor from compile-time facts and optional metadata;
3. add its parameter type descriptor;
4. recursively add every registered field type;
5. add optional state/resource schemas for inspection metadata, while retaining
   their non-editable default policy;
6. detect duplicate IDs and compare canonical descriptors;
7. detect unsupported fields and cycles;
8. publish a new immutable schema generation only if the batch is valid.

Two identical duplicate descriptors can be coalesced. Conflicting descriptors
with the same stable ID are errors which name both sources.

## 16.3 Capture budgets

Recommended configurable limits per window:

- Flow nodes;
- Clay nodes and roots;
- captured value nodes;
- recursion depth;
- sequence items per field;
- bytes per captured string;
- total copied string bytes;
- late schema registrations per frame.

All limits should come from the development capacity profile and should report
their required/available values through the frozen error contract.

---

## 17. Diagnostics and failure behavior

The first implementation should provide precise diagnostics for:

- duplicate stable type, field, enum value, or definition IDs;
- two element types claiming one `FlowDefinitionID` with different schemas;
- registered field lacking a type adapter;
- custom editor adapter named but not available;
- recursive schema cycle through an unsafe adapter;
- schema version zero or invalid migration alias;
- element invoked without catalogue anchoring;
- schema discovered after a snapshot began;
- unbalanced Flow capture tokens;
- a Clay root whose attachment parent is missing;
- a Clay node whose ordinal or parent index is invalid;
- Flow/Clay ID bridge collision;
- value, string, node, or depth budget truncation;
- attempted capture of a sensitive field without redaction policy.

The tree capture should fail soft wherever possible: preserve a valid partial
forest, attach the diagnostic to its window/frame/node, and keep the last
complete published snapshot available.

Registry conflicts are stricter. A conflicting stable schema must not publish a
new generation because every later edit and persistence feature relies on one
unambiguous meaning for each ID.

---

## 18. Performance expectations

Structural Flow capture should remain cheap enough for every development frame:

- one node append, one stack push/pop, and two Clay-count reads per invocation;
- no per-node heap allocation after buffers reach their configured reserve;
- intern source locations, type names, and authored debug names;
- compute child ranges and correlations in linear finalization passes;
- no global schema-map mutation on the normal path after catalogue warmup.

Clay forest copying is proportional to the number of Clay layout elements and
should be runtime-configurable. Recommended modes:

| Mode | Flow structure | Values | Clay forest |
|---|---:|---:|---:|
| Off | No | No | No |
| Flow | Yes | Selected | No |
| Inspect | Yes | Selected | Yes |
| Deep | Yes | All registered | Yes + extended text/config data |

The companion Interface normally uses `Inspect`. Headless schema validation can
operate without any per-frame capture.

Capture overhead—node counts, copied bytes, finalize time, Clay visitor time,
and truncation—must be published to `DevMonitoringAndReporting` as developer
tool cost rather than mixed into application cost.

---

## 19. Implementation stages

### Stage 0 — make the model boundary explicit

1. Rename `ElementTreePlaceholder` conceptually to `DevFlowForest` and separate
   it from override/snapshot maps currently co-located in `DevRuntime`.
2. Introduce `DevTreeCapture` and `DevUiSnapshotStore` interfaces without
   changing visible legacy tooling.
3. Add frame/window/generation and capture-quality metadata.
4. Preserve current flat preorder as an adapter for the legacy hierarchy panel.

### Stage 1 — robust Flow forest

1. Replace depth-only capture with token-paired capture scopes.
2. Add parent, roots, packed children, and subtree end indices.
3. Intern static/source strings and retain stable instance/definition identity.
4. Carry capture tokens in the constructed-element stack.
5. Add cancellation, auto-close, truncation, and balance tests.

### Stage 2 — schema descriptors and recursive values

1. Introduce stable type/field IDs, versions, aliases, editor contracts, and
   immutable schema generations.
2. Implement scalar, all-width enum, string, registered aggregate, optional,
   fixed-array, and common Clay adapters.
3. Implement the flat recursive captured-value arena.
4. Add typed descriptor macros and current-shape element inference.
5. Add FSEL and application catalogues plus late-discovery diagnostics.
6. Provide a compatibility adapter for current registration macros.

### Stage 3 — Clay forest

1. Add the guarded read-only Clay visitor and layout-element-count accessor.
2. Copy main, floating, text, clip, and synthetic nodes after layout.
3. Resolve attachment parents, root order, and snapshot-safe strings/config.
4. Record Flow emission intervals and build ownership/exact-ID links.
5. Add a Clay-only debug dump to compare against Clay's built-in debugger on
   representative layouts.

### Stage 4 — publish and retire the placeholder

1. Double-buffer completed per-window snapshots and publish immutable views to
   App-owned DevTooling.
2. Switch the legacy hierarchy consumer to the new Flow forest adapter.
3. Add a temporary Clay hierarchy diagnostic consumer.
4. Remove `appendCapturedClayElement` and hard-coded composite `DevValue` paths
   after downstream consumers migrate.
5. Verify DEV-off binary exclusion.

---

## 20. Testing and acceptance criteria

## 20.1 Flow forest tests

- several top-level Flow invocations produce several explicit roots;
- nested draw-style elements have correct parent, child order, depth, and
  subtree range;
- constructed elements retain children until explicit close;
- auto-close and exception/cancel paths produce balanced partial trees and
  quality flags;
- repeated loop instances retain distinct `ElementInstanceKey` values;
- internal-tool filtering does not corrupt surrounding parentage;
- capture capacity exhaustion produces a valid prefix and exact dropped count.

## 20.2 Clay forest tests

- unnamed containers and text nodes appear;
- final bounding boxes match `Clay_GetElementData` for known IDs;
- the main root and several floating roots are preserved separately;
- root z/paint order matches Clay's render traversal;
- popup attachment parent and clip edges resolve correctly;
- an invalid floating parent remains explicitly unresolved;
- culled non-rendering layout containers remain in the Clay forest;
- transition-exit nodes are flagged synthetic;
- enabling FlowUi capture does not change layout dimensions, render commands,
  hit testing, or Clay debug-mode state.

## 20.3 Schema/value tests

- signed and unsigned extrema retain exact values;
- enums with `uint8_t`, signed, and wider underlying types retain names and
  exact numerics;
- nested custom structs capture recursively and preserve field order/IDs;
- optionals, variants, fixed arrays, and configured bounded sequences produce
  the expected shapes;
- `std::string_view` is copied and labelled borrowed/view-only;
- pointers and callables never expose raw editable addresses;
- unsupported registered fields provide actionable compile/catalogue
  diagnostics;
- duplicate equal schemas coalesce and duplicate conflicting schemas fail;
- field rename aliases resolve to the new stable field;
- schema fingerprint is deterministic across registration order;
- macros and descriptor strings are absent in a DEV-off binary inspection.

## 20.4 Correlation tests

- a Flow element emitting several raw Clay children owns all of them;
- a nested Flow element wins over the parent's wider emission interval;
- a construct-style Flow element owns Clay children emitted before close;
- a popup is a Flow child, a Clay floating root, and has both ownership and
  attachment links;
- direct application Clay outside a Flow scope remains unowned rather than
  being attributed incorrectly;
- exact Flow/Clay identity links coexist with emission ownership;
- bridge collisions are reported and never used as unique identity.

Acceptance requires that the new Flow forest can replace the current hierarchy
input without losing definition, instance, source, or authored-key metadata,
and that the Clay forest can reproduce the structural rows of Clay's built-in
debugger without enabling that debugger or changing application layout.

---

## 21. Open choices for implementation review

The architecture does not depend on these lower-level choices, but the first
implementation should settle them explicitly:

1. whether the read-only Clay visitor is kept as a FlowUi-local extension or
   proposed upstream immediately;
2. whether completed snapshot vectors use ordinary retained capacity or the
   FlowUi storage system from the first stage;
3. the exact stable-ID string convention for application types and fields;
4. whether unsupported explicitly listed fields are a compile error by default
   or a catalogue error, with a strict build option promoting warnings;
5. which bounded sequence types ship in the first adapter set;
6. whether late-discovered schemas publish immediately or only at the next frame
   boundary;
7. whether the initial snapshot copies full Clay declarations or a compact
   pointer-free declaration view;
8. how much parameter data the default `Inspect` mode captures for unselected
   nodes.

Recommended defaults are: local Clay bridge first, retained vectors first,
reverse-DNS/library-prefixed stable IDs, catalogue error with strict-mode
promotion, fixed arrays before vectors, schema publication at a frame boundary,
pointer-free Clay copies, and full values only for selected/watched instances.

---

## Final assessment

The current implementation already captures the most important semantic event:
a typed Flow element invocation with stable definition and instance identity.
It should be upgraded, not replaced. The critical structural change is to make
that capture a robust per-window forest with explicit parentage, roots,
completion quality, and immutable publication.

Clay should be treated as a second authoritative hierarchy. Its built-in
debugger demonstrates where the data lives, but enabling the debugger changes
the layout being inspected. A narrow post-layout visitor beside the vendored
Clay implementation gives FlowUi the same tree and geometry without importing
Clay's presentation or side effects.

Popup subtrees show why the two-tree design matters. A popup belongs to its
component in Flow while being a sibling floating root in Clay. Explicit
emission, identity, and attachment edges preserve all three truths and give the
future Interface enough information to present different useful projections.

Finally, the registry should stop equating type support with copying bytes into
a hard-coded variant. Recursive registered aggregates, generic adapters, and a
flat schema-shaped value tree can cover deeply nested ordinary data. Truly
arbitrary types remain possible through semantic adapters, but only types with
a safe capture contract and an editor-serviceability contract should be called
editable. That boundary keeps the developer experience honest and prevents the
future property Interface from promising controls which the runtime cannot
safely service.
