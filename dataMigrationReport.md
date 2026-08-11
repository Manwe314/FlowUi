# Flow Element Data Migration Report

## Status and scope

The state and resource migration through Phase H is complete. Flow element definitions still declare their `Parameters`, `State`, and `Resources` types, but payload ownership in the new execution path now belongs to the app-wide element subsystem and `StorageSystem`. Deprecated static stores remain temporarily for unmigrated callers and are documented separately at the end of this report.

The resulting lifetime model is:

- element state is unique per `(WindowId, FlowElementId)`;
- the same Flow ID in different windows has independent state;
- resources are constructed once per element definition for the entire app;
- transient state expires after committed absent frames, while window-lifetime state survives until explicit erasure or window destruction;
- closing a window destroys all state owned by that window;
- application shutdown destroys element resources before their image, icon, font, and theme dependencies.

The current public surface is exported through [`FlowUi/Flow.hpp`](include/FlowUi/Flow.hpp), while the primary implementation types are in [`ElementManager.hpp`](include/managers/ElementManager.hpp), [`ElementManagerStructs.hpp`](include/managers/structs/ElementManagerStructs.hpp), and [`FlowUiElementStructs.hpp`](include/managers/structs/FlowUiElementStructs.hpp).

## Ownership and orchestration

```text
FlowUi::App (one per program)
|
+-- StorageSystem
|   +-- manager record: ElementStorageController
|   +-- persistent records: per-window State payloads
|   `-- persistent allocations: app-wide Resources payloads
|
+-- ElementManager
|   `-- public typed facade over ElementStorageController
|
`-- one or more windows
    `-- UiManager
        `-- non-owning connection to the same ElementManager
```

The responsibilities are deliberately separated:

| Component | Responsibility |
| --- | --- |
| [`App`](include/FlowUi/App.hpp) | Owns the single `ElementManager`, the storage system, windows, and resource-service managers. Exposes `app.elements()`. |
| [`ElementManager`](include/managers/ElementManager.hpp) | Public typed API for state reads, modification, erasure, garbage collection, and resource preparation. Stores a private non-owning `App*` so resource construction receives the correct `App&`. |
| [`ElementStorageController`](include/internal/ManagerStorage/ElementStorageController.hpp) | Internal lifecycle engine. Owns definition registration, resource state machines, per-window state indexes, frame transactions, and GC bookkeeping. |
| [`ElementDefinitionRegistry`](include/internal/ManagerStorage/ElementStorageController.hpp) | Maps `FlowDefinitionId` to stable definition records and validates that a reused definition ID has identical metadata. |
| [`WindowElementStateRegistry`](include/internal/ManagerStorage/ElementStorageController.hpp) | Maps one window's `FlowElementId` values to storage handles and tracks frame touches, deferred erasures, and GC candidates. |
| [`StorageSystem`](include/internal/StorageSystem/IStorageSystem.hpp) | Allocates, aligns, categorizes, validates, and eventually releases the controller and payload memory. |
| [`ElementBuilder`](include/managers/FlowUiElementBuilder.hpp) | Registers the definition automatically, resolves state once per invocation, creates callback contexts, and caches lazy resources. |
| [`ElementBuildContext` / `ElementInteractionContext`](include/managers/structs/FlowUiElementStructs.hpp) | Give callbacks typed `state()` and `resources()` access without exposing storage internals. |

### App and window wiring

During app initialization, `ElementManager::init()` creates an app-scoped manager record named `flowui.elements.root`. The record's payload is the `ElementStorageController`; `ElementManager` retains only its storage handle, resolved pointer, storage pointer, and owning `App*`. See [`ElementManager.cpp`](src/managers/ElementManager.cpp).

Each window is then registered with the controller and its `UiManager` receives a non-owning pointer to the same `ElementManager`. Main-window and secondary-window wiring is performed in [`FlowUi.cpp`](src/FlowUi.cpp).

This gives all windows access to the same definition/resource registry while retaining separate window state registries.

## Definition metadata and automatic registration

The current definition syntax remains source-compatible:

```cpp
struct ButtonParams {
    std::string_view label;
};

struct ButtonState {
    bool pressed = false;
};

struct ButtonResources {
    FlowUi::TextureHandle icon;

    explicit ButtonResources(FlowUi::App& app) {
        // Resolve app-wide dependencies through app managers.
    }

    ~ButtonResources() noexcept = default;
};

using ButtonDefinition = FlowUi::ElementDefinition<
    ButtonParams,
    ButtonState,
    ButtonResources,
    FLOW_DEF_ID("controls/button")>;

inline constexpr ButtonDefinition kButton{
    .buildElement = +[](ButtonDefinition::BuildContext& context) {
        ButtonState& state = context.state();
        const ButtonResources& resources = context.resources();
        // Emit the root Clay element and its children.
    },
};
```

[`makeElementDescriptor()`](include/managers/structs/ElementManagerStructs.hpp) converts the definition at compile time into immutable erased metadata:

- definition, parameter, state, and resource type hashes;
- payload sizes and alignments;
- whether state/resources exist;
- default construction, `App&` construction, and destruction operations;
- diagnostic type and definition names.

Normal construction or drawing automatically registers this descriptor. Typed manager operations also ensure registration. Users do not need a separate `registerElement()` call.

Registration is idempotent and keyed by `FlowDefinitionId`. Reusing a definition ID with incompatible type, size, alignment, or state/resource metadata is rejected before any typed payload cast. The erased internal metadata types live under `FlowUi::detail::element` in [`ElementRegistration.hpp`](include/internal/ManagerStorage/ElementRegistration.hpp); they are not public API types.

## How state memory is stored

### Identity and lookup

The state lookup key is exactly:

```text
(WindowId, FlowElementId)
```

Within each `WindowElementStateRegistry`:

```text
byFlowId[FlowElementId] -> PersistentRecordHandle -> StorageSystem record
```

Definition and state type hashes are intentionally not part of the key. They are stored in the record header and validated before returning the payload. This matches the UI identity rule: a Flow element's logical ID is also the logical ID of its root Clay element, so one window cannot safely host two typed elements with the same Flow ID.

### Physical state record

Every state instance is one stable, aligned `ResourceKind::UiElementState` persistent record owned by its window:

```text
[ ElementStateRecordHeader ][ alignment padding ][ State payload ]
```

The header records:

- `FlowElementId`;
- definition and state type hashes;
- payload size and alignment;
- last successfully committed frame in which the state was seen;
- retention policy and GC index;
- state destructor;
- diagnostic definition and state type names.

Record construction is implemented by [`createStateRecord()`](src/managers/ElementStorageController.cpp). `StorageSystem` creates a generational `PersistentRecordHandle`, allocates the header and payload together from persistent storage, invokes the state's default constructor, and rolls the allocation back if construction fails. The storage implementation is in [`FlowStorageSystem.cpp`](src/Storagesystem/FlowStorageSystem.cpp).

The `unordered_map`, GC vector, transaction sets, and mutexes are ordinary C++ bookkeeping owned by the storage-backed controller. The state object itself and its destruction callback are owned by `StorageSystem`; the maps contain only IDs and handles.

### Callback access

The builder resolves state once before callbacks and keeps the pointer in an invocation-local cache. Every event, logic, construct, and build context for that invocation receives the same address:

```cpp
void updateButton(ButtonDefinition::BuildContext& context) {
    ButtonState& state = context.state();
    state.pressed = /* current interaction result */;
}
```

`state()` is constrained away for stateless definitions. Mutable contexts return `State&`; const contexts return `const State&`. The context implementation is in [`FlowUiElementStructs.hpp`](include/managers/structs/FlowUiElementStructs.hpp), and the invocation cache is created in [`FlowUiElementBuilder.hpp`](include/managers/FlowUiElementBuilder.hpp).

The invocation guard prevents GC, explicit erasure, or window teardown from invalidating the cached pointer while callbacks are running.

### External state access

Existing state can be queried without creating it:

```cpp
const FlowUi::FlowElementId id = FLOW_ID("toolbar/save");

const ButtonState* observed =
    app.elements().readState(kButton, windowId, id);

ButtonState* editable =
    app.elements().modifyState(kButton, windowId, id);

bool removed =
    app.elements().eraseState(kButton, windowId, id);
```

These APIs are defined in [`ElementManager.hpp`](include/managers/ElementManager.hpp). `readState()` is const and returns `const State*`; `modifyState()` is non-const and returns `State*`. Both return `nullptr` if the instance does not exist. `eraseState()` returns whether an instance was found.

Pointers remain stable until explicit erasure, GC, or window destruction. A transient pointer must not be retained across frame boundaries. Erasure requested during an active frame/invocation is deferred until cached callback pointers are no longer live.

## Frame transactions and state garbage collection

State lifetime advances only through successfully committed window frames:

```text
beginWindowFrame(epoch)
    -> draw/construct elements
       -> resolve or create state
       -> record touched IDs, policies, and newly created IDs
    -> commitWindowFrame(epoch)
       -> update last-seen frame and policies
       -> drain safe deferred erasures
       -> scan up to 256 GC candidates
```

If UI building fails, `cancelWindowFrame()` removes state created during the canceled frame, discards queued erasures, and leaves pre-existing state ages unchanged. The transaction and GC implementation is in [`ElementStorageController.cpp`](src/managers/ElementStorageController.cpp), with app frame hooks in [`FlowUi.cpp`](src/FlowUi.cpp).

The two supported policies are declared in [`ElementStatePolicy.hpp`](include/managers/structs/ElementStatePolicy.hpp):

```cpp
inline constexpr ButtonDefinition kTransientButton{
    .statePolicy = FlowUi::ElementStatePolicy::transient(5),
};

inline constexpr ButtonDefinition kPersistentForWindow{
    .statePolicy = FlowUi::ElementStatePolicy::windowLifetime(),
};
```

- `Transient` is the default. With the default grace of five, it survives five successfully committed absent frames and becomes eligible afterward.
- `WindowLifetime` is excluded from GC and survives until explicit erase or window destruction.

Normal commits use a bounded scan of 256 candidates to avoid unbounded frame spikes. `app.elements().collectStateGarbage(windowId)` performs an aggressive scan of every currently eligible candidate. Window destruction bypasses retention and releases every `UiElementState` record belonging to that window.

## How resource memory is stored

### Definition-owned slot, storage-owned payload

Each stable `ElementDefinitionRecord` contains its immutable descriptor and one `ElementResourceSlot`:

```text
definitions[FlowDefinitionId]
    -> ElementDefinitionRecord
       +-- immutable descriptor
       `-- ElementResourceSlot
           +-- construction state / mutex / condition variable
           +-- atomic cached payload pointer
           `-- StorageSystem MemoryBlock
```

The resource payload is one app-scoped persistent allocation tagged as:

```text
MemoryClass::ResourceMetadata
ResourceKind::UiElementResources
window = InvalidWindowId
```

Its physical layout is:

```text
[ ElementResourceRecordHeader ][ alignment padding ][ Resources payload ]
```

The header contains definition/resource type hashes, size, alignment, destruction operation, and diagnostic names. Allocation and validation are implemented in [`ElementStorageController.cpp`](src/managers/ElementStorageController.cpp).

Resource construction and destruction run outside the `StorageSystem` mutex. This is intentional: `Resources(App&)` may call image, icon, font, theme, or storage-backed managers without recursively deadlocking storage. The resource block itself is still allocated and reported by `StorageSystem`.

### Construction state machine

[`ElementResourceSlot`](include/internal/ManagerStorage/ElementStorageController.hpp) has five states:

```text
Empty -> Constructing -> Ready -> Destroying
                  `----> Failed
```

- one thread changes `Empty` to `Constructing`;
- concurrent callers wait on the slot;
- successful construction publishes the immutable pointer with release/acquire ordering;
- subsequent reads use the atomic pointer fast path;
- a same-thread recursive request is rejected instead of deadlocking;
- lazy access rethrows a stored failure;
- explicit `prepare()` may retry a failed construction.

Resources are one object per definition, not one object per C++ resource type. Two different definitions using the same `Resources` type receive two distinct objects.

### Lazy context access

Resources are resolved only if a callback calls `resources()`:

```cpp
void buildButton(ButtonDefinition::BuildContext& context) {
    const ButtonResources& resources = context.resources();
    // Build with immutable app-wide resources.
}
```

`resources()` returns `const Resources&` and is constrained away when the definition has no resources. The invocation caches the pointer, so repeated callback access does not repeat manager lookup. The lazy resolution bridge is currently implemented in [`UiManager.cpp`](src/managers/UiManager.cpp).

### Eager preparation

Applications can construct resources before frame building:

```cpp
app.elements().prepare(kButton);

using SpacerDefinition = FlowUi::ElementDefinition<
    FlowUi::NoElementParameters,
    void,
    void,
    FLOW_DEF_ID("layout/spacer")>;

inline constexpr SpacerDefinition kResourceFreeSpacer{};
inline constexpr auto kApplicationElements = FlowUi::elementSet(
    kButton,
    kResourceFreeSpacer);

app.elements().prepare(kApplicationElements);
```

The API is in [`ElementManager.hpp`](include/managers/ElementManager.hpp), and `elementSet()` is in [`ElementManagerStructs.hpp`](include/managers/structs/ElementManagerStructs.hpp).

Passing a resource-free definition, directly or inside a set, is a complete silent no-op. There is intentionally no `prepareAllRegistered()` API: eager initialization is explicit and based on the application's element set, while ordinary include-and-draw use remains registration-free.

Eager preparation is recommended for resources whose constructors mutate app-wide managers. It front-loads construction failures and avoids attempting shared-manager mutation after storage has entered a sealed frame phase.

## Developer-only Flow ID enforcement

The state key assumes that each Flow root ID is unique within one `UiManager` frame. Developer builds enforce this before state/resource resolution or Clay emission and report both authored IDs and source locations when a collision occurs.

The tracker, its containers, claim collection, and diagnostics are entirely enclosed by `FLOW_UI_DEV_MODE` gates. They do not exist in production compilation. The claim path begins in [`FlowUiElementBuilder.hpp`](include/managers/FlowUiElementBuilder.hpp), while the tracker implementation is in [`UiManager.cpp`](src/managers/UiManager.cpp).

This is a diagnostic layer only. Stored-state metadata validation remains active in all builds so a type mismatch can never become an unchecked cast.

## Destruction order

Shutdown proceeds in dependency order:

1. destroy every window's state partition;
2. destroy app-wide element resources;
3. remove the element controller manager record;
4. clean up image, icon, font, window, renderer, and other app services while their owning objects remain valid;
5. shut down `StorageSystem`.

Resource slots detach and clear their published pointers before invoking user destructors. This prevents new readers from observing an object already being destroyed. The controller shutdown is in [`ElementStorageController.cpp`](src/managers/ElementStorageController.cpp), and app ordering is in [`FlowUi.cpp`](src/FlowUi.cpp).

## Test coverage

The migration tests are under [`tests/ElementResourceTests`](tests/ElementResourceTests):

- definition registration, descriptor collision, and identity contracts;
- typed context and manager state APIs;
- window separation and stable state addresses;
- commit/cancel transactions and deferred erasure;
- transient and window-lifetime GC;
- resource alignment, app construction, concurrency, retry, recursion, and destruction;
- compile-time absence of state/resources APIs on definitions that do not own those types;
- compile-time absence of `prepareAllRegistered()`;
- production exclusion of developer-only Flow ID tracking.

## Transitional parts and natural next steps

The migrated backend is usable now, but several compatibility layers intentionally remain because the final element-definition and callback model has not yet been selected.

### Transitional code still present

1. **Legacy definition-owned payloads.** [`ElementDefinition`](include/managers/structs/FlowUiElementStructs.hpp) still contains deprecated static `resources`, `statePool`, `getResources()`, `getOrCreateState()`, lookup, and erase helpers. These are separate compatibility stores; they are not aliases of manager-owned payloads. Code should not mix the old and new paths for the same logical data.
2. **Five-parameter `ElementDefinition`.** [`ElementManagerStructs.hpp`](include/managers/structs/ElementManagerStructs.hpp) currently normalizes the old aliases and flags into `ParametersOf`, `StateOf`, `ResourcesOf`, and the immutable descriptor expected by the new backend.
3. **Function-pointer callback dispatch.** The builder still invokes optional runtime function pointers even though definitions are normally inline compile-time objects.
4. **UiManager bridge functions and friendships.** [`FlowUiElementBridge.hpp`](include/internal/FlowUiElementBridge.hpp), [`UiManager.cpp`](src/managers/UiManager.cpp), [`UiManager.hpp`](include/managers/UiManager.hpp), and [`ElementManager.hpp`](include/managers/ElementManager.hpp) temporarily connect the header-defined builder to private manager operations.
5. **Temporary invocation structs.** `ElementInvocationState` and `ElementResourceInvocationState` carry erased cached pointers until the final typed invocation object owns state/resource access directly.
6. **Definition-instance state policy.** `statePolicy` is currently a value on the transitional definition object. The final compile-time element surface should expose the policy as a static trait or hook.
7. **First-party/dev elements and documentation.** They have deliberately not been migrated yet. Existing deprecation warnings identify the remaining legacy accesses, and broad API documentation is deferred until the final element surface is stable.
8. **Allocator topology.** The current baseline intentionally uses one stable persistent allocation per state or resource payload. No slab/page pooling should be added without benchmark evidence.

### Recommended next sequence

1. **Benchmark the completed backend without changing its API.** Measure allocation count, lookup cost, GC cost, fragmentation, and worst-frame latency for large dynamic lists, deep nesting, and multiple windows. This can proceed without touching dev elements or public documentation.
2. **Finalize the concept/static-hook element surface.** Replace optional callback function pointers with constrained static hooks and `if constexpr`, define the final compile-time state policy hook, and make the typed invocation object call `ElementManager` directly. The storage keys, descriptors, payload layouts, GC, and resource state machine can remain unchanged.
3. **Remove bridge-only friendships and invocation adapters.** Once the builder uses the final typed dispatch, delete the transitional `UiManager` bridge functions and temporary normalization paths that are no longer required.
4. **Migrate first-party/dev elements once.** Move their state access to `context.state()`/`ElementManager`, resource access to `context.resources()`, and eager dev resources to an internal `elementSet`. Then remove the deprecated static stores.
5. **Update tutorials and generated API documentation last.** Document the settled element syntax and provide one focused migration guide from the removed static APIs.

This ordering avoids migrating the dev element set twice and keeps the completed storage/lifetime architecture stable while the authoring and dispatch surface is redesigned.
