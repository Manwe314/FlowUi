# The New Flow Element Cycle

This report describes the element system after the compile-time `FlowElement`
and `ElementBuilder<Element>` migration. It follows an element from its authored
header, through a frame invocation, into app-owned state/resource storage, and
finally through frame commit or cancellation.

The central rule is:

> An element definition is an empty compile-time tag that declares types,
> metadata, and static hooks. Runtime instance data belongs to the builder,
> `ElementManager`, and `StorageSystem`; it does not belong to the tag object.

The implemented contract is defined in
[`FlowUiElementConcepts.hpp`](include/managers/structs/FlowUiElementConcepts.hpp),
the callback contexts are in
[`FlowUiElementStructs.hpp`](include/managers/structs/FlowUiElementStructs.hpp),
and the invocation pipeline is in
[`FlowUiElementBuilder.hpp`](include/managers/FlowUiElementBuilder.hpp).

## 1. Authoring an element in a header

A user writes a plain struct. There is no base class, virtual function,
registration call, callback table, or `ElementDefinition<...>` specialization.

```cpp
#pragma once

#include <FlowUi/Flow.hpp>

struct ButtonParameters {
    std::string label = "Button";
    bool enabled = true;
};

struct ButtonState {
    uint32_t pressCount = 0;
    bool held = false;

    ~ButtonState() noexcept = default;
};

struct ButtonResources {
    explicit ButtonResources(FlowUi::App& app) {
        // Resolve app-wide constant or expensive data here.
        // For example: font, image, or icon handles.
        (void)app;
    }

    ~ButtonResources() noexcept = default;
};

struct ButtonElement {
    using Parameters = ButtonParameters;
    using State = ButtonState;
    using Resources = ButtonResources;

    using BuildContext = FlowUi::ElementBuildContext<ButtonElement>;
    using InteractionContext = FlowUi::ElementInteractionContext<ButtonElement>;

    static constexpr FlowUi::FlowDefinitionId definitionId =
        FLOW_DEF_ID("shared/button");

    // Optional. Omit this for transient state with the default five-frame grace.
    static constexpr FlowUi::ElementStatePolicy statePolicy =
        FlowUi::ElementStatePolicy::transient(5);

    // Optional metadata.
    static constexpr std::string_view debugName = "ButtonElement";

    static void onPressed(InteractionContext& context) {
        if (!context.params.enabled) return;
        ++context.state().pressCount;
    }

    static void onHeld(InteractionContext& context) {
        context.state().held = true;
    }

    static void runLogic(InteractionContext& context) {
        // Runs once for every invocation unless SkipLogicCallback is requested.
        // It may query any id in the previous completed interaction snapshot.
        (void)context.previousInteraction;
    }

    static void buildElement(BuildContext& context) {
        const ButtonState& state = context.state();
        const ButtonResources& resources = context.resources();
        (void)state;
        (void)resources;

        const Clay_ElementId rootId =
            context.uiManager.toClayEID(context.elementID);

        Clay_ElementDeclaration root{};
        // Configure root from params, state, resources, and the active theme.
        CLAY(rootId, root) {
            // Nested Flow elements use ids scoped below this instance.
            // context.uiManager
            //     .createElement(kIcon, context.createChildElementId("icon"))
            //     .draw();
        }
    }
};

inline constexpr ButtonElement kButton{};

static_assert(FlowUi::FlowElement<ButtonElement>);
static_assert(FlowUi::DrawableFlowElement<ButtonElement>);
```

The context aliases are convenience aliases, not inheritance or registration.
They also make the static hook declarations easy to read inside the element.

### 1.1 Optional capabilities

The presence of an alias enables a capability:

| Declaration | Meaning |
| --- | --- |
| `using Parameters = T;` | Builder owns a default-constructed `T` and exposes it as `context.params`. |
| no `Parameters` alias | Builder uses the empty `FlowUi::NoElementParameters`. |
| `using State = T;` | Every `(WindowId, FlowElementId)` instance can resolve persistent `T` state. |
| no `State` alias | State code, storage resolution, and `context.state()` are absent. |
| `using Resources = T;` | The definition can resolve one app-wide immutable `T`. |
| no `Resources` alias | Resource code and `context.resources()` are absent. |

Do not write `using State = void` or `using Resources = void`. Absence is the
final representation of an absent capability.

### 1.2 Required and optional hooks

An element must provide at least one output hook:

```cpp
static void buildElement(FlowUi::ElementBuildContext<MyElement>&);

// and/or

static Clay_ElementDeclaration constructElement(
    FlowUi::ElementBuildContext<MyElement>&);
```

The optional interaction hooks are:

```cpp
static void onHovered(InteractionContext&);
static void onPressed(InteractionContext&);
static void onHeld(InteractionContext&);
static void onReleased(InteractionContext&);
static void runLogic(InteractionContext&);
```

There are no `nullptr` placeholders. If a static hook is absent, its dispatch
branch is absent from the instantiated builder.

### 1.3 Compile-time validation

`FlowElement<Element>` validates that:

1. `definitionId` exists, is compile-time usable, and is nonzero;
2. capability aliases, when present, name non-`void` object types;
3. parameters and state are default constructible;
4. state and resources have `noexcept` destructors;
5. resources can be constructed from `App&` or default constructed;
6. every named hook has its exact required static signature;
7. the element tag is empty, trivially default constructible, and trivially
   copyable;
8. at least one output hook is present.

`draw()` exists only for `DrawableFlowElement`; `construct()` exists only for
`ConstructibleFlowElement`. A type defining both hooks supports both terminals.

## 2. Optional eager resource preparation

Resources are normally created lazily on the first call to
`context.resources()`. An application can instead prepare them before a frame:

```cpp
inline constexpr auto kApplicationElements = FlowUi::elementSet(
    kButton,
    kPanel,
    kIcon);

int main() {
    FlowUi::App app = FlowUi::makeApplication(config);

    // Must be outside an active storage frame.
    app.elements().prepare(kApplicationElements);

    // ...frame loop...
}
```

`prepare(element)` constructs one resource payload when the type declares
`Resources`. `prepare(elementSet)` applies that operation to the set.
Resource-free types are silent no-ops. Construction prefers `Resources(App&)`
over the default constructor. The resulting resource is owned by app-wide
element storage, not by the constexpr tag or a particular window.

The current `ElementSet` remains a pointer-backed catalog of stable definition
tags. Converting it to a type-only pack is a later, smaller cleanup.

## 3. Using the element in a frame

The UI call shape remains unchanged:

```cpp
app.beginFrame(windowId);

FlowUi::UiManager& ui = app.ui(windowId);

ui.createElement(kButton, "header/actions/save")
    .setParameters(ButtonParameters{
        .label = "Save",
        .enabled = canSave,
    })
    .draw();

app.endFrame(windowId);
app.drawFrame(windowId);
```

The string is the logical instance identity. `toFlowId()` hashes it into the
`FlowElementId` used by state management, while `UiManager::toClayEID()` gives
Clay the root `Clay_ElementId`. The Flow element and its root Clay element must
represent the same logical string id.

The `ResourceKey` overload performs the same operation after resolving the key
to its normalized UI resource name:

```cpp
ui.createElement(kButton, buttonResourceKey).draw();
```

## 4. What `beginFrame()` establishes

Before any element is invoked, `App::beginFrame(windowId)` performs the window
frame setup in [`FlowUi.cpp`](src/FlowUi.cpp):

1. It validates that the window is idle and that no other window frame triplet
   is currently active.
2. It waits for the window's reusable graphics frame and collects completed
   storage work.
3. It opens a `StorageSystem` frame and obtains a new frame epoch.
4. It calls `ElementManager::beginWindowFrame(windowId, epoch)`.
5. `ElementStorageController` opens a state transaction for that window and
   rejects stale active invocations or deferred erasures.
6. Input is drained and converted into layout coordinates.
7. `UiManager::beginFrame()` starts the Clay layout and frame arenas.

All element state touches, creations, policy changes, and explicit erasures
after this point belong to the open window transaction.

## 5. `createElement()` and builder creation

The `UiManager` overloads are implemented in
[`UiManager.hpp`](include/managers/UiManager.hpp). For a call such as:

```cpp
auto builder = ui.createElement(kButton, "header/actions/save");
```

the following happens:

1. Template deduction obtains `Element = ButtonElement` from the empty tag.
2. The `FlowElement<Element>` constraint validates the authored type.
3. The tag value itself is discarded; it is not copied or stored.
4. `UiManager` constructs `ElementBuilder<ButtonElement>` with:
   - its own `UiManager&`;
   - the attached app-wide `ElementManager&`;
   - its `WindowId`;
   - an owned `std::string` containing the logical element id;
   - dev source-location data only in dev builds.
5. The builder default constructs `ParametersOf<ButtonElement>`.

Fluent functions operate only on this pending builder:

- `setParameters()` replaces its parameter object;
- `mergeParams()` mutates that object;
- `withElementID()` replaces the owned logical id;
- `setDevInternalCapture()` changes capture classification in dev builds and is
  compiled to a no-op in production.

No state/resource lookup and no user callback occurs until `draw()` or
`construct()` is called.

## 6. The shared invocation pipeline

Both terminals enter the same `ElementBuilder<Element>::invoke<OutputMode>()`
implementation in
[`FlowUiElementBuilder.hpp`](include/managers/FlowUiElementBuilder.hpp). The
output mode is a compile-time value, not runtime polymorphism.

The sequence is:

### 6.1 Derive the instance identity

The builder hashes its logical string with `toFlowId(elementID)`. The result is
the instance's `FlowElementId`.

In dev mode only, the builder claims this Flow root id for the current
`UiManager` frame. A duplicate claim emits a diagnostic. The tracker storage,
collection code, and call site are physically absent from production
compilation.

### 6.2 Begin `ElementInvocation<Element>`

The builder creates the non-copyable, non-movable RAII object from
[`ElementInvocation.hpp`](include/internal/ElementInvocation.hpp):

```cpp
auto invocation = ElementInvocation<Element>::begin(
    elementManager, uiManager, windowId, flowId);
```

This is the single owner of callback-visible managed data for the invocation.

First, it uses the inline constexpr descriptor generated in
[`ElementManagerStructs.hpp`](include/managers/structs/ElementManagerStructs.hpp).
That descriptor contains:

- definition id and definition-type hash;
- parameter/state/resource type hashes;
- payload sizes and alignments;
- capability flags;
- internal placement-construction and destruction operations;
- debug/type names.

The erased construction functions are only heterogeneous storage operations.
Element behavior itself remains direct static dispatch.

Then `ElementInvocation` branches at compile time:

- **Stateless element:** it only ensures that the immutable definition
  descriptor is registered with `ElementManager`.
- **Stateful element:** it calls `beginStateInvocation()` with the definition
  descriptor, `WindowId`, `FlowElementId`, and compile-time state policy.

State lookup uses `(WindowId, FlowElementId)` as the instance identity. It does
not add the C++ element type to that key because two Flow roots with one logical
id would already collide in Clay. The registered descriptor is used to reject
incompatible type/definition reuse.

If state already exists, its persistent record is validated and returned. If
it does not exist, storage allocates a persistent record shaped as:

```text
[ElementStateRecordHeader][alignment padding][State payload]
```

The state is default constructed in place, indexed by the window registry, and
recorded as created/touched in the open frame transaction. The controller
increments the active invocation count. `ElementInvocation` caches the returned
handle and typed payload pointer exactly once.

### 6.3 Resolve the Clay root id when needed

The builder asks `UiManager` for the root `Clay_ElementId` when the selected
operation needs it:

- every `construct()` needs it because the builder opens the root;
- a `draw()` needs it when event hooks must compare the previous interaction
  snapshot against the root;
- a draw-only element with no root event hooks does not pay for this builder-side
  conversion. Its `buildElement()` still emits its own root using
  `context.elementID`.

### 6.4 Begin dev capture when enabled

Dev builds create one capture record containing definition identity, type
identity, logical id, Flow id, internal classification, and source location.
This entire branch and its builder fields are excluded from production.

### 6.5 Dispatch interaction hooks

If the element has no event hooks and no `runLogic`, the compiler removes this
phase, including previous-snapshot retrieval and
`ElementInteractionContext` construction.

Otherwise, and when the corresponding draw options permit it:

1. The builder obtains the previous completed `InteractionSnapshot`.
2. It constructs one `ElementInteractionContext<Element>` referencing:
   - the typed invocation;
   - the active `UiManager`;
   - the logical string id;
   - the builder-owned parameters;
   - the previous interaction snapshot.
3. Present event hooks are considered in this order:
   `onHovered`, `onPressed`, `onHeld`, `onReleased`.
4. Each event hook runs only when the previous snapshot contains the root Clay
   id for that event.
5. If present and not skipped, `runLogic` runs after all event hooks.

Every hook is a direct call such as `Element::onPressed(context)` guarded by
`if constexpr`. There are no callback-pointer loads or null checks.

Parameter mutations made by an interaction hook remain visible to subsequent
hooks and to the output hook because all contexts reference the same builder
parameter object.

### 6.6 State and resource access inside contexts

Both context types delegate `state()` and `resources()` to the same typed
invocation.

`context.state()`:

- only exists when the element declares `State`;
- returns the pointer cached during invocation start;
- does not repeat map or storage lookup for later callbacks;
- has mutable and const overloads.

`context.resources()`:

- only exists when the element declares `Resources`;
- returns `const Resources&`;
- lazily asks `ElementManager` for the app-wide resource on first use;
- caches that pointer for every later context in the same invocation.

The resource storage record is shaped as:

```text
[ElementResourceRecordHeader][alignment padding][Resources payload]
```

Only one ready resource exists per registered element definition across all
windows. Construction is synchronized, recursive construction is rejected, and
a failed lazy construction remains failed until explicit eager preparation
retries it.

### 6.7 Apply dev parameter overrides

Immediately before an output hook, dev builds apply any registered parameter
overrides to the builder-owned parameter object. A skipped draw build does not
apply output-phase overrides, matching the existing draw-option semantics.

### 6.8 Execute the selected output terminal

The builder creates `ElementBuildContext<Element>` from the same invocation,
logical id, and parameter object.

For `draw()`:

1. `draw()` is available only if `buildElement` exists.
2. Unless `SkipBuildCallback` is set, the builder directly calls:

   ```cpp
   Element::buildElement(buildContext);
   ```

3. The authored hook owns emission of the complete Clay subtree, including its
   root.
4. In dev mode the capture closes as the builder scope exits.

For `construct()`:

1. `construct()` is available only if `constructElement` exists.
2. The builder directly calls the hook and receives a
   `Clay_ElementDeclaration`.
3. The builder opens a Clay element using the computed root id and configures it
   with that declaration.
4. It pushes the root onto `UiManager`'s constructed-element stack.
5. In dev mode it intentionally leaves capture open.
6. The caller emits arbitrary child Clay/Flow elements and finally calls:

   ```cpp
   ui.drawConstructed();
   ```

7. `drawConstructed()` closes the Clay root, pops the constructed stack, and
   closes the corresponding dev capture.

The state/resource invocation itself does not remain open until
`drawConstructed()`: all element callbacks have returned, so no context or
cached payload pointer is still live.

### 6.9 End the typed invocation

On every normal return or exception path, `ElementInvocation` is destroyed. For
a stateful element it calls `ElementManager::endStateInvocation(windowId)`,
decrementing the window's active invocation count and permitting deferred
erasure or window cleanup once no nested invocation remains.

This RAII boundary also supports recursive Flow composition. A parent callback
may invoke a child Flow element; both leases remain valid while nested callbacks
run, and each child closes its own lease before the parent resumes or returns.

## 7. What `endFrame()` commits

`App::endFrame(windowId)` completes UI construction and then commits the element
transaction:

1. `UiManager::endFrame()` auto-closes any forgotten constructed roots, closes
   their dev captures, and prints a diagnostic.
2. Clay ends layout and produces render commands.
3. `UiManager` records the current hover/press/hold/release snapshot for use by
   the next completed frame.
4. Rendering resources and texture bindings are prepared.
5. The `StorageSystem` frame is sealed for reading.
6. `ElementManager::commitWindowFrame(windowId, epoch)` commits state metadata.
7. Every touched state receives the new committed-frame number and the policy
   used by its latest invocation.
8. Deferred explicit erasures are drained only after every cached callback
   pointer has been released.
9. The controller performs bounded transient GC maintenance, scanning up to 256
   candidates on the normal commit path.

The default policy is `ElementStatePolicy::transient(5)`. Such state survives
five successfully committed frames in which its Flow id is absent and becomes
eligible afterward. `windowLifetime()` state survives until explicit erasure or
window destruction.

If frame construction or preparation throws, `App` cancels both the element
transaction and the storage frame. Cancellation:

- discards deferred erasures;
- removes state records created only by the canceled frame;
- does not advance committed-frame age;
- preserves state that existed before the canceled frame.

## 8. Identity and ownership summary

| Data | Identity | Owner | Lifetime |
| --- | --- | --- | --- |
| Element behavior | C++ element type | compiled program | compile time/static code |
| Definition metadata | `FlowDefinitionId` plus validated type metadata | app-wide `ElementManager` registry | app lifetime |
| Parameters | one pending builder invocation | `ElementBuilder<Element>` | create-to-terminal call |
| State | `(WindowId, FlowElementId)` | `StorageSystem`, indexed by `ElementStorageController` | policy/erase/window lifetime |
| Resources | registered element definition | app-wide `StorageSystem` | app lifetime |
| Callback data lease | one terminal invocation | `ElementInvocation<Element>` | all callbacks in that invocation |
| Root Clay identity | logical element string converted by `UiManager` | Clay frame | frame/layout lifetime |

Two windows may use the same logical element string without sharing state,
because `WindowId` participates in state identity. Definitions and resources
remain app-wide. If an application wants cross-window behavior synchronization,
it explicitly reads or modifies the separate states through `ElementManager` or
stores shared application data elsewhere.

## 9. What compile-time polymorphism removes

For each concrete `ElementBuilder<Element>` instantiation, the compiler knows:

- whether parameters are custom or empty;
- whether state exists;
- whether resources exist;
- exactly which interaction hooks exist;
- whether draw, construct, or both are supported;
- the state retention policy;
- whether the definition is dev-internal.

Consequently, absent features do not need runtime callback slots, null checks,
state/resource placeholders, or dispatch branches. The only internal function
pointers retained are placement construction/destruction operations inside
heterogeneous storage descriptors.

## 10. Remaining non-major migration work

The core user-authored element cycle and production builder are now on the new
surface. The following repository work remains intentionally separate:

1. FlowUi's existing dev-mode element headers and `debugView.cpp` still need to
   be rewritten as empty tag structs with static hooks. Until that migration,
   a complete `FLOW_UI_DEV_MODE=1` library build that compiles those first-party
   definitions is not expected to succeed; the new builder's dev branch itself
   is compile-checked with final-shape elements.
2. The dev capture bridge is marked transitional and can be consolidated when
   the dev registry and dev elements move together.
3. `ElementSet` can become a type-only pack rather than retaining pointers to
   global constexpr tags.
4. [`template.hpp`](template.hpp), tutorials, API docs, and generated HTML still
   describe the old callback-carrier authoring syntax and should be updated only
   after the first-party element migration settles the final examples.

These items do not change the new runtime sequence described above; they migrate
remaining library-authored consumers and documentation onto it.
