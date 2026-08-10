# Element State and Resources Migration Report

**Scope:** migrate Flow element state and resources out of mutable static `ElementDefinition` storage and into FlowUi's centralized `StorageSystem` architecture.

**Assumption:** one `FlowUi::App` exists per program. Multiple FlowUi windows may exist, and the storage design must remain valid if window UI builds become concurrent later.

**Out of scope for this migration:** final theme design, child-ID redesign, bindings/events, slots, and the complete replacement of function-pointer callbacks. The storage API must, however, be compatible with the planned compile-time/concept-based element system so it is not replaced again during that work.

## Executive decision

The recommended design is:

- Add one app-owned public `ElementManager` and one internal `ElementStorageController`.
- Store the controller root and all state/resource payload memory through `IStorageSystem`.
- Treat element definitions and their resources as app-wide.
- Treat ordinary element state as window-owned because state belongs to one rendered instance, not merely to a definition and string hash.
- Resolve state with the composite identity `(definition key, window, instance ID)`.
- Give each stateful invocation a retention policy. The default is garbage-collected transient state; callers may explicitly retain an instance for the window lifetime or promote it to app-lifetime state with an app-wide state key.
- Use stable, type-homogeneous state pages. Hash-table reallocation must never move live user state objects.
- Resolve a state at most once per element invocation and expose it as `context.state()`.
- Store one resource object per definition for the app, construct it once, expose it as `context.resources()`, and return it as `const` by default so multiple windows can read it concurrently.
- Use compile-time element traits/concepts to generate the type-erasure operations required by central storage. Mutable data is runtime-owned; only immutable type metadata and function addresses remain static.
- Integrate state frame tracking with `beginFrame`, successful frame commit, cancellation, window destruction, and app shutdown.

This preserves the immediate-mode element model. It does not create retained element objects or a retained UI tree. Only state/resources that already outlive one build invocation become centrally owned.

## 1. Current architecture and exact failure modes

The current `ElementDefinition` specialization owns:

```cpp
static inline std::optional<ResourcesType> resources{};
static inline std::vector<std::pair<uint64_t, StateType>> statePool{};
```

The public helpers linearly scan or mutate these static containers:

```cpp
Definition::getOrCreateState(flowId);
Definition::tryGetState(flowId);
Definition::tryGetStateConst(flowId);
Definition::eraseState(flowId);
Definition::getResources(app);
```

### 1.1 Static ownership does not match application ownership

Even with the explicit one-`App` invariant, mutable process-static storage is the wrong owner:

- it is initialized outside the central `StorageSystem`;
- it cannot participate in storage budgets, statistics, failure injection, or shutdown ordering;
- it survives independently of windows;
- it has no owning window information;
- it cannot be partitioned for future concurrent window builds;
- it exposes mutable implementation containers publicly;
- test processes cannot cleanly reset the element subsystem without manually visiting every instantiated definition type.

The one-`App` invariant removes the need for an app ID in every element key. It does not remove the need for app-controlled lifetime.

### 1.2 State identity currently aliases across windows

State lookup uses only the 64-bit `FlowElementId`. If two windows build the same element definition with the same string ID, both resolve the same static entry. That is accidental cross-window sharing of arbitrary mutable user state.

Ordinary state identity must instead be:

```text
definition + owning window + instance ID
```

Sharing state across windows should be explicit and use a separate app-lifetime state address.

### 1.3 Window destruction leaves permanent stale state

When a secondary window is destroyed, `UiManager` storage and window-local manager records are destroyed, but static element state has no connection to that lifecycle. Every state instance created by that window remains until the app process exits or user code happens to call `eraseState()` for every key.

This is no longer a theoretical leak. Creating and closing windows with different element instances permanently increases every affected static pool.

### 1.4 `std::vector` invalidates recursive state references

The current state getter returns a reference into `statePool`. A recursively nested element can do the following:

1. parent gets its `State&`;
2. parent draws a child of the same definition;
3. child inserts into the same vector;
4. vector grows and moves all entries;
5. the parent reference is dangling.

`eraseState()` also replaces an entry with the last vector item, invalidating the moved entry's previous address. This means a hash map alone is not enough; the payload allocation itself must be stable.

### 1.5 Lookup cost grows linearly

Every `getOrCreateState` and `tryGetState` scans all instances of that definition. Several current controls repeat lookup in hover, press, hold, logic, build, and nested callbacks. The dev elements currently contain 22 `getOrCreateState` calls and 17 `tryGetState`/`tryGetStateConst` calls across 14 implementation files.

The target is one average constant-time lookup per invocation and direct reuse of the resolved pointer through every callback phase.

### 1.6 Resources are mutable global data

Resources are app-wide in concept, which is correct, but the static optional is still the wrong implementation:

- construction is not synchronized;
- arbitrary mutation can race between future window worker threads;
- no storage allocation is tracked;
- destructor ordering relative to icons, images, fonts, themes, and storage is implicit;
- the dev resource initializer directly modifies public static optionals;
- resources can accidentally capture a window-specific `UiManager&`, even though the definition is not window-specific.

### 1.7 Definition type and definition object are currently conflated

Storage is attached to an `ElementDefinition<Params, State, Resources, Id, Dev>` template specialization. The intended future direction uses compile-time hooks and concepts, so the new controller must not depend on that exact five-argument specialization. It must consume normalized traits such as `StateOf<Element>` and `ResourcesOf<Element>`.

## 2. Required semantics

The migration should begin by locking these semantics in tests and documentation.

### 2.1 Definition semantics

- An element definition is app-wide and immutable.
- A definition ID identifies the authored element kind, not one rendered instance.
- A definition type may be used in any FlowUi window.
- A definition must not own mutable state or resource payloads in static memory.
- Static compile-time metadata and type-erasure function addresses are allowed because they are immutable.

### 2.2 Resource semantics

- Default cardinality: exactly one `Resources` object per definition for the app.
- Resources are not window-specific.
- Resources are lazily constructible, but an explicit preparation API is available.
- Resources are destroyed before app-shared managers/storage they may depend on.
- Normal build access is `const Resources&`.
- Mutable resource updates are explicit and only legal at a quiescent/app-manager synchronization point.
- Two windows can concurrently read the same resource object.
- A resource type may be non-copyable and non-movable.

### 2.3 State semantics

- Default cardinality: one `State` per definition, owning window, and element instance ID.
- The same definition and instance ID in two windows produces two state objects.
- A state object has a stable address for its entire lifetime.
- State is mutable by the owning window's UI build.
- State can be non-copyable and non-movable.
- State destruction is deterministic at a safe lifecycle boundary.
- Default state is eventually collected when an instance permanently disappears.
- A caller can explicitly request longer retention for a particular invocation.
- Closing a window releases every state owned by that window, except state explicitly addressed as app-lifetime state.
- App-lifetime state is not automatically shared merely because two windows use the same local element ID.
- Disk serialization is a separate feature; “app lifetime” in this report means in-memory lifetime until explicit erase or `App` shutdown.

### 2.4 Threading semantics

- The current one-active-window-frame restriction remains valid during migration.
- The data layout must permit future concurrent builds of different windows without redesign.
- Different window partitions must not contend on one global state mutex during ordinary lookup.
- FlowUi cannot make arbitrary user `State` types internally thread-safe. One window build owns its window-local state mutation.
- App-wide resources are immutable during builds.
- App-lifetime mutable state shared between windows requires explicit synchronization and should not be the default.

## 3. Public and internal ownership model

### 3.1 Public `ElementManager`

Add an app-owned manager reachable through:

```cpp
FlowUi::ElementManager& App::elements();
const FlowUi::ElementManager& App::elements() const;
```

`ElementManager` is the public lifecycle/query surface. It holds a storage handle or pointer to the internal controller but does not directly own payload memory.

Responsibilities:

- prepare resources for a definition;
- query or mutate state outside a build through safe callback/lease APIs;
- erase state explicitly;
- expose diagnostics and state counts;
- configure global state-GC defaults;
- bridge app/window lifecycle calls to the internal controller.

It should not expose raw storage pages, mutable maps, or controller locks.

### 3.2 Internal `ElementStorageController`

Add:

```text
include/internal/ManagerStorage/ElementStorageController.hpp
src/managers/ElementStorageController.cpp
```

The controller root should itself live in one app-scoped `ManagerRecord`, created during `App::Impl::init()`:

```cpp
const auto rootName = storage.intern("flowui.elements.root");
const auto handle = manager_storage::createState<ElementStorageController>(
    storage,
    storage::ResourceKey{
        .domain = storage::ResourceDomain::Internal,
        .name = rootName,
        .window = 0,
    },
    storage::ResourceKind::ManagerRoot,
    rootName,
    storage,
    elementConfig,
    resourceServices);
```

This matches the existing manager-state pattern. The controller then allocates its definition records, state pages, lookup metadata, and resource payloads through `IStorageSystem::allocatePersistent()`.

### 3.3 Connection to `UiManager`

Every `UiManager` receives a non-owning pointer/reference to the same app-wide `ElementManager` or controller, plus its own `WindowId`:

```cpp
window.ui.setElementManager(&elementManager);
```

The `UiManager` must not own the controller. It may cache stable per-definition/per-window partition pointers for its window.

### 3.4 No mutable cache on definition statics

Do not put a controller pointer, bucket pointer, resource pointer, or once flag back onto the inline definition object/type. That recreates hidden process-static lifetime and complicates shutdown/reinitialization tests.

Allowed static data:

```cpp
inline constexpr ElementTypeDescriptor descriptor = { /* immutable type metadata */ };
```

Runtime caches belong to `UiManager` or the controller.

## 4. Identity model

### 4.1 Definition key

Use both the authored ID and C++ definition type hash:

```cpp
struct ElementDefinitionKey {
    FlowDefinitionId definitionId = 0;
    uint64_t definitionTypeHash = 0;

    auto operator<=>(const ElementDefinitionKey&) const = default;
};
```

Why both:

- `definitionId` is stable, readable through debug registration, and suitable for persisted tooling data;
- `definitionTypeHash` catches two different C++ element types accidentally using the same authored ID;
- state/resource type hashes can be validated when a definition is first registered.

In developer mode, retain interned names for the definition type, state type, and resource type and produce a hard diagnostic on incompatible reuse.

### 4.2 Window-local state key

```cpp
struct WindowElementStateKey {
    ElementDefinitionKey definition;
    WindowId window = InvalidWindowId;
    FlowElementId instanceId = 0;
};
```

The owning window is part of identity, not just metadata. Same ID in another window is a distinct state.

### 4.3 App-lifetime state key

App-lifetime state must be explicitly addressed:

```cpp
struct AppElementStateKey {
    ElementDefinitionKey definition;
    FlowElementId persistentInstanceId = 0;
};
```

This key has no window. A caller opting into it acknowledges that two windows using the same app state key resolve the same object.

Do not silently “promote” a window key by dropping its window component. The builder must know the state policy/address before state resolution.

### 4.4 Definition collisions

On first use, register:

```cpp
struct ElementTypeRegistration {
    ElementDefinitionKey key;
    uint64_t stateTypeHash;
    uint64_t resourcesTypeHash;
    size_t stateSize;
    size_t stateAlignment;
    size_t resourcesSize;
    size_t resourcesAlignment;
    storage::StringId definitionName;
    storage::StringId stateTypeName;
    storage::StringId resourcesTypeName;
};
```

If the same `ElementDefinitionKey` appears with different type metadata, throw before constructing a payload. Release builds may omit display strings but should still validate hashes, sizes, and alignments.

## 5. State retention and garbage collection

### 5.1 Policy model

Use one explicit policy object rather than unrelated boolean flags:

```cpp
enum class ElementStateRetention : uint8_t {
    Transient,
    WindowLifetime,
    AppLifetime,
};

struct ElementStatePolicy {
    ElementStateRetention retention = ElementStateRetention::Transient;
    uint32_t graceFrames = UseElementDefaultGraceFrames;
    FlowElementId appStateId = 0; // Required only for AppLifetime.

    static constexpr ElementStatePolicy transient(uint32_t graceFrames = UseElementDefaultGraceFrames);
    static constexpr ElementStatePolicy windowLifetime();
    static constexpr ElementStatePolicy appLifetime(FlowElementId appStateId);
};
```

Semantics:

- **Transient:** mark the state as used whenever the element is invoked. Collect it after it has not appeared in a configured number of successfully completed frames for its window.
- **WindowLifetime:** do not collect it merely because it disappears from UI output. Destroy it when the owning window closes or the caller explicitly erases it.
- **AppLifetime:** store it in the app partition and keep it across window destruction. Destroy it on explicit erase or app shutdown.

Recommended defaults:

```cpp
ElementStatePolicy::transient(/* graceFrames = */ 2)
```

Two frames prevent accidental collection due to a single canceled/conditional frame while still cleaning rapidly changing lists. Make the app default configurable. Elements that implement tabs, cached editors, or expensive expandable trees can declare `WindowLifetime` as their compile-time default.

### 5.2 Per-definition default and per-instance override

Future element type:

```cpp
struct TreeView {
    using State = TreeViewState;
    static constexpr auto defaultStatePolicy =
        FlowUi::ElementStatePolicy::windowLifetime();
};
```

Per invocation:

```cpp
ui.element(kTreeView, "project-tree", props)
    .statePolicy(FlowUi::ElementStatePolicy::transient(30))
    .draw();
```

Explicit app-lifetime state:

```cpp
ui.element(kInspector, "window/inspector", props)
    .statePolicy(FlowUi::ElementStatePolicy::appLifetime(
        FLOW_ID("workspace/primary-inspector")))
    .draw();
```

Calling `.statePolicy(...)` for a stateless element should fail at compile time.

### 5.3 Policy consistency

Once a state address exists:

- changing transient grace is allowed;
- promoting `Transient` to `WindowLifetime` is allowed;
- demoting `WindowLifetime` to `Transient` is allowed only at a safe frame boundary;
- changing a window-local state to `AppLifetime` is not an in-place policy change because it changes identity;
- app-lifetime state requires the app state key before first resolution;
- using the same app state key with incompatible definition/type metadata is an error.

This avoids complicated rekeying while a user holds `State&`.

### 5.4 Frame transaction

GC must observe successfully authored frames, not partially failed ones.

Add controller hooks:

```cpp
void beginWindowFrame(WindowId window, uint64_t frameNumber, storage::FrameEpoch epoch);
void commitWindowFrame(WindowId window, storage::FrameEpoch epoch) noexcept;
void cancelWindowFrame(WindowId window, storage::FrameEpoch epoch) noexcept;
```

During a frame, each state access adds its handle to a touched list for that window/epoch. New states are also added to a created list.

On commit:

- update `lastSeenFrame` for touched state;
- apply queued policy changes and erases;
- advance the incremental GC cursor;
- clear transaction lists.

On cancellation:

- do not advance the GC frame;
- discard pending touches and policy changes;
- destroy state created only by the canceled transaction, or mark it immediately eligible for collection if rollback destruction would complicate exception handling;
- never collect pre-existing state because an incomplete frame omitted it.

The stronger recommendation is to destroy newly created canceled-frame state. It prevents a repeatedly failing UI build from accumulating state.

### 5.5 Safe collection point

Never collect while element callbacks can still be running. The recommended commit point is after `UiManager::endFrame()` and all frame preparation has succeeded, immediately before or after a successful storage frame seal. The commit operation must be no-throw; any GC maintenance failure should defer collection and report diagnostics rather than invalidate an otherwise prepared UI frame.

`cancelStorageFrame()` must also call `cancelWindowFrame()` before clearing the frame token.

### 5.6 Incremental collector

Do not scan every state in every window after every frame.

Each window partition should maintain:

- an intrusive/list index of its transient state slots;
- a sweep cursor;
- a count of live, retained, and eligible slots;
- the last successfully committed window frame number.

At each commit, inspect at most a configured budget:

```cpp
struct ElementStateGcConfig {
    uint32_t defaultGraceFrames = 2;
    uint32_t scanBudgetPerCommittedFrame = 256;
    uint32_t fullSweepIntervalFrames = 120;
};
```

A slot is collectible when:

```cpp
policy == Transient &&
committedFrame > lastSeenFrame &&
committedFrame - lastSeenFrame > graceFrames
```

An incremental scan makes ordinary work bounded. A full sweep may be requested during memory pressure, app idle time, or diagnostics. Window destruction bypasses incremental GC and destroys the entire window partition immediately.

An expiry-wheel design is another option, but moving every live state between expiry buckets every frame adds bookkeeping. Start with a budgeted cursor; benchmark before adding a timing wheel.

### 5.7 Explicit erasure

Public erasure:

```cpp
app.elements().eraseState(kSlider, windowId, FLOW_ID("volume"));
app.elements().eraseAppState(kInspector, FLOW_ID("workspace/primary-inspector"));
```

If called while the relevant window frame is building, queue erasure until commit/cancel handling. If called at a quiescent point, erase immediately. Never destroy a state that could still be referenced by a live `ElementContext`.

### 5.8 Window destruction

Add:

```cpp
elementManager.destroyWindow(windowId);
```

Call it after active work for the window is canceled/drained and before `storageSystem->unregisterWindow()`. It must:

- reject or cancel an active element frame transaction;
- destroy all transient and window-lifetime state for that window;
- release every window state page and lookup allocation;
- remove the window partition from all definition records/caches;
- leave app-lifetime state and definition resources untouched;
- invalidate external generational state handles;
- be idempotent for partial window-creation cleanup.

This is the primary fix for permanently stale state from closed windows.

## 6. State storage layout

### 6.1 Alternatives considered

#### One `ManagerRecord` per state instance

**Advantages:** reuses existing generational handles and window resource keys; destruction callbacks already exist.

**Disadvantages:** every state creates an interned composite name and a global manager-record entry; hot retrieval reaches the storage-wide mutex; thousands of elements inflate the manager-record map; GC performs many individually locked removals.

This is acceptable as a short prototype but not recommended as the final UI hot path.

#### One relocating vector per definition/window

**Advantages:** compact and simple.

**Disadvantages:** recreates the current dangling-reference bug and requires linear lookup or a second index.

Reject.

#### Type-homogeneous stable pages managed by `ElementStorageController`

**Advantages:** stable addresses, dense payload storage, cheap free-list reuse, one hash index per partition, storage-budget tracking, and no storage-global lock after partition resolution.

**Disadvantages:** more implementation work and custom destruction/GC logic.

This is the recommended final design.

### 6.2 Definition record

Conceptual internal structure:

```cpp
struct ElementDefinitionRecord {
    ElementTypeRegistration registration{};
    ElementTypeOps stateOps{};
    ElementTypeOps resourceOps{};

    ResourceSlot resources{}; // App-wide; absent for resource-free definitions.

    std::mutex partitionsMutex{};
    std::unordered_map<WindowId, WindowStatePartition*> windowPartitions{};
    AppStatePartition* appStatePartition = nullptr;
};
```

The definition record address must be stable. Controller registry rehashing must move only pointers/handles, not the record object.

### 6.3 Window state partition

Create one partition per stateful definition per window on first use:

```cpp
struct WindowStatePartition {
    WindowId window = InvalidWindowId;
    ElementDefinitionRecord* definition = nullptr;
    std::mutex mutex{};
    StatePageList pages{};
    StateFreeList freeSlots{};
    FlatHashMap<FlowElementId, ElementStateHandle> byInstance{};
    std::vector<ElementStateHandle> touchedThisFrame{};
    std::vector<ElementStateHandle> createdThisFrame{};
    size_t gcCursor = 0;
};
```

Future concurrent builds of two windows use different partition mutexes and maps. The definition's app-wide resource lock is separate.

### 6.4 State pages and slots

Each partition is type-homogeneous: every slot contains the same `State` type. Calculate an aligned stride once:

```text
[StateSlotHeader][alignment padding][State payload]
```

Header fields:

```cpp
struct StateSlotHeader {
    FlowElementId instanceId = 0;
    uint64_t lastSeenFrame = 0;
    uint32_t generation = 1;
    uint32_t graceFrames = 0;
    ElementStateRetention retention = ElementStateRetention::Transient;
    bool occupied = false;
    bool touchedInPendingFrame = false;
};
```

Pages are allocated with:

```cpp
storage.allocatePersistent(
    pageBytes,
    pageAlignment,
    storage::AllocationTag{
        .memoryClass = storage::MemoryClass::WindowPersistent,
        .resourceKind = storage::ResourceKind::UiElementState,
        .window = window,
        .debugName = definitionNameId,
    });
```

Add `UiElementState`, `UiElementResources`, and optionally `UiElementRegistry` to `ResourceKind` so storage diagnostics report them separately. Because this changes storage interface-visible enums, bump `IStorageSystem::CurrentInterfaceVersion`.

Pages never relocate. GC destroys a payload and returns its slot to the free list. Empty trailing pages can be released; retaining one empty page per active partition may reduce churn.

### 6.5 Handle format

External or deferred references use a generation:

```cpp
struct ElementStateHandle {
    uint32_t definitionSlot = 0;
    uint32_t partitionSlot = 0;
    uint32_t stateSlot = 0;
    uint32_t generation = 0;
};
```

The exact packing can change. Requirements:

- stale handles fail after erase/GC/window destruction;
- generation increments on slot reuse;
- raw pointers are never advertised as stable beyond their documented context/lease;
- developer tools can resolve a handle without scanning every definition pool.

### 6.6 Lookup cache

The first use of a definition in a `UiManager` may resolve its `WindowStatePartition*` through the controller registry. Cache that stable pointer in window-owned `UiManager` state:

```cpp
FlatHashMap<ElementDefinitionKey, WindowStatePartition*> elementPartitions;
```

Clear the cache when the window is destroyed. Do not cache it on the static definition type.

After this first resolution, state lookup is:

```text
UiManager cached partition -> partition hash lookup by instance ID -> stable slot payload
```

No global storage/controller lock is required on a cache hit.

## 7. Type erasure generated from compile-time element types

Central storage contains arbitrary user structs, so some runtime type erasure is unavoidable. Compile-time callback dispatch does not eliminate the need to call the correct destructor when a state is collected.

The important distinction is:

- element event/logic/build hooks use `if constexpr` static dispatch;
- state/resource payload lifetime uses a small immutable type-operation descriptor generated at compile time.

### 7.1 Type operations

```cpp
struct ElementTypeOps {
    uint64_t typeHash = 0;
    storage::StringId typeNameId = 0;
    size_t size = 0;
    size_t alignment = 0;

    void (*construct)(void* destination, const ElementObjectInitContext& init) = nullptr;
    void (*destroy)(void* object) noexcept = nullptr;
};
```

Generated by a template:

```cpp
template <typename T>
ElementTypeOps makeElementTypeOps(storage::IStorageSystem& storage) {
    return {
        .typeHash = FlowUi::detail::typeHash<T>(),
        .typeNameId = storage.intern(FlowUi::detail::typeToken<T>()),
        .size = sizeof(T),
        .alignment = alignof(T),
        .construct = +[](void* destination, const ElementObjectInitContext& init) {
            if constexpr (std::constructible_from<T, const ElementObjectInitContext&>) {
                ::new (destination) T(init);
            } else {
                static_assert(std::default_initializable<T>);
                ::new (destination) T();
            }
        },
        .destroy = +[](void* object) noexcept {
            static_cast<T*>(object)->~T();
        },
    };
}
```

The function pointers are immutable metadata. They do not reintroduce runtime-polymorphic element hooks or mutable definition storage.

### 7.2 Construction failure

State/resource constructors may throw. The controller must:

1. reserve a free slot or allocate a page;
2. invoke placement construction;
3. publish the map entry only after construction succeeds;
4. on failure, return the slot/page to its previous state;
5. leave no handle or partially occupied header published.

Destructors used by GC and shutdown must be `noexcept`. Enforce this with `static_assert(std::is_nothrow_destructible_v<T>)` or document that a throwing destructor terminates as normal C++ destruction would.

### 7.3 Over-aligned and non-movable types

Tests must include an `alignas(64)` state/resource and types with deleted copy/move constructors. Page stride and resource allocations must honor `alignof(T)`. GC must destroy in place and never compact by moving payloads.

## 8. Compatibility with the future concept-based element system

### 8.1 Do not bind storage to the current template parameter pack

The future element type can be a normal user-authored type:

```cpp
struct CounterElement {
    using Parameters = CounterParams;
    using State = CounterState;
    using Resources = CounterResources;

    static constexpr FlowDefinitionId definitionId =
        FLOW_DEF_ID("example/counter");
    static constexpr bool devInternal = false;

    static void onPressed(FlowUi::ElementContext<CounterElement>& context);
    static void build(FlowUi::ElementContext<CounterElement>& context);
};

inline constexpr CounterElement kCounter{};
```

FlowUi can offer an optional convenience base, but the concept should not require inheritance:

```cpp
template <typename E>
concept FlowElement = requires {
    { E::definitionId } -> std::convertible_to<FlowDefinitionId>;
} && HasBuildOrConstructHook<E>;
```

Trait normalization handles missing optional surfaces:

```cpp
template <typename E>
using ParametersOf = /* E::Parameters or NoElementParameters */;

template <typename E>
using StateOf = /* E::State or NoElementState */;

template <typename E>
using ResourcesOf = /* E::Resources or NoElementResources */;
```

This lets users continue defining arbitrary parameter/state/resource structs without requiring the old `ElementDefinition<P, S, R, Id, Dev>` specialization.

### 8.2 Compile-time hook execution

Future invocation pipeline:

```cpp
template <FlowElement E>
void runElement(E element, ElementInvocation<E>& invocation) {
    ElementContext<E> context(invocation);

    if constexpr (HasOnHovered<E>) {
        if (invocation.interaction.hovered()) E::onHovered(context);
    }
    if constexpr (HasOnPressed<E>) {
        if (invocation.interaction.pressed()) E::onPressed(context);
    }
    if constexpr (HasLogic<E>) {
        E::logic(context);
    }
    if constexpr (HasBuild<E>) {
        E::build(context);
    }
}
```

The storage controller is called through templated context methods, so state/resource type information remains compile-time even though payload lifetime is centrally managed.

### 8.3 Context must know the full element type

Current contexts are templated only on `Parameters`. That is insufficient for typed `context.state()` and `context.resources()`.

Change toward:

```cpp
template <FlowElement E>
class ElementContext {
public:
    using Parameters = ParametersOf<E>;
    using State = StateOf<E>;
    using Resources = ResourcesOf<E>;

    Parameters& params();

    State& state()
        requires HasState<E>;

    const Resources& resources()
        requires HasResources<E>;
};
```

For the transitional function-pointer definition, its aliases can use the complete definition specialization:

```cpp
using Self = ElementDefinition<Parameters, State, Resources, DefinitionId, IsDevInternal>;
using BuildContext = ElementBuildContext<Self>;
using InteractionContext = ElementInteractionContext<Self>;
```

This is a useful migration even before callbacks become static hooks.

### 8.4 State resolution frequency

For a stateful element, the builder/invocation should resolve state once before the first hook and store the pointer in the invocation/context. Every event, logic, and build hook receives the same pointer.

Do not make every `context.state()` call perform a hash lookup. In debug mode it may validate the handle/generation; in release it should normally dereference the already resolved pointer.

### 8.5 Resource resolution frequency

Resources can be resolved lazily on the first `context.resources()` call and cached in the definition record/context. Definitions with known resources may also resolve once at invocation creation. Because resources are one per app and pointer-stable, all later invocations should hit a cached definition-record pointer.

## 9. Proposed future user code

### 9.1 Ordinary stateful element

```cpp
struct ToggleState {
    bool pressed = false;
    float animation = 0.0f;
};

struct ToggleElement {
    using Parameters = ToggleParams;
    using State = ToggleState;

    static constexpr auto definitionId = FLOW_DEF_ID("flowui.basic/toggle");

    static void onPressed(FlowUi::ElementContext<ToggleElement>& context) {
        context.state().pressed = true;
    }

    static void logic(FlowUi::ElementContext<ToggleElement>& context) {
        auto& state = context.state();
        state.animation = updateToggleAnimation(state.animation, context.params().enabled);
    }

    static void build(FlowUi::ElementContext<ToggleElement>& context) {
        const auto& state = context.state();
        drawToggle(context, state.animation);
    }
};

inline constexpr ToggleElement kToggle{};
```

The state lookup happens once even though three hooks use it.

### 9.2 Ordinary call site with default GC

```cpp
ui.element(kToggle, "settings/show-grid", ToggleParams{
    .enabled = settings.showGrid,
})
.draw();
```

No state API appears at the ordinary call site. The state is transient with the element/default grace period and window-local identity.

### 9.3 Keep state while temporarily absent

```cpp
ui.element(kCodeEditor, "tabs/readme", editorParams)
    .statePolicy(FlowUi::ElementStatePolicy::windowLifetime())
    .draw();
```

The editor may disappear when another tab is active without losing cursor/scroll state. Closing the window still destroys it.

### 9.4 Explicit app-lifetime state

```cpp
ui.element(kWorkspaceTree, "secondary-window/tree", treeParams)
    .statePolicy(FlowUi::ElementStatePolicy::appLifetime(
        FLOW_ID("workspace/project-tree")))
    .draw();
```

Another window using the same app state ID intentionally shares the state. This is opt-in and should be documented as requiring coordination if windows build concurrently.

### 9.5 State access outside element callbacks

Avoid returning an unbounded raw pointer. Provide callback access:

```cpp
app.elements().withState(
    kToggle,
    windowId,
    FLOW_ID("settings/show-grid"),
    [](ToggleState& state) {
        state.animation = 0.0f;
    });
```

Read-only form:

```cpp
const bool found = app.elements().withStateConst(
    kToggle,
    windowId,
    FLOW_ID("settings/show-grid"),
    [](const ToggleState& state) {
        inspectAnimation(state.animation);
    });
```

These functions acquire the correct partition lock, validate type metadata, invoke the callable, and prevent the reference from being officially retained beyond the operation.

For advanced tooling, expose a generational `ElementStateHandle<E>` and an explicitly scoped `StateLease<E>`. Do not make raw `tryGetState()` pointers the primary API once GC exists.

### 9.6 Resource definition and use

```cpp
struct ToolbarResources {
    FlowUi::TextureRef saveIcon{};
    FlowUi::TextureRef openIcon{};

    explicit ToolbarResources(const FlowUi::ElementResourceInitContext& init) {
        saveIcon = init.icons().textureRef("flowui.basic/save");
        openIcon = init.icons().textureRef("flowui.basic/open");
    }
};

struct ToolbarElement {
    using Parameters = ToolbarParams;
    using Resources = ToolbarResources;

    static constexpr auto definitionId = FLOW_DEF_ID("flowui.basic/toolbar");

    static void build(FlowUi::ElementContext<ToolbarElement>& context) {
        const ToolbarResources& resources = context.resources();
        drawToolbar(context, resources.saveIcon, resources.openIcon);
    }
};
```

`ElementResourceInitContext` should expose app-shared services only:

```cpp
class ElementResourceInitContext {
public:
    FontManager& fonts() const;
    ImageManager& images() const;
    ThemeManager& themes() const;
#if FLOWUI_INCLUDE_ICON_MANAGER
    IconManager& icons() const;
#endif
};
```

Do not provide a window `UiManager&` here. A resource object that captures a window violates app-wide resource semantics.

### 9.7 Explicit resource preparation

```cpp
app.elements().prepare(kToolbar);
```

Preparation is useful during app/pack initialization for resources that register icons, images, or fonts. It must be idempotent and thread-safe.

Basic CPU-only resources may still construct lazily at first use. If a resource constructor performs app-shared manager publication that is illegal during an active frame, its element/pack must require preparation and produce a clear diagnostic rather than failing deep in storage code.

### 9.8 Explicit mutable resource update

Normal `context.resources()` is const. If mutation is genuinely required:

```cpp
app.elements().updateResources(kToolbar, [](ToolbarResources& resources) {
    resources.saveIcon = reloadSaveIcon();
});
```

Run this only during `pollEvents()`/app quiescence or queue it for the next shared-manager boundary. Do not permit unsynchronized mutation from a window build callback.

## 10. Resource construction and lifecycle

### 10.1 Resource state machine

Each definition resource slot should have:

```cpp
enum class ElementResourceState : uint8_t {
    Empty,
    Constructing,
    Ready,
    Failed,
    Destroying,
};
```

Construction rules:

- exactly one thread changes `Empty -> Constructing`;
- another window encountering `Constructing` waits on a condition variable or receives a documented preparation error;
- successful construction publishes the pointer with release semantics and moves to `Ready`;
- failure destroys partial data, stores an exception/diagnostic, and moves to `Failed` or back to `Empty` according to retry policy;
- recursive request for the same definition resources by their own constructor is detected and reported rather than deadlocking.

### 10.2 Resource allocation

Allocate one aligned payload block through storage:

```cpp
storage.allocatePersistent(
    sizeof(Resources),
    alignof(Resources),
    storage::AllocationTag{
        .memoryClass = storage::MemoryClass::ResourceMetadata,
        .resourceKind = storage::ResourceKind::UiElementResources,
        .window = 0,
        .debugName = definitionNameId,
    });
```

Publish its pointer only after construction succeeds. On app shutdown, destroy the object, then call `releasePersistent()`.

### 10.3 Resource sharing across definitions

The first implementation should preserve current semantics: one resource object per definition. Sharing a resource object between multiple definitions can later be supported through an explicit `ResourceTag`/pack resource key. Do not silently key only by `Resources` C++ type; two definitions may use the same struct type but expect independent instances.

### 10.4 App shutdown order

Current cleanup destroys image/icon/font managers before all window/UI cleanup. Element resources may contain handles acquired from those managers. The element migration should define and test this order:

1. stop/cancel active frames and join element build workers;
2. destroy all window state partitions;
3. destroy app-lifetime element state;
4. destroy element resource objects while app-shared managers are still alive;
5. destroy image/icon/font/theme managers and their records;
6. destroy the element controller root record;
7. shut down `StorageSystem`.

If element resource destructors are required to be passive value destruction only, state that contract. The safer ordering still destroys them before their dependencies.

## 11. Concurrency design

### 11.1 Lock hierarchy

Use a documented order to prevent deadlocks:

```text
definition registry lock
    -> definition partition registry lock
        -> one window/app state partition lock
            -> resource lock (normally never needed together)
```

Ordinary cached state access should acquire only the partition lock. Do not acquire the storage-system global mutex while holding a user callback lock if user code can call managers that re-enter storage.

Allocate a new page outside the partition lock where practical, then reacquire and publish it, or define a strict controller-to-storage lock order. Never invoke arbitrary user callbacks while holding the controller registry lock.

### 11.2 State references during build

The owning window build is the exclusive writer for its partition. After resolving the pointer, the context may release the partition mutex and use `State&` because:

- GC and explicit erase are deferred until the frame safe point;
- the window cannot be destroyed while its build is active;
- pages do not move;
- no second build for the same window runs concurrently.

This avoids holding a lock across arbitrary user element code.

### 11.3 Different windows

Two window workers using the same definition resolve different `WindowStatePartition` objects. They may mutate state concurrently without contention. They share only immutable resources and definition metadata.

### 11.4 App-lifetime state

App-lifetime state has a shared partition. The default external/build API should either:

- serialize mutation through that partition; or
- require the element to declare a synchronization strategy.

For the first implementation, retain the current one-active-window-frame rule for app-state mutation and mark this area explicitly before enabling concurrent window build. Do not claim arbitrary shared user state is thread-safe.

### 11.5 Resource reads and updates

After `Ready`, ordinary resource pointers are immutable and lock-free to read. `updateResources()` occurs at a quiescent point or creates a replacement object and atomically publishes it at a frame boundary. In-place unsynchronized mutation is not allowed.

## 12. Developer-mode integration

Developer mode currently understands definition IDs/types and reflects parameter/state/resource structs, but several dev elements also directly inspect static pools/optionals.

The new integration should:

- include the state handle and optional resource handle in captured element invocation metadata;
- resolve reflected state through the controller while the frame/context guarantees lifetime;
- queue developer edits through `withState`/`updateResources` instead of retaining raw pointers;
- show owning window, instance ID, retention policy, last-seen frame, generation, state bytes, and resource readiness;
- allow filtering stale/retained/app-lifetime states;
- show GC created/destroyed counts and page capacity;
- remove fallback behavior that returns `statePool.front()` when a known singleton ID is absent;
- migrate `initializeDevFlowElementResourcesFromApp()` to `app.elements().prepare(...)` or a dev element-pack preparation function.

Dev parameter overrides remain per invocation and do not need persistent element-state storage.

## 13. Storage-system changes

### Required

- Add `ResourceKind::UiElementState`.
- Add `ResourceKind::UiElementResources`.
- Optionally add `ResourceKind::UiElementRegistry`; otherwise use `ManagerRoot` for the controller root.
- Update `resourceStats()` and `validateHandle()` switches.
- Bump `IStorageSystem::CurrentInterfaceVersion` if enum/interface compatibility requires it.
- Add storage/config estimates for element state definitions, state instances, and page sizing, either to `StorageConfig` or an `ElementManagerConfig` inside `AppConfig`.
- Ensure `AllocationTag.window` is set for state pages and zero for resources.

### Not required for the first version

- A generic heterogeneous object API directly on `IStorageSystem`.
- One manager record per state.
- GPU retirement for CPU-only state/resource objects.
- Serialization of arbitrary state structs.

### Useful follow-up

- A storage-backed `std::pmr::memory_resource` adapter so controller hash tables/vectors are budgeted and tagged rather than using the global heap.
- Batch persistent allocation/release telemetry.
- Memory-pressure callback allowing `ElementManager` to request an aggressive transient-state sweep.

## 14. App and window lifecycle wiring

### App initialization

1. Initialize `StorageSystem`.
2. Initialize app-shared managers required by element resource services.
3. Create `ElementManager`/controller root.
4. Register the main window.
5. Initialize its `UiManager` and attach `ElementManager`.
6. Allow pack/resource preparation.

If icons/fonts must be initialized after `ElementManager`, the resource service context may be connected in a second step before any `prepare()` call. Document the ready phase.

### Secondary-window creation

1. Register window storage.
2. Initialize `UiManager`.
3. attach the app-wide `ElementManager` and the window ID.
4. Create state partitions lazily; no element resources are duplicated.

On partial failure, `destroyWindow(window)` on the element manager must be safe even if no partition was created.

### Frame begin

After acquiring the storage `FrameToken`, call:

```cpp
elementManager.beginWindowFrame(window.id, nextFrameNumber, window.storageFrame.epoch);
```

Then enter `UiManager::beginFrame()` and user UI build.

### Frame success

After UI build/preparation succeeds and the storage frame is successfully sealed, call the no-throw element frame commit. The exact placement must ensure every exception path before commit reaches cancellation.

### Frame cancellation

Extend `cancelStorageFrame()` to cancel the element transaction before discarding `window.storageFrame`.

### Window destruction

After canceling/draining active work and before unregistering storage:

```cpp
elementManager.destroyWindow(id);
```

Clear `UiManager` partition caches before controller pages are released.

### App destruction

Destroy all window partitions, app state, and resources before their service dependencies and storage. `ElementManager::destroy()` should be idempotent/noexcept for cleanup paths.

## 15. External state access contract

GC makes the current raw-pointer API unsafe unless its lifetime is tightly bounded.

Recommended public levels:

### Level 1: context reference

```cpp
State& context.state();
```

Valid for the active element/frame. This is the normal API.

### Level 2: callback access

```cpp
bool ElementManager::withState(element, window, id, callable);
bool ElementManager::withAppState(element, appStateId, callable);
```

The callable cannot safely retain the reference. This is the normal external API.

### Level 3: generational handle/lease

```cpp
ElementStateHandle<E> findStateHandle(...);
StateLease<StateOf<E>> acquire(handle);
```

For dev tools and advanced systems. A lease prevents collection until released or holds the correct partition read/write guard.

Do not retain compatibility functions returning `State*` indefinitely. A deprecated transitional pointer getter may be allowed only at documented quiescent points, but new documentation should not teach it.

## 16. Detailed migration plan

### Phase A: semantics and tests before changing storage

Add controller-level test types with construction/destruction counters and write tests for:

- same definition/ID in two windows yields different states;
- state reference remains stable after thousands of same-definition insertions;
- non-copyable/non-movable state works;
- over-aligned state works;
- constructor failure rolls back without a published entry;
- explicit erase destroys exactly once;
- default transient GC respects grace frames;
- one canceled frame does not collect existing state;
- state created in a canceled frame is rolled back;
- `WindowLifetime` survives absence but is destroyed on window close;
- app-lifetime state survives window close and is manually erasable;
- window destruction invalidates handles;
- duplicate definition ID with incompatible type metadata is rejected.

These tests define behavior before migrating real controls.

### Phase B: storage primitives

1. Add new resource kinds and diagnostics switches.
2. Add storage-backed page allocation helpers and aligned slot calculations.
3. Implement generational state slots/free list.
4. Implement a hash index from instance ID to handle.
5. Implement definition and window partition registries.
6. Implement rollback-safe construction and noexcept destruction.
7. Add storage statistics for state/resource bytes, pages, capacity, and live objects.

No public element API changes are needed yet.

### Phase C: `ElementManager` and lifecycle wiring

1. Add the public manager and `App::elements()`.
2. Create the internal controller root in app storage.
3. Attach it to every `UiManager`.
4. Add begin/commit/cancel frame hooks.
5. Add window destruction and app shutdown hooks.
6. Add partial-window-creation cleanup tests.

At this point, a controller unit test should prove no stale state remains after repeated secondary-window creation/destruction.

### Phase D: context-aware state backend

1. Change build/interaction contexts to know the full definition/element type.
2. Add `context.state()`.
3. Resolve state once in the builder invocation.
4. Keep existing function-pointer callbacks but route their state through the new controller.
5. Add manager-based external `withState` and erase APIs.
6. Mark static `statePool` and old getters deprecated.

Compatibility adapters may be:

```cpp
Definition::getOrCreateState(uiManager, flowId);
Definition::tryGetState(uiManager, flowId);
```

Do not keep the single-argument global form as the long-term API because it cannot identify a window.

### Phase E: garbage collector

1. Implement transaction touched/created lists.
2. Add transient/window/app policies.
3. Add incremental sweep budget.
4. Queue erasure/policy mutation during active builds.
5. Add dev diagnostics and forced collection.
6. Stress test large dynamic lists and frame cancellation.

Ship GC only after lifecycle tests prove that no collection occurs while contexts are live.

### Phase F: resources backend

1. Add definition resource slot/state machine.
2. Add app-shared service-only `ElementResourceInitContext`.
3. Add `context.resources()` returning const.
4. Add `app.elements().prepare(element)`.
5. Add queued/quiescent `updateResources()` if required.
6. Migrate dev resource initialization.
7. Correct app shutdown ordering.
8. Deprecate static resource optionals and `getResources(App&)`.

### Phase G: migrate repository elements and docs

Current implementation migration includes:

- 14 dev implementation files touching state pools/getters;
- 22 direct `getOrCreateState` call sites;
- 17 direct `tryGetState`/`tryGetStateConst` call sites;
- five dev files directly touching resource storage;
- `src/devMode/debugView.cpp`;
- quick-start, custom-element, input-field, concept, and API documentation;
- generated Doxygen output after source docs are correct;
- `template.hpp` and developer registration examples.

Replace singleton fallbacks such as “if known ID not found, return `statePool.front()`” with an explicit window plus element ID query. Silent first-state fallback is not valid in a multi-window system.

### Phase H: concept/static-hook migration

Once storage migration is stable:

1. define `FlowElement` concepts and normalized traits;
2. add static `if constexpr` hook dispatch;
3. let current `ElementDefinition` adapt to the concept;
4. migrate first-party elements;
5. remove function-pointer aggregate requirements later;
6. keep `ElementStorageController` unchanged because it already consumes normalized type metadata.

This sequence prevents state/resource work from depending on the final callback syntax.

## 17. Testing matrix

### Unit tests

- definition/type registration and collision detection;
- state slot allocation, reuse, generation, and stable address;
- resource construction state machine;
- policy transition validation;
- incremental GC cursor and grace arithmetic, including frame counter boundaries;
- canceled transaction rollback;
- constructor exception rollback;
- destruction counts;
- alignment and non-movable types;
- hash-table growth without payload movement.

### Window integration tests

- same local ID in main and secondary window remains independent;
- close secondary window releases all of its transient/window state bytes;
- repeatedly create/build/destroy 1,000 logical windows in controller tests without growth;
- app-lifetime state survives secondary-window destruction;
- partial window initialization failure cleans partitions;
- active-frame window destruction follows cancel semantics;
- main app cleanup destroys resources exactly once.

### Resource tests

- resource is constructed once when used by two windows;
- concurrent preparation calls construct once;
- resource constructor failure is reported and retry behavior is deterministic;
- recursive resource initialization is detected;
- resource pointer remains stable;
- resources are destroyed before mocked service dependencies;
- resource-free elements allocate nothing.

### Developer-mode tests

- captured instance maps to correct window state handle;
- state edit does not retain stale pointer after GC;
- dev view can list state from multiple windows;
- internal dev resource pack prepares through `ElementManager`;
- production build compiles out names/diagnostic-only fields.

### Performance tests

- 10,000 stateful elements with steady IDs;
- 10,000 changing list IDs with transient collection;
- depth-128 same-definition recursion while parent references stay valid;
- two controller threads operating on different window partitions;
- state lookup with 1, 100, 10,000, and 100,000 instances;
- resource lookup after warm-up;
- incremental GC worst-frame and average cost;
- zero unexpected general-heap allocations after pools warm up.

## 18. Diagnostics and performance counters

Expose at least:

```cpp
struct ElementStorageStats {
    uint32_t definitionCount;
    uint32_t windowPartitionCount;
    uint64_t liveStateCount;
    uint64_t transientStateCount;
    uint64_t retainedStateCount;
    uint64_t appStateCount;
    uint64_t stateCapacity;
    uint64_t stateBytes;
    uint64_t resourceBytes;
    uint64_t statesCreatedThisFrame;
    uint64_t statesCollectedThisFrame;
    uint64_t stateLookupCount;
    uint64_t stateLookupProbeCount;
    uint64_t gcScannedThisFrame;
    double gcCpuMs;
};
```

Integrate per-window frame values into `PerformanceDiagnostics`. Storage's `resourceStats(UiElementState)` and `resourceStats(UiElementResources)` should agree with controller totals.

Debug diagnostics should identify:

- definition name/type;
- owning window or app scope;
- element ID/debug path if available;
- policy and grace frames;
- last-seen committed frame;
- state size/alignment/generation;
- why a state is retained;
- resource construction state and failure message.

## 19. Failure behavior and edge cases

### Definition ID collision

Reject incompatible metadata immediately. Do not merge buckets based only on a 64-bit authored ID.

### Instance hash collision

The later `ElementKey` redesign should retain authored debug paths in developer mode and detect collisions. For this migration, preserve current `FlowElementId` behavior but include window and definition in the state key.

### Frame-number overflow

`App` already rejects window frame-number exhaustion. GC should use the committed `uint64_t` frame number and avoid subtraction unless `current >= lastSeen`.

### Generation overflow

Do not recycle a slot if its generation would wrap to the invalid or a previously valid generation. Retire that slot/page or use a wider generation internally.

### Constructor reentrancy

State construction should not invoke UI building. Resource construction requesting the same resource must report recursive initialization. Different resource dependencies need a documented order or cycle detection.

### Explicit erase during callback

Queue it. The current callback may hold `State&`.

### Window close with app state

App state survives because it is stored in an app partition and has no window owner. A window-local state cannot become app state implicitly at close.

### Hidden elements

Transient state disappears after grace. Elements requiring longer hidden-state preservation must set `WindowLifetime` in their definition or invocation.

### Storage pressure

Allow an aggressive collection request that sweeps all eligible transient state. Never collect retained/window/app state merely to meet a budget without an explicit eviction contract.

## 20. Acceptance criteria

The migration is complete when all of the following are true:

- `ElementDefinition` contains no mutable static state pool or resource optional.
- same definition and element ID in two windows does not share ordinary state;
- closing a window releases all state owned by that window;
- per-instance retention supports transient, window-lifetime, and explicit app-lifetime semantics;
- recursive same-definition builds cannot invalidate a parent `State&`;
- state lookup is average constant time and performed once per invocation;
- state/resource payload memory is allocated and reported through `StorageSystem`;
- resources are app-wide, constructed once, and const during builds;
- cleanup destroys resources before their manager dependencies;
- external state access no longer teaches long-lived raw pointers;
- canceled frames do not collect valid prior state or leak newly created state;
- developer tools resolve state/resources through handles/controller APIs;
- current function-pointer elements work through an adapter;
- the same controller API works unchanged with future concept/static-hook elements;
- state/resource tests and performance benchmarks pass in developer and production modes.

## Final recommendation

Implement a centralized `ElementManager`/`ElementStorageController` before replacing callback dispatch. Make the controller type-driven rather than `ElementDefinition`-template-driven, so both the current descriptor and future compile-time element concept produce the same immutable type registration.

The most important semantic split is:

```text
definition resources: app-wide, one per definition, const during builds
ordinary instance state: definition + window + instance ID, garbage-collected
explicit persistent state: definition + app state ID, retained until erase/app shutdown
```

Use stable typed pages, per-window partitions, frame-transaction GC, and per-instance policy overrides. This directly fixes window-close leaks, cross-window aliasing, vector invalidation, and linear lookup while establishing the storage foundation needed by the deeper compile-time ElementSystem redesign.
