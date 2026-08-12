# Element API

## Strong identity types

### **FlowDefinitionID**

Identity of an element definition, normally declared with `DefinitionID("name")`.

### **FlowElementID**

Resolved identity of one local element instance. Its value is composed from the parent scope, definition ID, and local name token.

### **GlobalFlowID**

Explicit definition-scoped identity that can be reproduced outside the element's local branch with `Global<kElement>("name")`.

### **FlowElementPart** and **FlowElementPartID**

`FlowElementPart` declares a semantic sub-element address. `context.part(declaration)` or `PartID(...)` binds that declaration to one owner instance and returns `FlowElementPartID`.

## Enums

### **ElementDrawOptions**


#### `enum class ElementDrawOptions : uint32_t`

Flags controlling which callbacks ElementBuilder executes. Combine values with operator| to skip event, logic, or build callbacks for one invocation.

## Public Structs

### **InteractionSnapshot**


#### `struct InteractionSnapshot`

Snapshot of hover, press, hold, and release state for one completed frame. Element callbacks query the previous snapshot for stable interaction decisions.

### **NoElementParameters**


#### `struct NoElementParameters`

Empty marker type used by elements with no parameter struct.

### **NoElementState**


#### `struct NoElementState`

Empty marker type used by elements with no state struct.

### **NoElementResources**


#### `struct NoElementResources`

Empty marker type used by elements with no shared resource struct.

### **ElementBuildContext**

#### `template <typename Element> struct ElementBuildContext`

Context passed to construct and build callbacks. It exposes `uiManager`, typed `id`, parameters, state/resources where present, `childID()`, `clayID()`, and semantic-part helpers.

### **ElementInteractionContext**


#### `template <typename Element> struct ElementInteractionContext`

Context passed to event and logic callbacks. It exposes the same typed identity and semantic helpers plus previous-frame interaction data.

### **ElementBuilder**


#### `template <FlowElement Element> class ElementBuilder`

Builder returned by `UiManager::createElement`. It stores the resolved typed identity and parameters until `draw()` or `construct()` executes the element flow.

## Public API

### **DefinitionID**

#### `consteval FlowDefinitionID DefinitionID(const char (&name)[N])`

Creates a strong definition identity from a stable literal.

### **Global**

#### `consteval GlobalFlowID Global<kElement>(const char (&name)[N])`

Creates an explicit globally addressable identity scoped by the target element definition.

### **Part** and **PartID**

`Part("name")` declares a semantic part. `PartID(kOwner, ownerId, declaration)` binds it outside an owner callback; inside a callback prefer `context.part(declaration)`.

### **Indexed**, **Keyed**, and **IndexedIDs**

These factories create numeric name tokens for repeated elements. Use `Keyed()` when identity should follow stable data and `Indexed()`/`IndexedIDs()` when identity deliberately follows position.

### **RuntimeName** and **AutoID**

`RuntimeName()` is the explicit runtime hashing escape hatch. `AutoID()` identifies a stable callsite and is also the default for `createElement(kElement)`; it must not be used repeatedly from one loop callsite.

### **operator|**


#### `ElementDrawOptions operator|(ElementDrawOptions a, ElementDrawOptions b)`

- **Returns:** `ElementDrawOptions`
- **Arguments:** `a` first option flag, `b` second option flag.

Combines draw-option flags for `ElementBuilder::draw()` and `ElementBuilder::construct()`. This is the normal way to skip multiple callback phases in one builder call.

**Example:**

```cpp
auto options = FlowUi::ElementDrawOptions::SkipEventCallbacks | FlowUi::ElementDrawOptions::SkipLogicCallback;
```

See: [Full Doxygen reference](group__flowui__element__system.html#ga83272f87e6796154839aa8e2df16f531).

### **elementDrawOptionsHas**


#### `bool elementDrawOptionsHas(ElementDrawOptions value, ElementDrawOptions flag)`

- **Returns:** `bool`
- **Arguments:** `value` combined option value, `flag` flag to test.

Checks whether an `ElementDrawOptions` value contains a specific flag. This is mostly useful inside infrastructure or advanced element helper code.

**Example:**

```cpp
const bool skipsLogic = FlowUi::elementDrawOptionsHas(options, FlowUi::ElementDrawOptions::SkipLogicCallback);
```

See: [Full Doxygen reference](group__flowui__element__system.html#ga1a34831c629e5d93e711ca00c1f12bf2).

### **ElementBuilder**


#### `ElementBuilder(UiManager& uiManager, const DefinitionType* definition, std::string elementID)`

- **Returns:** `ElementBuilder`
- **Arguments:** `uiManager` active UI manager, `definition` element definition pointer, `elementID` stable element instance id.

Constructs a builder for one element invocation. User code normally gets builders from `UiManager::createElement()` rather than calling this constructor directly.

**Example:**

```cpp
auto builder = app.ui().createElement(kButton, "toolbar/save");
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#a46780ec97486821e4b9f366bbab8fdab).

### **setParameters** `1/2`


#### `ElementBuilder& setParameters(const ParametersType& parameters)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `parameters` parameter values to copy.

Copies parameter values into the builder. The stored parameters are passed to interaction, logic, construct, and build callbacks.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").setParameters(ButtonParams{.label = "Save"}).draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#a55159e63934e32a22bd30889b5639a99).

### **setParameters** `2/2`


#### `ElementBuilder& setParameters(ParametersType&& parameters)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `parameters` parameter values to move.

Moves parameter values into the builder. Use this when parameter construction is dynamic or owns heavier data.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").setParameters(ButtonParams{.label = "Save"}).draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#a711158a45396b9b1a83522eaa051f26e).

### **mergeParams**


#### `template <typename MergeFn> ElementBuilder& mergeParams(MergeFn&& mergeFn)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `mergeFn` callable invoked with `ParametersType&`.

Mutates the builder's existing parameter storage. This is useful when defaults are mostly correct and only a few fields need to be changed.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").mergeParams([](ButtonParams& params) { params.enabled = false; }).draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#a0cb3694ec3a0bc7ac94feee7217f4788).

### **withElementID**


#### `ElementBuilder& withElementID(std::string_view elementID)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `elementID` replacement element id string.

Replaces the id stored by the builder. Use it when a builder is created before the final stable id is known.

**Example:**

```cpp
buttonBuilder.withElementID("toolbar/save-secondary").draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#addcbec2dd601b254be83f4114cb679f7).

### **setDevInternalCapture**


#### `ElementBuilder& setDevInternalCapture(bool isDevInternal = true)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `isDevInternal` whether dev capture should mark this invocation internal.

Controls how this element invocation is captured in developer mode. Normal user elements usually do not need this.

**Example:**

```cpp
app.ui().createElement(kDevPanel, "flowui/dev/panel").setDevInternalCapture(true).draw();
```

See: [Full Doxygen reference](classFlowUi_1_1ElementBuilder.html#ab2a74747085369a4b157d804c85f9699).

### **construct**


#### `void construct(ElementDrawOptions options = ElementDrawOptions::Default)`

- **Returns:** `void`
- **Arguments:** `options` callback phases to skip.

Runs enabled callbacks and opens a constructed Clay root using the definition's `constructElement` callback. Emit child nodes after this call, then close the element with `UiManager::drawConstructed()`.

**Example:**

```cpp
app.ui().createElement(kPanel, "settings").construct();
```

See: [Full Doxygen reference](group__flowui__element__system.html#gadd7adf768823bd01379c9054c516448f).

### **draw**


#### `void draw(ElementDrawOptions options = ElementDrawOptions::Default)`

- **Returns:** `void`
- **Arguments:** `options` callback phases to skip.

Runs enabled callbacks and emits the element through its `buildElement` callback. This is the most common final call for a FlowUi element builder.

**Example:**

```cpp
app.ui().createElement(kButton, "toolbar/save").draw();
```

See: [Full Doxygen reference](group__flowui__element__system.html#ga7cefdbb08aab1d49879d08adc61d0509).

### **contains**


#### `static bool contains(const std::vector<Clay_ElementId>& list, Clay_ElementId id)`

- **Returns:** `bool`
- **Arguments:** `list` interaction id list, `id` Clay element id to find.

Checks whether a Clay element id exists in an interaction list. Comparison uses the underlying Clay id value.

**Example:**

```cpp
const bool hasButton = FlowUi::InteractionSnapshot::contains(snapshot.pressedElementIds, buttonId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#a63d6503a6bcc503717b0ad8ee9fca8cd).

### **isHovered**


#### `bool isHovered(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element was hovered in this snapshot. Element callbacks commonly use this for child interaction checks.

**Example:**

```cpp
const bool hovered = context.previousInteraction.isHovered(rootId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#ad98a8c6a0e0664f27c24c97c11e1e031).

### **isPressed**


#### `bool isPressed(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element received a primary pointer press in this snapshot. This is based on the previous completed frame when used from element callbacks.

**Example:**

```cpp
const bool pressed = context.previousInteraction.isPressed(rootId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#a389ad759f1f175955c065a5fe6a88b5b).

### **isHeld**


#### `bool isHeld(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element was held by the primary pointer in this snapshot. Use it for drag-like or continuous pressed behavior.

**Example:**

```cpp
const bool held = context.previousInteraction.isHeld(rootId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#acb7231c76df9dbfbba2bdc1c02daeb02).

### **isReleased**


#### `bool isReleased(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element received a primary pointer release in this snapshot. Use it for release-triggered actions.

**Example:**

```cpp
const bool released = context.previousInteraction.isReleased(rootId);
```

See: [Full Doxygen reference](structFlowUi_1_1InteractionSnapshot.html#ad70c5be92f52d3e35a7bea1d7c904874).

### **createChildElementId**


#### `std::string createChildElementId(std::string_view localChildId) const`

- **Returns:** `std::string`
- **Arguments:** `localChildId` child id segment or relative child path.

Creates a stable child id by appending the local child id to the current element id. Use this for child Clay nodes or nested Flow elements owned by the current element.

**Example:**

```cpp
Clay_ElementId labelId = context.uiManager.toClayEID(context.createChildElementId("label"));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementBuildContext.html#ac92999f9addc15917028cea0cbdf39a3).

### **createChildElementId**


#### `std::string createChildElementId(std::string_view localChildId) const`

- **Returns:** `std::string`
- **Arguments:** `localChildId` child id segment or relative child path.

Creates a stable child id from inside interaction or logic callbacks. This is useful when querying previous interaction for child elements.

**Example:**

```cpp
Clay_ElementId labelId = context.uiManager.toClayEID(context.createChildElementId("label"));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementInteractionContext.html#ad1a4815b3e3cf5fdae8651902903b05a).

### **getResources**


#### `static ResourcesType& getResources(App& app)`

- **Returns:** `ResourcesType&`
- **Arguments:** `app` active FlowUi app used for resource construction.

Lazily creates and returns the shared resources instance for this element definition specialization. Available only when the `Resources` template argument is not `void`.

**Example:**

```cpp
ButtonResources& resources = ButtonDefinition::getResources(app);
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#acd4597db7232a8da38104d1c14fd8abb).

### **getOrCreateState**


#### `static StateType& getOrCreateState(uint64_t elementFlowId)`

- **Returns:** `StateType&`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Returns existing state for an element instance or creates default state when missing. Available only when the `State` template argument is not `void`.

**Example:**

```cpp
ButtonState& state = ButtonDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#ab8d37b94a61ee757c7656e01bd69849e).

### **tryGetState**


#### `static StateType* tryGetState(uint64_t elementFlowId)`

- **Returns:** `StateType*`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Looks up mutable state without creating it. Returns `nullptr` when the element instance has no stored state.

**Example:**

```cpp
if (ButtonState* state = ButtonDefinition::tryGetState(FLOW_ID("toolbar/save"))) { state->pressed = false; }
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#a8245c51a90d1d7e66df56670be390b05).

### **tryGetStateConst**


#### `static const StateType* tryGetStateConst(uint64_t elementFlowId)`

- **Returns:** `const StateType*`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Looks up immutable state without creating it. Use this for read-only checks outside element callbacks.

**Example:**

```cpp
const ButtonState* state = ButtonDefinition::tryGetStateConst(FLOW_ID("toolbar/save"));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#a1d8bacf80f61458c0b24b2b49b7d897d).

### **eraseState**


#### `static bool eraseState(uint64_t elementFlowId)`

- **Returns:** `bool`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Erases stored state for one element instance. FlowUi does not automatically garbage-collect custom element state, so dynamic UI can use this to keep state pools bounded.

**Example:**

```cpp
const bool erased = ButtonDefinition::eraseState(FLOW_ID("toolbar/save"));
```

See: [Full Doxygen reference](structFlowUi_1_1ElementDefinition.html#a86a3273921a616d575a46a3538b88382).
