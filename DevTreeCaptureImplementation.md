# FlowUi DevTreeCapture — Implementation Design

## 1. Purpose and scope

This report freezes the implementation design for `DevTreeCapture`. It narrows
the broader proposals in `DevRegistryTooling.md` to one responsibility:

> Capture the currently completed Flow element forest and the currently
> completed Clay element forest for each `UiManager`, then correlate and
> validate them.

This design adopts a stronger Flow element contract than the earlier report:

- a Flow element is a semantic and functional wrapper around **at least one
  Clay element**;
- every Flow instance has exactly one distinguished Clay root;
- that Clay root's ID is `FlowIDToClayID(flowInstanceId)`;
- the Flow instance debug name and that Clay root's ID string should match when
  debug names are available;
- all raw Clay nodes emitted directly by that Flow invocation must be inside
  the distinguished root's Clay subtree;
- another Flow instance created within that semantic/layout scope is a Flow
  child and owns its own distinguished Clay root;
- a Flow element which emits no matching Clay root is invalid use of the Flow
  element system, not a legitimate “logic-only Flow element.”

The Flow forest remains a forest containing only Flow nodes. The Clay forest
remains a complete forest containing only Clay nodes. Links provide the
relationship between them; the stored Flow tree is never expanded into a mixed
Flow/Clay hierarchy.

This report intentionally does not design:

- struct or element schema registration;
- parameter capture, editing, or overrides;
- prior-frame tree retention;
- pause/replay mechanics;
- picking, overlays, or final Interface widgets;
- persistence or baking.

The capture always describes the latest completed UI build. If later execution
tooling reconstructs an earlier frame, FlowUi rebuilds that UI and this same
capture path publishes the reconstructed current forests.

---

## 2. Frozen semantic model

## 2.1 A Flow instance and its Clay root are inseparable

For a Flow instance `F`, define:

```text
expectedClayId(F) = FlowIDToClayID(F.instanceId)
```

After `Clay_EndLayout`, exactly one authored Clay node directly emitted by `F`
must have this ID. That node is `F.clayRoot`.

For construct-style elements, `ElementBuilder` already enforces most of this
contract: it obtains `uiManager.toClayEID(elementId)`, invokes
`constructElement`, then opens and configures that exact Clay element. For
draw-style elements, the user's `buildElement` callback must open a root using
`context.clayID()` or the equivalent `uiManager.toClayEID(context.id)`.

Correct draw-style user code:

```cpp
struct Card {
    using Parameters = CardParameters;
    using BuildContext = ElementBuildContext<Card>;

    static constexpr FlowDefinitionID definitionId =
        DefinitionID("app.card");
    static constexpr std::string_view debugName = "Card";

    static void buildElement(BuildContext& context) {
        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;

        CLAY(context.clayID(), root) {
            // Raw Clay implementation detail owned by Card.
            CLAY(context.clayID("content"), {}) {
                CLAY_TEXT(
                    context.uiManager.toClayString(context.params.title),
                    CLAY_TEXT_CONFIG({}));
            }
        }
    }
};
```

Incorrect code:

```cpp
static void buildElement(BuildContext& context) {
    runSomeLogic(); // No Clay root: not a valid Flow element build.
}
```

Also incorrect:

```cpp
static void buildElement(BuildContext& context) {
    CLAY(context.clayID(), {}) {}
    CLAY(context.clayID("sibling"), {}) {} // Escapes the Flow root.
}
```

The second example should be two sibling Flow elements created at the owning
call site, or the additional Clay node should be nested inside the first root.

## 2.2 What a Flow child means

Flow parentage has two simultaneous requirements:

1. **semantic scope:** the child Flow invocation occurs while the parent Flow
   capture scope is active;
2. **Clay structure:** the child's distinguished Clay root is contained in the
   parent's distinguished Clay subtree, except when the child root is itself a
   Clay floating root declared from that scope.

For draw-style parents, this normally means the child is created inside the
parent's build callback and while the parent's root Clay scope is open:

```cpp
static void buildElement(BuildContext& context) {
    CLAY(context.clayID(), {}) {
        context.uiManager
            .createElement(kLabel, "title")
            .setParameters(LabelParameters{.text = "Title"})
            .draw();
    }
}
```

For construct-style parents, children may be created after `construct()` and
before `UiManager::drawConstructed()` closes the parent:

```cpp
ui.createElement(kPanel, "settings").construct();
ui.createElement(kLabel, "title")
    .setParameters(LabelParameters{.text = "Settings"})
    .draw();
ui.drawConstructed();
```

Both forms produce a Flow child and a Clay root inside the parent's active
scope.

The capture stack records the semantic parent immediately. Finalization checks
that Clay agrees. It does not silently rewrite the Flow tree from Clay data;
disagreement means the Flow element was authored incorrectly or capture was
incomplete.

A Flow leaf means “has no Flow children,” not “contains one Clay node.” It may
own a substantial raw Clay implementation subtree:

```text
Flow leaf: StatusCard

Clay root: StatusCard
├── Header row
│   ├── Icon
│   └── Text
├── Separator
└── Body row
    ├── Value
    └── Unit
```

Every Clay node above is directly contributed by `StatusCard` if no nested Flow
element emitted it. This is why the selected-Flow query needs a rooted Clay
subtree rather than assuming one Flow node equals one Clay node.

## 2.3 Floating children

A popup, tooltip, menu, or drag surface should be its own Flow element. Its
distinguished Clay root may be configured as floating. Clay then removes that
root from normal child containment and places it in
`layoutElementTreeRoots`.

The resulting relationships are intentionally:

```text
Flow forest                             Clay forest

ComboBox                                RootContainer
├── Trigger                             └── Trigger subtree
└── PopupSurface                        PopupSurfaceRoot [floating]
    └── Option                          └── Option subtree
```

`PopupSurface` remains a Flow child of `ComboBox` because it was invoked in the
combo box's semantic scope. Its Clay root is a separate floating root because
that is Clay's layout behavior. The root record retains the attachment parent,
clip dependency, z-index, and paint order.

This is the one expected exception to ordinary Flow-parent/Clay-ancestor
agreement. A floating child is valid only when its **own** distinguished root is
the floating root. A parent Flow element directly emitting a second floating
root without wrapping it in another Flow element is still an escaped sibling
and receives a diagnostic.

## 2.4 Raw Clay remains supported

The reverse relationship does not hold: every Flow instance has a Clay root,
but a Clay node need not have a Flow instance. Applications and Flow callbacks
may use raw `CLAY`/`CLAY_TEXT` declarations.

Raw Clay has three possible positions:

- inside one Flow element's root and directly emitted by it;
- around or between nested Flow roots;
- outside every Flow scope, including raw application roots.

All three appear in the Clay forest. Only Flow invocations appear in the Flow
forest. Unowned raw Clay is valid and has `directFlowOwner == InvalidFlowNode`.

## 2.5 Expansion property

The two forests and their links support a useful conceptual expansion:

1. start with a Flow node;
2. replace it with its distinguished Clay-rooted subtree;
3. treat nested Flow roots as substitution boundaries owned by child Flow
   nodes;
4. repeat recursively.

This reconstructs the Clay content contributed by the Flow forest. Raw Clay
outside every Flow scope is then added from the Clay forest to obtain the
complete Clay forest.

Two related queries must remain distinct:

- **full Clay subtree:** every Clay descendant of a Flow root, including nested
  child Flow roots;
- **direct Clay contribution:** only Clay nodes whose deepest emission interval
  belongs to that Flow node, excluding nested Flow contributions.

The future Interface can show either query for a selected Flow element.

---

## 3. Exact data model

## 3.1 Index and string types

All tree relations are frame-local dense indices. Flow and Clay IDs remain the
semantic cross-frame identities.

```cpp
namespace FlowUi::devSystems::tooling {

using DevFlowNodeIndex = uint32_t;
using DevClayNodeIndex = uint32_t;

inline constexpr DevFlowNodeIndex InvalidFlowNode = UINT32_MAX;
inline constexpr DevClayNodeIndex InvalidClayNode = UINT32_MAX;

struct DevTreeStringRef {
    uint32_t offset = 0;
    uint32_t length = 0;

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return length != 0;
    }
};

} // namespace FlowUi::devSystems::tooling
```

Runtime names and Clay text are copied into one snapshot-local
`std::vector<char>`. This avoids dangling views into the frame arena or Clay's
ephemeral memory. No `StorageSystem` allocation is used.

The first implementation should append strings without a deduplication hash
map. Most stable names are short, and a per-frame map adds significant
allocation and accounting complexity. A bounded dedup pass can be added later
if measured string duplication justifies it.

## 3.2 Flow node

```cpp
enum class DevFlowNodeFlag : uint32_t {
    None                    = 0,
    Constructed             = 1u << 0u,
    Drawn                   = 1u << 1u,
    InternalDev             = 1u << 2u,
    AutoClosed              = 1u << 3u,
    CaptureCanceled         = 1u << 4u,
    MissingClayRoot         = 1u << 5u,
    DuplicateClayRoot       = 1u << 6u,
    ClayNameMismatch        = 1u << 7u,
    EscapedClayEmission     = 1u << 8u,
    ClayParentMismatch      = 1u << 9u,
    FloatingClayRoot        = 1u << 10u,
    TruncatedClayLinks      = 1u << 11u,
};

struct DevFlowNode {
    // Flow-only forest navigation. Nodes are stored in Flow preorder.
    DevFlowNodeIndex parent = InvalidFlowNode;
    DevFlowNodeIndex firstChild = InvalidFlowNode;
    DevFlowNodeIndex nextSibling = InvalidFlowNode;
    uint32_t subtreeEnd = 0; // exclusive preorder index
    uint32_t depth = 0;

    FlowDefinitionID definition{};
    detail::element::ElementInstanceKey instance{};
    uint32_t expectedClayId = 0;

    DevTreeStringRef debugName{};
    DevTreeStringRef definitionName{};
    DevTreeStringRef sourceFile{};
    DevTreeStringRef sourceFunction{};
    uint32_t sourceLine = 0;
    uint32_t sourceColumn = 0;

    // Authored Clay layout-element ordinals, captured before Clay_EndLayout.
    uint32_t emissionBegin = 0;
    uint32_t emissionEnd = 0; // exclusive

    // Finalized Flow -> Clay links.
    DevClayNodeIndex clayRoot = InvalidClayNode;
    uint32_t firstDirectClay = 0;
    uint32_t directClayCount = 0;

    DevFlowNodeFlag flags = DevFlowNodeFlag::None;
};

struct DevFlowForest {
    std::vector<DevFlowNode> nodes{};
    std::vector<DevFlowNodeIndex> roots{};
    std::vector<DevClayNodeIndex> directClayNodes{};
};
```

`firstChild`/`nextSibling` is chosen over a separate child-index vector. Capture
already appends nodes in preorder, so this representation needs only one node
vector, provides direct child traversal, and lets a subtree be scanned as
`[nodeIndex, subtreeEnd)`.

`directClayNodes` is one packed adjacency vector. A Flow node's range lists all
Clay nodes directly owned by its emission interval. `clayRoot` also lies in
that range for a valid Flow node.

## 3.3 Pointer-free Clay values

The bridge sees Clay's final `Clay_LayoutElement`, whose configuration may have
been normalized or adjusted during layout and transitions. That is the useful
post-layout value, but pointer fields must not escape.

```cpp
struct DevClayPointerPresence {
    bool imageData = false;
    bool customData = false;
    bool userData = false;
    bool textUserData = false;
    bool transitionHandler = false;
    bool transitionInitialState = false;
    bool transitionFinalState = false;
};

struct DevClayResolvedDeclaration {
    Clay_LayoutConfig layout{};
    Clay_Color backgroundColor{};
    Clay_Color overlayColor{};
    Clay_CornerRadius cornerRadius{};
    Clay_AspectRatioElementConfig aspectRatio{};
    Clay_FloatingElementConfig floating{};
    Clay_ClipElementConfig clip{};
    Clay_BorderElementConfig border{};

    float transitionDuration = 0.0f;
    Clay_TransitionProperty transitionProperties =
        CLAY_TRANSITION_PROPERTY_NONE;
    Clay_TransitionInteractionHandlingType transitionInteraction{};
    Clay_TransitionEnterTriggerType transitionEnterTrigger{};
    Clay_TransitionExitTriggerType transitionExitTrigger{};
    Clay_ExitTransitionSiblingOrdering transitionSiblingOrdering{};

    DevClayPointerPresence pointerPresence{};
};
```

No raw function or data pointer is copied into the published snapshot. Future
resource correlation may add a stable resource ID beside `imageData == true`;
that is not part of initial tree capture.

## 3.4 Clay node and root

```cpp
enum class DevClayNodeFlag : uint32_t {
    None                = 0,
    Text                = 1u << 0u,
    Floating            = 1u << 1u,
    Exiting              = 1u << 2u,
    SyntheticAfterBuild = 1u << 3u,
    DuplicateId         = 1u << 4u,
    BoundsUnavailable   = 1u << 5u,
    UnownedRawClay      = 1u << 6u,
};

struct DevClayNode {
    // Clay-only forest navigation. Nodes are stored in Clay DFS preorder.
    DevClayNodeIndex parent = InvalidClayNode;
    DevClayNodeIndex firstChild = InvalidClayNode;
    DevClayNodeIndex nextSibling = InvalidClayNode;
    uint32_t subtreeEnd = 0; // exclusive Clay preorder index
    uint32_t depthWithinRoot = 0;
    uint32_t rootIndex = 0;

    // Index in Clay_Context::layoutElements: the emission ordinal.
    uint32_t layoutElementIndex = 0;
    uint32_t clayId = 0;
    DevTreeStringRef idString{};

    Clay_BoundingBox bounds{};
    Clay_Dimensions dimensions{};
    Clay_Dimensions minDimensions{};
    uint32_t clipClayId = 0;

    DevClayResolvedDeclaration declaration{};
    Clay_TextElementConfig textConfig{}; // userData is always nulled
    DevTreeStringRef text{};
    Clay_Dimensions unwrappedTextDimensions{};
    uint32_t wrappedLineCount = 0;

    // Deepest authored Flow emission interval containing this node.
    DevFlowNodeIndex directFlowOwner = InvalidFlowNode;
    DevClayNodeFlag flags = DevClayNodeFlag::None;
};

struct DevClayRoot {
    DevClayNodeIndex node = InvalidClayNode;
    uint32_t attachmentParentClayId = 0;
    DevClayNodeIndex attachmentParent = InvalidClayNode;
    uint32_t clipClayId = 0;
    DevClayNodeIndex clipNode = InvalidClayNode;
    int16_t zIndex = 0;
    uint32_t paintOrder = 0;
};

struct DevClayForest {
    std::vector<DevClayNode> nodes{};
    std::vector<DevClayRoot> roots{};
};
```

The Clay node vector is DFS preorder rather than Clay declaration order. This
makes every Clay subtree a contiguous range. `layoutElementIndex` preserves the
declaration/emission ordinal used for correlation.

Clay root order after `Clay_EndLayout` is paint order because Clay sorts
`layoutElementTreeRoots` by z-index before rendering. The main root and every
floating root are retained independently.

## 3.5 Snapshot, diagnostics, and statistics

```cpp
enum class DevTreeDiagnosticCode : uint16_t {
    FlowCaptureUnbalanced,
    FlowElementMissingClayRoot,
    FlowElementDuplicateClayRoot,
    FlowClayDebugNameMismatch,
    FlowElementEmittedClayOutsideRoot,
    FlowChildClayParentMismatch,
    ClayBridgeIdCollision,
    ClayAttachmentParentMissing,
    ClayClipNodeMissing,
    ClayBridgeTraversalFailed,
    FlowNodeCapacityExceeded,
    ClayNodeCapacityExceeded,
    StringCapacityExceeded,
    DirectLinkCapacityExceeded,
};

struct DevTreeDiagnostic {
    DevTreeDiagnosticCode code{};
    DevFlowNodeIndex flow = InvalidFlowNode;
    DevClayNodeIndex clay = InvalidClayNode;
    uint64_t expected = 0;
    uint64_t observed = 0;
};

struct DevTreeCaptureStats {
    uint64_t frameNumber = 0;
    uint32_t flowNodeCount = 0;
    uint32_t clayNodeCount = 0;
    uint32_t clayRootCount = 0;
    uint32_t directLinkCount = 0;
    uint32_t rawClayNodeCount = 0;
    uint32_t invalidFlowNodeCount = 0;
    uint32_t syntheticClayNodeCount = 0;
    uint32_t copiedStringBytes = 0;
    uint32_t vectorGrowthOperations = 0;
    uint64_t logicalLiveBytes = 0;
    uint64_t backingCapacityBytes = 0;
    uint64_t peakLogicalLiveBytes = 0;
    uint64_t peakBackingCapacityBytes = 0;
    uint64_t clayBridgeNs = 0;
    uint64_t correlationNs = 0;
    uint64_t validationNs = 0;
    bool complete = false;
    bool truncated = false;
};

struct DevTreeSnapshot {
    WindowId window = InvalidWindowId;
    uint64_t frameNumber = 0;
    uint64_t generation = 0;
    uint32_t authoredClayElementCount = 0;

    DevFlowForest flow{};
    DevClayForest clay{};
    std::vector<char> strings{};
    std::vector<DevTreeDiagnostic> diagnostics{};
    DevTreeCaptureStats stats{};
};
```

Tree diagnostics are compact facts. They are also forwarded through FlowUi's
error monitoring with source and frame context; the vector exists so the tree
consumer can place a warning directly on the affected node.

The initial query helpers are simple spans over these vectors:

```cpp
[[nodiscard]] std::span<const DevClayNode> fullClaySubtree(
    const DevTreeSnapshot& snapshot,
    DevFlowNodeIndex flow) noexcept {
    const DevClayNodeIndex root = snapshot.flow.nodes[flow].clayRoot;
    if (root == InvalidClayNode) return {};
    const uint32_t end = snapshot.clay.nodes[root].subtreeEnd;
    return std::span(snapshot.clay.nodes).subspan(root, end - root);
}

[[nodiscard]] std::span<const DevClayNodeIndex> directClayContribution(
    const DevTreeSnapshot& snapshot,
    DevFlowNodeIndex flow) noexcept {
    const DevFlowNode& node = snapshot.flow.nodes[flow];
    return std::span(snapshot.flow.directClayNodes)
        .subspan(node.firstDirectClay, node.directClayCount);
}
```

The first includes nested child Flow roots because they are real descendants of
the selected root. The second excludes them because their deepest emission
owner is the child Flow node.

---

## 4. `DevTreeCapture` class

## 4.1 Public/internal surface

`DevTreeCapture` is a per-`UiManager` internal producer, stored in
`UiManagerState`. It is not a new public manager.

```cpp
struct DevClayIdIndexEntry {
    uint32_t clayId = 0;
    DevClayNodeIndex node = InvalidClayNode;
};

enum class OwnershipEventKind : uint8_t { End, Begin };

struct DevFlowOwnershipEvent {
    uint32_t ordinal = 0;
    DevFlowNodeIndex flow = InvalidFlowNode;
    uint32_t depth = 0;
    OwnershipEventKind kind{};
};

class DevTreeCapture {
public:
    struct FlowBegin {
        FlowDefinitionID definition{};
        FlowElementID instance{};
        std::string_view definitionName{};
        std::string_view sourceFile{};
        std::string_view sourceFunction{};
        uint32_t sourceLine = 0;
        uint32_t sourceColumn = 0;
        bool constructed = false;
        bool internalDev = false;
    };

    struct Token {
        DevFlowNodeIndex node = InvalidFlowNode;
        uint64_t frameGeneration = 0;
        uint32_t scopeId = 0;

        [[nodiscard]] explicit operator bool() const noexcept {
            return scopeId != 0;
        }
    };

    void beginFrame(
        WindowId window,
        uint64_t frameNumber,
        Clay_Context& clay,
        devSystems::DevTimingRecorder* timing) noexcept;

    [[nodiscard]] Token beginFlow(const FlowBegin& begin) noexcept;
    void endFlow(Token token, bool autoClosed = false) noexcept;

    void noteAuthoredClayEnd() noexcept;
    void finishAfterClayLayout() noexcept;
    void cancelFrame() noexcept;

    [[nodiscard]] const DevTreeSnapshot& current() const noexcept {
        return published_;
    }

    void appendDevMemorySamples(
        devSystems::MemorySampleSink& sink) const noexcept;

private:
    DevTreeSnapshot building_{};
    DevTreeSnapshot published_{};
    struct ActiveFlowCapture {
        Token token{};
        bool suppressed = false;
    };

    std::vector<ActiveFlowCapture> activeFlowScopes_{};
    std::vector<DevFlowNodeIndex> lastFlowChild_{};
    std::vector<detail::ClayDevTraversalEntry> clayTraversalScratch_{};
    std::vector<DevClayNodeIndex> layoutIndexToClayNode_{};
    std::vector<DevClayIdIndexEntry> clayIdIndex_{};
    std::vector<DevClayNodeIndex> lastClayChild_{};
    std::vector<DevClayNodeIndex> clayOpenByDepth_{};
    std::vector<DevFlowOwnershipEvent> ownershipEvents_{};
    std::vector<DevFlowNodeIndex> ownershipActive_{};
    std::vector<uint32_t> directClayCounts_{};
    std::vector<uint32_t> directClayCursors_{};
    Clay_Context* clay_ = nullptr;
    devSystems::DevTimingRecorder* timing_ = nullptr;
    uint64_t frameGeneration_ = 0;
    uint32_t nextScopeId_ = 1;
    bool frameActive_ = false;
};
```

The builder-side scope is a small RAII wrapper:

```cpp
class DevTreeFlowScope {
public:
    DevTreeFlowScope(DevTreeCapture& capture, DevTreeCapture::Token token)
        noexcept
        : capture_(&capture), token_(token) {}

    ~DevTreeFlowScope() noexcept {
        if (active_ && capture_) capture_->endFlow(token_);
    }

    void leaveOpen() noexcept { active_ = false; }

private:
    DevTreeCapture* capture_ = nullptr;
    DevTreeCapture::Token token_{};
    bool active_ = true;
};
```

The implementation returns `noexcept` and reports bounded failures into the
snapshot/error channel. Tree tooling must not make a UI frame throw after the
application's UI was otherwise valid.

## 4.2 Vectors are the deliberate storage choice

All persistent and scratch storage uses `std::vector`. This subsystem compiles
out of production, so integrating it with `StorageSystem` would add ownership
and lifetime complexity without production benefit.

Vectors retain capacity across frames. `building_` and `published_` are swapped
at publication; the old published vectors become the next building buffers and
are immediately `clear()`ed without releasing capacity.

This is two-buffer publication, not frame history:

- only `published_` is queryable;
- `building_` is private and mutable;
- the preceding published content becomes inaccessible on the next swap;
- no `shared_ptr`, pin, ring buffer, or historical generation is retained;
- a returned view is valid only until the next successful publication for that
  window.

## 4.3 Capacity policy

The initial configuration should reserve, but not hard-cap, vectors. This keeps
implementation simple while monitoring reveals real requirements.

```cpp
struct DevTreeCaptureConfig {
    uint32_t flowNodeReserve = 512;
    uint32_t clayNodeReserve = 2048;
    uint32_t clayRootReserve = 32;
    uint32_t directLinkReserve = 2048;
    uint32_t stringByteReserve = 64 * 1024;
    uint32_t diagnosticReserve = 64;

    // Safety ceilings, not production tuning knobs.
    uint32_t maximumFlowNodes = 1u << 20;
    uint32_t maximumClayNodes = 1u << 22;
    uint32_t maximumStringBytes = 64u * 1024u * 1024u;
};
```

Every capacity change increments `vectorGrowthOperations`. Monitoring shows
live size, retained capacity, peaks, and growth. Once representative
applications establish stable numbers, the defaults can be improved without
moving capture into `StorageSystem`.

Safety ceilings prevent malformed UI or unbounded text from exhausting all
process memory. Crossing one truncates capture with an explicit diagnostic; it
does not truncate the actual application UI.

---

## 5. Flow capture implementation

## 5.1 Start capture after Clay begins

`Clay_BeginLayout` resets Clay's ephemeral arrays and opens Clay's own root
container. `DevTreeCapture::beginFrame` must therefore run after it, not before
it as the current `DevRuntime::beginFrame` does.

```cpp
Clay_BeginLayout();

#if FLOW_UI_DEV_MODE
state_->devTreeCapture.beginFrame(
    window_,
    state_->frameNumber,
    *state_->clayContext,
    devTimingRecorder_);
#endif
```

At this point `clayDevLayoutElementCount(clay)` normally returns one for
Clay's root container. That root is raw/unowned Clay and remains in the Clay
forest.

## 5.2 Capture only the UI-emission phase

Move Flow tree scope creation so it surrounds `buildElement` or the construct
operation, not interaction hooks. Interaction hooks and `runLogic` are behavior
phases and should not emit UI. Keeping them outside the tree interval makes
Clay ownership unambiguous.

For draw-style elements:

```cpp
if (elementDrawOptionsHas(options, ElementDrawOptions::SkipBuildCallback)) {
    return; // No current UI node, therefore no Flow-tree node.
}

#if FLOW_UI_DEV_MODE
const auto treeToken = uiManager_.devTreeCapture().beginFlow({
    .definition = ElementType::definitionId,
    .instance = elementId_,
    .definitionName = detail::element::debugName<ElementType>(),
    .sourceFile = sourceLocation_.file_name(),
    .sourceFunction = sourceLocation_.function_name(),
    .sourceLine = static_cast<uint32_t>(sourceLocation_.line()),
    .sourceColumn = static_cast<uint32_t>(sourceLocation_.column()),
    .constructed = false,
    .internalDev = captureAsDevInternal_,
});
DevTreeFlowScope treeScope{uiManager_.devTreeCapture(), treeToken};
#endif

ElementType::buildElement(buildContext);
```

`SkipBuildCallback` is explicitly not a UI build. It may still appear in timing
or behavior monitoring, but publishing it as a Flow tree node would violate the
root invariant.

For construct-style elements:

```cpp
#if FLOW_UI_DEV_MODE
const auto treeToken = uiManager_.devTreeCapture().beginFlow({
    .definition = ElementType::definitionId,
    .instance = elementId_,
    .definitionName = detail::element::debugName<ElementType>(),
    .sourceFile = sourceLocation_.file_name(),
    .sourceFunction = sourceLocation_.function_name(),
    .sourceLine = static_cast<uint32_t>(sourceLocation_.line()),
    .sourceColumn = static_cast<uint32_t>(sourceLocation_.column()),
    .constructed = true,
    .internalDev = captureAsDevInternal_,
});
DevTreeFlowScope treeScope{uiManager_.devTreeCapture(), treeToken};
#endif

Clay_ElementDeclaration declaration =
    ElementType::constructElement(buildContext);

uiManager_.retainConstructedElement(
    rootElementId,
    elementId_,
    flowScope.priorDepth
#if FLOW_UI_DEV_MODE
    , treeToken
#endif
);
Clay__OpenElementWithId(rootElementId);
Clay__ConfigureOpenElement(declaration);
#if FLOW_UI_DEV_MODE
treeScope.leaveOpen();
#endif
```

`ConstructedElementFrame` retains the token. `closeConstructedToDepth` closes
Clay first, then ends the exact Flow capture token:

```cpp
Clay__CloseElement();
state_->devTreeCapture.endFlow(frame.treeToken, autoClosed);
```

The token is not released by the builder's local RAII scope after a successful
construct. `DevTreeFlowScope` ends it during exception/unwind before
`leaveOpen()`, while `ConstructedElementFrame` owns the close after
`leaveOpen()`.

## 5.3 `beginFlow`

```cpp
DevTreeCapture::Token DevTreeCapture::beginFlow(
    const FlowBegin& begin) noexcept {
    if (!frameActive_) return {};

    const Token token{
        .node = InvalidFlowNode,
        .frameGeneration = frameGeneration_,
        .scopeId = nextScopeId_++,
    };
    const bool parentSuppressed =
        !activeFlowScopes_.empty() && activeFlowScopes_.back().suppressed;
    const bool atCapacity =
        building_.flow.nodes.size() >= maximumFlowNodes_;
    if (parentSuppressed || atCapacity) {
        if (atCapacity && !parentSuppressed) {
            noteFlowCapacityFailure(begin);
        }
        activeFlowScopes_.push_back({.token = token, .suppressed = true});
        return token;
    }

    const auto index = static_cast<DevFlowNodeIndex>(
        building_.flow.nodes.size());
    const DevFlowNodeIndex parent =
        activeFlowScopes_.empty()
            ? InvalidFlowNode
            : activeFlowScopes_.back().token.node;

    DevFlowNode node{};
    node.parent = parent;
    node.depth = static_cast<uint32_t>(activeFlowScopes_.size());
    node.definition = begin.definition;
    node.instance = detail::element::toInstanceKey(begin.instance);
    node.expectedClayId = FlowIDToClayID(begin.instance);
    node.debugName = copyString(begin.instance.debugName);
    node.definitionName = copyString(begin.definitionName);
    node.sourceFile = copyString(begin.sourceFile);
    node.sourceFunction = copyString(begin.sourceFunction);
    node.sourceLine = begin.sourceLine;
    node.sourceColumn = begin.sourceColumn;
    node.emissionBegin = detail::clayDevLayoutElementCount(*clay_);
    node.flags = begin.constructed
        ? DevFlowNodeFlag::Constructed
        : DevFlowNodeFlag::Drawn;
    if (begin.internalDev) node.flags |= DevFlowNodeFlag::InternalDev;

    building_.flow.nodes.push_back(node);
    lastFlowChild_.push_back(InvalidFlowNode);

    if (parent == InvalidFlowNode) {
        building_.flow.roots.push_back(index);
    } else {
        DevFlowNode& parentNode = building_.flow.nodes[parent];
        DevFlowNodeIndex& lastChild = lastFlowChild_[parent];
        if (lastChild == InvalidFlowNode) {
            parentNode.firstChild = index;
        } else {
            building_.flow.nodes[lastChild].nextSibling = index;
        }
        lastChild = index;
    }

    Token publishedToken = token;
    publishedToken.node = index;
    activeFlowScopes_.push_back({
        .token = publishedToken,
        .suppressed = false,
    });
    return publishedToken;
}
```

The actual implementation should use helpers for flags and capacity growth
tracking, but the data flow above is fixed.

## 5.4 `endFlow`

```cpp
void DevTreeCapture::endFlow(Token token, bool autoClosed) noexcept {
    if (!token || token.frameGeneration != frameGeneration_) return;

    if (activeFlowScopes_.empty() ||
        activeFlowScopes_.back().token.scopeId != token.scopeId) {
        recordUnbalancedToken(token);
        recoverStackThrough(token.scopeId);
        return;
    }

    const ActiveFlowCapture active = activeFlowScopes_.back();
    activeFlowScopes_.pop_back();
    if (active.suppressed) return;

    DevFlowNode& node = building_.flow.nodes[active.token.node];
    node.emissionEnd = detail::clayDevLayoutElementCount(*clay_);
    node.subtreeEnd = static_cast<uint32_t>(building_.flow.nodes.size());
    if (autoClosed) node.flags |= DevFlowNodeFlag::AutoClosed;
}
```

Because Flow nodes are appended in semantic preorder and parents close after
their children, `subtreeEnd` is correct without a final packing pass.

An element may have `emissionBegin == emissionEnd`; it remains in the building
forest until post-layout validation records `MissingClayRoot`. It is not hidden.

## 5.5 Suppressed internal UI

The new companion Interface should keep internal developer Flow nodes and mark
them `InternalDev`, allowing consumers to filter them. The legacy in-tree panel
can still request subtree suppression to avoid recursively capturing itself.

The `ActiveFlowCapture` stack shown in the class is therefore mandatory rather
than an optional refinement. A suppressed or capacity-truncated scope still
receives a valid `scopeId`, stays balanced, and suppresses its descendants. It
must never allow a nested child to be reparented to the nearest earlier
published ancestor. Suppression is a temporary compatibility mode, not the
default tree model.

---

## 6. Clay development bridge

## 6.1 Files and compile boundary

Add:

```text
include/internal/ClayDevTreeBridge.hpp
src/clay.cpp                         bridge definition after including clay.h
```

The header is included only by development-mode FlowUi internals. The bridge
definitions and Clay-forest fields are compiled only when both
`FLOW_UI_DEV_MODE` and the opt-in CMake/build configuration
`FLOW_UI_DEV_CAPTURE_CLAY` are enabled. `FLOW_UI_DEV_CAPTURE_CLAY` has no effect
when `FLOW_UI_DEV_MODE` is disabled. DEV-off builds expose no bridge symbols or
snapshot types; DEV builds with Clay capture disabled still publish the Flow
forest.

The bridge stays in `src/clay.cpp` because that translation unit defines
`CLAY_IMPLEMENTATION` and can see `Clay_Context`, `Clay_LayoutElement`, root
records, text data, child arrays, and hash-map items without exposing Clay's
private types to the rest of FlowUi.

## 6.2 Bridge POD views

```cpp
namespace FlowUi::detail {

struct ClayDevRootView {
    uint32_t paintOrder = 0;
    int32_t layoutElementIndex = -1;
    uint32_t attachmentParentClayId = 0;
    uint32_t clipClayId = 0;
    int16_t zIndex = 0;
};

struct ClayDevElementView {
    int32_t layoutElementIndex = -1;
    int32_t parentLayoutElementIndex = -1;
    uint32_t rootPaintOrder = 0;
    uint32_t depthWithinRoot = 0;

    uint32_t clayId = 0;
    Clay_String idString{};
    Clay_BoundingBox bounds{};
    bool boundsAvailable = false;
    Clay_Dimensions dimensions{};
    Clay_Dimensions minDimensions{};
    uint32_t clipClayId = 0;

    bool isText = false;
    bool exiting = false;
    Clay_ElementDeclaration declaration{};
    Clay_String text{};
    Clay_TextElementConfig textConfig{};
    Clay_Dimensions unwrappedTextDimensions{};
    uint32_t wrappedLineCount = 0;
};

struct ClayDevTraversalEntry {
    int32_t layoutElementIndex = -1;
    int32_t parentLayoutElementIndex = -1;
    uint32_t rootPaintOrder = 0;
    uint32_t depthWithinRoot = 0;
};

struct ClayDevVisitor {
    bool (*onRoot)(void*, const ClayDevRootView&) noexcept = nullptr;
    bool (*onElement)(void*, const ClayDevElementView&) noexcept = nullptr;
};

enum class ClayDevVisitResult : uint8_t {
    Complete,
    InvalidContext,
    ContextNotCurrent,
    ScratchTooSmall,
    VisitorStopped,
    InvalidClayIndex,
};

[[nodiscard]] uint32_t clayDevLayoutElementCount(
    const Clay_Context& context) noexcept;

[[nodiscard]] uint32_t clayDevRootCount(
    const Clay_Context& context) noexcept;

[[nodiscard]] ClayDevVisitResult clayDevVisitTree(
    Clay_Context& context,
    std::span<ClayDevTraversalEntry> scratch,
    ClayDevVisitor visitor,
    void* userData) noexcept;

} // namespace FlowUi::detail
```

The caller owns traversal scratch. The bridge does not allocate and does not
mutate Clay arrays. `scratch.size()` must be at least the current layout-element
count; `DevTreeCapture` retains that vector capacity across frames.

The bridge requires `Clay_GetCurrentContext() == &context`. `UiManager::endFrame`
already sets its context current. Returning `ContextNotCurrent` is safer than
temporarily changing Clay's process/thread-local current context inside an
inspection function.

## 6.3 Count accessor implementation

Defined after `#include "clay.h"` in `src/clay.cpp`:

```cpp
#if FLOW_UI_DEV_MODE
namespace FlowUi::detail {

uint32_t clayDevLayoutElementCount(const Clay_Context& context) noexcept {
    return context.layoutElements.length > 0
        ? static_cast<uint32_t>(context.layoutElements.length)
        : 0u;
}

uint32_t clayDevRootCount(const Clay_Context& context) noexcept {
    return context.layoutElementTreeRoots.length > 0
        ? static_cast<uint32_t>(context.layoutElementTreeRoots.length)
        : 0u;
}

} // namespace FlowUi::detail
#endif
```

This count is the declaration ordinal source used by Flow emission intervals.

## 6.4 Iterative visitor implementation

The visitor walks each final Clay root in paint order and emits nodes in DFS
preorder. Conceptual implementation:

```cpp
ClayDevVisitResult clayDevVisitTree(
    Clay_Context& context,
    std::span<ClayDevTraversalEntry> scratch,
    ClayDevVisitor visitor,
    void* userData) noexcept {
    if (Clay_GetCurrentContext() != &context) {
        return ClayDevVisitResult::ContextNotCurrent;
    }
    if (scratch.size() < static_cast<size_t>(context.layoutElements.length)) {
        return ClayDevVisitResult::ScratchTooSmall;
    }

    for (int32_t rootIndex = 0;
         rootIndex < context.layoutElementTreeRoots.length;
         ++rootIndex) {
        const auto& root = context.layoutElementTreeRoots.internalArray[rootIndex];
        if (root.layoutElementIndex < 0 ||
            root.layoutElementIndex >= context.layoutElements.length) {
            return ClayDevVisitResult::InvalidClayIndex;
        }

        const ClayDevRootView rootView{
            .paintOrder = static_cast<uint32_t>(rootIndex),
            .layoutElementIndex = root.layoutElementIndex,
            .attachmentParentClayId = root.parentId,
            .clipClayId = root.clipElementId,
            .zIndex = root.zIndex,
        };
        if (visitor.onRoot && !visitor.onRoot(userData, rootView)) {
            return ClayDevVisitResult::VisitorStopped;
        }

        size_t stackSize = 0;
        scratch[stackSize++] = {
            .layoutElementIndex = root.layoutElementIndex,
            .parentLayoutElementIndex = -1,
            .rootPaintOrder = static_cast<uint32_t>(rootIndex),
            .depthWithinRoot = 0,
        };

        while (stackSize != 0) {
            const ClayDevTraversalEntry entry = scratch[--stackSize];
            const Clay_LayoutElement& element =
                context.layoutElements.internalArray[entry.layoutElementIndex];

            const Clay_LayoutElementHashMapItem* hashItem =
                Clay__GetHashMapItem(element.id);
            const bool hasBounds =
                hashItem != nullptr &&
                hashItem != &Clay_LayoutElementHashMapItem_DEFAULT;

            const Clay_String idString =
                context.layoutElementIdStrings.internalArray[
                    entry.layoutElementIndex];
            const uint32_t clipClayId = static_cast<uint32_t>(
                context.layoutElementClipElementIds.internalArray[
                    entry.layoutElementIndex]);

            ClayDevElementView view{
                .layoutElementIndex = entry.layoutElementIndex,
                .parentLayoutElementIndex = entry.parentLayoutElementIndex,
                .rootPaintOrder = entry.rootPaintOrder,
                .depthWithinRoot = entry.depthWithinRoot,
                .clayId = element.id,
                .idString = idString,
                .bounds = hasBounds ? hashItem->boundingBox : Clay_BoundingBox{},
                .boundsAvailable = hasBounds,
                .dimensions = element.dimensions,
                .minDimensions = element.minDimensions,
                .clipClayId = clipClayId,
                .isText = element.isTextElement,
                .exiting = element.exiting,
                .declaration = element.isTextElement
                    ? Clay_ElementDeclaration{}
                    : element.config,
                .text = element.isTextElement
                    ? element.textElementData.text
                    : Clay_String{},
                .textConfig = element.isTextElement
                    ? element.textConfig
                    : Clay_TextElementConfig{},
                .unwrappedTextDimensions = element.isTextElement
                    ? element.textElementData.preferredDimensions
                    : Clay_Dimensions{},
                .wrappedLineCount = element.isTextElement
                    ? static_cast<uint32_t>(
                        element.textElementData.wrappedLines.length)
                    : 0u,
            };

            if (visitor.onElement && !visitor.onElement(userData, view)) {
                return ClayDevVisitResult::VisitorStopped;
            }

            // Reverse push preserves Clay child order in DFS output.
            for (int32_t child = element.children.length - 1;
                 child >= 0;
                 --child) {
                const int32_t childIndex = element.children.elements[child];
                if (childIndex < 0 || childIndex >= context.layoutElements.length) {
                    return ClayDevVisitResult::InvalidClayIndex;
                }
                scratch[stackSize++] = {
                    .layoutElementIndex = childIndex,
                    .parentLayoutElementIndex = entry.layoutElementIndex,
                    .rootPaintOrder = entry.rootPaintOrder,
                    .depthWithinRoot = entry.depthWithinRoot + 1u,
                };
            }
        }
    }

    return ClayDevVisitResult::Complete;
}
```

The implementation should bounds-check `layoutElementIdStrings` and clip arrays
as well as the main element array. Those checks are omitted above only to keep
the traversal readable.

The bridge reads `Clay__GetHashMapItem`, which depends on the current context;
the explicit current-context check is therefore required. Duplicate Clay IDs
can make ID-based bounds ambiguous. `DevTreeCapture` independently detects
duplicate IDs while copying and marks every affected snapshot node.

## 6.5 Why not enable Clay's debugger

`Clay_SetDebugModeEnabled(true)` is never called by `DevTreeCapture`. The built-in
debugger changes root width, emits its own UI, and owns presentation state. The
bridge reuses the same authoritative internal tree data without enabling those
side effects.

---

## 7. Clay copy and finalization

## 7.1 Capture the authored/synthetic boundary

Immediately before `Clay_EndLayout`, record the authored layout-element count:

```cpp
#if FLOW_UI_DEV_MODE
state_->devTreeCapture.noteAuthoredClayEnd();
#endif

Clay_RenderCommandArray renderCommands = Clay_EndLayout(deltaTime);

#if FLOW_UI_DEV_MODE
state_->devTreeCapture.finishAfterClayLayout();
#endif
```

Clay may append retained exit-transition nodes during `Clay_EndLayout`. A Clay
node with `layoutElementIndex >= authoredClayElementCount` is marked
`SyntheticAfterBuild`. It receives no direct Flow owner from current-frame
emission intervals.

## 7.2 Copy callbacks

`finishAfterClayLayout` resizes traversal and ordinal-map scratch, clears Clay
output vectors, then calls `clayDevVisitTree`.

The `onElement` callback:

1. appends one pointer-free `DevClayNode`;
2. copies the ID string and text into snapshot string storage;
3. sanitizes pointer-bearing declaration/text fields;
4. maps `layoutElementIndex -> DevClayNodeIndex`;
5. links `parent`, `firstChild`, and `nextSibling`;
6. appends a numeric-ID/index pair to a vector for later duplicate detection;
7. marks nodes added after the authored boundary synthetic.

The callback can link children with a temporary `lastClayChild_` vector indexed
by Clay node index. When DFS leaves a node, `subtreeEnd` is not directly
signaled. Because depth is known and output is preorder, compute all
`subtreeEnd` values in one reverse pass:

```cpp
for (uint32_t i = 0; i < clay.nodes.size(); ++i) {
    clay.nodes[i].subtreeEnd = static_cast<uint32_t>(clay.nodes.size());
}

std::vector<DevClayNodeIndex> openByDepth;
for (DevClayNodeIndex i = 0; i < clay.nodes.size(); ++i) {
    const uint32_t depth = clay.nodes[i].depthWithinRoot;
    while (openByDepth.size() > depth) {
        clay.nodes[openByDepth.back()].subtreeEnd = i;
        openByDepth.pop_back();
    }
    openByDepth.push_back(i);
}
while (!openByDepth.empty()) {
    clay.nodes[openByDepth.back()].subtreeEnd =
        static_cast<uint32_t>(clay.nodes.size());
    openByDepth.pop_back();
}
```

Reset `openByDepth` at each Clay root so equal depths in consecutive roots do
not form false ancestry.

## 7.3 Sanitize Clay values

```cpp
DevClayResolvedDeclaration sanitize(
    const Clay_ElementDeclaration& source) noexcept {
    DevClayResolvedDeclaration out{};
    out.layout = source.layout;
    out.backgroundColor = source.backgroundColor;
    out.overlayColor = source.overlayColor;
    out.cornerRadius = source.cornerRadius;
    out.aspectRatio = source.aspectRatio;
    out.floating = source.floating;
    out.clip = source.clip;
    out.border = source.border;

    out.transitionDuration = source.transition.duration;
    out.transitionProperties = source.transition.properties;
    out.transitionInteraction = source.transition.interactionHandling;
    out.transitionEnterTrigger = source.transition.enter.trigger;
    out.transitionExitTrigger = source.transition.exit.trigger;
    out.transitionSiblingOrdering = source.transition.exit.siblingOrdering;

    out.pointerPresence = {
        .imageData = source.image.imageData != nullptr,
        .customData = source.custom.customData != nullptr,
        .userData = source.userData != nullptr,
        .transitionHandler = source.transition.handler != nullptr,
        .transitionInitialState =
            source.transition.enter.setInitialState != nullptr,
        .transitionFinalState =
            source.transition.exit.setFinalState != nullptr,
    };
    return out;
}
```

For text, copy `Clay_TextElementConfig`, record whether `userData` was non-null,
then set `textConfig.userData = nullptr` in the snapshot.

## 7.4 Resolve root attachment and clip links

After every Clay node has been copied, sort a vector of
`{clayId, DevClayNodeIndex}` pairs. Adjacent equal IDs identify duplicates;
unique IDs support binary-search resolution for root, attachment, and clip
links. This avoids a per-frame `unordered_map` and keeps tree plus scratch
storage vector-based.

- duplicate IDs do not produce a unique link;
- `attachmentParentClayId == 0` is valid for the main root;
- a nonzero unresolved attachment parent produces
  `ClayAttachmentParentMissing`;
- a nonzero unresolved clip ID produces `ClayClipNodeMissing`.

Attachment is an edge, not Clay containment. `DevClayRoot::node.parent` remains
`InvalidClayNode`.

---

## 8. Emission ownership algorithm

## 8.1 Intervals

Each Flow node records a half-open authored Clay ordinal interval:

```text
[emissionBegin, emissionEnd)
```

Nested Flow calls create nested intervals. Direct raw Clay emitted before,
after, or around a child Flow interval stays owned by the parent. Clay emitted
inside the child interval belongs to the child.

## 8.2 Linear event sweep

Build the two `DevFlowOwnershipEvent` values declared with the capture class for
every nonempty Flow interval.

Sort by:

1. ordinal ascending;
2. `End` before `Begin` because intervals are half-open;
3. for equal `End`, depth descending;
4. for equal `Begin`, depth ascending.

Ignore zero-length intervals in the sweep; validation will mark them missing a
root.

Scan authored ordinals in increasing order while maintaining active Flow
indices. The deepest active Flow node owns that ordinal. Use
`layoutIndexToClayNode` to assign ownership only to Clay elements which remain
reachable in the final Clay forest.

Conceptually:

```cpp
size_t eventIndex = 0;
std::vector<DevFlowNodeIndex> active;

for (uint32_t ordinal = 0;
     ordinal < snapshot.authoredClayElementCount;
     ++ordinal) {
    applyEventsAt(ordinal, ownershipEvents_, eventIndex, active);

    const DevClayNodeIndex clay = layoutIndexToClayNode_[ordinal];
    if (clay == InvalidClayNode) continue;

    DevClayNode& clayNode = snapshot.clay.nodes[clay];
    clayNode.directFlowOwner =
        active.empty() ? InvalidFlowNode : active.back();
    if (active.empty()) clayNode.flags |= DevClayNodeFlag::UnownedRawClay;
}
```

The implementation validates that event nesting matches the Flow stack. A
recovery path may remove a named Flow index from `active`, but records
`FlowCaptureUnbalanced` rather than concealing it.

This is `O(F log F + C)` because event sorting dominates. If profiling shows
the sort matters, events can later be emitted in already ordered capture order;
the initial implementation should favor obvious correctness.

## 8.3 Pack reverse links

Count direct Clay nodes per Flow node, prefix-sum counts into
`firstDirectClay`, allocate `directClayNodes`, and fill it in Clay preorder.
This produces stable display order and constant-time access to a selected Flow
node's direct contribution.

```cpp
for (DevClayNodeIndex clay = 0; clay < clayNodes.size(); ++clay) {
    const DevFlowNodeIndex owner = clayNodes[clay].directFlowOwner;
    if (owner != InvalidFlowNode) ++counts[owner];
}

uint32_t offset = 0;
for (DevFlowNode& flow : flowNodes) {
    flow.firstDirectClay = offset;
    flow.directClayCount = counts[&flow - flowNodes.data()];
    offset += flow.directClayCount;
}
```

Use a separate cursor vector for the fill pass; do not mutate
`firstDirectClay`.

---

## 9. Root and structural validation

## 9.1 Find the exact root

For each Flow node, inspect its packed direct Clay range and find nodes whose
`clayId == expectedClayId`.

- zero matches: set `MissingClayRoot`;
- one match: assign `clayRoot`;
- more than one: set `DuplicateClayRoot` and leave the root unresolved unless
  one unique ID-string match disambiguates for diagnostics only.

A 32-bit folded ID plus matching debug string is stronger evidence, but debug
text never replaces numeric identity. Any Flow-to-Clay bridge collision already
reported by `UiManager` remains an invalid identity condition.

## 9.2 Debug-name check

If both the Flow debug name and root `idString` are nonempty, compare their
bytes. A mismatch sets `ClayNameMismatch` and records both snapshot string
references in diagnostic detail if supported.

An absent debug string is not an error; numeric identity is the contract which
survives DEV-off builds. In developer mode `toClayEID` should normally provide
the same `debugName`.

## 9.3 All direct Clay must be inside the root

For a resolved Flow root `R`, every directly owned Clay node `C` must satisfy:

```text
C == R OR C is a Clay descendant of R
```

With Clay preorder this is constant time:

```cpp
bool isInSubtree(
    DevClayNodeIndex root,
    DevClayNodeIndex candidate,
    const std::vector<DevClayNode>& nodes) noexcept {
    return candidate >= root && candidate < nodes[root].subtreeEnd;
}
```

Any failure sets `EscapedClayEmission`. This catches sibling raw roots and
balanced raw Clay blocks emitted before or after the Flow root.

Nested child Flow roots are not in the parent's direct list, but they may still
be in the parent's **full** Clay subtree. This is precisely why direct ownership
and full subtree are separate queries.

## 9.4 Validate Flow parent against Clay

For every non-root Flow node with resolved roots:

1. if the child Clay root is a floating `DevClayRoot`, accept semantic
   detachment and set `FloatingClayRoot`;
2. otherwise find the nearest Clay ancestor which is the distinguished root of
   a Flow node;
3. require that Flow node to equal the recorded Flow parent.

Raw Clay wrappers between parent and child are allowed. Another Flow root
between them is not; that would mean the semantic capture stack and Clay tree
disagree.

For a Flow root, being nested in an unowned raw Clay wrapper is allowed. It
remains a Flow-forest root because no Flow semantic owner exists.

## 9.5 Floating-root validation

If a Flow root is floating:

- the floating Clay root itself must be the distinguished exact-ID root;
- every direct Clay contribution must remain inside that floating root;
- its Flow parent comes from the semantic capture stack;
- attachment parent and clip links come from the Clay root record;
- another floating root directly owned by the same Flow node is an escaped
  emission.

This makes the encouraged popup form unambiguous: one popup Flow element, one
popup Clay floating root.

## 9.6 Invalid Flow nodes remain visible

Do not delete a Flow node which violates these contracts. Keep it in the Flow
forest with flags and diagnostics. This lets the future Interface select the
bad element, show its emission interval/direct nodes, and explain how to fix
the element implementation.

---

## 10. Publishing only the current forests

## 10.1 Finish order

`finishAfterClayLayout` performs:

1. verify the Flow stack is empty; auto-close capture metadata if necessary;
2. copy the final Clay forest through the bridge;
3. compute Clay subtree ends;
4. resolve duplicate IDs, root attachment, and clip links;
5. assign direct Flow ownership by emission intervals;
6. pack Flow-to-direct-Clay adjacency;
7. resolve each exact Flow Clay root;
8. validate debug names, direct containment, and Flow/Clay parent agreement;
9. finalize statistics and completeness;
10. swap `building_` into `published_`;
11. clear the new building buffer while retaining vector capacities.

Only a structurally traversable snapshot is published. Contract diagnostics do
not block publication. A bridge failure or corrupt indices do: retain the last
completed snapshot and forward an error stating that the current frame's tree
capture failed.

## 10.2 Generation semantics

`generation` increments on every successful publication. A consumer can query:

```cpp
struct DevTreeView {
    const DevTreeSnapshot* snapshot = nullptr;
    uint64_t generation = 0;
};
```

The view is platform-thread-only and valid until the next successful
publication for that `UiManager`. This contract is enough for the first local
Interface and avoids reference counting or retained past frames.

## 10.3 Cancellation

`UiManager::cancelFrameState()` calls `devTreeCapture.cancelFrame()` before
discarding frame state. Cancellation:

- marks open building Flow nodes `CaptureCanceled`;
- clears active stacks and scratch sizes;
- does not publish the incomplete forests;
- leaves `published_` unchanged;
- reports canceled capture timing with `TimingRecordFlag::Canceled`.

The published forest therefore always corresponds to the most recently
completed UI frame, never a half-built replacement.

---

## 11. DevMonitoringAndReporting integration

## 11.1 Timing

Tree capture is developer-tool work and must be attributed as such.

Add summary/balanced zones:

```cpp
FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
    timing_,
    devSystems::TimingCategory::DevTool,
    devSystems::TimingZoneRole::DevToolWork,
    "flowui.dev_tree.clay_bridge");

FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
    timing_,
    devSystems::TimingCategory::DevTool,
    devSystems::TimingZoneRole::DevToolWork,
    "flowui.dev_tree.correlate");

FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
    timing_,
    devSystems::TimingCategory::DevTool,
    devSystems::TimingZoneRole::DevToolWork,
    "flowui.dev_tree.validate");

FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
    timing_,
    devSystems::TimingCategory::DevTool,
    devSystems::TimingZoneRole::DevToolWork,
    "flowui.dev_tree.publish");
```

These cover the substantial contiguous work precisely.

`beginFlow`/`endFlow` are interleaved with application element work. Emitting a
full timing record for every tiny capture call would distort both capture cost
and trace volume. Use one of two configurable measurements:

- normal mode: count calls/nodes and rely on the contiguous finalization zones;
- deep monitoring mode: read the timing recorder's clock around begin/end and
  accumulate nanoseconds into one per-frame `flowCaptureBookkeepingNs` counter,
  without emitting per-node spans.

The second mode measures the incremental cost while adding only clock-read
overhead, and that overhead is itself part of the reported developer-tool cost.

Publish at least these counters per window/frame:

- Flow nodes;
- Clay nodes and roots;
- direct links;
- raw/unowned Clay nodes;
- invalid Flow nodes;
- synthetic Clay nodes;
- copied string bytes;
- vector growth operations;
- bridge/correlation/validation/bookkeeping time;
- capture complete/truncated/failed.

## 11.2 Memory sources

Add development memory sources rather than charging vectors to application UI
layout memory:

```cpp
inline constexpr auto kDevelopment = makeMemorySourceDescriptor(
    "flowui.memory.development",
    MemoryDomain::ManagerCpu,
    MemorySourceKind::Development,
    MemoryAccuracy::Estimate);

// Retain the existing stable monitoring source ID, but place it under the
// common development parent.
inline constexpr auto kMonitoring = makeMemorySourceDescriptor(
    "flowui.memory.development.monitoring",
    MemoryDomain::ManagerCpu,
    MemorySourceKind::Development,
    MemoryAccuracy::AllocatorRequested,
    kDevelopment.id);

inline constexpr auto kDevTooling = makeMemorySourceDescriptor(
    "flowui.memory.development.tooling",
    MemoryDomain::ManagerCpu,
    MemorySourceKind::Development,
    MemoryAccuracy::Estimate,
    kDevelopment.id);

inline constexpr auto kDevTreeCapture = makeMemorySourceDescriptor(
    "flowui.memory.development.tooling.tree_capture",
    MemoryDomain::ManagerCpu,
    MemorySourceKind::Development,
    MemoryAccuracy::AllocatorRequested,
    kDevTooling.id);
```

Add `kDevelopment`, the re-parented `kMonitoring`, `kDevTooling`, and
`kDevTreeCapture` to `memory_sources::kAll`. The monitoring source keeps its
existing stable ID; only its parent relationship changes.

## 11.3 Vector memory accounting

`DevTreeCapture::appendDevMemorySamples` accounts for both buffers and all
scratch vectors because retained capacity is the real cost:

```cpp
void DevTreeCapture::appendDevMemorySamples(
    devSystems::MemorySampleSink& sink) const noexcept {
    devSystems::DevContainerMemoryAccumulator memory{};

    const auto addSnapshot = [&](const DevTreeSnapshot& value) {
        memory.add(value.flow.nodes);
        memory.add(value.flow.roots);
        memory.add(value.flow.directClayNodes);
        memory.add(value.clay.nodes);
        memory.add(value.clay.roots);
        memory.add(value.strings);
        memory.add(value.diagnostics);
    };

    addSnapshot(building_);
    addSnapshot(published_);
    memory.add(activeFlowScopes_);
    memory.add(lastFlowChild_);
    memory.add(clayTraversalScratch_);
    memory.add(layoutIndexToClayNode_);
    memory.add(clayIdIndex_);
    memory.add(lastClayChild_);
    memory.add(clayOpenByDepth_);
    memory.add(ownershipEvents_);
    memory.add(ownershipActive_);
    memory.add(directClayCounts_);
    memory.add(directClayCursors_);

    devSystems::appendManagerSample(
        sink,
        devSystems::memory_sources::kDevTreeCapture.id,
        memory,
        published_.window);
}
```

This reports:

- logical live bytes from vector sizes;
- reusable retained bytes from `capacity - size`;
- total backing capacity;
- object/capacity counts;
- per-window ownership.

`DevTreeCaptureStats` separately retains peak live bytes, peak capacity, and
growth-operation counts so reports can recommend better reserves. Do not call
`shrink_to_fit()` every frame; retained vector capacity is intentional and must
remain visible to monitoring.

`UiManager::appendDevMemorySamples` forwards to
`state_->devTreeCapture.appendDevMemorySamples(sink)`.

---

## 12. Error integration and author guidance

Add focused error codes or development diagnostics with actionable messages:

| Condition | Message/action |
|---|---|
| Missing exact root | “Flow element emitted no Clay root with `context.clayID()`; every Flow build must emit one.” |
| Duplicate exact root | “Flow element emitted its root ID more than once; one Flow instance must have exactly one Clay root.” |
| Escaped direct emission | “Clay node was emitted beside the Flow root; nest it under the root or make it a sibling Flow element.” |
| Parent mismatch | “Child Flow invocation did not place its Clay root inside the parent Flow root.” |
| Name mismatch | “The folded Clay root ID matched, but its debug string did not match the Flow instance debug name.” |
| Floating sibling | “A second floating Clay root was emitted directly; wrap that surface in its own Flow element.” |
| Bridge collision | “Two Flow identities folded to the same Clay ID; assign stable distinct Flow identities.” |

Diagnostics should carry window, frame, definition ID, instance ID, source
location, expected Clay ID, and involved node indices. Repeated identical
contract errors should be coalesced by the existing error reporting layer while
remaining flagged on every affected current-tree node.

---

## 13. File-level implementation plan

Recommended files:

```text
include/devSystems/devTooling/tree/DevTreeTypes.hpp
    Published Flow/Clay node, root, snapshot, diagnostic, and stats types.

include/devSystems/devTooling/tree/DevTreeCapture.hpp
    Per-window producer API and private scratch declarations.

include/internal/ClayDevTreeBridge.hpp
    DEV-only POD bridge views, traversal scratch, visitor, and count functions.

src/devSystems/devTooling/tree/DevTreeCapture.cpp
    Flow capture, Clay copy callbacks, correlation, validation, publication,
    timing, and memory reporting.

src/clay.cpp
    Existing CLAY_IMPLEMENTATION plus guarded bridge function definitions.

include/internal/FlowUiElementBridge.hpp
src/managers/UiManager.cpp
include/managers/FlowUiElementBuilder.hpp
    Replace current DevRuntime tree bridge calls with token-based capture.

include/internal/ManagerStorage/UiManagerState.hpp
    Own DevTreeCapture in DEV builds.

include/devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp
    Register development tooling/tree-capture memory sources.
```

`DevRuntime` keeps overrides and other legacy responsibilities during
migration, but `ElementTreePlaceholder` stops being its authoritative tree.
The legacy hierarchy panel can receive a temporary adapter over
`DevTreeSnapshot::flow`.

---

## 14. Implementation order

### Step 1 — types and vector buffers

1. Add published tree types and `DevTreeCapture` with begin/cancel/publish
   skeletons.
2. Add it to `UiManagerState` under `FLOW_UI_DEV_MODE`.
3. Implement string copying, vector reserve/growth tracking, and current view.
4. Add memory sources and vector memory sampling immediately.

### Step 2 — tokenized Flow forest

1. Move capture start to the build/construct emission phase.
2. Return tokens from `beginFlow` and retain construct tokens in
   `ConstructedElementFrame`.
3. Build parent/child/sibling/subtree relations.
4. Exclude `SkipBuildCallback` invocations from the UI forest.
5. Implement cancellation, auto-close, and unbalanced-token diagnostics.

### Step 3 — Clay bridge

1. Add element/root count accessors.
2. Add caller-scratch iterative traversal.
3. Copy pointer-free final Clay values, ID strings, text, roots, clips, and
   attachment metadata.
4. Mark post-build synthetic transition nodes.
5. Compare a debug dump against Clay's built-in debugger on the same layouts,
   without enabling both in the measured capture.

### Step 4 — correlation and contract validation

1. Implement ownership-event sorting and sweep.
2. Pack reverse direct-Clay adjacency.
3. Resolve exact folded-ID roots and compare debug names.
4. Validate direct containment, Flow parentage, and floating-root rules.
5. Forward compact diagnostics to error monitoring.

### Step 5 — timing and migration

1. Add bridge/correlation/validation/publish DevTool timing zones.
2. Add deep-mode aggregate bookkeeping timing.
3. Publish per-frame tree counters.
4. Adapt the existing hierarchy view to the new Flow forest.
5. Remove current depth-only `ElementTreePlaceholder` capture and unused
   `appendCapturedClayElement` after parity tests pass.

---

## 15. Required tests

## 15.1 Valid Flow shapes

- draw element emits one `context.clayID()` root and raw Clay descendants;
- construct element's builder-opened root is linked correctly;
- nested draw Flow element becomes a semantic child and Clay descendant;
- child under one or several raw Clay wrapper nodes still resolves to the same
  Flow parent;
- child created between `construct()` and `drawConstructed()` is nested in both
  forests;
- popup Flow child owns exactly one floating Clay root and retains attachment,
  clip, z-index, and paint order;
- several top-level Flow elements produce several Flow roots;
- a Flow root nested inside raw application Clay remains a Flow root;
- raw Clay outside all Flow scopes is unowned but fully present.

## 15.2 Invalid Flow shapes

- draw callback emits no Clay node;
- draw callback emits Clay nodes but none with the folded Flow ID;
- callback emits the exact root twice;
- callback emits the exact root plus a raw sibling;
- callback emits two floating roots directly;
- child Flow is invoked in the semantic scope but its nonfloating Clay root is
  emitted outside the parent root;
- Flow/Clay numeric root matches but debug names differ;
- constructed scopes close out of order;
- frame cancellation leaves tokens open;
- `SkipBuildCallback` does not publish an invalid Flow node.

Each invalid case must retain a traversable snapshot where possible and attach
the expected node flag and diagnostic.

## 15.3 Clay bridge fidelity

- main root, normal containers, anonymous elements, and text nodes are copied;
- Clay DFS child order and subtree ranges match internal child arrays;
- all floating roots are copied in final paint order;
- final boxes match `Clay_GetElementData` for unique known IDs;
- normalized/final layout values match `Clay_LayoutElement` after
  `Clay_EndLayout`;
- pointer fields are represented only as presence bits;
- ID strings and text remain valid after the next `Clay_BeginLayout`;
- exit-transition clones are marked synthetic and unowned by current emission;
- duplicate Clay IDs are marked and never form unique attachment/root links;
- insufficient traversal scratch fails without mutating Clay;
- capture never changes debug-mode state, layout width, render commands, or hit
  testing.

## 15.4 Correlation

- parent raw nodes before and after a nested Flow interval remain parent-owned;
- child interval wins ownership for all of its directly emitted nodes;
- full Flow Clay subtree includes nested child Flow roots;
- direct Flow contribution excludes nested child Flow nodes;
- ordinal-to-Clay mapping handles floating nodes whose final DFS order differs
  from declaration order;
- synthetic nodes beyond the authored boundary receive no current Flow owner;
- empty intervals do not corrupt the ownership event stack.

## 15.5 Memory and timing

- both snapshot buffers and every scratch vector contribute to memory samples;
- clearing a vector moves bytes from logical live to reusable without hiding
  backing capacity;
- reserve growth increments growth counters and peak capacity;
- safety-ceiling truncation is reported and does not alter application UI;
- bridge, correlation, validation, and publication appear as `DevToolWork`;
- deep bookkeeping timing is aggregated rather than emitting per-node spans;
- DEV-off symbol/binary inspection finds no tree vectors, bridge functions,
  debug strings, memory sources, or timing zones.

---

## 16. Decisions explicitly closed by this report

1. A Flow element which emits no exact folded-ID Clay root is invalid.
2. One Flow instance has exactly one distinguished Clay root.
3. The distinguished root debug string should equal the Flow instance debug
   name when both exist.
4. A Flow child is determined by active semantic capture scope and validated
   against Clay containment.
5. A floating Flow child remains a Flow child while its Clay root is a separate
   Clay forest root.
6. Direct raw Clay siblings emitted by one Flow element are invalid; they must
   be nested or wrapped as sibling Flow elements.
7. The Flow forest contains only Flow nodes.
8. The Clay forest contains every reachable Clay node, including raw/unowned
   nodes and synthetic transition nodes.
9. Emission intervals determine direct Flow ownership; exact folded IDs
   determine distinguished roots.
10. Full Clay subtree and direct Clay contribution are separate supported
    queries.
11. Capture uses retained `std::vector` buffers, not `StorageSystem`.
12. Only the latest successfully completed snapshot is published; no past-frame
    tree history is retained.
13. Clay is copied after `Clay_EndLayout` through a read-only, caller-scratch
    bridge in `src/clay.cpp`.
14. Clay's built-in debugger is not enabled to obtain the tree.
15. Capture memory and time are reported as development-tool cost through
    `DevMonitoringAndReporting`.
16. Clay-forest capture is opt-in through `FLOW_UI_DEV_CAPTURE_CLAY`; the Clay
    bridge exists only when it and `FLOW_UI_DEV_MODE` are both enabled, while
    Flow-forest capture remains available in ordinary DEV builds.

---

## Final implementation assessment

The strengthened Flow contract makes `DevTreeCapture` more useful and simpler
to reason about. A Flow node is no longer merely a callback invocation which
might or might not correspond to layout. It is a semantic wrapper with one
provable Clay root. The folded Flow-to-Clay ID becomes an enforced bridge rather
than a best-effort correlation hint.

Emission intervals still solve the harder ownership problem: a Flow element can
emit several raw Clay implementation nodes, and nested Flow callbacks create
their own intervals. The deepest interval identifies direct contribution while
the exact-ID root and final Clay ancestry validate structure. Together these
facts let the Interface show either the Flow-only hierarchy, the complete Clay
hierarchy, or the exact Clay section implemented by a selected Flow instance
without storing a mixed tree.

The Clay bridge remains necessary because raw Clay is valid and because final
geometry, floating roots, clips, transitions, and resolved values only exist
authoritatively after Clay layout. Keeping the bridge read-only, allocation-free
inside Clay, and compiled out of production preserves that authority without
turning on Clay's layout-altering debugger.

Finally, retained vectors and current-only publication match the immediate
tooling requirement. Accurate memory samples and DevTool timing make their real
cost visible now, allowing later tuning to be based on measured applications
rather than speculative integration with production storage.
