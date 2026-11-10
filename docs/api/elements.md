# Element API

The supported authoring surface is defined by [element concepts](../../include/FlowUi/Public/elementAuthoring/ElementConcepts.hpp), [callback contexts](../../include/FlowUi/Public/elementAuthoring/ElementContexts.hpp), [ElementBuilder](../../include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp), and [ElementManager](../../include/FlowUi/Public/managers/ElementManager.hpp).

## Definition contracts

| Contract | Meaning |
| --- | --- |
| `FlowElement<Element>` | Complete empty-tag definition with strong `definitionId`, valid optional capabilities, and an output callback. |
| `DrawableFlowElement<Element>` | Supports `buildElement(ElementBuildContext<Element>&)` and `.draw()`. |
| `ConstructibleFlowElement<Element>` | Supports `constructElement(ElementBuildContext<Element>&)` returning a Clay declaration and `.construct()`. |
| `ParametersOf<Element>` | Declared `Parameters`, otherwise `NoElementParameters`. |
| `HasState<Element>`, `StateOf<Element>` | Optional instance-state capability and payload type. |
| `HasResources<Element>`, `ResourcesOf<Element>` | Optional app-wide immutable resource capability and payload type. |
| `HasParts<Element>` | Definition declares semantic part metadata through nested `Parts`. |

Declare static `onHovered`, `onPressed`, `onHeld`, `onReleased`, and `runLogic` only when needed. They take `ElementInteractionContext<Element>&`. `debugName` is optional descriptive metadata. `statePolicy` can select `ElementStatePolicy::transient(grace_frames)` or `windowLifetime()`.

See [the element-system concepts](../concepts/element_system.md) and [Quick Start](../tutorials/quick_start.md) for complete definitions.

## Identity

| Type or factory | Purpose |
| --- | --- |
| `FlowDefinitionID`, `DefinitionID("name")` | Stable identity of a reusable definition. |
| `FlowElementID` | Resolved local instance identity. |
| `LocalElementName` | Compile-time literal name resolved under a parent/definition scope. |
| `RuntimeName(text)` | Explicit runtime text hashing for a local name. |
| `Indexed("name", position)` | Position-based local name. |
| `Keyed("name", stable_key)` | Stable data-key local name. |
| `IndexedIDs("name", first)` | Sequence of positional local names. |
| `AutoID()` | Callsite identity; also the no-name builder default. |
| `Global<kElement>("name")`, `GlobalFlowID` | Explicit definition-scoped address reproducible outside the local branch. |
| `Part("name")`, `FlowElementPart` | Definition-time declaration of a semantic part. |
| `context.part(part)`, `PartID(...)`, `FlowElementPartID` | Part address bound to a concrete owner. |

The [identity header](../../include/FlowUi/Public/identificationTypes/ElementID.hpp) defines the exact overloads. Give repeated elements distinct names or stable keys. An automatic ID used repeatedly from one loop callsite does not create distinct instances.

## Builder

Obtain `ElementBuilder<Element>` through `UiManager::createElement()`:

```cpp
ui.createElement(FlowUi::kButton, "toolbar/save")
    .setParameters({.onActivate = save_action, .label = "Save"})
    .draw();
```

| Method | Behavior |
| --- | --- |
| `setParameters(const ParametersType&)` | Copy a complete parameter draft. |
| `setParameters(ParametersType&&)` | Move a complete parameter draft. |
| `mergeParams(callable)` | Mutate the current draft through `ParametersType&`. |
| `withID(name_or_id)` | Replace identity using a supported local/runtime/indexed/automatic/resolved/global/part token. Local names resolve against the captured parent. |
| `draw(options = Default)` | Run enabled event/logic callbacks and `buildElement`. Requires `DrawableFlowElement`. |
| `construct(options = Default)` | Run enabled callbacks and open the root returned by `constructElement`. Requires `ConstructibleFlowElement`. Close with `ui.drawConstructed()`. |
| `setDevInternalCapture(bool = true)` | Mark this invocation as internal developer UI when developer support is compiled in. |

`ElementDrawOptions : std::uint8_t` contains `Default`, `SkipEventCallbacks`, `SkipLogicCallback`, and `SkipBuildCallback`. Combine flags with `operator|`; `elementDrawOptionsHas(value, flag)` queries them. `SkipBuildCallback` affects `.draw()`; `.construct()` still obtains its root declaration.

## Callback contexts

Both contexts expose `UiManager& uiManager`, strong `FlowElementID id`, and `ParametersType& params`.

| Observer/helper | Behavior |
| --- | --- |
| `state()` | Return cached instance state; available only with `HasState`. A const context returns const state. |
| `resources() const` | Lazily resolve immutable app-wide resources; available only with `HasResources`. |
| `clayID()` | Bridge the current root identity to Clay. |
| `childID(name)` | Resolve a strong named child under this instance. |
| `clayID(name)` | Resolve and bridge a child in one call. |
| `part(declaration)` | Bind a semantic part to this instance. |
| `createPart(element, declaration)` | Create a child Flow element at a semantic part address. |
| `indexedIDs(name, first = 0)` | Make a positional child-name sequence. |

Interaction contexts additionally expose `previousInteraction`, `invoke(ActionCall)`, and `actionAvailability(ActionCall)`. Build contexts optionally expose `devTimingRecorder()` in developer builds.

Use `context.clayID()` for the element's root so normal builder interaction matches its actual Clay node:

```cpp
CLAY(context.clayID(), root) {
    CLAY(context.clayID("content"), content) {}
}
```

`InteractionSnapshot` stores contiguous hovered, pressed, held, and released Clay IDs from one completed frame. `isHovered`, `isPressed`, `isHeld`, and `isReleased` accept Clay IDs and supported strong Flow IDs. Event callbacks see the previous completed snapshot.

## App-owned runtime data

`App::elements()` owns state and resources through the storage system. Inside callbacks prefer the cached `context.state()`/`resources()` accessors. Outside callbacks:

```cpp
const auto* state = application.elements().getStatePointerConst(
    kCustomElement, application.mainWindowId(), resolved_element_id);
```

`getStatePointer()` and `getStatePointerConst()` query existing state without creating it, with overloads for local, global, and semantic-part identities. `eraseState()` removes one instance explicitly. `collectStateGarbage(window)` collects eligible transient state; ordinary successful frame completion already manages retirement. Transient-state pointers must not be retained across frame boundaries.

Resources are immutable, app-wide, and initialized through a default constructor or `Resources(App&)`. `prepare(element)`/`prepare(ElementSet<...>)` can predeclare definitions; normal invocation resolves the required data lazily.

## Modern developer tooling

`FLOW_UI_DEV_MODE` enables descriptors, capture, catalogues, and typed overrides under `FlowUi::devSystems::tooling`. Describe types with `FLOWUI_DEV_SCHEMA`/`FLOWUI_DEV_FIELD` and enums with `FLOWUI_DEV_ENUM_SCHEMA`/`FLOWUI_DEV_ENUM_VALUE`. No legacy registration or runtime API is provided. See [Developer Systems](../tutorials/developer_mode.md).
