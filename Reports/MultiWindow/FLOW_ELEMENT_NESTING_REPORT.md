# Scaling Nested Flow Elements

## Purpose

This report examines how FlowUi can keep its current typed, per-frame parameter structs for simple elements while making deeply nested compound elements easier to configure, reuse, and share.

The examples below describe possible future APIs. They are intentionally illustrative rather than detailed implementation proposals.

## Executive summary

The current system does **not** technically require a parent to contain every descendant's parameter struct. A parent can construct its children with internal defaults, and the existing code often does exactly that. The scaling problem appears when the caller of the outer component must be able to customize those descendants. Today, every such value has to be manually exposed and forwarded through every ancestor. This is the familiar problem of **prop drilling**.

FlowUi should keep the existing `Params` mechanism as the fast path, but stop asking `Params` to represent all of these different concerns at once:

| Concern | Best home |
|---|---|
| Per-frame data and high-level behavior for this element | `Params` |
| Persistent behavior of one element instance | `State` |
| Expensive definition-wide assets | `Resources` |
| Styling and services shared by a whole subtree | A typed inherited context/theme |
| Replaceable sections of a compound component | Named typed slots |
| Rare, precise overrides of deep internal parts | A cached named-part preset |

The three options below are independently adoptable mechanisms rather than three complete replacements for the current system. The strongest overall direction is a layered model:

1. Keep direct typed params unchanged for leaves and shallow composition.
2. Give compound elements small **semantic params** instead of mirrors of their descendants.
3. Add a **typed cascading theme/context** for normal cross-cutting styling and services.
4. Add **named slots** for structural and behavioral customization.
5. Add **named-part presets** only as an advanced escape hatch for surgical deep overrides.

If only one addition is built first to address the pressure visible in the current code, typed theme/style context will remove the most forwarding immediately. Named slots are the next architectural step for third-party composition and structural replacement. Named-part presets should remain an explicit advanced feature so that every private child does not accidentally become public API.

## What the current system does well

The present model has several properties worth preserving:

- `ElementDefinition<Parameters, State, Resources, ...>` gives every element a concrete, compile-time-known data shape. See `include/managers/structs/FlowUiElementStructs.hpp:299-320`.
- `ElementBuilder` owns one concrete `ParametersType` for one invocation. `setParameters()` copies or moves it, while `mergeParams()` mutates it directly. See `include/managers/FlowUiElementBuilder.hpp:101-179` and `:276-280`.
- Drawing is synchronous and immediate. Params are passed by reference through interaction, logic, and build callbacks without virtual dispatch or a mandatory property lookup. See `include/managers/FlowUiElementBuilder.hpp:384-470`.
- State remains associated with a definition and stable element ID, while resources are cached once per definition specialization. Deeply nested elements already get stable scoped identities through `createChildElementId()`. See `include/managers/structs/FlowUiElementStructs.hpp:180-193` and `:354-463`.
- Every child is still a normal Flow element invocation. It gets its own params and identity, participates in its definition's optional state and shared-resource policies, and keeps its own interaction and developer-mode capture. State currently persists until explicitly erased; it is not automatically garbage-collected.
- `construct()` and `drawConstructed()` already prove that caller-owned composition fits the immediate-mode model. They provide an open container, although they do not yet provide named internal slots.

These qualities make the leaf-element experience simple and performant. A nesting redesign should be additive rather than replacing this path with a mandatory runtime tree, generic property bag, or virtual component hierarchy.

## Where composition currently stops scaling

Current compound elements manually create a new typed params object for each child. That is reasonable by itself:

- The color-picker tutorial keeps `ColorPickerParams` fairly small, creates three `ColorChannelRowParams` values, and each row creates slider and input params. See `docs/tutorials/custom_elements.md:315-405` and `:417-505`.
- `mainDevViewParams` exposes only a few outer controls while its header, content, and footer mostly use their own defaults. See `include/devMode/devFlowElements/devMainView.hpp:8-63`.

The problem appears when more internal controls must become customizable:

- `devHeaderParams` contains a long group of `exportButton...` fields, then manually maps them into `devBasicButtonParams`. See `include/devMode/devFlowElements/devHeader.hpp:6-47` and `:113-150`.
- `devFloat4InputParams` carries general layout, typography, four input sizes, nine-split sizes, color-editor sizes, slider dimensions, and more. Its build callback then remaps these values into sliders, numeric fields, a basic input, and a nine-split editor. See `include/devMode/devFlowElements/devFloat4Input.hpp:93-171` and its child construction beginning around `:404`.
- `devPropertiesParams` manually promotes selected head and content values and maps them down one level. Adding a new leaf option at a deeper level would require adding and forwarding it through every intermediate params type. See `include/devMode/devFlowElements/devProperties.hpp:7-72`.

The developer UI also contains a conditional chain approximately ten Flow elements deep:

```text
MainView -> PanelContent -> Properties -> PropertiesContent -> PropertiesCard
         -> CompositeStructInput -> TaggedUnionInput -> Float2Input
         -> NumericInput -> BasicInputField
```

Typography is manually forwarded through much of this chain, while other leaf settings stop at an intermediate element and are unreachable from above. That is the exact failure mode the upgraded system needs to address: not rendering depth, but selective configuration becoming more expensive with every level.

That leaves component authors with four unsatisfactory choices:

1. Hide deep values behind hard-coded defaults.
2. Promote selected leaf fields through every ancestor.
3. Embed complete child parameter structs, causing the outer struct to mirror the tree.
4. Give up encapsulation and require the caller to manually build the entire subtree.

The resulting costs are broader than the size of one struct:

- A leaf rename or new option causes edits across multiple components.
- Outer components become coupled to the exact implementation types of their descendants.
- Large structs may repeatedly default-construct, copy, or move owning strings, vectors, and `std::function` objects each frame.
- A shared component's public API changes when an internal child changes.
- Visual options, application dependencies, data, and event callbacks become mixed into one flat parameter namespace.
- Developer-mode registration and source patching must understand an ever-growing facade rather than the leaf where a value is consumed.

State and resources are not good substitutes. They have different lifetimes and ownership: state represents persistent instance behavior, while resources represent cached definition-wide assets. The missing concepts are **subtree scope** and **composition contract**.

## Design goals

Any upgrade should aim for the following:

- Preserve `createElement(...).setParameters(...).draw()` as the normal leaf and shallow-element path.
- Keep params concrete and typed at the point where they are consumed.
- Let an outer element expose a stable semantic contract without publishing its entire private child tree.
- Make common styling and shared behavior available at arbitrary depth without forwarding it through every ancestor.
- Allow explicit structural replacement and precise deep overrides when an application genuinely needs them.
- Preserve stable child IDs for state, interaction, and developer capture.
- Avoid mandatory heap allocation, type erasure, and map lookup on every element in the common path.
- Let developer tooling inspect the final resolved params of each leaf and explain where an effective value came from.
- Allow existing components to migrate one at a time.

## Option 1: Typed named slots

### Idea

A compound element publishes a small set of stable, typed **slots** such as `Header`, `Body`, `EmptyState`, or `Actions`. Each slot has default content, but a caller can replace it with a synchronous render callback. The callback receives a scoped child builder/ID context so anything drawn inside it remains correctly nested and uniquely identified.

The compound's ordinary params contain only its semantic input and high-level actions. Child params are created where the child is actually drawn instead of being carried through the outer params.

This generalizes the idea behind `construct()` without forcing the caller to own the complete outer layout. The compound still owns its root and its default structure; the caller owns only the named sections it chooses to replace.

### Illustrative user example

```cpp
ui.createElement(kSettingsPanel, "settings")
    .setParameters(SettingsPanelParams{
        .model = &settings,
    })
    .slot<SettingsPanelSlots::Actions>([&](auto& slot) {
        slot.createElement(kButton, "save")
            .setParameters(ButtonParams{
                .text = "Save changes",
                .onPressed = saveSettings,
            })
            .draw();
    })
    .draw();
```

With no `.slot(...)` call, `kSettingsPanel` draws its standard actions. Slots can themselves contain components with further slots, so deep composition scales recursively without making `SettingsPanelParams` describe the whole tree.

For performance, the slot callback can be a synchronous callable whose lifetime only needs to cover the immediate `.draw()` call. The design should not require persistent owning callbacks or a heap allocation for every slot.

### Pros

- Removes prop drilling for content and behavior, not just appearance.
- Keeps descendant params next to the descendant invocation that consumes them.
- Gives shared components an explicit, documented extension contract.
- Typed slot tags can validate callback signatures and expose typed slot data at compile time. They cannot fully police every element an arbitrary render callback chooses to emit.
- Default slots preserve a very small normal call site.
- Compound internals remain private except for deliberately published slots.
- Fits FlowUi's synchronous immediate-mode execution and existing scoped child IDs.
- Can be introduced additively, beginning with a safer scoped/RAII form of the existing construct flow.

### Cons

- A heavily customized call site will visibly contain more UI construction code.
- Replacing a large slot may be excessive when the caller only wants to change one color or padding value.
- Slot layout expectations and provided data must be documented and versioned.
- Arbitrary lambdas require either a templated composed-builder shape, non-owning callable views, or carefully bounded type erasure.
- Dynamic/repeated slots need clear rules for keys and stable child IDs.
- Too many tiny slots can recreate the same giant public surface in another form.

### Best fit

Use slots when the caller may replace content, choose a child implementation, inject application-specific behavior, or add/remove sections. Slots are the strongest option for making compound elements reusable across users without requiring everyone to accept the same internal content.

## Option 2: Typed cascading theme and context

### Idea

Add a frame-local, inherited context stack to `UiManager`/`BuildContext`. A subtree can provide typed values, and any descendant can request them without intermediate parents knowing about them.

Two categories are especially useful:

- **Theme roles**, such as `PanelSurface`, `BodyText`, `PrimaryAction`, `CompactInput`, or `DangerAction`.
- **Services/controllers**, such as `SettingsCommands`, selection models, localization, or accessibility settings.

This is dependency injection scoped to an immediate-mode subtree, not global mutable state. Params continue to carry element-specific data; the context carries cross-cutting values shared by many descendants.

### Illustrative user example

```cpp
const auto compactDark = FlowUi::Theme{}
    .set(roles::PanelSurface, PanelStyle{
        .background = FlowUi::Flow_Color("#20242cff"),
        .gap = 8,
    })
    .set(roles::PrimaryAction, ButtonStyle{
        .background = FlowUi::Flow_Color("#4f7cff"),
        .padding = CLAY_PADDING_ALL(6),
    });

ui.withTheme(compactDark, [&] {
    ui.withContext(SettingsCommands{
        .save = saveSettings,
        .reset = resetSettings,
    }, [&] {
        ui.createElement(kSettingsPanel, "settings")
            .setParameters(SettingsPanelParams{.model = &settings})
            .draw();
    });
});
```

A save button several levels below the panel resolves `roles::PrimaryAction` and the settings command service directly. The panel, section, row, and action-bar params do not repeat the font, colors, padding, or command callbacks.

### Pros

- Eliminates the most common repeated forwarding: colors, fonts, spacing, density, accessibility values, localization, and shared commands.
- One scoped change consistently affects an arbitrarily deep subtree.
- Typed values and namespaced roles are safer than string-keyed property bags.
- Themes can be built once and shared as reusable dark, compact, accessible, or product-specific packs.
- A context can be represented by lightweight immutable handles and cached typed lookups.
- Parents no longer need to know which deep descendants consume a shared value.
- Existing params can remain available as explicit local overrides during migration.

### Cons

- Does not by itself replace structure or pass unique per-child data.
- Ambient dependencies are less visible at a call site than explicit params.
- The library must define clear lookup, shadowing, and override precedence.
- Shared component authors must agree on stable, namespaced semantic roles or adapters.
- Current default-valued style fields cannot distinguish "the caller explicitly chose this value" from "use the theme". A clean design will likely separate semantic props from style/style-patch values.
- Targeting only one particular deep child is awkward unless the component publishes a role or part for it.
- A naive dynamic type map lookup on every node would erode the current fast path.

### Best fit

Use cascading context for values that naturally apply to many descendants. This is the best single answer to visual-style growth and repeated service/callback forwarding, but it should be paired with slots or parts for exceptional structure and one-off leaf changes.

## Option 3: Sparse typed named-part presets

### Idea

A compound element publishes selected stable **part handles** for descendants that are safe to customize, for example `SettingsPanelParts::SaveButton` or `ColorPickerParts::ChannelInput`. Each handle encodes both a stable part ID and the expected params/style type.

A caller builds a sparse preset containing only exceptional overrides. The outer builder attaches one lightweight preset view. As nested children are constructed, the system routes matching patches to the relevant child before it runs interaction and build callbacks.

Presets should normally be built or frozen once and stored at application/resource lifetime. Passing a small immutable view each frame avoids constructing a giant nested params object or allocating an override map on every draw.

Raw paths such as `"footer/actions/save"` could exist as a debugging escape hatch, but they should not be the main public API. Typed part handles allow a component to preserve a public part while refactoring its private internal path.

### Illustrative user example

```cpp
const auto compactSettings = FlowUi::ElementPreset<SettingsPanelDefinition>{}
    .edit(SettingsPanelParts::SaveButton, [](ButtonParams& params) {
        params.text = "Apply";
        params.padding = CLAY_PADDING_ALL(6);
    })
    .edit(SettingsPanelParts::SearchField, [](TextFieldParams& params) {
        params.placeholder = "Filter settings";
    })
    .freeze();

ui.createElement(kSettingsPanel, "settings")
    .setParameters(SettingsPanelParams{
        .model = &settings,
        .onSave = saveSettings,
    })
    .withPreset(compactSettings)
    .draw();
```

The panel's semantic `onSave` remains part of its stable contract, while the preset changes the exact nested presentation. Advanced per-invocation presets could also patch callbacks, but portable/shared presets should generally contain data, styles, variants, or command IDs rather than captured lambdas.

### Pros

- Allows precise changes at any published depth while keeping root params small.
- A preset can be reused across many instances and distributed with a component library.
- Typed part handles prevent applying the wrong params type to a descendant.
- Sparse storage means the cost and memory can scale with actual overrides rather than total tree size.
- Frozen presets can be indexed once and passed as lightweight handles every frame.
- Supports branded, compact, accessible, or application-specific variants without copying full parameter trees.
- The no-preset path can remain the current direct path with little or no extra work.

### Cons

- Every published part becomes a versioned compatibility promise and reduces freedom to replace internals.
- Users making many deep patches still need to understand part of the internal component structure.
- Dynamic lists require stable item keys, indexed-part rules, or a selector model.
- Implementation requires typed routing infrastructure and clear precedence rules at preset boundaries.
- Building presets every frame, or storing many owning callbacks in them, would undermine the performance goal.
- String paths would be brittle and could turn refactors into silent failures.
- Developer-mode source export becomes more complex because an effective leaf value may originate in an outer preset rather than the child's local `.setParameters()` expression.

### Best fit

Use presets as a deliberate escape hatch for stock complex components whose default tree should remain intact but whose selected deep parts must be customizable. They are especially attractive for reusable theme packs, design-tool output, and application-level component variants.

## Comparison

| Question | Named slots | Cascading theme/context | Named-part presets |
|---|---|---|---|
| Main problem solved | Structural and behavioral composition | Cross-cutting style and dependencies | Surgical deep overrides |
| Keeps outer params small | Yes | Yes | Yes |
| Replaces child structure | Yes | No | Usually no |
| Styles an entire deep tree | Indirectly and verbosely | Excellent | Good for exceptions |
| Changes one exact internal leaf | By replacing its containing slot | Weak unless it has a role | Excellent if published as a part |
| Reusable across users | Strong slot contracts | Strong shared roles/themes | Strong versioned presets |
| Common-path runtime cost | Very low with synchronous callables | Very low with cached context handles | Very low without a preset; indexed lookup with one |
| Main architectural risk | Too many slots or callable complexity | Hidden dependencies/cascade ambiguity | Making private internals permanent public API |

No single option should be stretched to solve every category. A theme should not become a generic bag of arbitrary per-instance data; slots should not be created for every color; and presets should not expose every internal child by default.

## Recommended FlowUi direction

### 1. Redefine the intended role of Params

Keep the type and builder API, but document a stronger rule:

> Params describe the immediate semantic contract of one element invocation. They should not automatically reproduce the configuration surface of every descendant.

Good compound params include model/data views, high-level events such as `onSave`, true outer layout choices, and explicit variants. Fonts, repeated colors, every nested input size, and application-wide controllers usually belong elsewhere.

Leaf elements can remain richly configurable because their params are consumed directly and do not need to travel farther.

### 2. Add typed theme/context first for routine propagation

This should carry common style roles and typed services through the subtree. It directly removes much of the forwarding visible in the current compound dev elements without exposing their internal child definitions.

Current `Params` aggregates usually give style fields concrete defaults, so FlowUi cannot tell whether a value was explicitly supplied or merely default-initialized. Theme adoption therefore needs a deliberate `Props`/`StylePatch` distinction, optional style fields, or another small source/provenance mechanism. Simply overwriting today's params in an unspecified order would be fragile.

Theme and preset values that can affect behavior should be resolved before interaction callbacks. The current execution order is params assignment, event callbacks, `runLogic`, developer override, then build/construct. A future design must preserve that ordering intentionally rather than resolving new layers only at build time.

For styles, a sensible conceptual precedence is:

```text
leaf defaults
  < inherited theme/context
  < component-local part defaults
  < invocation preset/part patch
  < explicit invocation style patch
  < developer-mode live override
```

Event and logic callbacks may then mutate the resolved invocation before developer override and build, as they do today. Semantic data and styles should remain separate enough that a style cascade cannot accidentally replace a model pointer or business callback.

### 3. Add named slots for composition

Slots establish the component boundary needed for universal sharing. They let one user accept the default compound element while another injects application-specific actions or content. They also extend a model FlowUi already supports through `construct()` and synchronous child drawing.

Slot tags should be stable public names, and slot callbacks should receive a scoped ID/build context. Internal absolute IDs should never be required from the caller.

### 4. Add named-part presets only for selected public parts

Parts should be explicitly exported by the compound component. Do not automatically make every `createChildElementId()` path selectable: those paths already matter for state and developer capture, but turning them all into public selectors would prevent harmless internal refactors.

A component can re-export a deeply nested leaf as one stable public part handle. That lets its internal route change without breaking consumer presets.

### 5. Preserve the current fast path

When an element uses no slots, theme values, or preset, its execution should remain close to today's behavior:

- one concrete params object owned by the builder;
- direct reference access in callbacks;
- synchronous child emission;
- no required allocation or heterogeneous property map;
- child-level state identity, definition-level resource behavior, interaction, and dev capture.

Potential advanced objects should be lightweight views or immutable cached data. Slot captures only need to live through synchronous drawing. Presets should be frozen outside the frame loop. Theme/context lookup should be cached or resolved through a short typed scope chain rather than a general string map.

### 6. Integrate developer tooling at the resolved leaf

Today developer overrides are applied directly to each concrete params object immediately before its build callback. The upgraded system should preserve that valuable property.

The dev UI should display:

- the leaf's final effective params;
- whether each value came from a default, theme role, component part default, preset, explicit call-site override, or live dev override;
- the public slot/part name when one exists;
- warnings for stale preset parts after a component version change.

This makes the new layers easier to understand rather than turning them into invisible magic.

## Sharing and versioning guidance

To make Flow elements genuinely easy to exchange between users:

- Treat semantic params, slot tags, theme roles, and exported part handles as the component's public contract.
- Keep private child definitions and raw child ID paths out of that contract unless there is a deliberate reason to expose them.
- Namespace roles and parts to avoid collisions between independent libraries.
- Give compound components a small compatibility/version identifier so presets can detect an incompatible layout contract.
- Prefer data/style-only portable presets. Route functionality through semantic actions, typed controllers, or command IDs; use captured callbacks only for local per-frame configuration.
- Ship components with default themes and optional presets so they work immediately but remain adaptable.
- Keep source/header sharing as the initial target. The current template types and compiler-derived developer type hashes do not form a stable cross-compiler binary plugin ABI.

## Final recommendation

The core issue is not that FlowUi cannot nest elements; it already nests them effectively. The issue is that **deep customization has no channel except ancestor-by-ancestor parameter forwarding**.

The best evolution is to retain params as the fast, strongly typed local input while adding two missing scopes:

- a **composition scope** through named slots;
- an **inherited scope** through typed themes and contexts.

Then add sparse named-part presets for applications that need exact deep control without replacing default structure. This gives simple elements the same efficient API they have now, lets complex elements remain encapsulated, and gives shared component authors explicit, versionable extension points instead of ever-growing outer parameter structs.
