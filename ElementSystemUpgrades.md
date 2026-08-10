# Flow Element System Upgrade Report

**Repository reviewed:** FlowUi 0.10.0 (`394dc85`)

**Scope:** `ElementDefinition`, `ElementBuilder`, `UiManager::createElement`, element state/resources, nested element composition, interaction dispatch, and the new `ThemeManager`.

## Executive summary

The current Flow element API has a good immediate-mode foundation. A caller can create a typed element, set one parameter object, and draw it without registration boilerplate:

```cpp
ui.createElement(kButton, "toolbar/save")
    .setParameters(ButtonParams{.label = "Save"})
    .draw();
```

That call shape should be preserved. The main redesign should happen behind it and inside element callbacks.

The recommended direction is:

1. Move element state and resources out of static template storage and into an app/window-owned `ElementRuntime`.
2. Introduce a cheap hierarchical `ElementKey` and context-relative child APIs so nesting does not construct and re-hash path strings.
3. Split functional props from component styles, then resolve styles through layered component themes with built-in fallbacks and optional local overrides.
4. Add explicit bindings for ordinary data flow, scoped typed capabilities for genuinely cross-cutting nested behavior, and child slots for structural composition.
5. Add compact invocation helpers such as `ui.draw(...)`, `context.drawChild(...)`, and a scope-safe container API while retaining the existing builder for advanced use.
6. Provide a less fragile element-definition authoring form with named/static hooks instead of seven positional callback fields.
7. Define a small “element pack” contract so a header-only element set can bring defaults, themes, assets, and requirements to another app without depending on the original project.

These changes solve different parts of the problem. Themes eliminate visual/style prop drilling. Bindings and typed capabilities reduce functional prop drilling. An app-owned runtime removes app-specific static lifetime and makes recursively stateful elements safe. `ElementKey` and cached frame views keep the additional abstraction from increasing UI build time.

The highest-value first release is proposals 1, 2, and 3. They fix current lifetime problems, remove a common nesting cost, and make the new theme system suitable for a reusable base element library.

## Goals and evaluation criteria

The redesign is evaluated against the three requested goals, plus UI build performance.

| Goal | A successful design should provide |
|---|---|
| Ease of use | A short common call site, good defaults, named concepts, no mandatory setup for basic controls, and convenience APIs for repeated patterns. |
| Recursive nesting | Child IDs, state, styles, data flow, and open/close lifetime should compose without every intermediate element forwarding unrelated fields. |
| Shareability | An element header or element pack should work in a different app with documented dependencies, safe resource lifetime, default styling, and no access to the originating app’s globals. |
| Performance | No required heap allocation per element build, constant-time state/theme/interaction access, stable state references, and low overhead when developer mode is disabled. |

## Current system: strengths worth preserving

The existing system already makes several good choices:

- `ElementDefinition<Params, State, Resources, DefinitionId>` gives custom elements a consistent typed shape.
- Params are rebuilt per invocation, which matches Clay’s immediate-mode model.
- `draw()` and `construct()` cover both closed components and open-ended containers.
- Root hover/press/hold/release behavior is standardized.
- The caller owns the instance ID, so repeated and nested instances can be made stable.
- `ThemeManager` supports arbitrary typed themes and atomic frame-boundary updates.
- The definition is a plain inline C++ value. Sharing a header does not require a runtime class hierarchy or factory registration.
- Developer-mode capture is integrated at the invocation boundary, which is the right place to inspect reusable components.

The redesign should extend this model rather than turn Flow elements into a retained widget tree.

## Important current constraints found in the repository

These findings influence the proposals below.

### 1. Params still contain entire descendant style surfaces

The dev element library shows the original forwarding problem clearly. For example, `devFloat2InputParams` carries font ID, font size, hint color, value color, layout, and both child sizes, then forwards these into two `devNumericInputParams`. `devTaggedUnionInputParams` repeats and forwards the same styling into `devEnum1Input`, `devNumericInput`, and `devFloat2Input`.

The new theme manager provides the mechanism to remove this, but the existing elements have not yet been migrated to a component-style contract.

### 2. Child identity currently performs repeated string work

`ElementBuildContext::createChildElementId()` constructs `parent + "/" + child` as a new `std::string`. `ElementBuilder` copies the resulting path into another `std::string`. `toFlowId()` hashes it for element state/dev capture, and `toClayEID()` copies it to the frame arena before Clay hashes it again.

The dev element headers currently contain 153 calls to `createChildElementId()`. Deep nesting and repeated rows multiply this cost.

### 3. Stateful recursive elements are not reference-safe

Each `ElementDefinition` specialization owns:

```cpp
static inline std::vector<std::pair<uint64_t, StateType>> statePool{};
```

`getOrCreateState()` returns a reference into this vector. If a parent obtains that reference and then builds a same-definition child that inserts another state entry, vector reallocation can invalidate the parent reference. `eraseState()` can also move entries. This directly conflicts with safe recursive same-element nesting.

Lookup is also linear in the number of instances of that definition, state is never collected automatically, and the storage is shared across every app and window in the process.

### 4. Definition resources are process-static, not app-owned

`ElementDefinition::resources` is another static object. If a reusable element initializes an icon or handle from app A and is later used in app B, `getResources(appB)` returns the already initialized app-A resource object. The dev element resource initializer currently writes app-derived texture refs directly into these static containers.

This is a major blocker for truly shareable element packs and for multiple FlowUi application objects in one process.

### 5. Theme lookup is too expensive to put in every element as implemented

`ui.theme<T>()` currently reaches `ThemeStorageController::getActiveThemeVariant<T>()`, which locks a mutex, performs type and variant hash-map lookups, and asks the storage system for the record (which takes another lock). Once every built-in control uses a theme, doing this for every nested element every frame would put manager synchronization in the UI build hot path.

### 6. Functional callbacks are commonly rebuilt and forwarded

The dev controls use `std::function` fields for value changes and interactions. Parent elements create capturing lambdas and pass them down through child params. This works, but it is the functional equivalent of style prop drilling and may also perform per-frame callback construction or allocation.

### 7. Element definitions are positionally fragile

Definitions commonly look like six `nullptr` values followed by a build lambda. Adding, removing, or reordering a callback field changes every aggregate initializer and makes a shared element harder to review.

### 8. Interaction queries are linear

`InteractionSnapshot` stores four vectors, and each `isHovered`, `isPressed`, `isHeld`, or `isReleased` call linearly scans its vector. A large interactive UI can approach `number of elements × number of hovered IDs` work during the build.

## Theme correctness cleanup applied before the redesign

The following cleanup is part of the theme foundation used by this report:

1. Theme storage resource names include both the theme type hash and variant name. Retrieval remains ergonomic through `getTheme<T>(variantName)`, while different theme types may safely use the same variant name.
2. `flowTheme()` is the canonical `UiManager` convenience accessor throughout the public API and documentation.

`FlowUiTheme` token values are currently placeholders and their final palette/unit design is intentionally outside this report.

---

## Proposal 1: App/window-owned `ElementRuntime`

### What to change

Create an `ElementRuntime` owned by `UiManager` or by `App` with a window-local partition. Move state and definition resources into that runtime. Resolve them once when an element invocation starts and expose them directly through the callback context.

Target callback usage:

```cpp
static void onPressed(Button::InteractionContext& context) {
    context.state().pressed = true;
}

static void build(Button::BuildContext& context) {
    auto& state = context.state();
    auto& resources = context.resources();
    // Build with stable, already-resolved references.
}
```

The runtime key should include at least:

```text
(runtime/app identity, window or explicit scope, definition ID, definition type hash, element key)
```

The definition ID is useful for tools and stable serialization. The definition type hash protects against accidental ID reuse by a different C++ type. Debug builds should report a clear collision instead of silently sharing storage.

### State storage recommendation

Use an open-addressed lookup table from the composite key to a stable typed allocation or generational slot. Do not store state objects directly in a relocating vector. Requirements are:

- references remain valid while nested children insert their own state;
- average constant-time lookup;
- destruction when the owning runtime/window/app is destroyed;
- typed construction and destruction callbacks;
- a revision or generation for safe developer-tool handles.

Resolve state once per invocation and pass the same reference to event, logic, and build hooks. A stateful element should not need to hash its string ID and scan its pool in every hook.

Support explicit lifetime policies:

```cpp
enum class StateLifetime {
    Runtime,       // Current behavior, but released with the window/runtime.
    WhileRendered, // Collect after a configurable number of unseen frames.
    Manual
};
```

`Runtime` is the safest compatibility default. `WhileRendered` is useful for virtualized rows and changing data sets, but it needs a grace period so temporarily hidden tabs do not lose state.

### Resource storage recommendation

Resources should normally be app-scoped and keyed by definition/pack. A second app must receive a second resource instance. If resources are window-specific, allow the definition to declare that scope explicitly.

```cpp
struct IconButton : FlowUi::Element<IconButton, Props, State, Resources> {
    static constexpr auto resourceScope = FlowUi::ResourceScope::App;
};
```

The resource constructor can still receive `App&` or `UiManager&`; the difference is that lifetime and identity are now correct.

### How and why this helps

- **Ease of use:** `context.state()` and `context.resources()` replace repeated `Definition::getOrCreateState(toFlowId(context.elementID))` expressions.
- **Recursive nesting:** stable state addresses remain valid when a same-definition child creates state.
- **Shareability:** a shared definition no longer retains handles from whichever app happened to initialize its static optional first.
- **Performance:** one average constant-time state lookup per invocation replaces repeated linear scans and repeated ID hashing.

### Pros

- Fixes same-definition recursive invalidation.
- Correct app/window lifetime and deterministic cleanup.
- Supports multiple FlowUi apps and windows safely.
- Centralizes state diagnostics, counts, memory use, and developer-mode inspection.
- Makes automatic or policy-based state collection possible.
- Removes public mutable `statePool` and `resources` implementation details.

### Cons

- Requires type-erased construction/destruction metadata inside the runtime.
- External code can no longer use a process-static `Definition::tryGetState()` without identifying an app/window. It should migrate to `ui.tryElementState<Definition>(key)`.
- Runtime lookup code is more complex than a vector.
- A state-scope decision is required for multi-window elements. The recommended default is window-local; explicit app scope should be opt-in.

### Compatibility path

Keep the old static functions for one release as deprecated adapters where possible. New contexts should use runtime state immediately. Because a static adapter cannot infer an app safely, external access should require `UiManager&` in the v2 API rather than pretending the old global behavior is still correct.

---

## Proposal 2: Hierarchical `ElementKey` and context-relative child calls

### What to change

Replace owned path strings in the hot path with a small strong ID type:

```cpp
struct ElementKey {
    FlowElementId flowId;
    uint32_t clayId;
#if FLOW_UI_DEV_MODE
    std::string_view debugPath;
#endif
};
```

The exact Clay representation can differ, but the key requirements are that a parent key and child segment can be combined without allocating and that the resulting Flow/Clay identity is calculated once.

Provide literal and indexed child operations:

```cpp
auto label = context.childKey("label");
auto row   = context.childKey("row", rowIndex);

context.drawChild(kButton, "save", ButtonProps{.text = "Save"});
context.drawChild(kRow, rowIndex, RowProps{.item = item});
```

Top-level string and `ResourceKey` overloads should remain:

```cpp
ui.draw(kToolbar, "main/toolbar", props);
```

They convert to `ElementKey` once. Nested code should normally use the context-relative form.

In developer mode, keep a readable path table for hierarchy inspection and collision diagnostics. Production builds can retain only hashes/IDs unless Clay requires the string for a specific operation.

### How and why this helps

The parent automatically supplies the namespace. A child only declares its local semantic name or loop index, so deeply reusable components cannot accidentally forget to prepend the parent path. This removes ID plumbing and reduces per-frame string construction, copying, and hashing.

### Pros

- Shorter nested element call sites.
- No `std::string` concatenation for ordinary literal/index children.
- Hash/Clay ID can be cached and reused for state, interaction, Clay emission, and dev capture.
- Strong types prevent confusing raw text IDs, Flow hashes, and Clay IDs.
- Indexed list items become a first-class case.
- Parent-relative identity makes copied element headers less dependent on app naming conventions.

### Cons

- Hierarchical hash mixing will not automatically equal the old hash of the complete `"parent/child"` string. Persisted element state or developer overrides may need a migration map.
- Debug builds still need readable path storage.
- Hash collision handling must be explicit. Debug builds should retain the authored path and assert when two different paths produce the same key in one scope.
- Clay integration must avoid relying on unstable private Clay internals. If a readable `Clay_String` is required, FlowUi should own a small per-frame/per-runtime interner.

### Recommendation

Make `ElementKey` the internal canonical type, but retain string overloads indefinitely as an ergonomic boundary. This gives performance-sensitive nested components a zero-allocation path without making simple app code work with numeric hashes.

---

## Proposal 3: Separate props, styles, and events with layered component themes

### What to change

Treat the per-invocation data as three different concepts:

1. **Props:** the value/data/behavior that makes this instance unique.
2. **Style:** visual and layout defaults for this component type.
3. **Events or bindings:** how changes leave or enter the component.

For example:

```cpp
struct ButtonProps {
    std::string_view text;
    ButtonRole role = ButtonRole::Primary;
    bool enabled = true;
    FlowUi::Action<> onPress{};
};

struct ButtonStyle {
    Clay_Padding padding = CLAY_PADDING_ALL(8);
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIT(0),
        .height = CLAY_SIZING_FIT(0),
    };
    Clay_Color background{};
    Clay_Color hoverBackground{};
    Clay_Color textColor{};
    Clay_CornerRadius radius{};
    FontFamilyId fontFamily = 0;
    float fontSize = 14.0f;
};

struct ButtonTheme {
    ButtonStyle primary;
    ButtonStyle secondary;
    ButtonStyle danger;
};
```

The element resolves the style internally:

```cpp
const ButtonStyle& style = context.themeOr(ButtonTheme::defaults())
                                  .forRole(context.props().role);
```

### Layering model

Use one documented order:

```text
element/pack built-in defaults
    -> app active component theme
    -> nearest scoped theme override
    -> one-invocation style override
```

The last layer should be optional and concise:

```cpp
ui.element(kButton, "delete", ButtonProps{
        .text = "Delete",
        .role = ButtonRole::Danger,
    })
    .style([](ButtonStyle& style) {
        style.sizing.width = CLAY_SIZING_GROW(0);
    })
    .draw();
```

Do not put `std::optional` around every style field in every props struct. Store an optional override operation or override object in the builder and merge once. Most invocations should have no override.

### Component themes versus one monolithic theme

Keep `FlowUiTheme` as a small set of universal design tokens, but do not grow it to contain every property of every built-in control. Element packs should define component themes such as `ButtonTheme`, `InputTheme`, and `PanelTheme` that derive their defaults from core tokens.

This has better sharing boundaries:

- `FlowUiTheme` describes the app’s common palette, typography, radii, and spacing.
- `ButtonTheme` describes how a particular button family maps roles/states to those tokens.
- A third-party table element can ship `TableTheme` without modifying FlowUi’s central theme struct.

### Fallback and theme scope APIs

A shareable element must not throw just because an app did not register its optional theme. Add non-throwing lookup:

```cpp
const T* ui.tryTheme<T>() const noexcept;
const T& context.themeOr(const T& fallback) const noexcept;
```

Also add lexical overrides for a subtree:

```cpp
context.withTheme(localPanelTheme, [&] {
    context.drawChild(kSettingsForm, "form", props);
});
```

This handles modals, inspectors, dense toolbars, and embedded element packs without forwarding style params through every intermediate element.

### Frame-cached lookup

At `beginFrame`, produce an immutable `ThemeFrameView` containing the active theme pointers and revisions. `context.theme<T>()` should consult that view without taking a mutex or performing storage-manager locking. Theme updates are already staged at frame boundaries, so an immutable frame view matches the existing consistency model.

Scoped overrides can be a small typed stack owned by the build context. Push/pop is proportional to the number of explicit scopes, not the number of descendants.

### How and why this helps

- **Ease of use:** most call sites specify text/value/role and nothing visual.
- **Recursive nesting:** changing an input font or panel density no longer requires edits through every composite params type.
- **Shareability:** a component ships usable fallback defaults and its own optional component theme instead of assuming an app-specific `AppTheme` exists.
- **Performance:** immutable cached theme pointers avoid hot-path locks; style merging occurs only when an override exists.

### Pros

- Makes params smaller and semantically clearer.
- Allows global and subtree restyling without callback/data rewiring.
- Lets third-party components add styles without extending a central mega-theme.
- Built-in defaults make shared headers work immediately.
- Roles/variants such as primary, danger, compact, or toolbar are more stable than exposing every raw color at every call site.

### Cons

- Requires deciding which layout properties are semantic props and which are style. A practical rule is: if it changes what the component means or how it participates in parent layout, it may remain a prop; internal colors, typography, padding, child gaps, and child styling belong in style.
- Style layering and merge rules must be documented precisely.
- A theme type per component family increases the number of registered types, making the resource-key prerequisite above mandatory.
- Scoped themes introduce ambient behavior. Developer tools should show which layer supplied a final property.

### Recommendation

Use semantic roles plus component themes as the default customization path. Keep raw per-invocation style overrides as an escape hatch. Do not require apps to register every component theme; every shareable element/pack must have a built-in fallback.

---

## Proposal 4: Typed composition primitives for functional nesting

Themes solve style nesting, but not value and behavior nesting. No single mechanism fits every data-flow case. FlowUi should provide three small mechanisms with clear usage rules.

### Option A: Explicit `Binding<T>` and `Action<...>` — recommended default

Use a lightweight binding for a controlled value and a lightweight action for an output event:

```cpp
template <typename T>
struct Binding {
    const T* value = nullptr;
    Action<const T&> changed{};
};

struct SliderProps {
    Binding<double> value;
    Range<double> range{0.0, 1.0};
};
```

This makes ownership clear: app/model state remains outside the element, while the element owns only interaction state such as dragging. `Action` should use a documented no-allocation representation for common cases, such as a `(void*, function pointer)` pair or a small-buffer callable. A non-owning callable is appropriate only because params are consumed immediately; FlowUi must not retain it after `draw()`.

Bindings can support projections for nested data:

```cpp
auto color = ui.bind(model.color);
context.drawChild(kColorPicker, "color", ColorPickerProps{.value = color});
```

**Pros**

- Explicit and easy to reason about.
- Shareable elements depend only on `Binding<T>`, not an app model type.
- Separates canonical data from UI interaction state.
- Can avoid `std::function` allocation and repeated callback wrappers.
- Works well for direct parent-child composition.

**Cons**

- An intermediate element still has to forward a binding if it is merely structural.
- Non-owning action lifetime rules must be strict and tested.
- Two-way binding can hide when mutations occur unless the API consistently calls an action rather than writing directly.

### Option B: Scoped typed capabilities — recommended for cross-cutting nested behavior

Allow a parent to provide a narrow interface to an arbitrary descendant for the duration of a subtree build:

```cpp
struct FormActions {
    Action<FieldId, FieldValue> fieldChanged;
    Action<FieldId> requestFocus;
};

context.provide(FormActions{...}, [&] {
    context.drawChild(kAddressEditor, "address", props);
});
```

A deeply nested field can request it:

```cpp
if (const FormActions* form = context.find<FormActions>()) {
    form->fieldChanged(field, value);
}
```

This is dependency injection scoped to the immediate-mode element tree, not a global service locator. The capability type should live in the element pack’s public API and remain small and app-neutral.

Good uses are form dispatch, validation reporting, selection coordination, table editing sessions, localization, and accessibility metadata. Direct values that only one child needs should remain explicit props.

**Pros**

- Removes callback plumbing through structural intermediate elements.
- Keeps dependencies typed and local to a subtree.
- A reusable composite can work with any app that provides the public capability interface.
- Push/pop can be allocation-free with a small frame stack.

**Cons**

- Dependencies are less visible at the invocation line.
- Missing required capabilities need good diagnostics; optional lookup and required lookup should be different APIs.
- Overuse can recreate a service locator. Documentation must reserve it for cross-cutting subtree behavior.
- Type lookup adds a small cost; cache the nearest provider in the context or use a compact type-indexed stack.

### Option C: Child slots/content callbacks — recommended for structural composition

Closed components and open-ended `construct()` components should share a safe content API:

```cpp
ui.container(kCard, "profile", CardProps{.title = "Profile"}, [&](auto& card) {
    card.draw(kAvatar, "avatar", avatarProps);
    card.draw(kUserDetails, "details", detailsProps);
});
```

This automatically opens the root, runs the callback, and closes it even if the callback returns early or throws. A named-slot variant can support components such as dialogs:

```cpp
ui.element(kDialog, "confirm", dialogProps)
    .slot<DialogSlot::Body>(...)
    .slot<DialogSlot::Actions>(...)
    .draw();
```

Start with one content callback; add named slots only when real components need them.

**Pros**

- Eliminates manual `construct()` / `drawConstructed()` pairing for common containers.
- Makes reusable layout components accept arbitrary children without knowing their params.
- Naturally nests and keeps the UI-building section readable.
- Scope cleanup is deterministic.

**Cons**

- Templated callbacks can increase compile time and code size.
- Captures must remain frame-local.
- Named slots can become verbose if introduced before there are concrete use cases.

### Why not use an app-wide event bus as the main solution?

A global string-based event bus would remove forwarding, but it weakens type safety, hides dependencies, introduces naming collisions between shared packs, and makes local reasoning/testing harder. It can still exist at the application layer. It should not be the fundamental Flow element composition mechanism.

---

## Proposal 5: Compact invocation and safe convenience APIs

### What to change

Keep the current builder, but add direct overloads for the 90% cases:

```cpp
ui.draw(kButton, "save", ButtonProps{.text = "Save"});
ui.draw(kSpinner, "loading");

context.drawChild(kButton, "save", ButtonProps{.text = "Save"});
context.drawChild(kRow, rowIndex, RowProps{.item = item});
```

Also allow direct params construction in the builder:

```cpp
ui.element(kButton, "save", ButtonProps{.text = "Save"}).draw();
```

Today `ElementBuilder` default-constructs `params_` and then `setParameters()` assigns over it. Direct construction avoids redundant construction/assignment for props containing strings, vectors, or callbacks.

Retain fluent functions for uncommon behavior:

```cpp
ui.element(kButton, "save", props)
    .style(...)
    .eventsDisabled()
    .devCapture(...)
    .draw();
```

Replace negative skip flags in ordinary user code with positive named policies where useful. `ElementDrawOptions` can remain as the low-level API.

### How and why this helps

The visible UI-building code becomes shorter even after props/styles/bindings become more structured. Context-relative calls also automatically inherit parent identity, theme scopes, and capability scopes.

### Pros

- Common leaf invocation is one statement.
- Nested calls stop repeating `context.uiManager` and `context.createChildElementId(...)`.
- Direct parameter construction reduces per-frame work.
- Existing advanced builder functionality remains available.
- Easy migration: these are additive overloads.

### Cons

- More surface area and documentation aliases.
- Too many synonyms (`createElement`, `element`, `draw`) can confuse users. Pick one canonical spelling in new documentation and mark the others as compatibility/advanced forms.
- A direct draw function must still preserve developer-mode source-location capture.

### Recommendation

Use `ui.element(...).draw()` as the canonical fluent form and `ui.draw(...)` / `context.drawChild(...)` as convenience forms. Keep `createElement()` as a compatibility alias during migration.

---

## Proposal 6: Named/static element hooks instead of positional callback aggregates

### What to change

Offer a C++23 concept/CRTP authoring form that discovers optional static hooks:

```cpp
struct Button : FlowUi::Element<Button, ButtonProps, ButtonState, ButtonResources> {
    static constexpr auto definitionId = FLOW_DEF_ID("flowui.basic/button");
    static constexpr std::string_view name = "Button";

    static void onPressed(InteractionContext& context);
    static void logic(InteractionContext& context);
    static void build(BuildContext& context);
};

inline constexpr Button kButton{};
```

Missing functions are absent hooks; users no longer write `nullptr` placeholders. Concepts can provide focused errors such as “build hook must accept `Button::BuildContext&`”. The existing `ElementDefinition` descriptor can remain underneath for developer registry metadata or be generated at compile time.

An alternative with less template machinery is a named factory:

```cpp
inline constexpr auto kButton = FlowUi::defineElement<ButtonProps, ButtonState>(
    FLOW_DEF_ID("flowui.basic/button"),
    FlowUi::onPressed(...),
    FlowUi::build(...));
```

The static-hook form is recommended because it is easy to navigate, permits inlining, and gives a shared component a natural namespace for defaults and metadata.

### How and why this helps

- **Ease of use:** authors implement only the hooks they need.
- **Recursive nesting:** `Context`, state, resources, style metadata, and pack requirements can all be associated with one element type.
- **Shareability:** a shared header has a named component type instead of a positional aggregate whose layout can change with FlowUi versions.
- **Performance:** the compiler can statically eliminate missing phases and may inline hooks in release builds.

### Pros

- No chains of `nullptr` entries.
- Adding a new optional lifecycle hook does not break every definition initializer.
- Better compiler errors and code navigation.
- Compile-time hook dispatch can reduce function-pointer calls.
- Natural location for definition name, version, state lifetime, required capabilities, and default styles.

### Cons

- CRTP/concepts increase template complexity and may increase compile times.
- Static dispatch can increase code size if many elements instantiate large generic paths.
- Capturing lambdas cannot be stored directly as definition hooks; current definitions already use non-capturing function pointers, so this is usually not a regression.
- Developer-mode reflection and registration macros need adapters.

### Compatibility path

Make `UiManager` accept both the old descriptor and any type satisfying the new `FlowElement` concept. This allows built-in elements to migrate first and third-party headers to migrate gradually.

---

## Proposal 7: A formal shareable “element pack” contract

### What to change

Define what a set of elements must provide to be copied or installed into another app. A pack should contain:

- elements and their props/events/bindings;
- built-in default component themes;
- stable, namespaced definition IDs and resource keys;
- optional asset/resource installation;
- capability requirements;
- a small version number or compatibility constant.

Example shape:

```cpp
namespace Acme::Controls {

struct Pack {
    static constexpr std::string_view id = "com.acme.flowui.controls";
    static constexpr uint32_t apiVersion = 1;

    static void install(FlowUi::App& app); // Idempotent; assets and optional themes.
};

inline constexpr Button button{};
inline constexpr TextInput textInput{};

} // namespace Acme::Controls
```

Basic controls with no external assets should work without calling `install()`, using fallback themes and lazy app-owned resources. `install()` is for icons, fonts, or application-level registrations and must be idempotent.

Use reverse-domain or library-prefixed definition/resource names (`flowui.basic/button`, `com.acme.controls/date-picker`) and report collisions in debug mode.

The FlowUi repository can then ship a first-party `FlowUi::Elements` pack containing controls used by roughly 90% of apps: text, button, icon button, toggle, checkbox, radio/select, slider, numeric/text input, label, separator, scroll container, panel/card, stack/row, modal/popover primitives, and list/tree row foundations.

### Dependency rules for shareable elements

A pack element should depend on:

- FlowUi public managers or public capability interfaces;
- its own props/bindings and component theme;
- namespaced logical resource keys, not raw Vulkan or app-local object addresses;
- app-owned resources obtained through `context.resources()`.

It should not depend on:

- project globals or a project-specific singleton;
- a theme type that has no built-in fallback;
- numeric font/texture handles prepared by a different app;
- knowledge of the full parent path;
- state access through a process-static pool.

### Pros

- Gives users a clear target when authoring components for other apps.
- Enables an official base element library without special internal privileges.
- Makes dependencies and installation failures diagnosable.
- Namespacing reduces collisions between copied libraries.
- Encourages reusable semantic APIs instead of exposing every Clay field.

### Cons

- Adds packaging/versioning policy to a currently header-oriented system.
- Asset installation and optional compile features need careful handling.
- Too rigid a manifest could discourage small one-file components. Keep the pack wrapper optional for elements with no setup.
- ABI stability is difficult for template-heavy C++ headers; the contract should promise source compatibility within a documented range rather than binary compatibility unless FlowUi introduces a C ABI.

---

## Proposal 8: Hot-path interaction and theme views

This proposal is smaller architecturally but important once the basic library makes every UI consist of many Flow elements.

### Interaction snapshot

Build one frame-arena hash table from Clay element ID to interaction bits:

```cpp
enum InteractionBits : uint8_t {
    Hovered  = 1 << 0,
    Pressed  = 1 << 1,
    Held     = 1 << 2,
    Released = 1 << 3,
};
```

Then resolve the root interaction mask once per invocation. Event hooks test bits without four vector scans. Keep the vectors only if public iteration is needed.

### Theme view

As described in proposal 3, create an immutable per-frame active-theme view. Theme retrieval from an element callback should not acquire manager/storage mutexes.

### Builder execution

Unify the duplicated `draw()` and `construct()` callback pipeline internally and compute these values once:

- element key/Flow ID;
- Clay root ID;
- interaction mask;
- optional state pointer;
- optional resource pointer;
- theme/capability scope cursor.

Pass a compact invocation record to hooks. Developer-mode-only strings and reflection work should compile out when `FLOW_UI_DEV_MODE` is disabled.

### Props/callback allocation policy

Document that per-frame props should prefer views, spans, handles, bindings, and small actions over owning vectors, strings, and arbitrary `std::function`. Provide the efficient standard types so every element author does not invent incompatible alternatives.

### Pros

- Predictable constant-time interaction/theme access.
- Makes broad adoption of themes inexpensive.
- Removes duplicate work across event, logic, and build phases.
- Creates measurable hot-path boundaries for performance diagnostics.

### Cons

- Requires a custom frame hash/index or carefully selected container.
- A frame theme view must remain valid across multi-window frame sequencing and staged updates.
- Micro-optimizing before representative benchmarks can complicate the code. Introduce counters and benchmarks with each change.

---

## Recommended target API

The following sketch combines the proposals without making the ordinary UI call site larger.

### Element authoring

```cpp
struct ToggleProps {
    std::string_view label;
    FlowUi::Binding<bool> value;
    ToggleRole role = ToggleRole::Normal;
};

struct ToggleState {
    bool pointerDown = false;
};

struct ToggleStyle {
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIT(0),
        .height = CLAY_SIZING_FIT(0),
    };
    Clay_Padding padding = CLAY_PADDING_ALL(8);
    Clay_Color offColor{};
    Clay_Color onColor{};
    Clay_Color textColor{};
};

struct Toggle : FlowUi::Element<Toggle, ToggleProps, ToggleState> {
    static constexpr auto definitionId = FLOW_DEF_ID("flowui.basic/toggle");
    static constexpr auto stateLifetime = FlowUi::StateLifetime::Runtime;

    static void onPressed(InteractionContext& context) {
        auto& state = context.state();
        state.pointerDown = true;
        context.props().value.changed(!context.props().value.get());
    }

    static void build(BuildContext& context) {
        const auto& style = context.style<ToggleStyle>(context.props().role);
        const bool enabled = context.props().value.get();

        CLAY(context.rootClayId(), makeToggleRoot(style, enabled)) {
            // Internal child IDs are allocation-free relative keys.
            CLAY(context.childClayId("label"), makeLabel(style, context.props().label)) {}
        }
    }
};

inline constexpr Toggle kToggle{};
```

### App UI code

```cpp
ui.draw(kToggle, "settings/grid", ToggleProps{
    .label = "Show grid",
    .value = ui.bind(settings.showGrid),
});
```

### Composite element code

```cpp
static void build(SettingsPanel::BuildContext& context) {
    context.container(kPanel, "panel", PanelProps{.role = PanelRole::Settings}, [&](auto& panel) {
        panel.draw(kToggle, "grid", ToggleProps{
            .label = "Show grid",
            .value = context.props().showGrid,
        });

        panel.draw(kSlider, "opacity", SliderProps{
            .value = context.props().opacity,
            .range = {0.0, 1.0},
        });
    });
}
```

Style values are absent from both call sites. If several nested fields must communicate with a form controller, the panel can provide a scoped `FormActions` capability around the container rather than forwarding the same callback through every composite.

## Options comparison

Scores are relative: 5 is the strongest positive effect, 1 is little direct effect. “Risk” is implementation/migration risk, where 5 is highest.

| Proposal | Ease | Recursive nesting | Shareability | Performance | Risk | Priority |
|---|---:|---:|---:|---:|---:|---|
| App/window-owned element runtime | 4 | 5 | 5 | 5 | 4 | P0 |
| Hierarchical `ElementKey` and child API | 5 | 5 | 4 | 5 | 3 | P0 |
| Props/style split and component themes | 5 | 5 | 5 | 4 | 4 | P0 |
| Bindings, capabilities, and slots | 4 | 5 | 5 | 4 | 4 | P1 |
| Compact invocation helpers | 5 | 4 | 3 | 3 | 2 | P1 |
| Static/named definition hooks | 4 | 3 | 4 | 3 | 3 | P1 |
| Element pack contract/base library | 5 | 4 | 5 | 3 | 3 | P1 after runtime/themes |
| Interaction and frame views | 2 | 2 | 2 | 5 | 2 | P1, benchmark-driven |

## Suggested migration sequence

### Phase 0: Correctness and measurement

- Retain regression coverage for registering two theme types with the same variant name.
- Add a recursive same-definition state test that forces multiple insertions.
- Add multi-app and multi-window state/resource tests.
- Add benchmarks for shallow leaves, deep nesting, stateful lists, interaction-heavy controls, and developer mode on/off.

### Phase 1: Additive ergonomic and hot-path work

- Introduce `ElementKey` internally and string compatibility overloads.
- Add direct-props builder constructors, `ui.draw`, `context.drawChild`, and a scope-safe container helper.
- Add `tryTheme<T>()` and immutable `ThemeFrameView`.
- Replace interaction vector scans with an interaction mask lookup.

These changes can ship without removing the existing API.

### Phase 2: Runtime-owned state and resources

- Add `ElementRuntime` and context `state()` / `resources()`.
- Migrate first-party/dev elements.
- Add runtime-scoped external state access.
- Deprecate static state/resource storage.
- Add state lifetime policies after the basic runtime is stable.

### Phase 3: Component styles and theme scopes

- Define core token units and naming conventions.
- Create a few component themes (`ButtonTheme`, `InputTheme`, `PanelTheme`).
- Migrate representative deeply nested dev elements first, such as tagged-union and numeric inputs, to verify that style fields actually disappear from intermediate props.
- Add developer tooling that displays final style provenance.

### Phase 4: Functional composition and base elements

- Introduce `Action`, `Binding`, and one-content-slot containers.
- Add scoped typed capabilities only after at least two real nesting cases demonstrate the need.
- Convert a vertical slice of controls into a first-party element pack.
- Validate the pack in two small apps with unrelated themes and resource sets.

### Phase 5: New definition syntax

- Let old and new definition forms coexist.
- Migrate first-party elements and templates/tutorials.
- Deprecate positional aggregate examples before deprecating the underlying type.

## Performance validation plan

Performance should be treated as an API acceptance criterion, not a later implementation detail.

Recommended benchmarks:

1. **10,000 stateless leaves:** measures builder/context/ID overhead.
2. **5,000 stateful controls:** measures state resolution and interaction dispatch.
3. **Depth-64 recursive composite:** catches state reference invalidation, scope-stack bugs, and child-ID cost.
4. **10,000-row churn:** alternating keys measures state creation/collection and indexed IDs.
5. **Theme-heavy tree:** every element resolves core and component theme; verifies zero locks during build.
6. **Callback-heavy form:** bindings/actions and scoped capabilities compared with `std::function` props.
7. **Developer mode A/B:** confirms source capture/debug paths do not affect production builds.

Track at least:

- total UI build time and time per Flow element;
- heap allocations during build (target: zero for steady-state ordinary elements);
- frame-arena bytes used for IDs/strings;
- state/theme/interaction lookup counts and probe lengths;
- element count and maximum nesting depth;
- dev capture time separately from element build time.

The existing `PerformanceDiagnostics` system is a natural place to expose these counters in developer mode.

## Final recommendation

Do not make themes carry all composition responsibility. The clean boundary is:

- **themes/component styles** carry visual defaults;
- **props and bindings** carry instance data and direct behavior;
- **scoped capabilities** carry cross-cutting subtree behavior;
- **slots/containers** carry arbitrary child structure;
- **ElementRuntime** carries state and resources with correct app/window lifetime;
- **ElementKey** carries identity cheaply through recursive nesting.

This keeps the best property of the existing API—the concise immediate-mode draw call—while making the internals safe and efficient enough for deep recursive elements and making element headers genuinely portable between applications.

The base element library should be built only after runtime-owned resources, theme fallbacks, and namespaced identities exist. Otherwise the first-party base elements will encode the same app-specific assumptions the redesign is intended to remove.
