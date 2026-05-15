# Element API

## Aliases

### **FlowElementId**
---

#### `using FlowElementId = uint64_t`

Stable hashed id for an element instance. It is used for state lookup and stable generated child ids.

### **FlowDefinitionId**
---

#### `using FlowDefinitionId = uint64_t`

Stable hashed id for an element definition. It identifies the element definition specialization.

## Enums

### **ElementDrawOptions**
---

#### `enum class ElementDrawOptions : uint32_t`

Flags controlling which callbacks ElementBuilder executes. Combine values with operator| to skip event, logic, or build callbacks for one invocation.

## Public Structs

### **InteractionSnapshot**
---

#### `struct InteractionSnapshot`

Snapshot of hover, press, hold, and release state for one completed frame. Element callbacks query the previous snapshot for stable interaction decisions.

### **NoElementParameters**
---

#### `struct NoElementParameters`

Empty marker type used by elements with no parameter struct.

### **NoElementState**
---

#### `struct NoElementState`

Empty marker type used by elements with no state struct.

### **NoElementResources**
---

#### `struct NoElementResources`

Empty marker type used by elements with no shared resource struct.

### **ElementBuildContext**
---

#### `template <typename Parameters> struct ElementBuildContext`

Context passed to construct and build callbacks. It exposes the active UiManager, current element id, parameters, and child-id helper.

### **ElementInteractionContext**
---

#### `template <typename Parameters> struct ElementInteractionContext`

Context passed to event and logic callbacks. It exposes UiManager, element id, parameters, previous interaction data, and child-id helper.

### **ElementDefinition**
---

#### `template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal> struct ElementDefinition`

Typed definition for a FlowUi element. It binds params, state, resources, definition id, callback fields, resource storage, and state storage to an element builder flow.

### **ElementBuilder**
---

#### `template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal> class ElementBuilder`

Builder returned by UiManager::createElement. It stores definition, id, and params until draw or construct executes the element flow.

## Public API

### **toFlowId** `1/2`
---

#### `constexpr FlowElementId toFlowId(std::string_view elementName) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `elementName` string to hash into an element instance id.

Hashes a runtime string into a stable FlowUi element id. Use this when looking up or managing state for an element instance outside the builder path.

### **toFlowId** `2/2`
---

#### `template <std::size_t N> constexpr FlowElementId toFlowId(const char (&elementName)[N]) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `elementName` string literal to hash into an element instance id.

String-literal overload for `toFlowId`. It avoids counting the terminating null byte and can be used in constant expressions.

### **toFlowDefinitionId** `1/2`
---

#### `constexpr FlowDefinitionId toFlowDefinitionId(std::string_view definitionName) noexcept`

- **Returns:** `FlowDefinitionId`
- **Arguments:** `definitionName` string to hash into an element definition id.

Hashes a runtime string into a stable FlowUi element definition id. This is the function behind definition ids used by `ElementDefinition`.

### **toFlowDefinitionId** `2/2`
---

#### `template <std::size_t N> constexpr FlowDefinitionId toFlowDefinitionId(const char (&definitionName)[N]) noexcept`

- **Returns:** `FlowDefinitionId`
- **Arguments:** `definitionName` string literal to hash into an element definition id.

String-literal overload for `toFlowDefinitionId`. Prefer this through `FLOW_DEF_ID("name")` when declaring custom element definition types.

### **createIndexedFlowId** `1/3`
---

#### `constexpr FlowElementId createIndexedFlowId(FlowElementId rootId, uint64_t index) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `rootId` parent/root Flow id, `index` numeric child/index value.

Creates a stable child-style id by mixing an existing Flow id with an index. This is useful for repeated UI rows or generated children where a string id would be awkward.

### **createIndexedFlowId** `2/3`
---

#### `constexpr FlowElementId createIndexedFlowId(std::string_view rootName, uint64_t index) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `rootName` parent/root name, `index` numeric child/index value.

Hashes the root name and then mixes in the numeric index. Use this when generating stable ids from a named collection or repeated layout section.

### **createIndexedFlowId** `3/3`
---

#### `template <std::size_t N> constexpr FlowElementId createIndexedFlowId(const char (&rootName)[N], uint64_t index) noexcept`

- **Returns:** `FlowElementId`
- **Arguments:** `rootName` string literal parent/root name, `index` numeric child/index value.

String-literal overload for indexed id creation. It is useful for compile-time root names paired with runtime loop indexes.

### **operator|**
---

#### `ElementDrawOptions operator|(ElementDrawOptions a, ElementDrawOptions b)`

- **Returns:** `ElementDrawOptions`
- **Arguments:** `a` first option flag, `b` second option flag.

Combines draw-option flags for `ElementBuilder::draw()` and `ElementBuilder::construct()`. This is the normal way to skip multiple callback phases in one builder call.

### **elementDrawOptionsHas**
---

#### `bool elementDrawOptionsHas(ElementDrawOptions value, ElementDrawOptions flag)`

- **Returns:** `bool`
- **Arguments:** `value` combined option value, `flag` flag to test.

Checks whether an `ElementDrawOptions` value contains a specific flag. This is mostly useful inside infrastructure or advanced element helper code.

### **ElementBuilder**
---

#### `ElementBuilder(UiManager& uiManager, const DefinitionType* definition, std::string elementID)`

- **Returns:** `ElementBuilder`
- **Arguments:** `uiManager` active UI manager, `definition` element definition pointer, `elementID` stable element instance id.

Constructs a builder for one element invocation. User code normally gets builders from `UiManager::createElement()` rather than calling this constructor directly.

### **setParameters** `1/2`
---

#### `ElementBuilder& setParameters(const ParametersType& parameters)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `parameters` parameter values to copy.

Copies parameter values into the builder. The stored parameters are passed to interaction, logic, construct, and build callbacks.

### **setParameters** `2/2`
---

#### `ElementBuilder& setParameters(ParametersType&& parameters)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `parameters` parameter values to move.

Moves parameter values into the builder. Use this when parameter construction is dynamic or owns heavier data.

### **mergeParams**
---

#### `template <typename MergeFn> ElementBuilder& mergeParams(MergeFn&& mergeFn)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `mergeFn` callable invoked with `ParametersType&`.

Mutates the builder's existing parameter storage. This is useful when defaults are mostly correct and only a few fields need to be changed.

### **withElementID**
---

#### `ElementBuilder& withElementID(std::string_view elementID)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `elementID` replacement element id string.

Replaces the id stored by the builder. Use it when a builder is created before the final stable id is known.

### **setDevInternalCapture**
---

#### `ElementBuilder& setDevInternalCapture(bool isDevInternal = true)`

- **Returns:** `ElementBuilder&`
- **Arguments:** `isDevInternal` whether dev capture should mark this invocation internal.

Controls how this element invocation is captured in developer mode. Normal user elements usually do not need this.

### **construct**
---

#### `void construct(ElementDrawOptions options = ElementDrawOptions::Default)`

- **Returns:** `void`
- **Arguments:** `options` callback phases to skip.

Runs enabled callbacks and opens a constructed Clay root using the definition's `constructElement` callback. Emit child nodes after this call, then close the element with `UiManager::drawConstructed()`.

### **draw**
---

#### `void draw(ElementDrawOptions options = ElementDrawOptions::Default)`

- **Returns:** `void`
- **Arguments:** `options` callback phases to skip.

Runs enabled callbacks and emits the element through its `buildElement` callback. This is the most common final call for a FlowUi element builder.

### **contains**
---

#### `static bool contains(const std::vector<Clay_ElementId>& list, Clay_ElementId id)`

- **Returns:** `bool`
- **Arguments:** `list` interaction id list, `id` Clay element id to find.

Checks whether a Clay element id exists in an interaction list. Comparison uses the underlying Clay id value.

### **isHovered**
---

#### `bool isHovered(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element was hovered in this snapshot. Element callbacks commonly use this for child interaction checks.

### **isPressed**
---

#### `bool isPressed(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element received a primary pointer press in this snapshot. This is based on the previous completed frame when used from element callbacks.

### **isHeld**
---

#### `bool isHeld(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element was held by the primary pointer in this snapshot. Use it for drag-like or continuous pressed behavior.

### **isReleased**
---

#### `bool isReleased(Clay_ElementId id) const`

- **Returns:** `bool`
- **Arguments:** `id` Clay element id.

Reports whether the element received a primary pointer release in this snapshot. Use it for release-triggered actions.

### **createChildElementId**
---

#### `std::string createChildElementId(std::string_view localChildId) const`

- **Returns:** `std::string`
- **Arguments:** `localChildId` child id segment or relative child path.

Creates a stable child id by appending the local child id to the current element id. Use this for child Clay nodes or nested Flow elements owned by the current element.

### **createChildElementId**
---

#### `std::string createChildElementId(std::string_view localChildId) const`

- **Returns:** `std::string`
- **Arguments:** `localChildId` child id segment or relative child path.

Creates a stable child id from inside interaction or logic callbacks. This is useful when querying previous interaction for child elements.

### **getResources**
---

#### `static ResourcesType& getResources(App& app)`

- **Returns:** `ResourcesType&`
- **Arguments:** `app` active FlowUi app used for resource construction.

Lazily creates and returns the shared resources instance for this element definition specialization. Available only when the `Resources` template argument is not `void`.

### **getOrCreateState**
---

#### `static StateType& getOrCreateState(uint64_t elementFlowId)`

- **Returns:** `StateType&`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Returns existing state for an element instance or creates default state when missing. Available only when the `State` template argument is not `void`.

### **tryGetState**
---

#### `static StateType* tryGetState(uint64_t elementFlowId)`

- **Returns:** `StateType*`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Looks up mutable state without creating it. Returns `nullptr` when the element instance has no stored state.

### **tryGetStateConst**
---

#### `static const StateType* tryGetStateConst(uint64_t elementFlowId)`

- **Returns:** `const StateType*`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Looks up immutable state without creating it. Use this for read-only checks outside element callbacks.

### **eraseState**
---

#### `static bool eraseState(uint64_t elementFlowId)`

- **Returns:** `bool`
- **Arguments:** `elementFlowId` Flow id for one element instance.

Erases stored state for one element instance. FlowUi does not automatically garbage-collect custom element state, so dynamic UI can use this to keep state pools bounded.
