# Element System Concepts Implementation

## Purpose

This report defines a target compile-time authoring model for Flow elements, shows how it preserves the existing UI-building call site, maps the completed state/resource migration onto that model, and provides a staged implementation plan.

The target removes two different kinds of legacy surface:

1. the callback function-pointer fields currently stored in [`ElementDefinition`](include/managers/structs/FlowUiElementStructs.hpp);
2. the deprecated definition-owned state/resource stores and helpers in the same type.

It does **not** remove the internal erased construction/destruction operations in [`ElementRegistration.hpp`](include/internal/ManagerStorage/ElementRegistration.hpp). Those thunks are not element callback dispatch. They allow a heterogeneous runtime manager to destroy state and resources safely after their concrete C++ type is no longer on the call stack.

The storage architecture described in [`dataMigrationReport.md`](dataMigrationReport.md) remains the backend. This redesign changes how an element type declares behavior and how `ElementBuilder` dispatches it; it does not change state identity, resource ownership, GC, or window lifetime semantics.

## Part I — Final element authoring and builder surface

### 1. Recommended final element surface

#### 1.1 Core model

The library class template currently named `ElementDefinition<Parameters, State, Resources, Id, Dev>` should disappear. In the final model, an **element definition is a user-authored type that satisfies the `FlowElement` concept**.

An element type carries:

- required compile-time definition identity;
- optional parameter, state, and resource type aliases;
- optional compile-time policy metadata;
- optional static interaction/logic hooks;
- at least one static output hook: `buildElement` and/or `constructElement`.

The value passed to `createElement()` is an empty constexpr tag used for type deduction:

```cpp
struct ButtonParams {
    std::string label = "Button";
};

struct ButtonState {
    bool pressed = false;
};

struct ButtonResources {
    FlowUi::TextureRef icon{};

    explicit ButtonResources(FlowUi::App& app) {
        // Resolve app-wide dependencies here.
    }

    ~ButtonResources() noexcept = default;
};

struct ButtonElement {
    using Parameters = ButtonParams;
    using State = ButtonState;
    using Resources = ButtonResources;

    static constexpr FlowUi::FlowDefinitionId definitionId =
        FLOW_DEF_ID("controls/button");
    static constexpr FlowUi::ElementStatePolicy statePolicy =
        FlowUi::ElementStatePolicy::transient();

    using InteractionContext = FlowUi::ElementInteractionContext<ButtonElement>;
    using BuildContext = FlowUi::ElementBuildContext<ButtonElement>;

    static void onPressed(InteractionContext& context) {
        context.state().pressed = true;
    }

    static void onReleased(InteractionContext& context) {
        context.state().pressed = false;
    }

    static void buildElement(BuildContext& context) {
        const ButtonResources& resources = context.resources();
        const bool pressed = context.state().pressed;
        // Emit the root Clay element and any nested Flow elements.
    }
};

inline constexpr ButtonElement kButton{};
static_assert(FlowUi::FlowElement<ButtonElement>);
```

Static member functions defined inside the class body are implicitly inline, so this shape is suitable for shareable header-only element libraries.

The library should ensure `ElementBuildContext<Element>` and `ElementInteractionContext<Element>` can be named while the element class is being completed, provided the type aliases appear before the context aliases and hooks. If compiler behavior or trait instantiation order makes that fragile, the documented fallback should be out-of-class inline hook definitions rather than forcing a runtime callback object.

#### 1.2 Why the tag should be empty

The recommended contract makes hook dispatch and policy type-level. `kButton` exists to retain the current ergonomic call site and to support type deduction for `elementSet()` and manager operations; the builder does not need to retain its address.

Requiring the tag type to be empty has useful semantics:

- two values of the same element type cannot silently carry different behavior while sharing one definition ID and one resource slot;
- `ElementBuilder` does not store a definition pointer;
- `elementSet()` can store only a type pack;
- runtime variation stays where it is visible: parameters, themes, app models, or per-instance state;
- one element type corresponds to one descriptor and one app-wide resource object.

This is a design choice rather than a technical requirement. The alternatives are discussed later, but an empty tag is the clearest match for how Flow elements are already used.

#### 1.3 Required and optional members

Recommended final contract:

| Member | Requirement | Meaning |
| --- | --- | --- |
| `static constexpr definitionId` | Required | Stable definition/type identity used by registration and resource ownership. |
| `using Parameters` | Optional | Builder-owned per-invocation parameters. Absence means `NoElementParameters`. |
| `using State` | Optional | Per-`(WindowId, FlowElementId)` managed state. Absence means stateless. |
| `using Resources` | Optional | One app-wide managed resource object for this definition. Absence means resource-free. |
| `static constexpr statePolicy` | Optional | Definition state retention. Absence means transient with five grace frames. |
| `static constexpr isDevInternal` | Optional | Dev capture classification. Absence means `false`. |
| `static constexpr debugName` | Optional | Human-readable diagnostic name. Absence falls back to the type token. |
| Event and logic hooks | Optional | Compiled in only when their exact expression is valid. |
| `buildElement` / `constructElement` | At least one required | Defines whether `.draw()`, `.construct()`, or both are available. |

Omitting a type alias is cleaner than spelling `using State = void`. During migration, traits may accept both forms, but the final documentation should teach omission.

#### 1.4 Concepts and hook detection

The concept layer should separate base metadata from optional capabilities. A representative shape is:

```cpp
template <typename Element>
using ParametersOf = /* Element::Parameters or NoElementParameters */;

template <typename Element>
using StateOf = /* Element::State or NoElementState */;

template <typename Element>
using ResourcesOf = /* Element::Resources or NoElementResources */;

template <typename Element>
inline constexpr bool HasState =
    !std::same_as<StateOf<Element>, NoElementState>;

template <typename Element>
inline constexpr bool HasResources =
    !std::same_as<ResourcesOf<Element>, NoElementResources>;

template <typename Element>
concept ElementMetadata = requires {
    { Element::definitionId } -> std::convertible_to<FlowDefinitionId>;
};

template <typename Element>
concept HasBuildElement = requires(ElementBuildContext<Element>& context) {
    { Element::buildElement(context) } -> std::same_as<void>;
};

template <typename Element>
concept HasConstructElement = requires(ElementBuildContext<Element>& context) {
    { Element::constructElement(context) } ->
        std::same_as<Clay_ElementDeclaration>;
};

template <typename Element>
concept FlowElement =
    ElementMetadata<Element> &&
    (HasBuildElement<Element> || HasConstructElement<Element>);
```

Each optional interaction hook gets its own detection concept:

```cpp
template <typename Element>
concept HasOnHovered =
    requires(ElementInteractionContext<Element>& context) {
        { Element::onHovered(context) } -> std::same_as<void>;
    };

template <typename Element>
concept HasRunLogic =
    requires(ElementInteractionContext<Element>& context) {
        { Element::runLogic(context) } -> std::same_as<void>;
    };
```

Equivalent checks are needed for `onPressed`, `onHeld`, `onReleased`, and the two output hooks. The implementation should validate exact return types and usable parameter types so misspelled or incorrectly shaped hooks fail at the element declaration boundary rather than being silently ignored.

Pure `requires` failures can produce indirect compiler messages. A `consteval validateElement<Element>()` invoked by `elementDescriptor<Element>` and `UiManager::createElement()` is recommended for focused `static_assert` messages such as:

```text
FlowUi: ButtonElement::buildElement must be callable as
static void buildElement(ElementBuildContext<ButtonElement>&).
```

#### 1.5 Hook names

The recommended first implementation retains the existing semantic names:

- `onHovered`
- `onPressed`
- `onHeld`
- `onReleased`
- `runLogic`
- `constructElement`
- `buildElement`

This limits the redesign to dispatch and ownership rather than mixing in a naming migration. Shortening the output hooks to `construct` and `build` is possible, but it provides little functional benefit and makes it easier to confuse element hooks with `ElementBuilder::construct()` and `ElementBuilder::draw()`.

#### 1.6 Compile-time dispatch

The current builder repeatedly performs pointer/null checks such as those in [`FlowUiElementBuilder.hpp`](include/managers/FlowUiElementBuilder.hpp). The final builder should use static selection:

```cpp
if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipEventCallbacks)) {
    if constexpr (HasOnHovered<Element>) {
        if (previousInteraction.isHovered(rootElementId)) {
            Element::onHovered(context);
        }
    }
    if constexpr (HasOnPressed<Element>) {
        if (previousInteraction.isPressed(rootElementId)) {
            Element::onPressed(context);
        }
    }
}

if constexpr (HasRunLogic<Element>) {
    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipLogicCallback)) {
        Element::runLogic(context);
    }
}
```

Missing hooks generate no function-pointer field, no null check, and no call site. If an element has no interaction or logic hooks, the builder should avoid retrieving the previous interaction snapshot and avoid creating an interaction context altogether.

Runtime `ElementDrawOptions` remain runtime options because callers choose them per invocation. Compile-time polymorphism removes element-shape branching, not intentional per-draw behavior.

#### 1.7 Parameter callbacks that reference the element context

Some current dev parameters contain application callbacks whose argument is the element interaction context, for example [`devBasicButton.hpp`](include/devMode/devFlowElements/devBasicButton.hpp). Those callbacks are per-invocation data and should remain runtime callables; they are not the definition hook pointers being removed.

They introduce a type-ordering cycle that the final context traits must support. The element can be declared before the complete parameter struct while already exposing the parameter alias:

```cpp
struct ButtonParams;

struct ButtonElement {
    using Parameters = ButtonParams;
    using InteractionContext =
        FlowUi::ElementInteractionContext<ButtonElement>;
    using BuildContext =
        FlowUi::ElementBuildContext<ButtonElement>;

    static constexpr FlowUi::FlowDefinitionId definitionId =
        FLOW_DEF_ID("controls/button");

    static void onPressed(InteractionContext& context);
    static void buildElement(BuildContext& context);
};

using ButtonInteractionContext = ButtonElement::InteractionContext;

struct ButtonParams {
    std::function<void(ButtonInteractionContext)> onPressed;
};

inline void ButtonElement::onPressed(InteractionContext& context) {
    if (context.params.onPressed) context.params.onPressed(context);
}

inline void ButtonElement::buildElement(BuildContext& context) {
    // Parameters is complete here.
}
```

For this to work reliably, `ElementBuildContext<Element>` and `ElementInteractionContext<Element>` must be valid when `Element::Parameters` is declared but still incomplete. Context declarations should store parameters by reference and avoid eager `sizeof`, construction, or descriptor validation. Full type validation belongs in `validateElement<Element>()` after the authored element and parameter types are complete.

This forward-declaration form is needed only for cyclic callback signatures. Ordinary parameters can remain defined before the element as shown in section 1.1.

### 2. Recommended final `ElementBuilder`

#### 2.1 Type shape

The five-template-parameter builder in [`FlowUiElementBuilder.hpp`](include/managers/FlowUiElementBuilder.hpp) should become:

```cpp
template <FlowElement Element>
class ElementBuilder {
public:
    using ElementType = std::remove_cvref_t<Element>;
    using ParametersType = ParametersOf<ElementType>;
    using StateType = StateOf<ElementType>;
    using ResourcesType = ResourcesOf<ElementType>;
    using BuildContext = ElementBuildContext<ElementType>;
    using InteractionContext = ElementInteractionContext<ElementType>;

    ElementBuilder& setParameters(const ParametersType& parameters);
    ElementBuilder& setParameters(ParametersType&& parameters);

    template <typename MergeFn>
    ElementBuilder& mergeParams(MergeFn&& mergeFn);

    ElementBuilder& withElementID(std::string_view elementID);
    ElementBuilder& setDevInternalCapture(bool enabled = true);

    void draw(ElementDrawOptions options = ElementDrawOptions::Default)
        requires HasBuildElement<ElementType>;

    void construct(ElementDrawOptions options = ElementDrawOptions::Default)
        requires HasConstructElement<ElementType>;

private:
    UiManager& uiManager_;
    ElementManager& elementManager_;
    WindowId window_ = InvalidWindowId;
    std::string elementID_;
    [[no_unique_address]] ParametersType params_{};
    bool captureAsDevInternal_ = elementIsDevInternal<ElementType>;
};
```

The definition pointer disappears. The element type itself selects every hook and descriptor. `draw()` is unavailable when the type has no `buildElement`; `construct()` is unavailable when it has no `constructElement`.

Using constrained members gives a correct API surface in generic code. To improve diagnostics for direct calls, the implementation may keep unconstrained forwarding members with targeted `static_assert`s, but it should not fall back to runtime exceptions for a compile-time shape error.

#### 2.2 Preserved UI-building API

The visible UI-building syntax remains unchanged:

```cpp
ui.createElement(kButton, "toolbar/save")
    .setParameters(ButtonParams{
        .label = "Save",
    })
    .mergeParams([](ButtonParams& params) {
        params.label += " document";
    })
    .draw();
```

Constructed/open-ended elements remain unchanged:

```cpp
ui.createElement(kPanel, "settings/panel")
    .setParameters(PanelParams{})
    .construct();

// Emit children.

ui.drawConstructed();
```

The `ResourceKey` overload should remain as well. Only deduction changes inside [`UiManager::createElement()`](include/managers/UiManager.hpp):

```cpp
template <FlowElement Element>
auto createElement(const Element&, std::string_view elementID)
    -> ElementBuilder<std::remove_cvref_t<Element>>;
```

The tag argument is used for deduction and is not stored. This preserves include-and-use ergonomics while making the actual dispatch type-driven.

#### 2.3 Shared invocation pipeline

`draw()` and `construct()` currently duplicate registration, state lookup, event dispatch, logic dispatch, dev capture, context creation, and cleanup. The redesign should extract one typed common pipeline and keep only final Clay emission separate.

A recommended internal shape is:

```cpp
template <FlowElement Element>
class ElementInvocation {
public:
    static ElementInvocation begin(
        ElementManager& elements,
        UiManager& ui,
        WindowId window,
        FlowElementId flowId);

    StateOf<Element>* state() noexcept;
    const ResourcesOf<Element>& resources()
        requires HasResources<Element>;

    ~ElementInvocation(); // closes the state pointer lifetime/lease

private:
    // Descriptor, state handle/pointer, resource cache, and invocation-active flag.
};
```

The builder pipeline becomes conceptually:

```text
derive Flow ID and root Clay ID
-> dev-only root-ID claim
-> ensure immutable definition registration
-> begin typed data invocation
-> run present interaction hooks
-> run present logic hook
-> apply dev parameter overrides
-> invoke buildElement or constructElement
-> close data invocation at scope exit
```

The invocation object should own the current state lease and lazy resource cache. Contexts should reference it rather than separately carrying a raw state pointer and `ElementResourceInvocationState`. This is the natural place to remove the transitional resource bridge without changing the storage controller.

`construct()` needs separate dev-capture handling because the Clay root remains open until `drawConstructed()`. The data invocation may still end when `construct()` returns because no callback retains its context or state pointer; the dev capture stack remains associated with the constructed Clay stack.

#### 2.4 Low-risk backing optimizations

The concept migration enables these optimizations without changing UI call sites:

- instantiate `ElementBuilder<Element>` instead of repeating five template arguments;
- remove the stored definition pointer and null validation;
- compile out missing callbacks and their context construction;
- skip state invocation code entirely with `if constexpr (!HasState<Element>)`;
- omit the resource cache for resource-free elements with `[[no_unique_address]]` or a conditional empty holder;
- compute the immutable descriptor once as `inline constexpr elementDescriptor<Element>`;
- turn `ElementSet` into an empty type pack instead of storing pointers to definition objects;
- centralize duplicated draw/construct phases into one inline typed routine.

The builder's owned `std::string elementID_` should remain during this migration. Replacing it with a frame-arena string or `ResourceKey`-only representation could remove allocations, but it changes lifetime assumptions for builders stored in local variables and should be benchmarked separately.

### 3. `elementSet` and resource preparation

With empty tag elements, the set only needs types:

```cpp
template <FlowElement... Elements>
struct ElementSet {};

template <FlowElement... Elements>
constexpr auto elementSet(const Elements&...) noexcept {
    return ElementSet<std::remove_cvref_t<Elements>...>{};
}
```

Usage remains:

```cpp
inline constexpr auto kApplicationElements = FlowUi::elementSet(
    kButton,
    kPanel,
    kSpacer);

app.elements().prepare(kApplicationElements);
```

`ElementManager::prepare(element)` and `prepare(elementSet)` continue using `elementDescriptor<Element>`. Resource-free elements remain silent no-ops. Removing stored object pointers also removes the current lvalue/dangling concern in [`ElementSet`](include/managers/structs/ElementManagerStructs.hpp).

### 4. Proposed `template.hpp`

The current [`template.hpp`](template.hpp) teaches positional function pointers, `nullptr` placeholders, and deprecated static state/resource access. It should instead generate one element type with only the hooks the author needs.

A proposed replacement is:

```cpp
#pragma once

#include <FlowUi/Flow.hpp>
#include <devMode/devApi.hpp>

struct templateParams {
    int importantInt = 42;
};

struct templateState {
    bool enabled = true;
};

struct templateResources {
    explicit templateResources(FlowUi::App& app) {
        (void)app;
    }

    ~templateResources() noexcept = default;
};

struct TemplateElement {
    using Parameters = templateParams;
    using State = templateState;
    using Resources = templateResources;

    static constexpr FlowUi::FlowDefinitionId definitionId =
        FLOW_DEF_ID("template");
    static constexpr FlowUi::ElementStatePolicy statePolicy =
        FlowUi::ElementStatePolicy::transient();

    using InteractionContext =
        FlowUi::ElementInteractionContext<TemplateElement>;
    using BuildContext =
        FlowUi::ElementBuildContext<TemplateElement>;

    // Add only the event hooks this element actually uses:
    // static void onHovered(InteractionContext& context) {}
    // static void onPressed(InteractionContext& context) {}
    // static void onHeld(InteractionContext& context) {}
    // static void onReleased(InteractionContext& context) {}
    // static void runLogic(InteractionContext& context) {}

    static void buildElement(BuildContext& context) {
        templateState& state = context.state();
        const templateResources& resources = context.resources();
        (void)state;
        (void)resources;

        Clay_ElementDeclaration root{};
        const Clay_ElementId rootId =
            context.uiManager.toClayEID(context.elementID);

        CLAY(rootId, root) {
            // Emit children or nested Flow elements.
        };
    }

    // Add this only when the element supports the open-ended construct flow:
    // static Clay_ElementDeclaration constructElement(BuildContext& context) {
    //     return Clay_ElementDeclaration{};
    // }
};

inline constexpr TemplateElement kTemplate{};

FLOWUI_DEV_REGISTER_STRUCT(
    templateParams,
    FLOWUI_DEV_REFLECT_FIELD(templateParams, importantInt));

FLOWUI_DEV_REGISTER_STRUCT(
    templateState,
    FLOWUI_DEV_REFLECT_FIELD(templateState, enabled));

FLOWUI_DEV_REGISTER_STRUCT(templateResources);
FLOWUI_DEV_REGISTER_ELEMENT(TemplateElement, "Template");
```

Important changes:

- no `ElementDefinition<...>` alias;
- no positional aggregate initialization;
- no unary `+` lambda conversion;
- no `nullptr` placeholders;
- optional hooks are absent, not null;
- state uses `context.state()`;
- resources use `context.resources()`;
- state retention is type-level metadata;
- `kTemplate` is an empty constexpr tag;
- dev registration targets the authored element type.

The template should not emit all optional hooks as active no-ops. Doing so would make concepts detect them and would reintroduce unnecessary interaction phases. Commented examples or generator flags are preferable.

## Part II — StorageSystem integration and transitional cleanup

### 5. Fitting the StorageSystem migration into the new shape

#### 5.1 What remains unchanged

The following completed contracts remain valid:

- state identity is `(WindowId, FlowElementId)`;
- definition/state metadata is validated before typed casts;
- state records use stable aligned persistent storage;
- resources are app-wide and unique per definition;
- state is resolved once per invocation;
- resources are lazy and cached per invocation;
- transient and window-lifetime policies retain their current semantics;
- commit/cancel transactions and bounded GC remain unchanged;
- duplicate Flow-root tracking remains entirely dev-mode gated;
- external state access remains `readState`, `modifyState`, and `eraseState`;
- eager resources remain `prepare(element)` and `prepare(elementSet)`.

The public manager surface in [`ElementManager.hpp`](include/managers/ElementManager.hpp) already templates on an element type and can work with the new concept once `StateOf`, `ResourcesOf`, and `elementDescriptor` understand the final aliases.

#### 5.2 Descriptor generation remains the bridge to runtime storage

Compile-time hooks do not eliminate the need for an immutable runtime descriptor. The manager must store heterogeneous definitions and later destroy heterogeneous state/resource payloads during GC, window teardown, or app shutdown.

The final path remains:

```text
user element type
-> compile-time traits and validation
-> constexpr ElementRegistrationDescriptor
-> ElementManager / ElementStorageController
-> erased aligned state/resource record
```

[`makeElementDescriptor()`](include/managers/structs/ElementManagerStructs.hpp) should be rewritten to read only the final aliases and traits. It should continue generating internal placement-construction and destruction thunks:

```cpp
descriptor.stateOperations = makeDefaultTypeOperations<StateOf<Element>>();
descriptor.resourceOperations = makeResourceTypeOperations<ResourcesOf<Element>>();
```

These internal pointers are called only at allocation/destruction boundaries. They are not invoked for hover, press, logic, build, or construct dispatch and should remain private under `detail::element`.

Renaming `ElementTypeOperations` to `ElementPayloadOperations` is optional but recommended because it makes this distinction obvious.

#### 5.3 Context changes

[`ElementBuildContext`](include/managers/structs/FlowUiElementStructs.hpp) and [`ElementInteractionContext`](include/managers/structs/FlowUiElementStructs.hpp) currently depend on legacy members such as `ElementType::ParametersType` and `ElementType::hasState`. They should instead use:

```cpp
using ParametersType = ParametersOf<ElementType>;
using StateType = StateOf<ElementType>;
using ResourcesType = ResourcesOf<ElementType>;

StateType& state() requires HasState<ElementType>;
const ResourcesType& resources() const requires HasResources<ElementType>;
```

The two context types should remain separate. `InteractionContext` exposes `previousInteraction`; `BuildContext` represents the later output phase. Combining them would make phase-specific data available where it has no semantic meaning and is not required by the concept migration.

Contexts should hold a reference to the typed invocation object rather than the transitional raw state/resource cache fields. This keeps pointer lifetime enforcement in one RAII owner.

#### 5.4 Registration and manager access

The builder should still ensure registration before state/resource access. Automatic registration preserves the current include-and-draw ease of use.

The free functions in [`FlowUiElementBridge.hpp`](include/internal/FlowUiElementBridge.hpp) were introduced because the current header-defined builder needs access through `UiManager` private state. The final builder can remove them by receiving the already attached `ElementManager&` and `WindowId` from `UiManager::createElement()`:

```cpp
return ElementBuilder<Element>{
    *this,
    *elementManager_,
    window_,
    std::string(elementID),
    sourceLocation,
};
```

Then either:

- make `ElementBuilder<Element>` a narrow friend of `ElementManager`; or
- expose one internal `beginInvocation<Element>()` operation through a private implementation header.

The friend approach is recommended because it keeps invocation methods off the public manager surface and removes a chain of free bridge functions. Friendship should target the builder/invocation templates only, not expose `App*` or the controller.

#### 5.5 Developer tooling

The dev registry in [`registry.hpp`](include/devMode/registry.hpp) currently reads `DefinitionT::ParametersType`, `StateType`, and `ResourcesType`. It should use the same public traits as the manager:

```cpp
descriptor.paramsStructTypeHash = typeHash<ParametersOf<DefinitionT>>();
descriptor.stateStructTypeHash = typeHash<StateOf<DefinitionT>>();
descriptor.resourcesStructTypeHash = typeHash<ResourcesOf<DefinitionT>>();
```

`FLOWUI_DEV_REGISTER_ELEMENT(ElementType, "Name")` can remain unchanged. Dev capture can continue using the definition type hash, definition ID, parameter type, source location, and `isDevInternal` trait.

The Flow-root collision tracker remains under `#if FLOW_UI_DEV_MODE`; only the point from which the builder calls it changes.

### 6. Transitional pieces and their final disposition

| Transitional piece | Final treatment |
| --- | --- |
| Static `ElementDefinition::resources` | Delete after all repository elements use `context.resources()` or `prepare()`. |
| Static `statePool` and state helpers | Delete after all repository elements use `context.state()` and `ElementManager`. |
| Five-parameter `ElementDefinition` class template | Delete; user-authored structs satisfying `FlowElement` replace it. |
| `elementParametersTypeIdentity()` compatibility branches | Replace with final alias detection only. |
| `elementStateTypeIdentity()` / `elementResourcesTypeIdentity()` legacy flags | Replace with optional `State` / `Resources` detection. |
| Definition value `statePolicy` | Replace with optional static constexpr policy trait and default fallback. |
| Callback pointer fields | Delete; hooks are static functions selected with concepts. |
| Five-parameter `ElementBuilder` | Replace with `ElementBuilder<Element>`. |
| Stored `elementDefinition_` pointer | Delete; the element tag is used only for type deduction. |
| `ElementInvocationState` | Fold into a typed RAII invocation lease or keep only as a private controller result. |
| `ElementResourceInvocationState` | Delete after the typed invocation owns lazy resource caching. |
| `FlowUiElementBridge` registration/state/resource functions | Delete after the builder receives direct narrow manager access. |
| UiManager/ElementManager bridge friendships | Replace with friendship for the final builder/invocation template only. |
| Pointer-backed `ElementSet` | Replace with an empty type pack. |
| Dev registry legacy aliases | Replace with `ParametersOf`, `StateOf`, and `ResourcesOf`. |
| Dev element resource initialization helpers | Replace with a dev `elementSet` preparation call during the later dev migration. |

The old and new authoring systems should not coexist in a shipped release. A temporary adapter can keep intermediate commits buildable, but deprecated static stores must not be retained as long-term compatibility because they create a second, unrelated source of state/resource truth.

## Part III — Decisions and staged implementation

### 7. Design choices that need an explicit decision

#### 7.1 Static hooks versus const member hooks

**Option A — static hooks on an empty tag (recommended):**

```cpp
static void buildElement(BuildContext& context);
```

Pros:

- definition object is not stored;
- one type unambiguously means one behavior/resource definition;
- simplest concept detection and `if constexpr` dispatch;
- best compile-time removal of unused behavior.

Cons:

- runtime configuration cannot live on the definition object;
- different fixed variants require different types or ordinary parameters.

**Option B — const member hooks on a constexpr object:**

```cpp
void buildElement(BuildContext& context) const;
```

Pros:

- permits immutable configuration fields in the tag object;
- can create several constexpr presets of one C++ type.

Cons:

- builder must retain an object pointer or copy;
- several values share one type-derived descriptor and resource slot unless value identity is added;
- reintroduces ambiguity between definition identity and object configuration.

Because parameters and themes already represent runtime/configuration variation, static hooks are the stronger fit.

#### 7.2 Pure concept versus convenience base

**Option A — plain struct satisfying a concept (recommended core contract):** no inheritance and maximum author freedom.

**Option B — optional non-polymorphic helper base:**

```cpp
struct ButtonElement : FlowUi::ElementDefaults<ButtonElement> {
    // aliases, metadata, hooks
};
```

A helper could supply context aliases and default metadata, but incomplete-type rules make a CRTP helper easy to overcomplicate. If provided, it should be optional sugar over the same concepts, contain no virtual functions or data, and not become the actual contract.

#### 7.3 Optional alias spelling

Options:

- absent alias means the capability is absent;
- require `using State = void` / `using Resources = void`;
- require sentinel aliases for every type.

Absence is recommended for the final API because it minimizes declaration clutter. The migration traits can temporarily accept `void`.

#### 7.4 Empty tag enforcement

Options:

- enforce `std::is_empty_v<Element>` and trivial/constexpr construction;
- merely document that object fields are ignored;
- allow member-hook/configured definition objects.

Enforcement is recommended if static hooks are chosen. Silently accepting fields that the builder does not retain would be a serious usability trap.

#### 7.5 Main output hook requirements

Options:

- require exactly one of `buildElement` or `constructElement`;
- allow either or both and constrain builder terminal operations accordingly;
- require `buildElement` and treat construct support as an optional extension.

Allowing either or both is recommended because it preserves current capability without runtime null checks.

#### 7.6 Hook naming

Retaining current names is recommended for the first implementation. A rename can be evaluated independently after the system compiles and benchmarks cleanly.

#### 7.7 Context split

Keeping build and interaction contexts separate is recommended. A unified context reduces type count but weakens phase semantics and carries interaction data into elements that never use it.

#### 7.8 Definition ID source

Options:

- continue requiring `FLOW_DEF_ID("stable/name")`;
- derive the ID from a `static constexpr std::string_view name`;
- derive it from the compiler type token.

An explicit stable logical ID is recommended. Compiler type tokens are not a durable cross-toolchain identity, while coupling identity to a display name makes cosmetic renames behavioral changes.

#### 7.9 Incremental versus atomic repository migration

**Atomic cutover:** migrate builder, template, tests, first-party/dev elements, and delete the old definition in one change. This leaves no adapter but creates a large review.

**Short-lived dual dispatch (recommended implementation tactic):** teach the generalized builder to recognize the new static-hook concept while retaining the old pointer path only long enough to migrate repository elements in reviewable batches. Mark it transitional and delete it before the final merge/release.

No external compatibility promise should be built around the adapter; the old state/resource stores are already deprecated and intentionally being removed.

#### 7.10 Runtime/plugin polymorphism

The proposed core is source-level, compile-time polymorphism. If a future binary plugin needs to supply elements whose type is unknown to the application at compile time, it should use a separate explicitly erased adapter/interface. The core header-defined element path should not pay indirect-call and ownership costs for a use case it does not currently have.

### 8. Staged implementation plan

#### Stage A — freeze the final compile-time contract in tests

1. Decide the items in section 7, especially static hooks, empty tags, optional aliases, and hook names.
2. Add compile-only definitions covering:
   - minimal stateless draw-only element;
   - construct-only element;
   - element supporting both terminal operations;
   - parameters only;
   - managed state and static policy;
   - managed `Resources(App&)`;
   - every optional interaction/logic hook;
   - over-aligned and non-copyable state/resources;
   - resource-free members of `elementSet`.
3. Add negative compile fixtures or concept assertions for:
   - missing/zero definition ID;
   - no output hook;
   - wrong hook context or return type;
   - non-`noexcept` payload destructor;
   - non-default-constructible state;
   - non-empty tag if that rule is selected.
4. Preserve compile tests for the existing UI call syntax.

No runtime implementation should be changed until these tests describe the intended authoring contract.

#### Stage B — introduce final traits, concepts, and descriptors

1. Put the final public traits and concepts in a focused header rather than growing the storage structs header indefinitely. A natural location is a new `include/managers/structs/FlowUiElementConcepts.hpp`, re-exported by [`Flow.hpp`](include/FlowUi/Flow.hpp).
2. Implement `ParametersOf`, `StateOf`, `ResourcesOf`, `HasState`, and `HasResources` for the final aliases.
3. Implement defaulted metadata helpers for `statePolicy`, `isDevInternal`, and `debugName`.
4. Implement exact hook concepts and `validateElement<Element>()` diagnostics.
5. Rewrite `makeElementDescriptor()` against those traits.
6. Keep descriptor and payload operations under `detail::element`.
7. Verify current definition-ID collision and payload metadata tests against new authored types.

At this stage the storage controller should not change.

#### Stage C — add the typed invocation lease and update contexts

1. Add internal `ElementInvocation<Element>` RAII ownership.
2. Move automatic descriptor registration into invocation start.
3. Resolve state once only when `HasState<Element>`.
4. Keep the invocation active until every callback context is destroyed.
5. Move lazy resource resolution and cached pointer ownership into the invocation.
6. Update both context types to use public traits and the typed invocation reference.
7. Preserve mutable/const `state()` overloads and const-only `resources()`.
8. Test explicit erase, GC, cancellation, recursive nesting, and window destruction against cached context pointers.

This stage replaces `ElementResourceInvocationState` and most uses of `ElementInvocationState` without changing storage record layout.

#### Stage D — generalize `ElementBuilder` and `UiManager::createElement`

1. Replace `ElementBuilder<Parameters, State, Resources, Id, Dev>` with `ElementBuilder<Element>`.
2. Change both `UiManager::createElement` overloads to deduce any `FlowElement` type.
3. Stop storing the element definition pointer.
4. Preserve `setParameters`, `mergeParams`, `withElementID`, dev capture override, `draw`, and `construct` syntax.
5. Implement optional hook dispatch with `if constexpr`.
6. Compile out unused interaction snapshots, contexts, state, resources, and dev code.
7. Consolidate common draw/construct orchestration.
8. Constrain terminal methods by output-hook capability.
9. Route direct manager/window access through the narrow friendship selected in section 5.4.
10. Keep `std::string` ID ownership until separate profiling justifies a change.

#### Stage E — update resource sets and developer registration

1. Convert `ElementSet` to a type-only pack.
2. Keep both `prepare()` overloads and resource-free no-op behavior.
3. Update dev registry metadata to use the final public traits.
4. Keep all Flow ID tracking storage and code under `FLOW_UI_DEV_MODE`.
5. Update dev capture to use final policy/identity helpers without depending on legacy aliases.

#### Stage F — update `template.hpp` and focused examples

1. Replace [`template.hpp`](template.hpp) with the shape in section 4.
2. Remove positional callback/null examples and every deprecated state/resource example.
3. Add one small nested-element example proving `createChildElementId()` and recursive `createElement()` usage remain unchanged.
4. Compile the template in production and dev configurations.

This is the first user-facing artifact that should switch, after the new concepts and builder are operational.

#### Stage G — migrate first-party and dev elements

1. Convert each old definition alias/object pair into one authored element type plus one constexpr tag.
2. Convert non-null callback lambdas to static hooks; omit every old `nullptr` slot.
3. Replace all `getOrCreateState()` calls with `context.state()`.
4. Replace direct static resource optionals with `context.resources()`.
5. Create an internal dev `elementSet` for eager resource preparation where necessary.
6. Update nested callback parameter aliases that currently refer to `Definition::InteractionContext`.
7. Convert [`debugView.cpp`](src/devMode/debugView.cpp) and any non-header first-party definition.
8. Compile after each logical element family to keep migration errors local.

The migration is primarily mechanical, but state access must be reviewed because old static pools did not include `WindowId` while managed state does.

#### Stage H — delete compatibility code

1. Delete the old `ElementDefinition` class template.
2. Delete static `resources`, `statePool`, and all deprecated helper functions.
3. Delete callback pointer fields and the legacy builder dispatch branch.
4. Delete old alias/flag normalization paths.
5. Delete the stored definition pointer and pointer-backed `ElementSet` implementation.
6. Delete `ElementResourceInvocationState` and obsolete bridge declarations/definitions.
7. Reduce `UiManager` and `ElementManager` friendships to the final typed builder/invocation surface.
8. Remove now-unused `<optional>`, `<vector>`, and compatibility includes.
9. Add repository searches in review/CI that reject old APIs and `ElementDefinition<` usage.

No shipped build should contain both the manager-owned and static definition-owned payload stores.

#### Stage I — verification and performance baseline

1. Run production and dev builds.
2. Run all state/resource lifecycle and failure-injection tests.
3. Add dispatch-order tests for every combination of optional hooks and draw options.
4. Verify draw-only elements cannot call `.construct()` and construct-only elements cannot call `.draw()`.
5. Verify nested elements and constructed elements keep dev capture balanced on exceptions.
6. Compare generated code or microbenchmarks for:
   - elements with only `buildElement`;
   - fully interactive stateful elements;
   - deeply nested reusable elements;
   - large repeated lists;
   - dev-off builds.
7. Benchmark element ID string ownership separately before changing it.
8. Update broad tutorials/API documentation only after the final syntax and diagnostics are accepted.

### 9. Expected result

After the migration, element authors define behavior as ordinary static C++ functions in a shareable type. The compiler determines the exact capabilities of that type, and `ElementBuilder` emits only the relevant phases. UI composition remains:

```cpp
ui.createElement(kElement, "logical/id")
    .setParameters(...)
    .draw();
```

State and resources remain centralized, multi-window-safe, garbage-collected, and app-owned through the existing manager/controller architecture. The redesign removes runtime callback flexibility that the library does not use, without removing the runtime data and lifetime services that Flow elements require.
