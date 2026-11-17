# Action Manager API

[ActionManager.hpp](include/FlowUi/Public/managers/ActionManager.hpp) dispatches retained app actions and compact local UI calls. Its App-owned facade is available through checked `App::actions()`; `UiManager::actions()` returns a direct borrowed reference and throws if detached.

```cpp
int saveCount = 0;
auto managerResult = app.actions();
if (!managerResult) {
    throw FlowUi::FlowUiException(managerResult.error());
}
FlowUi::ActionManager& manager = managerResult->get();
auto result = manager.appActions().bind(FlowUi::AppAction("editor.save"),
    [](int& count) { ++count; }, std::ref(saveCount));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::AppActionCall save = *result;
```

The referenced `saveCount` must outlive binding retirement and every active invocation. Bind accepts function/member pointers, stateless concrete callables or explicitly borrowed callable objects. Arguments supplied as pointers or `std::ref`/`std::cref` are borrowed. Existing `AppActionCall` values resolve the current binding by identity; `rebind()` updates future calls while active invocations finish their original binding.

`bind()`/`rebind()` return checked `Result<AppActionCall>` while arbitrary callable construction exceptions can propagate. Ordinary invocation returns a direct `ActionInvocationStatus`; typed invocation uses the separate `ActionResult<T>` dispatch-error domain. Callback/result-movement exceptions propagate unchanged and side effects are not rolled back or automatically retried. See [result handling](docs/api/result_handling.md).

UI recipes must be stateless, trivial, concrete and return `void`. `UiActionCall::InlineBytes` is `2 * sizeof(void*)`: 16 bytes on a platform with 64-bit pointers. Reference arguments require matching lvalues; value arguments are stored in the fixed payload. Fallible argument conversion must happen before the `noexcept make()` call. Release-mode size assertions bound `UiActionCall` and `ActionCall` to 24 and 32 bytes respectively on supported 64-bit targets; development metadata can enlarge them.

`ActionCall{}` is inert. Availability observers do not invoke anything. `AppActionAvailability{.enabled = false}` disables a live app binding; UI calls with an operation are enabled directly. Conditional developer observation methods do not alter these application contracts.

### **AppActions::bind**

#### `template <typename Callable, typename... Resources> [[nodiscard]] Result<AppActionCall> bind(AppActionID id, Callable&& callable, Resources&&... resources);`

Create a retained application binding and return its AppActionCall. The ID must be nonzero and unbound; the descriptor overload also copies its diagnostic name.

Hybrid contract: structured binding validation/publication failures are returned; arbitrary callable construction exceptions propagate unchanged. Resources supplied as pointers/std::ref/std::cref are borrowed and must outlive binding retirement and all in-progress calls. Callable objects must meet the bind restrictions: function/member pointer, stateless concrete callable, or explicitly borrowed callable.

**Example:**

```cpp
int saveCount = 0;
auto result = actions.bind(FlowUi::AppAction("docs.save"), [](int& saveCount) { ++saveCount; }, std::ref(saveCount));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::AppActionCall save = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::bind**

#### `template <typename Callable, typename... Resources> [[nodiscard]] Result<AppActionCall> bind(AppActionDescriptor descriptor, Callable&& callable, Resources&&... resources);`

Create a retained application binding and return its AppActionCall. The ID must be nonzero and unbound; the descriptor overload also copies its diagnostic name.

Hybrid contract: structured binding validation/publication failures are returned; arbitrary callable construction exceptions propagate unchanged. Resources supplied as pointers/std::ref/std::cref are borrowed and must outlive binding retirement and all in-progress calls. Callable objects must meet the bind restrictions: function/member pointer, stateless concrete callable, or explicitly borrowed callable.

**Example:**

```cpp
int saveCount = 0;
auto result = actions.bind(FlowUi::AppActionDescriptor{.id = FlowUi::AppAction("docs.save"), .debugName = "Save"}, [](int& saveCount) { ++saveCount; }, std::ref(saveCount));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::AppActionCall save = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::select**

#### `[[nodiscard]] AppActionCall select(AppActionID id) const noexcept;`

Create a direct AppActionCall containing this identity without checking or creating its binding.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::isBound**

#### `[[nodiscard]] bool isBound(AppActionID id) const noexcept;`

Return whether the ID or call currently resolves to a live binding; an empty or unavailable binding is false.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::isBound**

#### `[[nodiscard]] bool isBound(AppActionCall call) const noexcept;`

Return whether the ID or call currently resolves to a live binding; an empty or unavailable binding is false.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::unbind**

#### `[[nodiscard]] Result<bool> unbind(AppActionID id) noexcept;`

Retire a binding and return whether one was removed. Successful false is ordinary absence; already active invocations may finish.

**Example:**

```cpp
auto result = actions.unbind(id);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool removed = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::rebind**

#### `template <typename Callable, typename... Resources> [[nodiscard]] Result<AppActionCall> rebind(AppActionDescriptor descriptor, Callable&& callable, Resources&&... resources);`

Replace an existing retained binding at the same identity. Active invocations finish on their original binding; future calls resolve the replacement. A missing binding is returned as an error.

Hybrid contract: structured binding validation/publication failures are returned; arbitrary callable construction exceptions propagate unchanged. Resources supplied as pointers/std::ref/std::cref are borrowed and must outlive binding retirement and all in-progress calls. Callable objects must meet the bind restrictions: function/member pointer, stateless concrete callable, or explicitly borrowed callable.

**Example:**

```cpp
int saveCount = 0;
auto result = actions.rebind(FlowUi::AppActionDescriptor{.id = FlowUi::AppAction("docs.save"), .debugName = "Save"}, [](int& saveCount) { ++saveCount; }, std::ref(saveCount));
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::AppActionCall save = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::invoke**

#### `[[nodiscard]] ActionInvocationStatus invoke(AppActionCall call);`

Invoke synchronously and directly return Invoked, Empty, Unbound or Disabled. An unavailable call does not run its callable.

Callback and owned-result movement exceptions propagate unchanged. Side effects are not rolled back or automatically retried. invokeFor requires nonthrowing destruction of owned payloads; reference and array result types are rejected.

**Example:**

```cpp
const FlowUi::ActionInvocationStatus outcome = actions.invoke(call);
const bool invoked = outcome == FlowUi::ActionInvocationStatus::Invoked;
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::invokeFor**

#### `template <typename Result> [[nodiscard]] ActionResult<Result> invokeFor(AppActionCall call);`

Invoke only when the bound result type exactly matches Result. Return ActionResult<Result>, whose errors are Empty, Unbound, Disabled and ResultTypeMismatch. This is distinct from FlowUi::Result<T>.

Callback and owned-result movement exceptions propagate unchanged. Side effects are not rolled back or automatically retried. invokeFor requires nonthrowing destruction of owned payloads; reference and array result types are rejected.

**Example:**

```cpp
auto result = actions.invokeFor<int>(countCall);
if (!result) {
    const FlowUi::ActionInvokeError error = result.error();
    return; // Handle the dispatch failure in the surrounding application code.
}
const int displayedCount = result.value();
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::availability**

#### `[[nodiscard]] AppActionAvailability availability(AppActionCall call) const noexcept;`

Observe effective enabled state directly; empty, unavailable and unbound calls are disabled.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **AppActions::setAvailability**

#### `[[nodiscard]] Result<bool> setAvailability(AppActionID id, AppActionAvailability availability) noexcept;`

Update the enabled flag of a live binding. Successful false means that no live binding matched the identity.

**Example:**

```cpp
auto result = actions.setAvailability(id, {.enabled = false});
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool updated = *result;
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **UiActions::make**

#### `template <typename Operation, typename... Resources> [[nodiscard]] UiActionCall make( UiActionRecipe<Operation> recipe, Resources&&... resources) const noexcept;`

Pack a stateless, trivial, concrete void-returning UiAction recipe and its arguments into a direct UiActionCall with no retained app binding.

Arguments must match the recipe and fit UiActionCall::InlineBytes = 2 * sizeof(void*), which is 16 bytes with 64-bit pointers. Referenced lvalues must outlive every call copy; value arguments are stored inline. Argument conversion must be nothrow; perform fallible conversion before make(). Size and argument rules are compile-time checks.

**Example:**

```cpp
bool panelOpen = false;
constexpr auto toggle = FlowUi::UiAction("docs.toggle", [](bool& open) { open = !open; });
const FlowUi::UiActionCall call = actions.make(toggle, panelOpen);
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::ActionManager**

#### `ActionManager() noexcept;`

Construct a detached manager; application code normally obtains the initialized App-owned manager.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::~ActionManager**

#### `~ActionManager();`

Release owned service data. The App controls this manager lifetime.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::ActionManager**

#### `ActionManager(const ActionManager&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::operator =**

#### `ActionManager& operator=(const ActionManager&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::ActionManager**

#### `ActionManager(ActionManager&&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::operator =**

#### `ActionManager& operator=(ActionManager&&) = delete;`

Copying and moving are disabled as declared; use the App-owned instance by reference.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::appActions**

#### `[[nodiscard]] AppActions& appActions() noexcept;`

Borrow the mutable or const facade for retained application actions. The manager owns this facade.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::appActions**

#### `[[nodiscard]] const AppActions& appActions() const noexcept;`

Borrow the mutable or const facade for retained application actions. The manager owns this facade.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::uiActions**

#### `[[nodiscard]] UiActions& uiActions() noexcept;`

Borrow the mutable or const facade for inline UI action creation. The manager owns this facade.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::uiActions**

#### `[[nodiscard]] const UiActions& uiActions() const noexcept;`

Borrow the mutable or const facade for inline UI action creation. The manager owns this facade.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::invoke**

#### `[[nodiscard]] ActionInvocationStatus invoke(ActionCall call, ActionInvocationSource source = {});`

Invoke synchronously and directly return Invoked, Empty, Unbound or Disabled. An unavailable call does not run its callable.

Callback and owned-result movement exceptions propagate unchanged. Side effects are not rolled back or automatically retried. invokeFor requires nonthrowing destruction of owned payloads; reference and array result types are rejected.

**Example:**

```cpp
const FlowUi::ActionInvocationStatus outcome = actions.invoke(call);
const bool invoked = outcome == FlowUi::ActionInvocationStatus::Invoked;
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::availability**

#### `[[nodiscard]] ActionAvailability availability(ActionCall call) const noexcept;`

Observe effective enabled state directly; empty, unavailable and unbound calls are disabled.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

### **ActionManager::debugInfo**

#### `[[nodiscard]] std::optional<ActionDebugInfo> debugInfo(ActionCall call) const noexcept;`

Available only with FLOW_UI_DEV_MODE. Return a direct diagnostic snapshot of the call without invoking it.

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).

## Action value types and helpers

`AppActionID` is authored with `AppAction("name")`; an ID being nonzero does not mean it is bound. `AppActionCall` stores that ID. `AppActionDescriptor` adds a diagnostic `debugName` copied at bind time. `UiActionRecipe<Fn>` stores its stateless operation and generated identity; use `UiAction()` rather than editing that identity. `ActionCall` wraps either kind and copying it never extends a borrowed resource lifetime. `ActionStableReference` stores a kind and numeric stable identity without a live binding.

`ActionInvocationStatus` contains `Invoked`, `Empty`, `Unbound` and `Disabled`. `ActionInvokeError` additionally describes `ResultTypeMismatch` for typed invocation. `ActionInvocationSource` carries `kind`, `window`, `element` and `sourceId` attribution; it does not own the invoking UI or element. `ActionInvocationSourceKind` identifies direct, UI manager, element-hook or shortcut sources, with additional agentic attribution when enabled. Developer builds also expose `ActionSourceLocation::current(...)` and `ActionDebugInfo` snapshots; their source/name text remains borrowed.

### **AppActionCall::operator bool**

#### `[[nodiscard]] constexpr explicit operator bool() const noexcept;`

Return whether the call contains a nonempty operation identity. This does not resolve app binding, resource lifetime or enabled state.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **UiActionCall::operator bool**

#### `[[nodiscard]] constexpr explicit operator bool() const noexcept;`

Return whether the call contains a nonempty operation identity. This does not resolve app binding, resource lifetime or enabled state.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionCall::operator bool**

#### `[[nodiscard]] constexpr explicit operator bool() const noexcept;`

Return whether the call contains a nonempty operation identity. This does not resolve app binding, resource lifetime or enabled state.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **UiActionCall::UiActionCall**

#### `constexpr UiActionCall() noexcept = default;`

Create an empty UI call. Obtain populated calls from UiActions::make().

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionCall::ActionCall**

#### `constexpr ActionCall() noexcept;`

Construct an empty type-erased call.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionCall::ActionCall**

#### `constexpr ActionCall(AppActionCall call) noexcept;`

Wrap an app call without creating a binding. An invalid ID becomes an empty call.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionCall::ActionCall**

#### `constexpr ActionCall(UiActionCall call) noexcept;`

Wrap an inline UI call without extending the lifetime of its borrowed resources.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionCall::kind**

#### `[[nodiscard]] constexpr ActionCallKind kind() const noexcept;`

Return the stored representation: None, App, Ui, or the unresolved UiRecipe representation. This is an observation, not dispatch.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionCall::stableReference**

#### `[[nodiscard]] constexpr ActionStableReference stableReference() const noexcept;`

Return a reproducible app identity or an unresolved UI recipe identity. A normal inline UI call only exposes its recipe identity in development builds when reconstructable; otherwise this returns an empty reference. It does not serialize borrowed UI argument values.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionCall::fromStable**

#### `[[nodiscard]] static constexpr ActionCall fromStable( ActionCallKind kind, uint64_t stableId) noexcept;`

Create an app call or unresolved UI recipe reference. Zero/unsupported input yields an empty call. No binding is created; the receiving dispatcher must resolve a published recipe, and an unavailable recipe reports Unbound.

**Example:**

```cpp
const FlowUi::ActionCall call{original};
const auto stable = call.stableReference();
const FlowUi::ActionCall restored = FlowUi::ActionCall::fromStable(stable.kind, stable.id);
```

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<Result>::ActionResult**

#### `ActionResult(Result result);`

Own a successful result value; copying/moving that value may throw.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<Result>::ActionResult**

#### `ActionResult(ActionInvokeError error) noexcept;`

Store a dispatch failure without running a callable.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<Result>::hasValue**

#### `[[nodiscard]] bool hasValue() const noexcept;`

Return whether invocation produced an owned result.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<Result>::operator bool**

#### `[[nodiscard]] explicit operator bool() const noexcept;`

Return hasValue(); inspect this before value().

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<Result>::value**

#### `[[nodiscard]] Result& value() &;`

Borrow the successful value mutably; failure throws std::bad_optional_access.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<Result>::value**

#### `[[nodiscard]] const Result& value() const&;`

Borrow the successful value immutably; failure throws std::bad_optional_access.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<Result>::value**

#### `[[nodiscard]] Result&& value() &&;`

Move from the successful value; failure throws std::bad_optional_access. The returned reference must not outlive this ActionResult.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<Result>::error**

#### `[[nodiscard]] ActionInvokeError error() const noexcept;`

Return the dispatch error; meaningful only on failure.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<void>::success**

#### `[[nodiscard]] static ActionResult success() noexcept;`

Create the successful no-value outcome.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<void>::ActionResult**

#### `ActionResult(ActionInvokeError error) noexcept;`

Create a failed no-value outcome.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<void>::hasValue**

#### `[[nodiscard]] bool hasValue() const noexcept;`

Return whether the void action completed successfully.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<void>::operator bool**

#### `[[nodiscard]] explicit operator bool() const noexcept;`

Return whether invocation succeeded. Check this before treating the void action as complete.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<void>::value**

#### `void value() const noexcept;`

No-value compatibility observer. This performs no success validation; always check bool/hasValue() first.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **ActionResult<void>::error**

#### `[[nodiscard]] ActionInvokeError error() const noexcept;`

Return the dispatch error; meaningful only on failure.

**Example:**

```cpp
auto result = actions.invokeFor<void>(save);
if (!result) {
    const FlowUi::ActionInvokeError error = result.error();
    return; // Handle the dispatch failure before continuing.
}
result.value(); // No payload; the invocation's success was checked above.
```

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **FlowUi::UiAction**

#### `template <typename Fn> [[nodiscard]] constexpr auto UiAction(Fn fn) noexcept;`

Create a recipe whose identity follows the concrete callable type. The operation must be empty, trivial, default constructible, non-generic and return void. Development builds append an optional ActionSourceLocation argument recording the definition location.

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

### **FlowUi::UiAction**

#### `template <std::size_t N, typename Fn> [[nodiscard]] consteval auto UiAction(const char (&name)[N], Fn fn) noexcept;`

Create a recipe with identity hashed from a nonempty literal name. Reusing that name denotes the same authored recipe identity; keep the operation signature consistent. Development builds append an optional ActionSourceLocation argument recording the definition location.

**Example:**

```cpp
bool open = false;
constexpr auto recipe = FlowUi::UiAction([](bool& value) { value = !value; });
const FlowUi::UiActionCall call = actions.make(recipe, open);
```

See: [Owning header](include/FlowUi/Public/managerStructs/ActionTypes.hpp).

## Checked development catalogue and preview hooks

The following optional hooks exist only with `FLOW_UI_DEV_MODE`. The same checked Result/Status usage applies to supported development services. The preview examples use the complete `Counter` definition in [the element reference](docs/api/elements.md).

### **ActionManager::visitDevActions**

#### `[[nodiscard]] Result<bool> visitDevActions( void* userData, DevActionVisitor visitor) const noexcept;`

Synchronously visit borrowed catalogue entries. Successful true means the visit completed; successful false means a visitor stopped early or storage/visitor was unavailable. Thrown visitor/storage failures are converted to structured errors. Do not retain views or references to their metadata beyond the call.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
std::size_t boundCount = 0;
auto result = actions.visitDevActions(&boundCount,
    [](void* userData, const FlowUi::ActionManager::DevActionView& view) {
        auto& count = *static_cast<std::size_t*>(userData);
        if (view.debug.bound) ++count;
        return true;
    });
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool completed = *result;
#endif
```

See: [Owning header](include/FlowUi/Public/managers/ActionManager.hpp).
