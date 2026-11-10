# Element System

Flow elements package reusable Clay UI, interaction, and runtime data behind a typed C++ contract. They retain immediate-mode construction: the application invokes each visible element every frame, while app-owned managers retain its persistent data.

## Definition and capabilities

An element is an empty tag struct satisfying `FlowUi::FlowElement`. It declares a strong `FlowDefinitionID`, optional descriptive `debugName`, and at least one static draw or construct callback. Optional aliases add parameter, state, resource, and semantic-part capabilities.

```cpp
struct DisclosureParameters {
    std::string_view title = "Details";
};

struct DisclosureState {
    bool expanded = false;
};

struct Disclosure {
    using Parameters = DisclosureParameters;
    using State = DisclosureState;
    using BuildContext = FlowUi::ElementBuildContext<Disclosure>;
    using InteractionContext = FlowUi::ElementInteractionContext<Disclosure>;

    static constexpr auto definitionId = FlowUi::DefinitionID("example.disclosure");
    static constexpr std::string_view debugName = "Disclosure";

    static void onPressed(InteractionContext& context) {
        context.state().expanded = !context.state().expanded;
    }

    static void buildElement(BuildContext& context) {
        Clay_ElementDeclaration root{};
        root.layout.sizing = {
            .width = CLAY_SIZING_FIXED(240.0f),
            .height = CLAY_SIZING_FIXED(context.state().expanded ? 160.0f : 40.0f),
        };
        root.backgroundColor = FlowUi::Flow_Color("#354052ff");
        CLAY(context.clayID(), root) {}
    }
};

inline constexpr Disclosure kDisclosure{};
static_assert(FlowUi::FlowElement<Disclosure>);
```

Declare only capabilities the element uses. An element without parameters receives `NoElementParameters`. An element without state or resources omits those aliases; `context.state()` and `context.resources()` are then unavailable at compile time. `ParametersOf`, `StateOf`, `ResourcesOf`, `HasState`, `HasResources`, and `HasParts` expose these contracts to generic code.

## Invocation and callbacks

```cpp
ui.createElement(kDisclosure, "sidebar/details")
    .setParameters({.title = "Details"})
    .draw();
```

`ElementBuilder<Element>` stores the resolved identity and parameter draft for the invocation. `setParameters()` copies or moves a complete value; `mergeParams()` mutates the draft. `withID()` selects another supported strong/name token against the builder's captured parent scope.

Callbacks are detected at compile time. Implement `onHovered`, `onPressed`, `onHeld`, and `onReleased` for root interaction, and `runLogic` for per-invocation behavior. They receive `ElementInteractionContext<Element>` with `params`, `id`, `uiManager`, `previousInteraction`, and capability-constrained data access. The builder processes event callbacks using the previous completed frame's snapshot, then logic, then output. `ElementDrawOptions` can skip callback phases.

`buildElement(ElementBuildContext<Element>&)` emits the complete subtree for `.draw()`. For an open-ended container, implement `constructElement(...)` returning `Clay_ElementDeclaration`, call `.construct()`, emit caller-owned children, then close with `ui.drawConstructed()`.

## Identity and child scopes

A `FlowDefinitionID` identifies the reusable type. A resolved `FlowElementID` identifies a concrete local invocation under a parent, using the definition and local name. Reusing one local name under different parent instances creates distinct state. Use a stable literal for ordinary names, `RuntimeName()` for explicit runtime text, and `Keyed()` for repeated items whose identity must follow stable data. Positional `Indexed()`/`IndexedIDs()` deliberately make state follow position.

`Global<kElement>("name")` creates a definition-scoped `GlobalFlowID` reproducible outside the local branch. `Part("name")` declares a semantic sub-element; `context.part()` binds it to a concrete owner. `AutoID()` identifies the callsite and is the default when no name is supplied; repeated invocations from one loop callsite need distinct explicit names or keys.

Inside callbacks, use the resolved root and child helpers:

```cpp
CLAY(context.clayID(), root) {
    CLAY(context.clayID("content"), content) {}
    context.uiManager.createElement(kDisclosure, "nested/details").draw();
}
```

`context.childID("content")` returns a strong child identity, and `context.clayID("content")` resolves and bridges it. Nested Flow builders automatically inherit the current invocation scope. Names are identity tokens, not concatenated path strings.

## State and resources

`App::elements()` owns definition metadata, window-scoped instance state, app-wide resources, and state retirement through the storage system. Definitions do not own static state pools.

`context.state()` resolves and caches this invocation's state. A definition can choose `ElementStatePolicy::transient()` or `windowLifetime()` through its supported state-policy declaration. Default transient state is retained through a short grace interval of successfully committed absent frames. Failed/canceled frames do not count as successful absence. Window destruction releases that window's state.

`context.resources()` lazily constructs immutable app-wide resources from either a default constructor or `Resources(App&)`. Resources are shared by all instances of the definition within the application and released with its storage ownership.

Outside a callback, use the typed `ElementManager::getStatePointer()`/`getStatePointerConst()` APIs with the element tag, window, and resolved identity. They query existing state without creating it. Do not retain transient-state pointers across frame boundaries; explicit erase, collection, or window destruction can invalidate them. Prefer explicit borrowed application data in parameters when related controls share an authoritative value.

## Developer integration

With `FLOW_UI_DEV_MODE`, normal invocation discovers element schemas, captures effective parameters, and correlates Flow and Clay trees. Describe editable structs and enums with `FLOWUI_DEV_SCHEMA`, `FLOWUI_DEV_FIELD`, and `FLOWUI_DEV_ENUM_SCHEMA`; the descriptor and registry types live in `FlowUi::devSystems::tooling`. Overrides apply to the parameter draft and commit through the modern engine at application safe points.

See [Developer Systems](../tutorials/developer_mode.md), [custom elements](../tutorials/custom_elements.md), and the [element API](../api/elements.md). The [current element concepts](../../include/FlowUi/Public/elementAuthoring/ElementConcepts.hpp), [contexts](../../include/FlowUi/Public/elementAuthoring/ElementContexts.hpp), and [builder](../../include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp) define the supported signatures.
