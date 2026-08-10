# Element State and Resources Migration Follow-up

**Purpose:** record the design decisions made after reviewing `statenResourcesMigrationReport.md`, explain how those decisions change the proposed architecture, resolve the element-registration question, and replace the previous report's section 16 migration plan.

This document is a delta to the original report. Recommendations that are not contradicted here remain valid, particularly the app-owned `ElementManager`, internal storage controller, stable aligned payload storage, O(1) lookup, frame-safe lifetime contracts, incremental garbage collection, and compatibility with a future concept/static-dispatch element API.

## Executive decisions from the follow-up

The updated design is:

1. A Flow element's instance identity is exactly `(WindowId, FlowElementId)`.
2. `FlowElementId` and the root `Clay_ElementId` represent the same logical human-readable string, although their numeric hash representations need not be equal.
3. A definition/type ID is not part of state lookup identity. It is stored in the state header and used to validate that a typed state access matches the element that owns the Flow ID.
4. Two different Flow elements in one `UiManager` must never claim the same Flow ID. FlowUi should detect duplicate Flow-element root IDs before invoking Clay.
5. The same Flow ID in two windows always produces two independent state objects.
6. The element subsystem will not provide cross-window shared state or app-lifetime element state. Cross-window synchronization belongs to user/application logic and may use `ElementManager` state-access conveniences.
7. Closing a window destroys every element state owned by that window, regardless of its within-window retention policy.
8. The state/resource C++ types remain attached to the element definition. Only mutable payload ownership moves into `ElementManager`/`StorageSystem`.
9. Drawing or querying through a typed element API automatically ensures its immutable definition metadata is registered. No mandatory per-element `registerElement()` call is needed.
10. Resources remain one app-wide object per definition and are constructed with `FlowUi::App&`.
11. Lazy resource construction preserves include-and-draw ease of use. Eager “construct all resources” requires an explicit compile-time element set/catalog because C++ cannot discover every included inline definition without global registration side effects.

## 1. Flow identity is a Clay-root identity

### 1.1 Logical identity contract

FlowUi is an immediate-mode runtime helper over Clay. A Flow element is not a second retained widget identity layered above Clay. Its ID names the root Clay element emitted by that Flow element.

For an authored path:

```text
"header/button/main"
```

FlowUi derives a `FlowElementId`, and Clay derives a `Clay_ElementId`. Those hashes may use different algorithms and widths, but both originate from the same logical string:

```text
Flow element logical ID string == root Clay element logical ID string
FlowElementId numeric value     != necessarily Clay_ElementId numeric value
```

This leads to an important invariant:

> Within one `UiManager`/window identity scope, every Flow element instance must have a unique logical root ID.

Giving a button and slider the same logical Flow ID is invalid even if their definition types differ. Their root Clay nodes would still claim the same Clay identity.

### 1.2 Revised state key

The primary state key becomes:

```cpp
struct ElementStateKey {
    WindowId window = InvalidWindowId;
    FlowElementId flowId = 0;

    auto operator<=>(const ElementStateKey&) const = default;
};
```

No definition ID, definition type hash, state type hash, or resource type is part of lookup equality.

This is simpler and matches the UI system's actual semantics:

```text
one window + one logical Flow root ID = at most one Flow element instance = at most one state payload
```

### 1.3 Why type metadata is still required

Although definition/type metadata is removed from the lookup key, the manager still needs it in the state record header:

```cpp
struct ElementStateRecordHeader {
    FlowElementId flowId = 0;

    FlowDefinitionId definitionId = 0;
    uint64_t definitionTypeHash = 0;
    uint64_t stateTypeHash = 0;
    storage::StringId definitionNameId = 0;
    storage::StringId stateTypeNameId = 0;

    uint64_t lastSeenCommittedFrame = 0;
    uint32_t generation = 1;
    uint32_t graceFrames = 0;
    ElementStateRetention retention = ElementStateRetention::Transient;

    size_t payloadSize = 0;
    size_t payloadAlignment = 0;
    void (*destroy)(void*) noexcept = nullptr;

    [[nodiscard]] void* payload() noexcept;
};
```

Metadata serves four purposes without affecting identity:

- validate a typed cast before returning `State&`;
- call the correct destructor during GC/window teardown;
- diagnose two definition types trying to reuse one Flow ID;
- expose state type/definition information in developer tools.

Typed lookup behavior is:

```text
lookup (window, FlowID)
    -> missing: construct the requesting element's State
    -> present and metadata matches: return payload
    -> present and metadata differs: report an ID/type contract violation
```

### 1.4 Type changes at the same Flow ID

Using a button at one Flow ID in one frame and a slider at the same Flow ID in a later frame should not silently reinterpret or replace state. Under the stated identity contract, these are different logical elements and should use different IDs.

Recommended behavior:

- all builds validate state metadata before casting;
- developer mode reports the old and new definition names, state types, window, Flow ID, and authored string when available;
- production builds fail with a focused exception/assertion instead of silently destroying and replacing user state.

If a future use case genuinely needs type replacement at one logical slot, it should be introduced as an explicit state-reset/rebind operation, not accidental behavior of ordinary lookup.

## 2. Enforcing root-ID uniqueness

### 2.1 Per-frame Flow root claims

Add a frame-local identity tracker to `UiManager` or its element frame state:

```cpp
struct FlowRootClaim {
    FlowElementId flowId = 0;
    FlowDefinitionId definitionId = 0;
    uint64_t definitionTypeHash = 0;
#if FLOW_UI_DEV_MODE
    std::string_view logicalId;
    std::source_location source;
#endif
};
```

Before the builder processes interaction, logic, state, resources, or Clay emission:

```cpp
elementManager.claimFlowRoot(windowId, frameEpoch, flowId, descriptor);
```

The tracker is cleared on each `beginFrame`. A second claim of the same Flow ID in the same window frame is an error, regardless of whether the definitions match.

This check should cover stateless elements too. State-map collision detection alone cannot detect two stateless Flow elements with the same root ID.

### 2.2 What FlowUi can and cannot enforce

FlowUi can reliably enforce uniqueness among Flow elements invoked through `ElementBuilder`/future typed invocation APIs.

It cannot automatically see every raw Clay ID emitted directly through Clay macros unless those IDs are also routed through a tracked `UiManager` API. Therefore:

- Flow elements must reserve their root logical ID;
- element authors must not emit another raw Clay node with the same ID;
- Clay remains the final authority for collisions involving untracked raw Clay code.

Developer-mode authored strings make diagnostics much better, but production identity remains the compact Flow ID.

## 3. State is always window-owned

### 3.1 Remove app-lifetime/shared element state

The following ideas from the original report are withdrawn:

- `AppElementStateKey`;
- `ElementStateRetention::AppLifetime`;
- `withAppState()` and app-state partitions;
- automatic or opt-in cross-window sharing of one element state payload.

Every managed element state is associated with exactly one window and Flow ID. The same logical Flow ID in two windows produces two separate records:

```text
(window A, "header/button/main") -> state A
(window B, "header/button/main") -> state B
```

No state-object lock or synchronization protocol is needed merely because two windows use the same element definition or logical ID.

### 3.2 Revised retention policy

Retention now describes only how long state remains within its owning window:

```cpp
enum class ElementStateRetention : uint8_t {
    Transient,
    WindowLifetime,
};

struct ElementStatePolicy {
    ElementStateRetention retention = ElementStateRetention::Transient;
    uint32_t graceFrames = UseElementDefaultGraceFrames;

    static constexpr ElementStatePolicy transient(
        uint32_t graceFrames = UseElementDefaultGraceFrames);
    static constexpr ElementStatePolicy windowLifetime();
};
```

- **Transient:** collected after it has not been touched for its configured number of successfully committed frames.
- **WindowLifetime:** survives temporary removal from the UI tree and is destroyed on explicit erase or window destruction.

Window destruction is unconditional for both policies. There is no retained orphan partition after a window is gone.

### 3.3 Cross-window synchronization belongs to the application

If two independent element instances need synchronized behavior, the application can:

- keep canonical data in its own model and pass it to both elements;
- read one state and modify the other through `ElementManager`;
- subscribe both elements to the same app-level command/event source;
- copy selected state fields explicitly.

The manager should make state access easy, but should not define automatic synchronization semantics.

Possible convenience API:

```cpp
std::optional<float> sourceValue;

app.elements().withStateConst(
    kSlider,
    sourceWindow,
    FLOW_ID("toolbar/zoom"),
    [&](const SliderState& source) {
        sourceValue = source.value;
    });

if (sourceValue) {
    app.elements().withState(
        kSlider,
        targetWindow,
        FLOW_ID("toolbar/zoom"),
        [&](SliderState& target) {
            target.value = *sourceValue;
        });
}
```

The two calls are intentionally not nested. A manager implementation may pin or lock a window partition for the duration of a callback; finishing the read before starting the write avoids cross-window lock-order problems. A `copyState` convenience must establish a deterministic window-lock order internally.

For copyable state, a constrained helper may be added later:

```cpp
app.elements().copyState(
    kSlider,
    {.window = sourceWindow, .flowId = FLOW_ID("toolbar/zoom")},
    {.window = targetWindow, .flowId = FLOW_ID("toolbar/zoom")});
```

This is an explicit copy, not a shared state relationship.

### 3.4 Surviving window recreation

If data must survive destroying and recreating a window, it is application data rather than window element state. The application should extract/store the value before destruction or use an app model as the canonical source. The recreated element initializes its new state from params/model data.

This keeps element state lifecycle predictable and avoids stale state keyed by dead `WindowId` values.

## 4. Revised state storage topology

### 4.1 One registry per window

Because Flow IDs are globally unique within a `UiManager`, the direct lookup structure should be one window registry:

```cpp
struct WindowElementStateRegistry {
    WindowId window = InvalidWindowId;
    FlatHashMap<FlowElementId, ElementStateHandle> byFlowId{};
    StableStateStorage storage{};
    FrameStateTransaction transaction{};
    IncrementalGcState gc{};
};
```

The lookup path is:

```text
UiManager -> cached WindowElementStateRegistry* -> O(1) lookup by FlowID -> header/payload
```

There is no first lookup by definition and no per-definition map required for identity.

### 4.2 Stable payload allocation

Retain the previous report's stable persistent-pool requirement. A state object must not move when:

- the Flow-ID hash table rehashes;
- another state is inserted;
- same-definition recursion creates more instances;
- unrelated state is erased;
- GC advances.

The concrete payload shape remains:

```text
[ElementStateRecordHeader][alignment padding][user State payload]
```

There are two acceptable implementation strategies:

#### Individually allocated records

Allocate header/payload together through `IStorageSystem::allocatePersistent()`.

Advantages:

- directly mirrors the proven theme-record layout;
- simple alignment and destruction;
- heterogeneous state types fit naturally in one window registry;
- easy first implementation.

Costs:

- one persistent allocation per live state;
- more allocator metadata and possible fragmentation.

#### Stable pages with heterogeneous slots

Allocate pages and place variable-size records without ever compacting live payloads.

Advantages:

- fewer underlying persistent allocations;
- better locality and amortized allocation cost.

Costs:

- variable-size free-space management is more complicated than the previous type-homogeneous-page proposal;
- reclaiming fragmented pages is harder.

Recommended sequencing: begin with individually allocated aligned records plus O(1) window maps. Benchmark representative UIs. Add size-classed pages only if persistent allocation overhead is significant. Correct identity/lifetime matters more than prematurely building a heterogeneous slab allocator.

Type-homogeneous backing pools may still be used as an internal allocation optimization, but they must sit behind the one `(window, FlowID)` registry. Definition/type must not become part of public or primary lookup identity again.

### 4.3 State handle

The hash map stores a generational handle, not the payload inline:

```cpp
struct ElementStateHandle {
    uint32_t slot = 0;
    uint32_t generation = 0;
};
```

The controller resolves the handle to the stable record header. Erasure increments generation before reuse. Developer tools and deferred manager operations use handles; element callbacks use a frame-bounded cached pointer.

### 4.4 Builder caching

For each invocation of a stateful element, the builder performs exactly one typed resolution:

```cpp
auto resolved = elementManager.resolveOrCreateState<Element>(
    uiManager.windowId(),
    flowId,
    statePolicy,
    frameToken);
```

It caches:

```cpp
struct ResolvedElementState {
    ElementStateHandle handle{};
    void* payload = nullptr;
};
```

Every enabled callback receives a context that points to the same resolved state. `context.state()` is a typed dereference, not another hash lookup.

The state is pinned by the active frame contract: GC, explicit erase, and window teardown cannot destroy it until callbacks and frame authoring have reached the safe boundary.

## 5. What remains attached to the element definition

Moving payload ownership does not mean losing the element's compile-time state/resource surface.

### 5.1 Type aliases remain local to the header

Future definition:

```cpp
struct ButtonElement {
    using Parameters = ButtonParams;
    using State = ButtonState;
    using Resources = ButtonResources;

    static constexpr FlowDefinitionId definitionId =
        FLOW_DEF_ID("flowui.basic/button");

    static void build(FlowUi::ElementContext<ButtonElement>& context);
};

inline constexpr ButtonElement kButton{};
```

Including the header still gives users immediate access to:

```cpp
ButtonElement::Parameters
ButtonElement::State
ButtonElement::Resources
```

With the transitional `ElementDefinition`, the existing `ParametersType`, `StateType`, and `ResourcesType` aliases remain available.

The manager owns instances of those types; it does not own or hide their C++ definitions.

### 5.2 Immutable descriptor remains attached

Each element type can expose or generate an immutable descriptor:

```cpp
struct ElementRegistrationDescriptor {
    FlowDefinitionId definitionId = 0;
    uint64_t definitionTypeHash = 0;
    uint64_t stateTypeHash = 0;
    uint64_t resourcesTypeHash = 0;
    size_t stateSize = 0;
    size_t stateAlignment = 0;
    size_t resourcesSize = 0;
    size_t resourcesAlignment = 0;
    ElementTypeOps stateOps{};
    ElementTypeOps resourceOps{};
};

template <FlowElement E>
inline constexpr auto elementDescriptor = makeElementDescriptor<E>();
```

This descriptor may be static because it is immutable metadata. It contains no state vector, resource optional, controller pointer, mutex, or mutable registration flag.

### 5.3 Registration and payload ownership are separate

“Registration” means the controller has validated/copied the immutable descriptor and created any required internal definition/resource entry.

“Payload ownership” means state/resource memory is allocated through `StorageSystem` and owned by `ElementManager`.

The definition remains the source of type information while the manager remains the source of mutable lifetime.

## 6. Automatic registration without global static mutation

### 6.1 No mandatory user registration call

The typed builder already knows the complete element definition type. It should call an internal idempotent function before state/resource use:

```cpp
template <FlowElement E>
ElementDefinitionRecord& ElementManager::ensureRegistered(E) {
    return controller_->ensureDefinition(elementDescriptor<E>);
}
```

Normal code remains:

```cpp
ui.createElement(kButton, "header/button/main")
    .setParameters(ButtonParams{...})
    .draw();
```

No separate `registerElement(kButton)` is required. On first typed use, FlowUi:

1. obtains the compile-time descriptor;
2. checks the definition registry in O(1);
3. registers/validates it if absent;
4. caches the resulting definition record where appropriate;
5. resolves state/resources.

Later uses hit the cached/registered record.

### 6.2 Typed manager queries also ensure registration

External code may query before an element has been drawn:

```cpp
app.elements().withState(
    kButton,
    windowId,
    FLOW_ID("header/button/main"),
    [](ButtonState& state) { /* ... */ });
```

The typed query can ensure the definition descriptor is registered, then return `false` because no state payload exists. Registration does not imply state creation for a read-only query.

An explicit “get or create outside drawing” API should be separate because it creates UI state that has not been touched by an element frame:

```cpp
app.elements().withOrCreateState(...);
```

Its retention/last-seen semantics must be explicit. It should not be the default query.

### 6.3 Why not global-constructor auto-registration

C++ has no reflection mechanism that enumerates every included inline element variable. Automatically discovering all definitions before use would require one of:

- global constructors that mutate a process registry;
- linker sections/platform-specific enumeration;
- macros producing hidden registrar objects;
- an explicit catalog.

Global constructors/registrar objects reintroduce static mutable lifetime, initialization-order problems, dead stripping surprises, dynamic-library ambiguity, and tests that cannot cleanly reset the registry. They are not recommended for core state/resource registration.

Lazy typed registration provides the desired include-and-use behavior without those costs.

### 6.4 Developer registry is a separate concern

`FLOWUI_DEV_REGISTER_ELEMENT` currently supports developer-mode discovery/reflection. It may later consume the same immutable descriptor, but dev discovery should not become the owner of production state/resource registration.

Production element use must work when developer mode is disabled.

## 7. App-wide resource construction

### 7.1 Revised constructor contract

Resources are one per element definition for the app and are constructed through the owning app:

```cpp
struct ButtonResources {
    explicit ButtonResources(FlowUi::App& app) {
        icon = app.icons().textureRef("flowui.basic/button/icon");
    }

    FlowUi::TextureRef icon{};
};
```

Constructor selection should become:

```text
Resources(App&) preferred
Resources() fallback
Resources(UiManager&) removed/deprecated
```

`UiManager&` is deliberately removed because resources must not capture or initialize from a particular window.

The `App&` is an initialization dependency. Resource objects should not retain the address unless explicitly necessary; app-shared manager handles are preferable.

### 7.2 Binding `ElementManager` to `App`

The manager needs access to the owning `App` for lazy construction:

```cpp
elementManager.init(app, *storageSystem);
```

Because `App` is currently movable, its move constructor/assignment must rebind the manager's non-owning owner pointer to the destination `App` and clear the moved-from binding. Alternatively, resource preparation can be routed through an `App` member that passes `*this` for each construction. The implementation must not retain a stale pre-move `App*`.

The one-App-per-program rule means no app ID is needed in resource keys or state keys.

### 7.3 Lazy construction

Default behavior remains convenient:

```cpp
const ButtonResources& resources = context.resources();
```

On first request:

1. ensure the element definition is registered;
2. allocate `[resource header][alignment][Resources payload]` through storage;
3. construct `Resources(app)` or `Resources()`;
4. publish the stable pointer;
5. cache it in the app-wide definition/resource record.

Two windows requesting the resource concurrently must construct it once through the resource state machine described in the original report.

### 7.4 Eager preparation for one element

Provide:

```cpp
app.elements().prepare(kButton);
```

This registers the descriptor and constructs its resources immediately. For a stateless/resource-free element it only validates/registers metadata and is otherwise cheap.

### 7.5 Eager preparation for a set or pack

To construct all resources before any frame, FlowUi needs a compile-time list:

```cpp
inline constexpr auto BasicControls = FlowUi::elementSet(
    kButton,
    kToggle,
    kSlider,
    kTextInput);

app.elements().prepare(BasicControls);
```

A shareable header pack can expose its own set:

```cpp
namespace Acme::Controls {
inline constexpr auto Elements = FlowUi::elementSet(
    kDatePicker,
    kSearchBox,
    kPropertyGrid);
}

app.elements().prepare(Acme::Controls::Elements);
```

This is one convenience call per pack, not one mandatory registration call per definition.

### 7.6 `prepareAllRegistered()` has limited semantics

FlowUi may also offer:

```cpp
app.elements().prepareAllRegistered();
```

It can only construct resources for definitions already seen through a typed builder/query/catalog. Immediately after app creation that set may be empty. Therefore it is useful for late preparation or developer tooling, but it cannot replace an explicit element set when the goal is “all definitions included in this program before first draw.”

### 7.7 Frame-safety of lazy constructors

`Resources(App&)` may call app-shared managers to register icons, images, fonts, or themes. Some manager mutations are only legal at quiescent/frame-boundary phases. Therefore:

- pure CPU/default resources may safely remain lazy;
- packs with app-shared manager mutations should call `prepare(elementSet)` during app initialization;
- a lazy construction attempted in an illegal active-frame phase must produce a focused “prepare this element's resources before drawing” diagnostic;
- resource construction should never partially publish its payload on failure.

This is the only unavoidable tradeoff: C++ cannot provide both eager construction of every arbitrary header-defined element and zero registration/catalog participation without hidden global side effects.

## 8. Revised `ElementManager` state API

### 8.1 Address type

```cpp
struct ElementStateAddress {
    WindowId window = InvalidWindowId;
    FlowElementId flowId = 0;
};
```

Typed element arguments supply validation/casting, not address identity.

### 8.2 Safe callback access

```cpp
template <FlowElement E, typename Fn>
bool ElementManager::withState(
    E element,
    WindowId window,
    FlowElementId flowId,
    Fn&& fn);

template <FlowElement E, typename Fn>
bool ElementManager::withStateConst(
    E element,
    WindowId window,
    FlowElementId flowId,
    Fn&& fn) const;
```

The manager:

- validates window and Flow ID;
- looks up exactly `(window, FlowID)`;
- validates the header against `StateOf<E>` and the definition descriptor;
- invokes the callable only when a matching live state exists;
- prevents GC/erase for the operation's duration;
- does not advertise pointer validity after the callable returns.

### 8.3 Explicit mutation helpers

Names can make intent clearer:

```cpp
app.elements().readState(kSlider, window, flowId, reader);
app.elements().modifyState(kSlider, window, flowId, mutator);
app.elements().eraseState(kSlider, window, flowId);
```

`withStateConst`/`withState` may be sufficient; avoid unnecessary synonyms until usage clarifies the preferred surface.

### 8.4 Handles and leases

Developer tools or long operations may use:

```cpp
ElementStateHandle<ButtonElement> handle =
    app.elements().findState(kButton, window, flowId);

auto lease = app.elements().acquire(handle);
```

The handle includes generation and typed descriptor identity for validation, but lookup equality remains `(WindowId, FlowID)`. A lease is scoped and blocks/defer collection. Ordinary app code should prefer callback access.

### 8.5 No automatic cross-window write-through

Do not add flags such as `shared`, `mirrored`, or `syncAcrossWindows` to state policy. Those semantics belong to the user's model/controller. The manager offers access primitives, not a distributed state system.

## 9. Concrete changes to the original report

The following table summarizes the delta.

| Original proposal | Updated decision |
|---|---|
| State key includes definition, window, and instance ID | State key is only `(WindowId, FlowID)` |
| Definition/type participates in identity | Definition/type lives in the header for validation/destruction only |
| Optional app-lifetime element state | Removed |
| Same element may intentionally share one state across windows | Never implicit or managed; states are always separate |
| Definition-specific/window partition maps | One direct Flow-ID registry per window; optional type pools are allocation details |
| Type-homogeneous pages recommended immediately | Begin with stable aligned records; add pages/size classes based on benchmarks |
| `ElementResourceInitContext` with app services | Prefer direct `Resources(App&)`, default constructor fallback |
| Registration question left mostly implicit | Typed builder/query automatically calls `ensureRegistered<E>()` |
| Eager resource preparation for one definition | Retained as `app.elements().prepare(element)` |
| “Prepare all” concept | Requires explicit `elementSet`/pack catalog, or only prepares definitions already registered |

Everything else remains conceptually compatible: central manager/controller ownership, storage-backed memory, state headers, stable payload addresses, generational handles, frame transactions, incremental GC, safe external leases/callbacks, resource state machine, dev diagnostics, and future concept/static-hook dispatch.

---

## 10. Replacement for section 16: detailed migration plan

This section replaces section 16 of `statenResourcesMigrationReport.md`.

### Phase A: freeze identity and lifecycle semantics in tests

Before moving storage, add tests that establish the revised contract:

1. Same Flow ID claimed twice in one `UiManager` frame is rejected, even for different element definitions.
2. Same Flow ID in two different windows is valid and creates two distinct state payloads.
3. State lookup uses only window plus Flow ID.
4. A stored header whose definition/state metadata differs from the typed requester is rejected before casting.
5. Reusing one Flow ID for a different definition in a later frame produces a focused diagnostic.
6. Closing a window destroys all of its transient and window-lifetime states.
7. No app-lifetime state or cross-window shared-state path exists.
8. A state reference remains stable while other states are inserted/erased and during same-definition recursion.
9. Canceled frames do not collect pre-existing states or leave newly created state permanently live.
10. Resource construction occurs once per definition for the app and never once per window.

These tests should initially target the controller directly so semantics do not depend on the old builder implementation.

### Phase B: add storage resource types and aligned record primitives

1. Add `ResourceKind::UiElementState` and `ResourceKind::UiElementResources`.
2. Update resource statistics, validation switches, telemetry, and storage interface version as needed.
3. Implement a reusable aligned record allocation helper:

   ```text
   [header][padding][payload]
   ```

4. Support arbitrary size/alignment, non-copyable/non-movable payloads, rollback-safe construction, and `noexcept` destruction.
5. Implement generational record handles and stale-handle validation.
6. Tag state allocations with their owning window and resource allocations with app scope.
7. Add failure-injection tests for allocation and constructor exceptions.

Start with one stable persistent allocation per state/resource record. Record benchmark data before adding slab/page complexity.

### Phase C: implement `ElementManager` and `ElementStorageController`

1. Add public `ElementManager` and `App::elements()` accessors.
2. Add internal `ElementStorageController` stored through the existing manager-root/storage pattern.
3. Bind the manager to the owning `App` and `StorageSystem`.
4. Rebind the owner correctly in `App` move construction/assignment, or route resource construction through an `App` method that supplies the current `App&`.
5. Attach the same manager/controller to every `UiManager` with its `WindowId`.
6. Add one `WindowElementStateRegistry` per registered window.
7. Add idempotent partial-window cleanup for failed secondary-window construction.
8. Add controller shutdown before storage shutdown.

At the end of this phase, the manager exists but the old static state/resource path may still be active.

### Phase D: attach immutable descriptors and automatic registration

1. Define normalized element traits for current `ElementDefinition` and future concept-based types:

   ```cpp
   ParametersOf<E>
   StateOf<E>
   ResourcesOf<E>
   HasState<E>
   HasResources<E>
   elementDescriptor<E>
   ```

2. Keep parameter/state/resource type aliases on the definition type.
3. Generate immutable state/resource type operations at compile time.
4. Implement `ElementManager::ensureRegistered(E)` as an internal idempotent O(1) operation.
5. Make typed builder invocation call `ensureRegistered` automatically.
6. Make typed external manager queries ensure descriptor registration without creating missing state.
7. Add collision tests for reused definition IDs with incompatible metadata.
8. Keep production registration independent from `FLOWUI_DEV_REGISTER_ELEMENT`.

No user-facing `registerElement()` call is required in ordinary code.

### Phase E: enforce Flow-root uniqueness

1. Add a frame-local Flow-root claim table to each `UiManager`.
2. Clear it at `beginFrame` and discard it on cancellation/end.
3. Claim `(window, FlowID)` before any element callback/state/resource/Clay build work.
4. Reject a duplicate claim regardless of definition type or statefulness.
5. In developer mode, store the authored logical string and source location for both claims.
6. Ensure constructed and normal draw flows use the same claim path.
7. Test duplicates among stateful, stateless, nested, and constructed elements.

This phase makes the state identity rule match the root Clay identity rule.

### Phase F: move state lookup and callback context to the manager

1. Change interaction/build contexts to know the complete element type rather than only parameters.
2. Add typed `context.state()` constrained to stateful elements.
3. In the builder, resolve `(WindowId, FlowID)` exactly once per invocation.
4. On missing state, allocate/construct `[header][payload]` using compile-time type ops.
5. On existing state, validate definition/state metadata before returning the pointer.
6. Cache the handle/pointer in the invocation and reuse it for hover, press, hold, release, logic, construct, and build callbacks.
7. Defer erase/GC/window teardown while an invocation/frame may hold the pointer.
8. Add manager callback access and explicit erase:

   ```cpp
   withState(element, window, flowId, callable)
   withStateConst(element, window, flowId, callable)
   eraseState(element, window, flowId)
   ```

9. Deprecate static `statePool`, `getOrCreateState(flowId)`, and raw `tryGetState(flowId)` APIs.
10. If needed, provide temporary compatibility adapters requiring `UiManager&` or `ElementManager&` plus `WindowId`.

Do not implement app-state partitions or shared-state policy.

### Phase G: integrate frame transactions and garbage collection

1. Add `beginWindowFrame`, no-throw successful commit, and cancellation hooks.
2. Track touched and newly created state handles for the active window frame.
3. Add only `Transient` and `WindowLifetime` retention.
4. Use successfully committed window frame numbers for grace calculations.
5. Roll back newly created canceled-frame state.
6. Queue explicit erasure/policy changes until the safe boundary when called during a build.
7. Implement bounded incremental scanning of transient records.
8. Destroy the entire window state registry synchronously when the window closes.
9. Invalidate all handles for destroyed records.
10. Add aggressive eligible-state collection for diagnostics/memory pressure without evicting window-lifetime state.

Window teardown must not leave orphan state records.

### Phase H: move resources to app-wide manager ownership

1. Add one resource record/state machine per registered element definition.
2. Remove `Resources(UiManager&)` construction.
3. Prefer `Resources(App&)`; fall back to `Resources()`.
4. Add `context.resources()` returning `const Resources&` by default.
5. Construct lazily once per app with synchronization and recursive-construction detection.
6. Add `app.elements().prepare(element)`.
7. Add `FlowUi::elementSet(...)` and `app.elements().prepare(elementSet)` for packs/all-known definitions.
8. Add `prepareAllRegistered()` with documentation that it only covers already registered definitions.
9. Detect illegal active-frame app-manager mutation and instruct users to eagerly prepare that element/pack.
10. Destroy resources before image/icon/font/theme managers and before `StorageSystem` shutdown.
11. Deprecate static resource optionals and `Definition::getResources(App&)`.

The ordinary include-and-draw path stays registration-free; eager resource construction uses a catalog only when requested.

### Phase I: migrate first-party/dev elements and developer tooling

1. Replace all direct static state pool/getter access with `context.state()` or `ElementManager` queries.
2. Remove singleton fallbacks that return `statePool.front()`.
3. Replace direct resource optional access with `context.resources()`.
4. Replace `initializeDevFlowElementResourcesFromApp()` with an internal dev `elementSet` preparation call.
5. Update dev capture nodes to store state handles, owning window, definition/type metadata, retention, and last-seen frame.
6. Queue dev state modifications through manager APIs instead of retaining raw pointers.
7. Update templates, tutorials, public API docs, concept docs, and generated Doxygen output.
8. Add migration notes for external users that previously accessed static pools.

### Phase J: benchmark and choose the final allocator topology

Benchmark the simple stable-record implementation with:

- 10,000 steady stateful elements;
- 10,000 changing list IDs with GC;
- depth-128 same-definition recursion;
- repeated secondary-window creation/destruction;
- future two-window controller concurrency;
- heterogeneous small/medium/over-aligned state sizes.

Measure persistent allocation count, CPU time, fragmentation, cache behavior, state lookup probes, GC cost, and worst-frame latency.

Only if individual records are a demonstrated bottleneck, add size-classed pages or type-homogeneous payload pools behind the unchanged `(WindowId, FlowID)` registry and handle API.

### Phase K: migrate callback dispatch to concepts/static hooks

1. Define the final `FlowElement` concept.
2. Preserve user-authored nested `Parameters`, `State`, and `Resources` surfaces.
3. Replace optional function-pointer hook checks with `if constexpr` static hook detection.
4. Let transitional `ElementDefinition` adapt to the same traits/descriptor/controller APIs.
5. Migrate first-party elements incrementally.
6. Keep `ElementManager`, state identity, GC, resources, and automatic registration unchanged.

The state/resource migration therefore does not depend on the final callback syntax.

## 11. Updated acceptance criteria

The implementation is ready when:

- the only primary state address is `(WindowId, FlowID)`;
- two Flow elements cannot claim the same Flow ID in one `UiManager` frame;
- two windows using the same Flow ID receive independent state;
- no automatic shared/app-lifetime element-state feature exists;
- every window's state is destroyed when that window closes;
- transient and window-lifetime retention work within a live window;
- typed metadata is validated before casting a state payload;
- state payload addresses remain stable for their complete lifetime;
- the builder performs one state lookup per invocation and caches it across callbacks;
- state/resource C++ types and aliases remain attached to the included element definition;
- normal typed draw/query use automatically ensures immutable registration;
- ordinary users do not call `registerElement()`;
- one app-wide resource object exists per definition;
- resources construct through `Resources(App&)` or default construction, never a window `UiManager&`;
- lazy resources preserve include-and-draw usability;
- an `elementSet` can eagerly register and prepare all resources in a shared pack;
- state/resource payload memory and statistics belong to `StorageSystem`;
- external state access is convenient but lifetime-safe;
- the architecture supports future compile-time hook dispatch without changing storage identity or ownership again.

## Final position

The revised model aligns Flow state identity with the system it wraps:

```text
same logical string -> Flow element identity and root Clay identity
state address       -> WindowId + FlowID
definition metadata -> validation, construction, destruction, diagnostics
resources           -> definition-wide and app-owned
```

The element definition does not lose its `State` or `Resources` type surface. It loses only mutable static payload ownership. Automatic typed `ensureRegistered` calls preserve the current include-and-draw workflow, while compile-time `elementSet` catalogs provide deterministic eager resource construction without global static registrars.

This reduces the implementation and API surface compared with the original report: there is no app-state partition, no cross-window state synchronization contract, and no definition component in the state key. The manager remains responsible for efficient storage and safe access; the application remains responsible for deciding when independent window elements should share behavior.
