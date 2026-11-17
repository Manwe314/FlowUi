# Element API

The application authoring surface is defined by [concepts](include/FlowUi/Public/elementAuthoring/ElementConcepts.hpp), [callback contexts](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp), [ElementBuilder](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp) and [ElementManager](include/FlowUi/Public/managers/ElementManager.hpp).

`FlowElement<Element>` requires a complete, empty, trivially default-constructible and trivially copyable definition tag, nonzero constexpr `definitionId` and at least one output hook. Optional `Parameters` must be default constructible. `State` must be default constructible and nothrow destructible; `Resources` must be default constructible or constructible from `App&` and nothrow destructible. `DrawableFlowElement` enables `draw()` and `ConstructibleFlowElement` enables `construct()`. `ParametersOf` resolves to declared parameters or `NoElementParameters`; `HasState`/`StateOf`, `HasResources`/`ResourcesOf` and `HasParts` describe the corresponding optional capability. `Parts` must be a complete class type.

Optional `onHovered`, `onPressed`, `onHeld`, `onReleased` and `runLogic` hooks take `ElementInteractionContext<Element>&` and return `void`. `buildElement` takes `ElementBuildContext<Element>&` and returns `void`; `constructElement` returns `Clay_ElementDeclaration`. Optional `statePolicy`, `debugName` and `isDevInternal` metadata must be constexpr. Hooks execute synchronously. Pointer callbacks use the previous completed interaction snapshot, followed by enabled logic and output hooks.

The following complete definition is shared by the examples below. It has real per-window instance state and immutable app-wide resources:

```cpp
struct Counter {
    struct Parameters { float width = 120.0f; };
    struct State { int count = 0; };
    struct Resources { float rowHeight = 24.0f; };
    static constexpr auto definitionId = FlowUi::DefinitionID("docs.counter");
    static constexpr auto statePolicy = FlowUi::ElementStatePolicy::transient(5);
    static Clay_ElementDeclaration constructElement(
        FlowUi::ElementBuildContext<Counter>& context) {
        Clay_ElementDeclaration root{};
        root.layout.sizing.width = CLAY_SIZING_FIXED(context.params.width);
        root.layout.sizing.height = CLAY_SIZING_FIXED(context.resources().rowHeight);
        return root;
    }
    static void buildElement(FlowUi::ElementBuildContext<Counter>& context) {
        auto rootId = context.clayID();
        const auto root = constructElement(context);
        CLAY(rootId, root) {}
    }
    static void onPressed(FlowUi::ElementInteractionContext<Counter>& context) {
        ++context.state().count;
    }
};
inline constexpr Counter counter{};
static_assert(FlowUi::validate_flow_element<Counter>());
```

Builders own a parameter draft; views/pointers within that draft remain borrowed through rendering. Fluent setters return `ElementBuilder&`, while `draw()` and `construct()` return `void`. Library validation throws `FlowUiException`; arbitrary parameter, resource and hook exceptions propagate and poison the frame after scope cleanup. Abandon a failed builder and cancel its frame. `SkipEventCallbacks`, `SkipLogicCallback` and `SkipBuildCallback` can be combined with `operator|`; `SkipBuildCallback` skips draw output but does not suppress `constructElement()`.

Both contexts expose borrowed `UiManager& uiManager`, strong `FlowElementID id` and `ParametersType& params`. Interaction contexts also expose `previousInteraction`. Contexts, state references and cached resource references must not escape the callback. `state()`/`resources()` return direct references; the root `clayID()` returns directly, while named identity and action-availability helpers retain checked Results. Use public `FlowUi::ValueOrThrow` when deliberately escalating one of those retained results. See [result handling](docs/api/result_handling.md).

| Identity | Application use |
| --- | --- |
| `DefinitionID("name")` | Stable reusable definition identity. |
| `LocalElementName` literal / `RuntimeName(text)` | Named local identity under the captured parent. Runtime text is hashed; developer metadata can borrow its backing bytes. |
| `Indexed("row", position)` / `IndexedIDs("row", first)` | Positional local names; state follows position. |
| `Keyed("row", stableKey)` | Stable data-key identity across reordering. |
| `AutoID()` | Callsite identity and the default no-name builder address; repeated loop calls at one callsite need explicit distinct identities. |
| `FlowElementID` | Already resolved local instance address. |
| `Global<kElement>("name")` / `GlobalFlowID` | Explicit definition-scoped address reproducible outside a local branch. |
| `Part("name")` / `context.part(...)` / `FlowElementPartID` | Semantic declaration and a concrete owner-bound part address. |

Exact identity factories and overloads are documented in [the public API catalog](docs/api/all_public_api.md) and [ElementID.hpp](include/FlowUi/Public/identificationTypes/ElementID.hpp).

`App::elements()` returns a checked manager wrapper; `ui.elements()` returns a direct reference. Outside callbacks, check a state lookup error and then check the successful pointer for null. Queries never create state. Pointers are borrowed until erasure, successful-frame transient collection or window destruction. State erasures requested during a build commit only with that successful frame; canceled frames discard them. `ElementStatePolicy::transient(n)` tolerates n committed absent frames and expires on the next; canceled frames do not count. `windowLifetime()` retains until explicit erase or window closure.

```cpp
auto managerResult = app.elements();
if (!managerResult) {
    throw FlowUi::FlowUiException(managerResult.error());
}
FlowUi::ElementManager& elements = managerResult->get();
auto status = elements.prepare(counter);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

Resource preparation is hybrid: known owner validation failures return Status, while resource constructors/allocation can throw; earlier prepared definitions stay ready. Conditional developer schema/capture helpers are described in [developer mode](docs/tutorials/developer_mode.md).

### **ElementBuilder::setParameters**

#### `[[nodiscard]] ElementBuilder& setParameters(const ParametersType& parameters);`

Copy or move a complete parameter draft into the builder and return this builder by reference. Any views/pointers in the parameters remain borrowed.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
Counter::Parameters parameters{.width = 160.0f};
builder.setParameters(parameters).draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::setParameters**

#### `[[nodiscard]] ElementBuilder& setParameters(ParametersType&& parameters);`

Copy or move a complete parameter draft into the builder and return this builder by reference. Any views/pointers in the parameters remain borrowed.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
Counter::Parameters parameters{.width = 160.0f};
builder.setParameters(std::move(parameters)).draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::mergeParams**

#### `template <typename MergeFn> [[nodiscard]] ElementBuilder& mergeParams(MergeFn&& mergeFn);`

Invoke the mutator immediately on the builder-owned ParametersType& and return this builder by reference. No callback is retained.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.mergeParams([](Counter::Parameters& parameters) { parameters.width = 160.0f; }).draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::withID**

#### `[[nodiscard]] ElementBuilder& withID(LocalElementName name);`

Replace this builder identity and return this builder by reference. Local/runtime/indexed/automatic names resolve against the parent captured at creation; explicit resolved/global/part IDs must be nonzero.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.withID("renamed").draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::withID**

#### `[[nodiscard]] ElementBuilder& withID(RuntimeElementName name);`

Replace this builder identity and return this builder by reference. Local/runtime/indexed/automatic names resolve against the parent captured at creation; explicit resolved/global/part IDs must be nonzero.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.withID(FlowUi::RuntimeName("renamed")).draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::withID**

#### `[[nodiscard]] ElementBuilder& withID(IndexedElementName name);`

Replace this builder identity and return this builder by reference. Local/runtime/indexed/automatic names resolve against the parent captured at creation; explicit resolved/global/part IDs must be nonzero.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.withID(FlowUi::Indexed("row", 1)).draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::withID**

#### `[[nodiscard]] ElementBuilder& withID(AutoElementName name);`

Replace this builder identity and return this builder by reference. Local/runtime/indexed/automatic names resolve against the parent captured at creation; explicit resolved/global/part IDs must be nonzero.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.withID(FlowUi::AutoID()).draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::withID**

#### `[[nodiscard]] ElementBuilder& withID(FlowElementID id);`

Replace this builder identity and return this builder by reference. Local/runtime/indexed/automatic names resolve against the parent captured at creation; explicit resolved/global/part IDs must be nonzero.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.withID(id).draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::withID**

#### `[[nodiscard]] ElementBuilder& withID(GlobalFlowID id);`

Replace this builder identity and return this builder by reference. Local/runtime/indexed/automatic names resolve against the parent captured at creation; explicit resolved/global/part IDs must be nonzero.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.withID(id).draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::withID**

#### `[[nodiscard]] ElementBuilder& withID(FlowElementPartID id);`

Replace this builder identity and return this builder by reference. Local/runtime/indexed/automatic names resolve against the parent captured at creation; explicit resolved/global/part IDs must be nonzero.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.withID(id).draw();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::setDevInternalCapture**

#### `ElementBuilder& setDevInternalCapture(bool isDevInternal = true) noexcept;`

Return this builder after selecting whether developer capture treats this invocation as internal UI. Without developer support this is a no-op setting.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::construct**

#### `void construct(ElementDrawOptions options = ElementDrawOptions::Default) requires ConstructibleFlowElement<ElementType>;`

Run enabled pointer callbacks, runLogic and constructElement synchronously, then open the declared Clay root. Requires ConstructibleFlowElement; close using UiManager::drawConstructed().

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.construct();
ui.createElement(counter, "child").draw();
ui.drawConstructed();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **ElementBuilder::draw**

#### `void draw(ElementDrawOptions options = ElementDrawOptions::Default) requires DrawableFlowElement<ElementType>;`

Run enabled pointer callbacks, runLogic and buildElement synchronously. Requires DrawableFlowElement. Returns void.

Direct throwing API: library validation raises FlowUiException; user assignment/mutator/callback exceptions propagate unchanged and latch frame failure. Scope cleanup does not roll back application state; abandon this builder and cancel the failed frame. Borrowed parameter members must remain valid through rendering.

**Example:**

```cpp
auto builder = ui.createElement(counter, "counter");
builder.draw(FlowUi::ElementDrawOptions::SkipEventCallbacks);
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementBuilder.hpp).

### **FlowUi::validate_flow_element**

#### `template <typename Element> [[nodiscard]] consteval bool validate_flow_element() noexcept;`

Validate the complete element definition at compile time with descriptive assertions; returns true when valid. Use static_assert after the complete definition.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ElementBuildContext::ElementBuildContext**

#### `ElementBuildContext( detail::element::ElementInvocation<ElementType>& invocation, ParametersType& parameters) noexcept;`

Framework-created callback context bound to an active invocation and its parameter draft. Application elements receive this by reference; do not retain it.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::state**

#### `template <typename E = ElementType> requires HasState<E> && std::same_as<E, ElementType> [[nodiscard]] StateOf<E>& state();`

Return a direct mutable or const state reference for this exact element type. Available only with HasState; the template argument cannot select another element type.

The reference borrows manager-owned per-window state for this callback through the active invocation lease. State validation failures propagate and latch frame failure; cancel the failed frame.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::state**

#### `template <typename E = ElementType> requires HasState<E> && std::same_as<E, ElementType> [[nodiscard]] const StateOf<E>& state() const;`

Return a direct mutable or const state reference for this exact element type. Available only with HasState; the template argument cannot select another element type.

The reference borrows manager-owned per-window state for this callback through the active invocation lease. State validation failures propagate and latch frame failure; cancel the failed frame.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::resources**

#### `template <typename E = ElementType> requires HasResources<E> && std::same_as<E, ElementType> [[nodiscard]] const ResourcesOf<E>& resources() const;`

Lazily initialize and return this exact definition immutable app-wide resources by const reference. Available only with HasResources.

The reference borrows manager-owned app-wide data for this callback. Resource-constructor and allocation exceptions propagate and latch frame failure; cancel the failed frame. Resources are shared across all instances of this definition.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::clayID**

#### `[[nodiscard]] Clay_ElementId clayID() const;`

Return this element root Clay ID directly.

The root ID returns directly. Operational bridge failures throw and leave the failed frame; no private unwrap helper is needed. Named `clayID(name)` overloads retain checked results.

**Example:**

```cpp
const Clay_ElementId id = context.clayID();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::childID**

#### `[[nodiscard]] Result<FlowElementID> childID(LocalElementName name) const noexcept;`

Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed; developer metadata can borrow its backing bytes.

**Example:**

```cpp
auto result = context.childID("label");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowElementID id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::childID**

#### `[[nodiscard]] Result<FlowElementID> childID(RuntimeElementName name) const noexcept;`

Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed; developer metadata can borrow its backing bytes.

**Example:**

```cpp
auto result = context.childID(FlowUi::RuntimeName("label"));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowElementID id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::childID**

#### `[[nodiscard]] Result<FlowElementID> childID(IndexedElementName name) const noexcept;`

Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed; developer metadata can borrow its backing bytes.

**Example:**

```cpp
auto result = context.childID(FlowUi::Indexed("row", 0));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowElementID id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::clayID**

#### `[[nodiscard]] Result<Clay_ElementId> clayID(LocalElementName name) const noexcept;`

Return this element root Clay ID, or resolve and bridge a named child. Check the Result before using its value in Clay.

**Example:**

```cpp
auto result = context.clayID("label");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const Clay_ElementId id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::clayID**

#### `[[nodiscard]] Result<Clay_ElementId> clayID(RuntimeElementName name) const noexcept;`

Return this element root Clay ID, or resolve and bridge a named child. Check the Result before using its value in Clay.

**Example:**

```cpp
auto result = context.clayID(FlowUi::RuntimeName("label"));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const Clay_ElementId id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::clayID**

#### `[[nodiscard]] Result<Clay_ElementId> clayID(IndexedElementName name) const noexcept;`

Return this element root Clay ID, or resolve and bridge a named child. Check the Result before using its value in Clay.

**Example:**

```cpp
auto result = context.clayID(FlowUi::Indexed("row", 0));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const Clay_ElementId id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::part**

#### `[[nodiscard]] Result<FlowElementPartID> part(FlowElementPart declaration) const noexcept;`

Bind a definition-time semantic part declaration to this concrete owner and return its FlowElementPartID.

**Example:**

```cpp
constexpr auto declaration = FlowUi::Part("label");
auto result = context.part(declaration);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowElementPartID id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::createPart**

#### `template <FlowElement PartElement> [[nodiscard]] ElementBuilder<PartElement> createPart( const PartElement& element, FlowElementPart declaration) const;`

Create a direct child builder at a semantic part address. Validation and parameter-construction exceptions propagate and fail the frame.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementBuildContext::indexedIDs**

#### `[[nodiscard]] constexpr IndexedElementIDSequence indexedIDs( LocalElementName baseName, uint64_t firstIndex = 0) const noexcept;`

Create a positional child-name sequence. Names follow position after reordering; use Keyed names for stable data identity.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::ElementInteractionContext**

#### `ElementInteractionContext( detail::element::ElementInvocation<ElementType>& invocation, ParametersType& parameters, const InteractionSnapshot& interaction) noexcept;`

Framework-created callback context with the previous completed interaction snapshot. Application hooks receive this by reference; do not retain it.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::state**

#### `template <typename E = ElementType> requires HasState<E> && std::same_as<E, ElementType> [[nodiscard]] StateOf<E>& state();`

Return a direct mutable or const state reference for this exact element type. Available only with HasState; the template argument cannot select another element type.

The reference borrows manager-owned per-window state for this callback through the active invocation lease. State validation failures propagate and latch frame failure; cancel the failed frame.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::state**

#### `template <typename E = ElementType> requires HasState<E> && std::same_as<E, ElementType> [[nodiscard]] const StateOf<E>& state() const;`

Return a direct mutable or const state reference for this exact element type. Available only with HasState; the template argument cannot select another element type.

The reference borrows manager-owned per-window state for this callback through the active invocation lease. State validation failures propagate and latch frame failure; cancel the failed frame.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::resources**

#### `template <typename E = ElementType> requires HasResources<E> && std::same_as<E, ElementType> [[nodiscard]] const ResourcesOf<E>& resources() const;`

Lazily initialize and return this exact definition immutable app-wide resources by const reference. Available only with HasResources.

The reference borrows manager-owned app-wide data for this callback. Resource-constructor and allocation exceptions propagate and latch frame failure; cancel the failed frame. Resources are shared across all instances of this definition.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::clayID**

#### `[[nodiscard]] Clay_ElementId clayID() const;`

Return this element root Clay ID directly.

The root ID returns directly. Operational bridge failures throw and leave the failed frame; no private unwrap helper is needed. Named `clayID(name)` overloads retain checked results.

**Example:**

```cpp
const Clay_ElementId id = context.clayID();
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::childID**

#### `[[nodiscard]] Result<FlowElementID> childID(LocalElementName name) const noexcept;`

Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed; developer metadata can borrow its backing bytes.

**Example:**

```cpp
auto result = context.childID("label");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowElementID id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::childID**

#### `[[nodiscard]] Result<FlowElementID> childID(RuntimeElementName name) const noexcept;`

Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed; developer metadata can borrow its backing bytes.

**Example:**

```cpp
auto result = context.childID(FlowUi::RuntimeName("label"));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowElementID id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::childID**

#### `[[nodiscard]] Result<FlowElementID> childID(IndexedElementName name) const noexcept;`

Resolve a local, runtime or indexed/keyed child name under this concrete element and return its strong FlowElementID. Runtime text is hashed; developer metadata can borrow its backing bytes.

**Example:**

```cpp
auto result = context.childID(FlowUi::Indexed("row", 0));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowElementID id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::clayID**

#### `[[nodiscard]] Result<Clay_ElementId> clayID(LocalElementName name) const noexcept;`

Return this element root Clay ID, or resolve and bridge a named child. Check the Result before using its value in Clay.

**Example:**

```cpp
auto result = context.clayID("label");
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const Clay_ElementId id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::clayID**

#### `[[nodiscard]] Result<Clay_ElementId> clayID(RuntimeElementName name) const noexcept;`

Return this element root Clay ID, or resolve and bridge a named child. Check the Result before using its value in Clay.

**Example:**

```cpp
auto result = context.clayID(FlowUi::RuntimeName("label"));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const Clay_ElementId id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::clayID**

#### `[[nodiscard]] Result<Clay_ElementId> clayID(IndexedElementName name) const noexcept;`

Return this element root Clay ID, or resolve and bridge a named child. Check the Result before using its value in Clay.

**Example:**

```cpp
auto result = context.clayID(FlowUi::Indexed("row", 0));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const Clay_ElementId id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::part**

#### `[[nodiscard]] Result<FlowElementPartID> part(FlowElementPart declaration) const noexcept;`

Bind a definition-time semantic part declaration to this concrete owner and return its FlowElementPartID.

**Example:**

```cpp
constexpr auto declaration = FlowUi::Part("label");
auto result = context.part(declaration);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FlowElementPartID id = *result;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::createPart**

#### `template <FlowElement PartElement> [[nodiscard]] ElementBuilder<PartElement> createPart( const PartElement& element, FlowElementPart declaration) const;`

Create a direct child builder at a semantic part address. Validation and parameter-construction exceptions propagate and fail the frame.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::indexedIDs**

#### `[[nodiscard]] constexpr IndexedElementIDSequence indexedIDs( LocalElementName baseName, uint64_t firstIndex = 0) const noexcept;`

Create a positional child-name sequence. Names follow position after reordering; use Keyed names for stable data identity.

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::invoke**

#### `[[nodiscard]] ActionInvocationStatus invoke(ActionCall call);`

Invoke this action synchronously with this window/element/hook as source. Return a direct ActionInvocationStatus; callback exceptions propagate unchanged.

**Example:**

```cpp
const FlowUi::ActionInvocationStatus outcome = actions.invoke(call);
const bool invoked = outcome == FlowUi::ActionInvocationStatus::Invoked;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **ElementInteractionContext::actionAvailability**

#### `[[nodiscard]] Result<ActionAvailability> actionAvailability(ActionCall call) const noexcept;`

Return checked effective action availability without invoking the call. Unavailable/empty/unbound calls have enabled=false; structured resolution failures remain distinct errors.

**Example:**

```cpp
auto result = context.actionAvailability(call);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool enabled = result->enabled;
```

See: [Owning header](include/FlowUi/Public/elementAuthoring/ElementContexts.hpp).

### **FlowUi::operator |**

#### `[[nodiscard]] constexpr ElementDrawOptions operator|(ElementDrawOptions first_options, ElementDrawOptions second_options) noexcept;`

Combine callback skip flags without throwing. SkipBuildCallback affects draw(), while construct() still calls constructElement().

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **FlowUi::elementDrawOptionsHas**

#### `[[nodiscard]] constexpr bool elementDrawOptionsHas(ElementDrawOptions value, ElementDrawOptions flag) noexcept;`

Return whether value and flag share any set bit; Default (zero) is never reported as set.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **FlowUi::elementSet**

#### `template <typename... Elements> requires ((FlowElement<std::remove_cvref_t<Elements>>) && ...) [[nodiscard]] constexpr auto elementSet(const Elements&...) noexcept;`

Deduce a zero-data catalog of element definition types. Definition objects and their addresses are not retained.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ElementStatePolicy::transient**

#### `[[nodiscard]] static constexpr ElementStatePolicy transient( uint32_t expiryFrames = DefaultElementStateGraceFrames) noexcept;`

Create a transient policy retaining state for expiryFrames successfully committed absent frames, then expiring on the next absent frame. Canceled frames do not advance retention.

### **ElementStatePolicy::windowLifetime**

#### `[[nodiscard]] static constexpr ElementStatePolicy windowLifetime() noexcept;`

Create a policy retaining state until explicit erasure or owning-window destruction.

### **ElementStatePolicy::operator <=>**

#### `[[nodiscard]] auto operator<=>(const ElementStatePolicy&) const noexcept = default;`

Compare retention mode and grace period by value.

### **ElementManager::ElementManager**

#### `ElementManager() = default;`

Construct a detached manager; application code normally obtains the initialized App-owned manager.

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::~ElementManager**

#### `~ElementManager();`

Release owned service data. The App controls this manager lifetime.

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::ElementManager**

#### `ElementManager(const ElementManager&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::operator =**

#### `ElementManager& operator=(const ElementManager&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::ElementManager**

#### `ElementManager(ElementManager&&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::operator =**

#### `ElementManager& operator=(ElementManager&&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::getStatePointerConst**

#### `template <FlowElement Element> requires HasState<Element> [[nodiscard]] Result<const StateOf<Element>*> getStatePointerConst( const Element&, WindowId window, FlowElementID elementId) const noexcept;`

Look up immutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.

The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.

**Example:**

```cpp
auto result = elements.getStatePointerConst(counter, window, id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto* state = *result;
if (state) {
    const int count = state->count;
}
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::getStatePointerConst**

#### `template <FlowElement Element> requires HasState<Element> [[nodiscard]] Result<const StateOf<Element>*> getStatePointerConst( const Element&, WindowId window, GlobalFlowID elementId) const noexcept;`

Look up immutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.

The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.

**Example:**

```cpp
auto result = elements.getStatePointerConst(counter, window, id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto* state = *result;
if (state) {
    const int count = state->count;
}
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::getStatePointerConst**

#### `template <FlowElement Element> requires HasState<Element> [[nodiscard]] Result<const StateOf<Element>*> getStatePointerConst( const Element&, WindowId window, FlowElementPartID elementId) const noexcept;`

Look up immutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.

The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.

**Example:**

```cpp
auto result = elements.getStatePointerConst(counter, window, id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto* state = *result;
if (state) {
    const int count = state->count;
}
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::getStatePointer**

#### `template <FlowElement Element> requires HasState<Element> [[nodiscard]] Result<StateOf<Element>*> getStatePointer( const Element&, WindowId window, FlowElementID elementId) noexcept;`

Look up mutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.

The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.

**Example:**

```cpp
auto result = elements.getStatePointer(counter, window, id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto* state = *result;
if (state) {
    const int count = state->count;
}
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::getStatePointer**

#### `template <FlowElement Element> requires HasState<Element> [[nodiscard]] Result<StateOf<Element>*> getStatePointer( const Element&, WindowId window, FlowElementPartID elementId) noexcept;`

Look up mutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.

The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.

**Example:**

```cpp
auto result = elements.getStatePointer(counter, window, id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto* state = *result;
if (state) {
    const int count = state->count;
}
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::getStatePointer**

#### `template <FlowElement Element> requires HasState<Element> [[nodiscard]] Result<StateOf<Element>*> getStatePointer( const Element&, WindowId window, GlobalFlowID elementId) noexcept;`

Look up mutable existing state for this definition, window and typed identity without creating state. Check failure separately from a successful nullptr.

The pointer is borrowed until explicit erasure, transient collection at a later successful frame boundary or window destruction. Do not retain transient state pointers across frames. Invalid owner/identity/access conditions are returned as errors.

**Example:**

```cpp
auto result = elements.getStatePointer(counter, window, id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto* state = *result;
if (state) {
    const int count = state->count;
}
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::eraseState**

#### `template <FlowElement Element> requires HasState<Element> [[nodiscard]] Result<bool> eraseState( const Element&, WindowId window, FlowElementID elementId) noexcept;`

Erase this instance state, returning whether an existing state was selected for erasure. During a window build erasure is deferred until successful commit; cancellation discards queued erasures.

**Example:**

```cpp
auto result = elements.eraseState(counter, window, id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool erased = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::eraseState**

#### `template <FlowElement Element> requires HasState<Element> [[nodiscard]] Result<bool> eraseState( const Element&, WindowId window, FlowElementPartID elementId) noexcept;`

Erase this instance state, returning whether an existing state was selected for erasure. During a window build erasure is deferred until successful commit; cancellation discards queued erasures.

**Example:**

```cpp
auto result = elements.eraseState(counter, window, id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool erased = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::eraseState**

#### `template <FlowElement Element> requires HasState<Element> [[nodiscard]] Result<bool> eraseState( const Element&, WindowId window, GlobalFlowID elementId) noexcept;`

Erase this instance state, returning whether an existing state was selected for erasure. During a window build erasure is deferred until successful commit; cancellation discards queued erasures.

**Example:**

```cpp
auto result = elements.eraseState(counter, window, id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool erased = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::collectStateGarbage**

#### `[[nodiscard]] size_t collectStateGarbage(WindowId window) noexcept;`

Collect all currently expired transient state for one window and return the number removed. Successful frame completion normally handles retirement.

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::prepare**

#### `template <FlowElement Element> [[nodiscard]] Status prepare(const Element&);`

Eagerly prepare the immutable app-wide resources of one definition or every definition in an ElementSet. Resource-free definitions are successful no-ops.

Hybrid contract: a resourceful definition without an initialized owner returns ObjectNotInitialized. Resource constructor and allocation exceptions propagate; already prepared definitions remain ready and preparation can be retried.

**Example:**

```cpp
auto result = elements.prepare(counter);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

### **ElementManager::prepare**

#### `template <FlowElement... Elements> [[nodiscard]] Status prepare(const ElementSet<Elements...>&);`

Eagerly prepare the immutable app-wide resources of one definition or every definition in an ElementSet. Resource-free definitions are successful no-ops.

Hybrid contract: a resourceful definition without an initialized owner returns ObjectNotInitialized. Resource constructor and allocation exceptions propagate; already prepared definitions remain ready and preparation can be retried.

**Example:**

```cpp
auto result = elements.prepare(FlowUi::elementSet(counter, FlowUi::kButton));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
```

See: [Owning header](include/FlowUi/Public/managers/ElementManager.hpp).

## Standard Button callback API

The [standard Button](include/FlowUi/Public/elements/Button.hpp) is a stateless definition exposed as `FlowUi::kButton`, with aliases `Parameters = ButtonParameters`, `BuildContext = ElementBuildContext<Button>` and `InteractionContext = ElementInteractionContext<Button>`. Application code normally configures it through `ui.createElement(FlowUi::kButton, "save").setParameters(...).draw()`. The builder creates and supplies borrowed callback contexts; these hooks operate on an active invocation and do not construct a standalone context or retain it.

| Public hook signature | Current behavior |
| --- | --- |
| `[[nodiscard]] static bool isEnabled(const InteractionContext& context) noexcept` | Require `params.enabled` and checked effective action availability. A failed availability lookup records the frame error and returns false. |
| `static void onHovered(InteractionContext& context) noexcept` | Request the pointing-hand cursor when this control and action are enabled. |
| `static void onPressed(InteractionContext& context)` | Synchronously invoke `params.onActivate` when enabled. User callback exceptions propagate through the builder frame boundary. |
| `static void buildElement(BuildContext& context) noexcept` | Emit the root and label using checked identity/text conversions. Failed conversions record the frame error and return; disabled state selects disabled styling. |

`ButtonParameters` supplies `onActivate`, borrowed `label`, `enabled`, root `sizing`, background/text/border colors (including disabled colors), `cornerRadius`, `borderWidth`, `padding`, `fontId` and `fontSize`. Keep the label and any action resources alive through their use. Button has no retained instance state; action availability drives activation behavior.
