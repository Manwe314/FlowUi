# Element System

## Introduction

FlowUi's element system is a C++ wrapper pattern for building reusable UI blocks on top of Clay. Clay already makes reuse possible because a Clay UI block is just code that emits layout nodes during the frame. FlowUi keeps that immediate-mode shape, but adds a standard typed definition object, a builder API, root interaction callbacks, per-instance state storage, shared resources, and developer-mode capture hooks. The result is still Clay-driven UI, but reusable blocks have a consistent structure that FlowUi can invoke, inspect, configure, and share.

## TL;DR

A Flow element is a typed reusable UI definition that eventually emits one root Clay element and usually a small Clay subtree below it. `ElementDefinition<Params, State, Resources, Id>` describes the shape and callbacks, while `UiManager::createElement(...).setParameters(...).draw()` or `.construct()` invokes that definition during a frame.

Params are per-frame configuration, state is persistent per instance, and resources are shared per definition. This gives reusable UI blocks a standard home for immediate-mode inputs, instance-specific memory, and expensive app-lifetime data.

## Conceptual Idea Behind Flow Elements

A normal Clay element is created with a macro shape like:

```cpp
CLAY(rootId, declaration) {
    // child Clay nodes here
}
```

That macro is a convenient wrapper around Clay's element API. The declaration carries classic UI configuration: sizing, layout direction, padding, border, background color, corner radius, image data, and other values that describe how this node participates in the UI tree.

If you only use Clay directly, reusable UI can be as simple as writing a function:

```cpp
struct SimpleCardConfig {
    Clay_ElementId id{};
    Clay_Color background{};
    Clay_String title{};
};

void drawSimpleCard(const SimpleCardConfig& config) {
    Clay_ElementDeclaration root{};
    root.layout.sizing = {
        .width = CLAY_SIZING_GROW(0),
        .height = CLAY_SIZING_FIT(0),
    };
    root.layout.padding = CLAY_PADDING_ALL(12);
    root.backgroundColor = config.background;
    root.cornerRadius = CLAY_CORNER_RADIUS(8);

    CLAY(config.id, root) {
        Clay_TextElementConfig text{};
        text.fontSize = 16;
        text.textColor = Clay_Color{255, 255, 255, 255};
        CLAY_TEXT(config.title, CLAY_TEXT_CONFIG(text));
    }
}
```

Then reuse is just a function call:

```cpp
drawSimpleCard({
    .id = ui.toClayEID("sidebar/status-card"),
    .background = FlowUi::Flow_Color("#20242cff"),
    .title = ui.toClayString("Status"),
});
```

This is fast, simple, and close to Clay. For many small UI blocks, this style is perfectly reasonable.

The problem appears when reusable blocks become larger and more behavioral. A function argument list can grow for colors, font ids, icons, images, sizing, flags, callbacks, and data. The function still does not have a standard place for hover/press/release behavior. Persistent state has to be stored somewhere else and manually keyed by id. Shared resources have to be created somewhere else and passed in or captured. Sharing one reusable block with another user is also less standardized because every function can have a different shape and lifecycle expectation.

FlowUi's element system exists to help with those larger-picture issues. It does not make Clay nodes reusable; Clay already makes that easy. FlowUi makes reusable Clay blocks easier to manage, invoke, inspect, and standardize as an application grows.

The first step is turning what would have been an arbitrary function into a data-driven object with a known shape:

```cpp
using StatusCardDefinition = FlowUi::ElementDefinition<
    StatusCardParams,
    StatusCardState,
    StatusCardResources,
    FLOW_DEF_ID("status_card")>;

inline const StatusCardDefinition kStatusCard = {
    // callbacks go here
};
```

Because `ElementDefinition` is an object with a standard type shape, `UiManager::createElement()` can invoke it through `ElementBuilder`. That means user code can build any Flow element with the same surface:

```cpp
app.ui()
    .createElement(kStatusCard, "sidebar/status-card")
    .setParameters(StatusCardParams{.title = "Status"})
    .draw();
```

The element definition gives FlowUi two major conveniences. First, FlowUi can manage root interaction callbacks for the element. You can write `onHovered`, `onPressed`, `onHeld`, and `onReleased` callbacks, and the builder calls them when the element's root Clay node appeared in the previous frame's interaction snapshot.

Second, FlowUi gives element data a standard shape. What would have become a growing function argument list is split into three user-defined structs: params, state, and resources.

## ElementDefinition Shape

The public definition type is:

```cpp
template <
    typename Parameters = NoElementParameters,
    typename State = void,
    typename Resources = void,
    uint64_t DefinitionId = 0,
    bool IsDevInternal = false>
struct ElementDefinition;
```

In normal user code, this becomes a named alias:

```cpp
using ToggleDefinition = FlowUi::ElementDefinition<
    ToggleParams,
    ToggleState,
    ToggleResources,
    FLOW_DEF_ID("toggle")>;
```

Internally, this specialization owns static storage for its state pool and resources:

```cpp
static inline std::optional<ResourcesType> resources{};
static inline std::vector<StatePoolEntry> statePool{};
```

That storage belongs to the definition specialization, not to the `inline const` variable by itself. This is why two instances of the same definition share the same resources container and state pool shape, while individual element instances are separated by `FlowElementId`.

`ElementDefinition` also defines callback context types:

```cpp
using BuildContext = ElementBuildContext<Parameters>;
using InteractionContext = ElementInteractionContext<Parameters>;
```

`BuildContext` is passed to `constructElement` and `buildElement`. It exposes `uiManager`, `elementID`, `params`, and `createChildElementId()`.

`InteractionContext` is passed to event callbacks and `runLogic`. It exposes the same element id and params, plus `previousInteraction`, which is the interaction snapshot captured at the end of the previous frame.

## Parameters

`Params` are the per-frame configuration for an element invocation. They are the direct replacement for most function arguments in a plain Clay helper function.

```cpp
struct ToggleParams {
    std::string_view label = "Enabled";
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIXED(220.0f),
        .height = CLAY_SIZING_FIXED(40.0f),
    };
    FlowUi::FontId fontId = 0;
    Clay_Color enabledColor = FlowUi::Flow_Color("#22c55eff");
    Clay_Color disabledColor = FlowUi::Flow_Color("#374151ff");
};
```

The more values you place in params, the more configurable and data-driven the element becomes. That is useful for reusable controls, themed components, and developer-mode visual iteration. It is also optional: total configurability is not always desirable. Some elements should intentionally hide implementation details and expose only the few values the caller should control.

Parameters are normally rebuilt each frame:

```cpp
ui.createElement(kToggle, "settings/grid")
    .setParameters(ToggleParams{.label = "Show grid", .fontId = bodyFont})
    .draw();
```

For trivial data like integers, floats, colors, ids, and small structs, this is usually cheap. But parameters are still per-frame data. If a UI has many elements and each params struct constructs heavy containers, parses data, or allocates memory every frame, the cost can become visible. In that case, keep heavy long-lived data elsewhere and pass lightweight handles, ids, spans, or references through params.

`ElementBuilder` supports both copying and moving params:

```cpp
ToggleParams params{};
params.label = computeDynamicLabel();

ui.createElement(kToggle, "settings/grid")
    .setParameters(std::move(params))
    .draw();
```

Moving can help when the params object already exists and owns data, but it does not change the conceptual lifetime: params belong to this one element invocation in this one frame.

## State

`State` is persistent per-instance data for one element definition. It is keyed by `FlowElementId`, usually derived from the string id passed to `createElement()`.

```cpp
struct ToggleState {
    bool enabled = false;
};
```

Inside a callback, state is commonly accessed like this:

```cpp
ToggleState& state = ToggleDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
```

This is useful for element-specific interaction memory: whether a toggle is enabled, the current drag value of a slider, whether a disclosure panel is open, or the current row/column view state of a reusable table container.

State is not intended to replace core application storage. Application-wide document data, business state, loaded project data, and persistent model data should still live in your app. Element state is for UI-instance behavior that naturally belongs to a reusable visual block.

Because FlowUi and Clay are immediate-mode systems, state must be used carefully. If the same string id is reused for two different logical instances, they will share the same state entry. If a dynamic element disappears forever and its state is no longer needed, use `eraseState()` to remove it. FlowUi does not automatically know when a user-defined state entry is no longer meaningful for your app.

The lookup helpers reflect those choices:

```cpp
ToggleState& state = ToggleDefinition::getOrCreateState(FLOW_ID("settings/grid"));
ToggleState* maybeState = ToggleDefinition::tryGetState(FLOW_ID("settings/grid"));
const ToggleState* readOnlyState = ToggleDefinition::tryGetStateConst(FLOW_ID("settings/grid"));
const bool erased = ToggleDefinition::eraseState(FLOW_ID("settings/grid"));
```

## Resources

`Resources` are shared per definition, not per instance and not per frame. They are the place for long-lived data that is expensive or unnecessary to recreate every frame and is useful to every instance of the same element definition.

```cpp
struct ToggleResources {
    std::array<Clay_Color, 2> trackColors{
        FlowUi::Flow_Color("#374151ff"),
        FlowUi::Flow_Color("#22c55eff"),
    };

    explicit ToggleResources(FlowUi::App& app) {
        (void)app;
    }
};
```

Resources are lazily initialized through:

```cpp
ToggleResources& resources = ToggleDefinition::getResources(app);
```

The constructor selection prefers `ResourcesType(App&)`, then `ResourcesType(UiManager&)`, then a default constructor. This lets a resource object use app-owned managers during setup when needed.

Good resource candidates are constant palettes, precomputed lookup tables, shared icon keys, cached handles, or definition-wide data that should be created once and reused. A resource is a poor fit for data that changes every frame, differs for every instance, or belongs to the application model rather than the element definition.

The important distinction is:

- Params: built or supplied every frame for one invocation.
- State: persistent data for one element instance.
- Resources: persistent data shared by all instances of one definition.

## Callback Flow and Interaction

`ElementDefinition` has seven callback fields:

```cpp
void (*onHovered)(InteractionContext&) = nullptr;
void (*onPressed)(InteractionContext&) = nullptr;
void (*onHeld)(InteractionContext&) = nullptr;
void (*onReleased)(InteractionContext&) = nullptr;
void (*runLogic)(InteractionContext&) = nullptr;
Clay_ElementDeclaration (*constructElement)(BuildContext&) = nullptr;
void (*buildElement)(BuildContext&) = nullptr;
```

When `ElementBuilder::draw()` runs, the internal order is:

```text
convert element id to root Clay id
begin developer capture when enabled
run root interaction callbacks from previousInteraction
run runLogic when present
apply dev-mode parameter overrides when enabled
call buildElement
end developer capture when enabled
```

`construct()` follows the same event and logic path, but calls `constructElement` instead of `buildElement`, opens the root Clay element, and leaves it open until `UiManager::drawConstructed()`.

The event callbacks are root-level conveniences. FlowUi checks whether the element's root Clay id was hovered, pressed, held, or released in the previous completed frame, then calls the matching callback if it exists. This means you do not need to manually query the root id for the most common interaction events.

For child nodes, custom hit targets, or cross-element logic, use `context.previousInteraction` directly:

```cpp
+[](ToggleDefinition::InteractionContext& context) {
    const Clay_ElementId knobId = context.uiManager.toClayEID(context.createChildElementId("knob"));
    if (context.previousInteraction.isHeld(knobId)) {
        ToggleState& state = ToggleDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        state.enabled = true;
    }
}
```

## ElementBuilder

`ElementBuilder` is the object returned by `UiManager::createElement()`. It stores:

- the active `UiManager`
- a pointer to the element definition
- the element id string
- the current params object
- developer capture state when dev mode is enabled

The common chain is:

```cpp
ui.createElement(kToggle, "settings/grid")
    .setParameters(ToggleParams{.label = "Show grid", .fontId = bodyFont})
    .draw();
```

`setParameters()` replaces the stored params by copy or move. Use it when the caller knows the full params object.

```cpp
ui.createElement(kToggle, "settings/grid")
    .setParameters(ToggleParams{.label = "Show grid"})
    .draw();
```

`mergeParams()` mutates the builder's existing default params. Use it when defaults are mostly correct and only a few fields need changing.

```cpp
ui.createElement(kToggle, "settings/grid")
    .mergeParams([&](ToggleParams& params) {
        params.label = "Show grid";
        params.fontId = bodyFont;
    })
    .draw();
```

`withElementID()` replaces the stored element id before the element is emitted. This is useful when a builder is configured before the final id is known.

```cpp
auto rowToggle = ui.createElement(kToggle, "pending-row-toggle")
    .setParameters(ToggleParams{.label = "Visible"});

rowToggle.withElementID(rowIsPinned ? "rows/pinned/visible" : "rows/normal/visible").draw();
```

Storing an `ElementBuilder` in a local variable and reusing it during the same UI construction pass is a valid FlowUi pattern. This can be useful when several call sites share the same definition and mostly the same params, but need different stable ids. In that case, configure the builder once, then call `withElementID()` before each final builder action so each emitted instance gets its own Flow/Clay root identity.

```cpp
auto visibleToggle = ui.createElement(kToggle, "pending-visible-toggle")
    .setParameters(ToggleParams{.label = "Visible", .fontId = bodyFont});

visibleToggle.withElementID("rows/header/visible").draw();
visibleToggle.withElementID("rows/footer/visible").draw();
```

The important rule is that the id must be set before `draw()` or `construct()`. Those calls consume the builder's current element id when they run interaction callbacks, dev capture, state lookup, and root Clay id conversion.

`draw()` is the normal closed element flow. It requires `buildElement` to be set, and the callback emits the full Clay subtree.

`construct()` is the open container flow. It requires `constructElement` to be set, opens the root element, and expects user code to emit children before calling `drawConstructed()`.

```cpp
ui.createElement(kPanel, "settings")
    .setParameters(PanelParams{.title = "Settings"})
    .construct();

ui.createElement(kToggle, "settings/grid")
    .setParameters(ToggleParams{.label = "Show grid"})
    .draw();

ui.drawConstructed();
```

If constructed elements are left open at `UiManager::endFrame()`, FlowUi auto-closes them and prints a warning. That helps keep Clay layout balanced, but user code should still close constructed elements intentionally.

## Drawing Elements During a Frame

Flow elements are built between `app.beginFrame()` and `app.endFrame()`.

```cpp
while (!app.shouldClose()) {
    app.beginFrame();

    app.ui()
        .createElement(kToggle, "settings/grid")
        .setParameters(ToggleParams{.label = "Show grid"})
        .draw();

    app.endFrame();
    app.drawFrame();
}
```

This placement matters because the builder depends on frame-local systems:

- Clay layout must be active.
- `UiManager` frame arenas must be active for strings and texture refs.
- Previous interaction snapshots must be available.
- Dev capture must be inside a frame.
- Input field and shortcut managers must be using current frame input.

Calling element builders outside the active frame does not match the runtime model. Elements are immediate-mode UI descriptions, so they belong in the UI construction phase.

## Root IDs and Child IDs

The element id passed to `createElement()` should be treated as the identity of the Flow element instance and the identity of the root Clay node that the element builds.

```cpp
ui.createElement(kToggle, "settings/grid").draw();
```

Inside `buildElement`, the root Clay node should use the same id:

```cpp
CLAY(context.uiManager.toClayEID(context.elementID), root) {
    CLAY(context.uiManager.toClayEID(context.createChildElementId("label")), labelBox) {}
}
```

This convention connects Flow state, interaction callbacks, developer capture, and Clay layout. The root id is how FlowUi decides whether `onHovered`, `onPressed`, `onHeld`, or `onReleased` should fire.

For children, use `createChildElementId()`. It appends a local child name to the element id, which keeps child ids stable but scoped to the parent.

```cpp
std::string labelId = context.createChildElementId("label");
std::string iconId = context.createChildElementId("icon");
```

For repeated elements, use stable names or indexed id helpers. Do not reuse the same id for different logical rows unless they intentionally share state and interaction identity.

## A Compact Full Example

This example shows the shape of a small toggle element. It uses params for configuration, state for the current enabled value, resources for shared colors, `onPressed` for root interaction, and `buildElement` to emit Clay nodes.

```cpp
struct ToggleParams {
    std::string_view label = "Toggle";
    FlowUi::FontId fontId = 0;
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIXED(220.0f),
        .height = CLAY_SIZING_FIXED(40.0f),
    };
};

struct ToggleState {
    bool enabled = false;
};

struct ToggleResources {
    Clay_Color offColor = FlowUi::Flow_Color("#374151ff");
    Clay_Color onColor = FlowUi::Flow_Color("#22c55eff");
};

using ToggleDefinition = FlowUi::ElementDefinition<
    ToggleParams,
    ToggleState,
    ToggleResources,
    FLOW_DEF_ID("toggle")>;

inline const ToggleDefinition kToggle = {
    nullptr,
    +[](ToggleDefinition::InteractionContext& context) {
        ToggleState& state = ToggleDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        state.enabled = !state.enabled;
    },
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    +[](ToggleDefinition::BuildContext& context) {
        ToggleState& state = ToggleDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        ToggleResources& resources = ToggleDefinition::resources.value();

        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.layout.childAlignment = {.y = CLAY_ALIGN_Y_CENTER};
        root.layout.padding = CLAY_PADDING_ALL(10);
        root.backgroundColor = state.enabled ? resources.onColor : resources.offColor;
        root.cornerRadius = CLAY_CORNER_RADIUS(8);

        CLAY(context.uiManager.toClayEID(context.elementID), root) {
            Clay_TextElementConfig text{};
            text.fontId = context.params.fontId;
            text.fontSize = 14;
            text.textColor = Clay_Color{255, 255, 255, 255};
            CLAY_TEXT(context.uiManager.toClayString(context.params.label), CLAY_TEXT_CONFIG(text));
        }
    },
};
```

Before drawing, initialize resources once:

```cpp
(void)ToggleDefinition::getResources(app);
```

Then draw it during the frame:

```cpp
app.ui()
    .createElement(kToggle, "settings/grid")
    .setParameters(ToggleParams{.label = "Show grid", .fontId = bodyFont})
    .draw();
```

In a real app, the enabled value might be mirrored into application state or driven by application state instead. The example uses element state only to show where per-instance UI behavior can live.

## What to Read Next

- [Core Mental Model](mental_model.md)
- [Frame Lifecycle](frame_lifecycle.md)
- [Managers](managers.md)
- [Element API](../api/elements.md)
