# App API

[App.hpp](include/FlowUi/Public/App.hpp) declares the exact overloads; [AppElementWindows.hpp](include/FlowUi/Public/AppElementWindows.hpp) defines the managed-element templates. App owns the runtime, app-wide managers and window-owned UI/viewport managers. Use C++23 and keep platform operations on the creating thread.

## Checked results and comfortable manager references

App accessors deliberately remain checked because an App can be empty or moved from and a runtime window ID can be stale. `Result<T>` truth tests indicate **success**, not the contained boolean. `Status` is `Result<void>`: check it before continuing. Error values retain their code/site/subject/native attribution.

```cpp
auto uiResult = application.ui();
if (!uiResult) {
    throw FlowUi::FlowUiException(uiResult.error());
}
FlowUi::UiManager& ui = uiResult->get();
```

Use `T& manager = result->get()` or `const T& manager = result->get()` after checking; do not copy an App-owned manager. The Result wrapper can leave scope while the borrowed reference stays valid, provided the owner remains alive. Window managers expire at window destruction; app managers expire at App destruction/replacement.

App frame methods are explicit containing boundaries. Ordinary `ui.createElement(...).setParameters(...).draw()`, `context.state()`, `context.resources()` and action invocation now use direct values/references or void and may propagate exceptions. `runFrame()` catches their failure, cancels its frame and returns Status. A latched frame error takes precedence over a later unlatched callback exception, so later allocation/foreign exceptions do not erase its original attribution. Each manager retains its first error; `UiManager::frameStatus()` selects UI, then input-field, then popup failure state. Managed-window UI callback containment uses that same precedence. Caller-side argument construction is outside the called function's noexcept promise.

## Window and frame lifecycle

Use `runFrame()` for a single main window. It polls events, builds/submits the main frame, then dispatches managed windows. `runFrame(id, ...)` does not poll or dispatch other windows. For manual multi-window work, call `pollEvents()` once per outer iteration and complete `beginFrame(id)`, `endFrame(id)`, `drawFrame(id)` before beginning another window. No-argument beginFrame polls first; avoid an unintended duplicate poll.

A failed builder/frame must be abandoned, not resubmitted. Failure after native image acquisition can stop the affected window; actual shared native failure can stop rendering across windows. `hasWindow`, `shouldClose` (true), callback removal and explicit secondary destruction remain the documented inspection/teardown path. A failed secondary teardown remains Closing and can be retried. The main window is retired by whole-App lifetime. Shared graphics failure requires ending that App and constructing a fresh one; no hot device recreation is promised.

Runtime native requests are capability checked. Close sets a close flag; it does not destroy a window inline. Frame-time resize-related requests are deferred to the next poll; later failures reach the configured error observer. Maximize rejects fullscreen windows; fullscreen restoration retains the supported windowed state. Native state can settle on later polls.

## Public API

Examples assume an existing live `application` and a valid `window` where needed. Each snippet uses the function's phase/thread prerequisites. Throwing `FlowUiException` is one caller-side error policy; include `FlowUi/Public/errorManagement/Exception.hpp` for it. Build gates shown in examples apply to the declarations too.

### **App**

#### `App() noexcept`

- **Returns:** `App`
- **Arguments:** none.

Construct an empty App handle without initializing a runtime.

An empty handle can receive a later move assignment; makeApplication() creates an initialized application. Checked operations on an empty or moved-from App return AppUnavailable; hasWindow() is false and mainWindowId() is invalid.

**Example:**

```cpp
FlowUi::App emptyHandle{};
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **App**

#### `App(App&&) noexcept`

- **Returns:** `App`
- **Arguments:** App handle to move from.

Transfer the runtime and manager ownership; leave the source empty.

**Requires:** Perform ownership transfer while the App is quiescent, on its platform thread. Reacquire borrowed manager references through the destination after the move. Move assignment destroys the destination runtime first.

**Example:**

```cpp
FlowUi::App destination(std::move(source));
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **operator=**

#### `App& operator=(App&&) noexcept`

- **Returns:** `App&`.
- **Arguments:** App handle to move from.

Release the destination runtime and transfer ownership from the source.

**Requires:** Perform ownership transfer while the App is quiescent, on its platform thread. Reacquire borrowed manager references through the destination after the move. Move assignment destroys the destination runtime first.

**Example:**

```cpp
destination = std::move(source);
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **App**

#### `App(const App&) = delete`

- **Returns:** Copying is disabled.
- **Arguments:** App handle to copy (disabled).

Copying App owners is disabled; move the owner instead.

Copy construction/assignment is deleted; transfer the owner by move instead.

See: [Owning header](include/FlowUi/Public/App.hpp).

### **operator=**

#### `App& operator=(const App&) = delete`

- **Returns:** `App&`.
- **Arguments:** App handle to copy (disabled).

Copying App owners is disabled; move the owner instead.

Copy construction/assignment is deleted; transfer the owner by move instead.

See: [Owning header](include/FlowUi/Public/App.hpp).

### **~App**

#### `~App() noexcept`

- **Returns:** none.
- **Arguments:** none.

Destroy the app and owned runtime resources.

**Example:**

```cpp
{
    FlowUi::App scopedApplication = std::move(application);
} // The App destructor releases its runtime here.
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **mainWindowId**

#### `[[nodiscard]] WindowId mainWindowId() const noexcept`

- **Returns:** Stable main-window identity.
- **Arguments:** none.

Return the stable identity of the semantic main window.

No-argument window and frame functions operate on this window.

**Example:**

```cpp
const FlowUi::WindowId mainWindow = application.mainWindowId();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **reportError**

#### `void reportError(FlowUiError error) const noexcept`

- **Returns:** none.
- **Arguments:** `error`.

Route a caller-owned FlowUi error through this App's configured reporting
sink and/or compact default writer. Reporting does not resolve the error or
alter application control flow.

**Example:**

```cpp
const auto error = FlowUi::makeError(
    FlowUi::ErrorCode::ColorInvalid, FlowUi::ErrorSite::ColorParse);
application.reportError(error);
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindow**

#### `[[nodiscard]] Result<WindowId> createWindow(const WindowConfig& config) noexcept`

- **Returns:** Stable window identity, or an application, configuration, frame-phase, platform-thread, native-window, or rendering-resource error.
- **Arguments:** `config` Native-window configuration for the new window.

Create and fully initialize a secondary window.

The new window uses the app's Vulkan and UI configuration together with
the supplied native-window configuration.

**Requires:** Call on the platform thread with no active window frame.

**Example:**

```cpp
FlowUi::WindowConfig configuration{};
configuration.title = "Inspector";
configuration.width = 640;
configuration.height = 480;
auto result = application.createWindow(configuration);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindowLikeMain**

#### `[[nodiscard]] Result<WindowId> createWindowLikeMain() noexcept`

- **Returns:** New identity, or the same structured failures as createWindow(const WindowConfig&).
- **Arguments:** none.

Create a secondary window using the main configuration and an automatically suffixed title.

**Example:**

```cpp
auto result = application.createWindowLikeMain();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindow**

#### `[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides) noexcept`

- **Returns:** New identity, or the same structured failures as createWindow(const WindowConfig&).
- **Arguments:** `overrides` Values to replace; an absent title receives a unique window suffix.

Apply overrides to the original main-window configuration and create a secondary window.

**Example:**

```cpp
FlowUi::WindowConfigOverrides overrides{};
overrides.title = "Inspector";
overrides.width = 640;
overrides.height = 480;
auto result = application.createWindow(overrides);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindow**

#### `[[nodiscard]] Result<WindowId> createWindow(std::string_view title, int width = 0, int height = 0) noexcept`

- **Returns:** New identity, or a structured creation/configuration error.
- **Arguments:** `title` Copied title for the new native window.; `width` Width override; zero inherits the main configuration.; `height` Height override; zero inherits the main configuration.

Create a secondary window with an explicit title and optional dimensions.

**Example:**

```cpp
auto result = application.createWindow("Inspector", 640, 480);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindow**

#### `[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides, const UiBuildCallback& buildUi, ManagedWindowFlags flags = {}) noexcept`

- **Returns:** New identity or a creation/registration error. Callback ownership and registration capacity are prepared before native window creation.
- **Arguments:** `overrides` Main-configuration overrides for the new window.; `buildUi` Nonempty callback retained until replacement, removal, or window destruction. Captured references must outlive registration and any active invocation.; `flags` Managed dispatch and close behavior.

Create a secondary window and retain its synchronous UI callback.

**Example:**

```cpp
const FlowUi::UiBuildCallback buildUi =
    [](FlowUi::UiManager& ui, FlowUi::WindowId) {
        ui.createElement(FlowUi::kButton, "inspector/action")
            .setParameters({.label = "Inspector"}).draw();
    };
FlowUi::WindowConfigOverrides overrides{};
overrides.title = "Inspector";
overrides.width = 640;
overrides.height = 480;
auto result = application.createWindow(overrides, buildUi, FlowUi::ManagedWindowFlags{.autoDestroyOnClose = true});
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setWindowUiCallback**

#### `[[nodiscard]] Status setWindowUiCallback(WindowId id, const UiBuildCallback& buildUi, ManagedWindowFlags flags = {}) noexcept`

- **Returns:** Success, or an app, identity, configuration, thread, or phase error.
- **Arguments:** `id` Registered secondary window.; `buildUi` Nonempty retained build callback. References captured by it must remain valid while registered and during active invocations.; `flags` Dispatch and close behavior for this registration.

Attach or replace the managed UI callback for a secondary window.

The semantic main window is not eligible because its frame remains under
the no-argument frame API's control.

**Example:**

```cpp
const FlowUi::UiBuildCallback buildUi =
    [](FlowUi::UiManager& ui, FlowUi::WindowId) {
        ui.createElement(FlowUi::kButton, "inspector/action")
            .setParameters({.label = "Inspector"}).draw();
    };
const auto status = application.setWindowUiCallback(window, buildUi, FlowUi::ManagedWindowFlags{.autoDestroyOnClose = true});
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **removeWindowUiCallback**

#### `[[nodiscard]] Status removeWindowUiCallback(WindowId id) noexcept`

- **Returns:** Success when removed or already absent; an unavailable-App, platform-thread, or prohibited-phase error otherwise.
- **Arguments:** `id`.

Return a managed window to explicit/manual frame control on the platform thread.
May remove the current callback or one belonging to a stopped/closing window.
Unknown identities are successful no-ops.

**Example:**

```cpp
const auto status = application.removeWindowUiCallback(window);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **dispatchManagedWindows**

#### `[[nodiscard]] Status dispatchManagedWindows() noexcept`

- **Returns:** Success, or a structured frame/dispatch error after canceling the active frame.
- **Arguments:** none.

Execute all managed secondary-window frame triads in registration order.

Dispatch is synchronous on the platform thread. Closed windows with
autoDestroyOnClose enabled are drained and destroyed before returning.

**Example:**

```cpp
const auto status = application.dispatchManagedWindows();
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **runFrame**

#### `[[nodiscard]] Status runFrame(const UiBuildCallback& buildUi) noexcept`

- **Returns:** Success or the original FlowUiError; foreign callback exceptions become ForeignExceptionObserved and allocation failures become AllocationFailed.
- **Arguments:** `buildUi`.

Execute the main window frame under a library-owned error boundary.
Poll events, begin the frame, invoke buildUi, finish and draw the frame,
then dispatch managed windows. Any build failure cancels the active frame.

**Example:**

```cpp
const FlowUi::UiBuildCallback buildUi =
    [](FlowUi::UiManager& ui, FlowUi::WindowId) {
        ui.createElement(FlowUi::kButton, "inspector/action")
            .setParameters({.label = "Inspector"}).draw();
    };
const auto status = application.runFrame(buildUi);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **runFrame**

#### `[[nodiscard]] Status runFrame(WindowId id, const UiBuildCallback& buildUi) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `id`, `buildUi`.

Execute a registered window frame without polling native events.
Invoke pollEvents() once before processing explicit window frames.
The callback and all frame phases execute inside the result boundary.

**Example:**

```cpp
const FlowUi::UiBuildCallback buildUi =
    [](FlowUi::UiManager& ui, FlowUi::WindowId) {
        ui.createElement(FlowUi::kButton, "inspector/action")
            .setParameters({.label = "Inspector"}).draw();
    };
const auto status = application.runFrame(window, buildUi);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindow**

#### `template <typename Callback> requires (!std::same_as<std::remove_cvref_t<Callback>, UiBuildCallback> && std::invocable<Callback&, UiManager&, WindowId>) [[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides, Callback&& buildUi, ManagedWindowFlags flags = {}) noexcept`

- **Returns:** `Result<WindowId>`.
- **Arguments:** `overrides`, `buildUi`, `flags`.

Retain a callable after constructing its owning callback inside the API boundary.
A returned Status is resolved by managed frame dispatch.

**Note:** Only an exact Status return is interpreted as a recoverable callback result. Other callback return values are ignored. Captured data must remain alive for every retained callback invocation.

**Example:**

```cpp
FlowUi::WindowConfigOverrides overrides{};
overrides.title = "Inspector";
overrides.width = 640;
overrides.height = 480;
auto buildUi = [](FlowUi::UiManager& ui, FlowUi::WindowId) -> FlowUi::Status {
    ui.createElement(FlowUi::kButton, "inspector/action")
        .setParameters({.label = "Inspector"}).draw();
    return ui.frameStatus();
};
auto result = application.createWindow(overrides, buildUi);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setWindowUiCallback**

#### `template <typename Callback> requires (!std::same_as<std::remove_cvref_t<Callback>, UiBuildCallback> && std::invocable<Callback&, UiManager&, WindowId>) [[nodiscard]] Status setWindowUiCallback(WindowId id, Callback&& buildUi, ManagedWindowFlags flags = {}) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `id`, `buildUi`, `flags`.

Retain a callable while containing callback allocation or copying failures.

**Note:** Only an exact Status return is interpreted as a recoverable callback result. Other callback return values are ignored. Captured data must remain alive for every retained callback invocation.

**Example:**

```cpp
auto buildUi = [](FlowUi::UiManager& ui, FlowUi::WindowId) -> FlowUi::Status {
    ui.createElement(FlowUi::kButton, "managed/action")
        .setParameters({.label = "Managed window"}).draw();
    return ui.frameStatus();
};
const auto status = application.setWindowUiCallback(window, buildUi, FlowUi::ManagedWindowFlags{.autoDestroyOnClose = true});
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **runFrame**

#### `template <typename Callback> requires (!std::same_as<std::remove_cvref_t<Callback>, UiBuildCallback> && std::invocable<Callback&, UiManager&, WindowId>) [[nodiscard]] Status runFrame(Callback&& buildUi) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `buildUi`.

Construct the main-window build callback inside the result boundary.
A callback returning Status propagates its failure through runFrame().

**Note:** Only an exact Status return is interpreted as a recoverable callback result. Other callback return values are ignored. Captured data must remain alive for every retained callback invocation.

**Example:**

```cpp
auto buildUi = [](FlowUi::UiManager& ui, FlowUi::WindowId) -> FlowUi::Status {
    ui.createElement(FlowUi::kButton, "main/action")
        .setParameters({.label = "Save"}).draw();
    return ui.frameStatus();
};
const auto status = application.runFrame(buildUi);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **runFrame**

#### `template <typename Callback> requires (!std::same_as<std::remove_cvref_t<Callback>, UiBuildCallback> && std::invocable<Callback&, UiManager&, WindowId>) [[nodiscard]] Status runFrame(WindowId id, Callback&& buildUi) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `id`, `buildUi`.

Construct an explicit-window build callback inside the result boundary.

**Note:** Only an exact Status return is interpreted as a recoverable callback result. Other callback return values are ignored. Captured data must remain alive for every retained callback invocation.

**Example:**

```cpp
auto buildUi = [](FlowUi::UiManager& ui, FlowUi::WindowId) -> FlowUi::Status {
    ui.createElement(FlowUi::kButton, "main/action")
        .setParameters({.label = "Save"}).draw();
    return ui.frameStatus();
};
const auto status = application.runFrame(window, buildUi);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindow**

#### `template <FlowElement Element> [[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides, const Element& element, const ParametersOf<Element>& params, LocalElementName localName = LocalElementName{"window/root-element"}) noexcept`

- **Returns:** New window identity or a structured creation/registration error.
- **Arguments:** `overrides` Window-configuration overrides.; `element` Element tag copied into the retained callback.; `params` Parameters copied into that callback; any borrowed members must outlive their use by the managed window.; `localName` Root element name; borrowed name storage must remain valid.

Create a managed window drawing one element with retained parameters.

**Template parameter:** Element Flow element definition to draw as the window root.

**Example:**

```cpp
FlowUi::WindowConfigOverrides overrides{};
overrides.title = "Inspector";
overrides.width = 640;
overrides.height = 480;
auto result = application.createWindow(overrides, FlowUi::kButton, FlowUi::ButtonParameters{.label = "Inspector"});
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindow**

#### `template <FlowElement Element> [[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides, const Element& element) noexcept`

- **Returns:** `Result<WindowId>`.
- **Arguments:** `overrides`, `element`.

Create a managed element window with parameters constructed inside the error boundary.

**Example:**

```cpp
FlowUi::WindowConfigOverrides overrides{};
overrides.title = "Inspector";
overrides.width = 640;
overrides.height = 480;
auto result = application.createWindow(overrides, FlowUi::kButton);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindow**

#### `template <FlowElement Element> [[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides, const Element& element, const std::function<void(ElementBuilder<Element>&, WindowId)>& configurator, LocalElementName localName = LocalElementName{"window/root-element"}) noexcept`

- **Returns:** New identity, or InvalidWindowConfiguration for an empty configurator, or a structured creation/registration error.
- **Arguments:** `overrides` Window-configuration overrides.; `element` Element tag copied into the retained callback.; `configurator` Nonempty retained callback invoked before the root is drawn. Captured references must outlive registration and active callback execution.; `localName` Root element name with storage valid for the window lifetime.

Create a managed window whose root builder is configured on every frame.

**Template parameter:** Element Flow element definition to draw.

**Example:**

```cpp
FlowUi::WindowConfigOverrides overrides{};
overrides.title = "Inspector";
overrides.width = 640;
overrides.height = 480;
const std::function<void(FlowUi::ElementBuilder<FlowUi::Button>&, FlowUi::WindowId)> configure =
    [](FlowUi::ElementBuilder<FlowUi::Button>& builder, FlowUi::WindowId) {
        (void)builder.setParameters({.label = "Inspector"});
    };
auto result = application.createWindow(overrides, FlowUi::kButton, configure);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindow**

#### `template <FlowElement Element, typename Configurator> requires (!std::same_as<std::remove_cvref_t<Configurator>, std::function<void(ElementBuilder<Element>&, WindowId)>> && std::invocable<Configurator&, ElementBuilder<Element>&, WindowId>) [[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides, const Element& element, Configurator&& configurator, LocalElementName localName = LocalElementName{"window/root-element"}) noexcept`

- **Returns:** `Result<WindowId>`.
- **Arguments:** `overrides`, `element`, `configurator`, `localName`.

Construct an owning element configurator inside the result boundary.
A returned Status is resolved by managed frame dispatch.

**Note:** The adapter resolves an exact Status return before drawing the root. Other configurator return values are ignored. Borrowed captured data must outlive the managed window registration.

**Example:**

```cpp
FlowUi::WindowConfigOverrides overrides{};
overrides.title = "Inspector";
overrides.width = 640;
overrides.height = 480;
auto configure = [](FlowUi::ElementBuilder<FlowUi::Button>& builder, FlowUi::WindowId) -> FlowUi::Status {
    (void)builder.setParameters({.label = "Inspector"});
    return {};
};
auto result = application.createWindow(overrides, FlowUi::kButton, configure);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **createWindowWithState**

#### `template <FlowElement Element> [[nodiscard]] Result<WindowId> createWindowWithState(const WindowConfigOverrides& overrides, const Element& element, std::shared_ptr<ParametersOf<Element>> sharedParams, LocalElementName localName = LocalElementName{"window/root-element"}) noexcept`

- **Returns:** New identity, or InvalidWindowConfiguration for null shared state, or a structured creation/registration error.
- **Arguments:** `overrides` Window-configuration overrides.; `element` Element tag copied into the retained callback.; `sharedParams` Nonnull shared parameter object retained by the callback. Updates are observed on later synchronous builds; borrowed parameter members remain caller-owned.; `localName` Root name with storage valid for the window lifetime.

Create a managed window that reads shared root parameters each frame.

**Template parameter:** Element Flow element definition to draw.

**Example:**

```cpp
FlowUi::WindowConfigOverrides overrides{};
overrides.title = "Shared inspector";
auto parameters = std::make_shared<FlowUi::ButtonParameters>();
parameters->label = "Shared inspector";
auto result = application.createWindowWithState(overrides, FlowUi::kButton, parameters);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowId createdWindow = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **destroyWindow**

#### `[[nodiscard]] Status destroyWindow(WindowId id) noexcept`

- **Returns:** Success, or an unavailable-app, invalid-window, main-window, platform-thread, frame-phase, or resource-drain error.
- **Arguments:** `id` Secondary-window identity returned by createWindow().

Destroy a secondary window after draining its outstanding work.

The semantic main window cannot be explicitly destroyed. A failed secondary
teardown remains registered and can be retried; frame and native-window
operations reject it while closing. A stopped secondary window can be
destroyed and replaced. Shared graphics failure requires whole-app teardown.

**Example:**

```cpp
const auto status = application.destroyWindow(window);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **hasWindow**

#### `[[nodiscard]] bool hasWindow(WindowId id) const noexcept`

- **Returns:** `bool`.
- **Arguments:** `id` Window identity to query.

Return whether an identity currently names a registered window.

**Outcome:** true id identifies a current window.

**Outcome:** false id is invalid, unknown, or was destroyed.

**Example:**

```cpp
const bool windowExists = application.hasWindow(window);
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **pollEvents**

#### `[[nodiscard]] Status pollEvents() noexcept`

- **Returns:** Success, or an app, platform-thread, or frame-phase error.
- **Arguments:** none.

Poll global platform events and advance app-shared maintenance.

Multi-window applications should call this once per outer application-loop
iteration before beginning explicit window frames. The function is global,
not associated with one WindowId, and cannot run while a window frame
triplet is active.

Single-main-window applications normally do not call this directly because
the no-argument beginFrame() overload performs the same polling first.

**Example:**

```cpp
const auto status = application.pollEvents();
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **shouldClose**

#### `[[nodiscard]] Result<bool> shouldClose() const noexcept`

- **Returns:** True when shutdown was requested or rendering stopped, false otherwise, or an app/window access error.
- **Arguments:** none.

Query whether the window backend requested shutdown.

Use this function to drive the app lifecycle loop.

**Example:**

```cpp
auto result = application.shouldClose();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool closeRequested = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **shouldClose**

#### `[[nodiscard]] Result<bool> shouldClose(WindowId id) const noexcept`

- **Returns:** True when shutdown was requested or the window is Stopped/Closing, false otherwise, or an app/window/thread access error.
- **Arguments:** `id` Registered window identity.

Query whether a specific window requested shutdown.

**Example:**

```cpp
auto result = application.shouldClose(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool closeRequested = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setShouldClose**

#### `[[nodiscard]] Status setShouldClose(int value) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `value` Zero clears the close flag; nonzero requests closing.

Set whether the window backend should request shutdown.

The `value` argument updates the selected platform provider close flag. Zero
clears the window close flag; a non-zero value sets it.

**Note:** Setting the close flag does not destroy a window inline. A Stopped/Closing window rejects this ordinary native-window operation; use shouldClose(), removeWindowUiCallback(), or secondary destroyWindow() for teardown.

**Example:**

```cpp
const auto status = application.setShouldClose(1);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setShouldClose**

#### `[[nodiscard]] Status setShouldClose(WindowId id, int value) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `id` Registered window identity.; `value` Zero clears the request; a non-zero value sets it.

Set or clear a specific window's shutdown request.

**Note:** Setting the close flag does not destroy a window inline. A Stopped/Closing window rejects this ordinary native-window operation; use shouldClose(), removeWindowUiCallback(), or secondary destroyWindow() for teardown.

**Example:**

```cpp
const auto status = application.setShouldClose(window, 1);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **window_operation_capabilities**

#### `[[nodiscard]] Result<WindowOperationCapabilities> window_operation_capabilities(WindowId window_id) const noexcept`

- **Returns:** `Result<WindowOperationCapabilities>`.
- **Arguments:** `window_id`.

Report native operations supported by a live window on the platform thread.

**Example:**

```cpp
auto result = application.window_operation_capabilities(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowOperationCapabilities capabilities = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **request_window_close**

#### `[[nodiscard]] Status request_window_close(WindowId window_id) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `window_id`.

Request closure of this window without destroying it during input dispatch.

**Example:**

```cpp
const auto status = application.request_window_close(window);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **minimize_window**

#### `[[nodiscard]] Status minimize_window(WindowId window_id) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `window_id`.

Minimize this window; requests during a frame run at the next event-poll boundary.

**Example:**

```cpp
const auto status = application.minimize_window(window);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **toggle_window_maximized**

#### `[[nodiscard]] Status toggle_window_maximized(WindowId window_id) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `window_id`.

Toggle observed maximized state; rejected while fullscreen. Frame requests are deferred.

**Example:**

```cpp
const auto status = application.toggle_window_maximized(window);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **toggle_window_fullscreen**

#### `[[nodiscard]] Status toggle_window_fullscreen(WindowId window_id) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `window_id`.

Toggle fullscreen and restore windowed geometry/state. Frame requests are deferred.

**Example:**

```cpp
const auto status = application.toggle_window_fullscreen(window);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **beginFrame**

#### `[[nodiscard]] Status beginFrame() noexcept`

- **Returns:** Success when UI construction can begin, or a structured frame error.
- **Arguments:** none.

Poll events and begin a frame for the semantic main window.

This convenience overload first performs the same global event polling and
app-shared maintenance as pollEvents(), then begins the window returned by
mainWindowId(). It preserves the classic single-window frame loop without a
separate polling call.

**Requires:** The FlowUi::App instance is initialized and its window/UI systems are valid.

**After success:** On success, the frame is active and its UI is ready for construction.

**Note:** Do not also call pollEvents() in the same loop iteration unless a
second global event-pump pass is intentional.

**Example:**

```cpp
const auto status = application.beginFrame();
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **beginFrame**

#### `[[nodiscard]] Status beginFrame(WindowId id) noexcept`

- **Returns:** Success or a structured app, identity, thread, or frame-phase error.
- **Arguments:** `id` Registered window to build.

Begin one explicit window frame without polling global events.

Use this overload after one pollEvents() call when driving multiple windows;
`id` must identify a registered window.
A window's beginFrame(id), endFrame(id), and drawFrame(id) triplet must finish
before another window frame is begun.

**Requires:** pollEvents() was called at the cadence required by the application.

**After success:** On success, the selected UI manager is ready for construction.

**Example:**

```cpp
const auto status = application.beginFrame(window);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **endFrame**

#### `[[nodiscard]] Status endFrame() noexcept`

- **Returns:** Success or a structured frame/resource/allocation error.
- **Arguments:** none.

End UI construction and produce render commands.

Completes UI construction, produces render commands for this frame, and
prepares viewport and icon manager resources.

**Requires:** FlowUi::App::beginFrame() was called and UI was built afterward.

**After success:** On success, render commands are ready and viewport handles are sized.

**Note:** This function should be called exactly once per frame.

**Example:**

```cpp
const auto status = application.endFrame();
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **endFrame**

#### `[[nodiscard]] Status endFrame(WindowId id) noexcept`

- **Returns:** Success or a structured identity, phase, frame/resource, or allocation error.
- **Arguments:** `id` Window identity passed to the matching beginFrame(id).

End UI construction for a specific active window frame.

**Example:**

```cpp
const auto status = application.endFrame(window);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **drawFrame**

#### `[[nodiscard]] Status drawFrame() noexcept`

- **Returns:** Success or a structured render, presentation, allocation, or managed-dispatch error.
- **Arguments:** none.

Submit the current frame for rendering and presentation.

Takes the render commands output by endFrame(), builds GPU draw runs,
renders them, and passes the output to the swapchain for presentation.

**Requires:** FlowUi::App::endFrame() was called successfully.

**After success:** On success, frame submission/presentation processing has completed.

**Note:** This function should be called exactly once per frame.

**Note:** After presenting the main window, this overload synchronously calls
dispatchManagedWindows(). The WindowId overload never dispatches managed
windows implicitly.

**Example:**

```cpp
const auto status = application.drawFrame();
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **drawFrame**

#### `[[nodiscard]] Status drawFrame(WindowId id) noexcept`

- **Returns:** Success or a structured identity, phase, render, allocation, or presentation error.
- **Arguments:** `id` Window identity passed to the matching beginFrame(id) and endFrame(id).

Submit and present a specific prepared window frame.

**Example:**

```cpp
const auto status = application.drawFrame(window);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **fonts**

#### `[[nodiscard]] Result<std::reference_wrapper<FontManager>> fonts() noexcept`

- **Returns:** Borrowed reference wrapper, or AppUnavailable, for FlowUi::FontManager.
- **Arguments:** none.

Access the font manager.

**Example:**

```cpp
auto result = application.fonts();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::FontManager& fonts = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **fonts**

#### `[[nodiscard]] Result<std::reference_wrapper<const FontManager>> fonts() const noexcept`

- **Returns:** Borrowed const reference wrapper, or AppUnavailable, for FlowUi::FontManager.
- **Arguments:** none.

Access the font manager.

**Example:**

```cpp
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.fonts();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::FontManager& fonts = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **images**

#### `[[nodiscard]] Result<std::reference_wrapper<ImageManager>> images() noexcept`

- **Returns:** Borrowed reference wrapper, or AppUnavailable, for FlowUi::ImageManager.
- **Arguments:** none.

Access the image manager.

**Example:**

```cpp
auto result = application.images();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ImageManager& images = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **images**

#### `[[nodiscard]] Result<std::reference_wrapper<const ImageManager>> images() const noexcept`

- **Returns:** Borrowed const reference wrapper, or AppUnavailable, for FlowUi::ImageManager.
- **Arguments:** none.

Access the image manager.

**Example:**

```cpp
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.images();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::ImageManager& images = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **themes**

#### `[[nodiscard]] Result<std::reference_wrapper<ThemeManager>> themes() noexcept`

- **Returns:** Borrowed reference wrapper, or AppUnavailable, for FlowUi::ThemeManager.
- **Arguments:** none.

Access the theme manager.

**Example:**

```cpp
auto result = application.themes();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ThemeManager& themes = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **themes**

#### `[[nodiscard]] Result<std::reference_wrapper<const ThemeManager>> themes() const noexcept`

- **Returns:** Borrowed const reference wrapper, or AppUnavailable, for FlowUi::ThemeManager.
- **Arguments:** none.

Access the theme manager.

**Example:**

```cpp
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.themes();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::ThemeManager& themes = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **elements**

#### `[[nodiscard]] Result<std::reference_wrapper<ElementManager>> elements() noexcept`

- **Returns:** Borrowed reference wrapper, or AppUnavailable, for FlowUi::ElementManager owned by this app.
- **Arguments:** none.

Access the app-wide Flow element state/resource manager.

**Example:**

```cpp
auto result = application.elements();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ElementManager& elements = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **elements**

#### `[[nodiscard]] Result<std::reference_wrapper<const ElementManager>> elements() const noexcept`

- **Returns:** Borrowed const reference wrapper, or AppUnavailable, for FlowUi::ElementManager owned by this app.
- **Arguments:** none.

Access the app-wide Flow element state/resource manager.

**Example:**

```cpp
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.elements();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::ElementManager& elements = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **agentic_workflow**

#### `[[nodiscard]] Result<std::reference_wrapper<devSystems::agentic::AgenticWorkflowService>> agentic_workflow() noexcept`

- **Returns:** Checked borrowed manager reference; the result does not extend its owner's lifetime.
- **Arguments:** none.

**Build condition:** `FLOW_UI_AGENTIC_WORKFLOW`.

App-owned evidence coordinator; mutations are admitted at application safe points.

**Example:**

```cpp
#if FLOW_UI_AGENTIC_WORKFLOW
auto result = application.agentic_workflow();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::devSystems::agentic::AgenticWorkflowService& workflow = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **agentic_workflow**

#### `[[nodiscard]] Result<std::reference_wrapper<const devSystems::agentic::AgenticWorkflowService>> agentic_workflow() const noexcept`

- **Returns:** Checked borrowed manager reference; the result does not extend its owner's lifetime.
- **Arguments:** none.

**Build condition:** `FLOW_UI_AGENTIC_WORKFLOW`.

Inspect the application's agentic service without activating it.

**Example:**

```cpp
#if FLOW_UI_AGENTIC_WORKFLOW
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.agentic_workflow();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::devSystems::agentic::AgenticWorkflowService& workflow = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **actions**

#### `[[nodiscard]] Result<std::reference_wrapper<ActionManager>> actions() noexcept`

- **Returns:** Checked borrowed manager reference; the result does not extend its owner's lifetime.
- **Arguments:** none.

Access the app-wide action manager.

**Example:**

```cpp
auto result = application.actions();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ActionManager& actions = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **actions**

#### `[[nodiscard]] Result<std::reference_wrapper<const ActionManager>> actions() const noexcept`

- **Returns:** Checked borrowed manager reference; the result does not extend its owner's lifetime.
- **Arguments:** none.

Access the immutable app-wide action manager.

**Example:**

```cpp
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.actions();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::ActionManager& actions = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **request_dev_interface_toggle**

#### `void request_dev_interface_toggle() noexcept`

- **Returns:** none.
- **Arguments:** none.

**Build condition:** `FLOW_UI_DEV_MODE`.

Queue a manual developer-interface toggle, equivalent to its panel shortcut.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
application.request_dev_interface_toggle();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **request_dev_performance_capture**

#### `[[nodiscard]] Status request_dev_performance_capture( const devSystems::PerformanceCaptureSettings& settings) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `settings`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Validate capture intent against this application's panel shortcut.

Include [DevPerformanceCapture.hpp](include/devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp) for `PerformanceCaptureSettings`.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
FlowUi::devSystems::PerformanceCaptureSettings settings{};
const auto status = application.request_dev_performance_capture(settings);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **set_dev_window_suspended**

#### `[[nodiscard]] Status set_dev_window_suspended(WindowId window, bool suspended) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `window`, `suspended`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Suspend developer-window dispatch and visibility while preserving its session.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const auto status = application.set_dev_window_suspended(window, true);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **dev_shortcut_down**

#### `[[nodiscard]] Result<bool> dev_shortcut_down(const DevShortcutChord& chord) const noexcept`

- **Returns:** `Result<bool>`.
- **Arguments:** `chord`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Poll a configured chord on focused native application windows before UI work.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
FlowUi::DevShortcutChord chord{};
auto result = application.dev_shortcut_down(chord);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool chordDown = *result;
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **dev_shortcut_pressed**

#### `[[nodiscard]] Result<bool> dev_shortcut_pressed(const DevShortcutChord& chord) const noexcept`

- **Returns:** `Result<bool>`.
- **Arguments:** `chord`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Read a native press edge before UI dispatch; repeat events are excluded.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
FlowUi::DevShortcutChord chord{};
auto result = application.dev_shortcut_pressed(chord);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool chordPressed = *result;
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **devMonitoring**

#### `[[nodiscard]] Result<std::reference_wrapper<devSystems::DevMonitoringAndReporting>> devMonitoring() noexcept`

- **Returns:** Checked borrowed manager reference; the result does not extend its owner's lifetime.
- **Arguments:** none.

**Build condition:** `FLOW_UI_DEV_MODE`.

Development-only access to timing monitoring and retained reports.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
auto result = application.devMonitoring();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::devSystems::DevMonitoringAndReporting& monitoring = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **devMonitoring**

#### `[[nodiscard]] Result<std::reference_wrapper<const devSystems::DevMonitoringAndReporting>> devMonitoring() const noexcept`

- **Returns:** Checked borrowed manager reference; the result does not extend its owner's lifetime.
- **Arguments:** none.

**Build condition:** `FLOW_UI_DEV_MODE`.

Borrow immutable development timing, memory, and error reporting services.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.devMonitoring();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::devSystems::DevMonitoringAndReporting& monitoring = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **devTooling**

#### `[[nodiscard]] Result<std::reference_wrapper<devSystems::DevTooling>> devTooling() noexcept`

- **Returns:** Checked borrowed manager reference; the result does not extend its owner's lifetime.
- **Arguments:** none.

**Build condition:** `FLOW_UI_DEV_MODE`.

Development-only schema catalogue, discovery, and publication service.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
auto result = application.devTooling();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::devSystems::DevTooling& tooling = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **devTooling**

#### `[[nodiscard]] Result<std::reference_wrapper<const devSystems::DevTooling>> devTooling() const noexcept`

- **Returns:** Checked borrowed manager reference; the result does not extend its owner's lifetime.
- **Arguments:** none.

**Build condition:** `FLOW_UI_DEV_MODE`.

Borrow immutable development schema, discovery, and publication services.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.devTooling();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::devSystems::DevTooling& tooling = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **devWindowSnapshot**

#### `[[nodiscard]] Result<std::vector<DevWindowInfo>> devWindowSnapshot() const noexcept`

- **Returns:** `Result<std::vector<DevWindowInfo>>`.
- **Arguments:** none.

**Build condition:** `FLOW_UI_DEV_MODE`.

Snapshot registered windows in stable identity order for development UI.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
auto result = application.devWindowSnapshot();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
auto windows = std::move(*result);
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **devViewportSnapshot**

#### `[[nodiscard]] Result<std::vector<DevViewportInfo>> devViewportSnapshot(WindowId window) const noexcept`

- **Returns:** `Result<std::vector<DevViewportInfo>>`.
- **Arguments:** `window`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Observe window-local viewport metadata independently of public Vulkan interop.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
auto result = application.devViewportSnapshot(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
auto viewports = std::move(*result);
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **request_dev_viewport_capture**

#### `[[nodiscard]] Status request_dev_viewport_capture(WindowId window, std::string_view key) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `window`, `key`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Request one immutable GPU copy at the viewport's next natural source render.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const std::string_view key = "preview/scene";
const auto status = application.request_dev_viewport_capture(window, key);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **release_dev_viewport_capture**

#### `void release_dev_viewport_capture(WindowId window, std::string_view key) noexcept`

- **Returns:** none.
- **Arguments:** `window`, `key`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Retire this tool's backend capture cache; submitted consumers retain their own leases.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const std::string_view key = "preview/scene";
application.release_dev_viewport_capture(window, key);
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **retain_dev_viewport_capture**

#### `[[nodiscard]] Result<TextureHandle> retain_dev_viewport_capture(WindowId window, std::string_view key, TextureHandle expected_capture) noexcept`

- **Returns:** `Result<TextureHandle>`.
- **Arguments:** `window`, `key`, `expected_capture`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Own an independent binding of a completed immutable copy; survives source removal.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const std::string_view key = "preview/scene";
auto result = application.retain_dev_viewport_capture(window, key, expectedCapture);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::TextureHandle retainedCapture = *result;
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **release_dev_viewport_capture_texture**

#### `[[nodiscard]] Status release_dev_viewport_capture_texture(TextureHandle texture) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `texture`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Release a binding returned by retain_dev_viewport_capture().

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const auto status = application.release_dev_viewport_capture_texture(capture);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **devUiReplaySource**

#### `[[nodiscard]] devSystems::DevUiReplaySource devUiReplaySource(WindowId id) noexcept`

- **Returns:** `devSystems::DevUiReplaySource`.
- **Arguments:** `id`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Return the latest owned raw-command packet captured for development replay.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const auto replaySource = application.devUiReplaySource(window);
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **requestDevUiReplay**

#### `[[nodiscard]] Status requestDevUiReplay(WindowId id, uint64_t requestKey, std::span<const uint32_t> clayCommandIds, Clay_BoundingBox sourceRootBounds) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `id`, `requestKey`, `clayCommandIds`, `sourceRootBounds`.

**Build condition:** `FLOW_UI_DEV_MODE`.

Request capture of raw Clay commands owned by the supplied subtree IDs.

**Example:**

```cpp
#if FLOW_UI_DEV_MODE
const auto status = application.requestDevUiReplay(window, requestKey, commandIds, rootBounds);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **icons**

#### `[[nodiscard]] Result<std::reference_wrapper<IconManager>> icons() noexcept`

- **Returns:** Borrowed reference wrapper, or AppUnavailable, for FlowUi::IconManager.
- **Arguments:** none.

**Build condition:** `FLOWUI_INCLUDE_ICON_MANAGER`.

Access the icon manager.

**Note:** FlowUi must be built with FLOWUI_INCLUDE_ICON_MANAGER enabled to use IconManager.

**Example:**

```cpp
#if FLOWUI_INCLUDE_ICON_MANAGER
auto result = application.icons();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::IconManager& icons = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **icons**

#### `[[nodiscard]] Result<std::reference_wrapper<const IconManager>> icons() const noexcept`

- **Returns:** Borrowed const reference wrapper, or AppUnavailable, for FlowUi::IconManager.
- **Arguments:** none.

**Build condition:** `FLOWUI_INCLUDE_ICON_MANAGER`.

Access the icon manager.

**Note:** FlowUi must be built with FLOWUI_INCLUDE_ICON_MANAGER enabled to use IconManager.

**Example:**

```cpp
#if FLOWUI_INCLUDE_ICON_MANAGER
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.icons();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::IconManager& icons = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **viewPorts**

#### `[[nodiscard]] Result<std::reference_wrapper<ViewPortManager>> viewPorts() noexcept`

- **Returns:** Borrowed reference wrapper, or an unavailable-app/window access error, for FlowUi::ViewPortManager.
- **Arguments:** none.

**Build condition:** `FLOWUI_PUBLIC_VULKAN_INTEROP`.

Access the viewport manager.

**Note:** FlowUi must be built with FLOWUI_PUBLIC_VULKAN_INTEROP enabled to use ViewPortManager.

**Example:**

```cpp
#if FLOWUI_PUBLIC_VULKAN_INTEROP
auto result = application.viewPorts();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ViewPortManager& viewports = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **viewPorts**

#### `[[nodiscard]] Result<std::reference_wrapper<ViewPortManager>> viewPorts(WindowId id) noexcept`

- **Returns:** Borrowed mutable viewport manager wrapper, or an app/window access error.
- **Arguments:** `id` Registered window identity.

**Build condition:** `FLOWUI_PUBLIC_VULKAN_INTEROP`.

Access the viewport manager owned by a specific window.

**Example:**

```cpp
#if FLOWUI_PUBLIC_VULKAN_INTEROP
auto result = application.viewPorts(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::ViewPortManager& viewports = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **viewPorts**

#### `[[nodiscard]] Result<std::reference_wrapper<const ViewPortManager>> viewPorts() const noexcept`

- **Returns:** Borrowed const reference wrapper, or an unavailable-app/window access error, for FlowUi::ViewPortManager.
- **Arguments:** none.

**Build condition:** `FLOWUI_PUBLIC_VULKAN_INTEROP`.

Access the viewport manager.

**Note:** FlowUi must be built with FLOWUI_PUBLIC_VULKAN_INTEROP enabled to use ViewPortManager.

**Example:**

```cpp
#if FLOWUI_PUBLIC_VULKAN_INTEROP
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.viewPorts();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::ViewPortManager& viewports = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **viewPorts**

#### `[[nodiscard]] Result<std::reference_wrapper<const ViewPortManager>> viewPorts(WindowId id) const noexcept`

- **Returns:** Borrowed immutable viewport manager wrapper, or an app/window access error.
- **Arguments:** `id` Registered window identity.

**Build condition:** `FLOWUI_PUBLIC_VULKAN_INTEROP`.

Access the immutable viewport manager owned by a specific window.

**Example:**

```cpp
#if FLOWUI_PUBLIC_VULKAN_INTEROP
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.viewPorts(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::ViewPortManager& viewports = result->get();
#endif
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **ui**

#### `[[nodiscard]] Result<std::reference_wrapper<UiManager>> ui() noexcept`

- **Returns:** Borrowed reference wrapper, or an unavailable-app/window access error, for FlowUi::UiManager.
- **Arguments:** none.

Access the UI manager.

**Example:**

```cpp
auto result = application.ui();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::UiManager& ui = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **ui**

#### `[[nodiscard]] Result<std::reference_wrapper<UiManager>> ui(WindowId id) noexcept`

- **Returns:** Borrowed mutable UI manager wrapper, or an app/window access error.
- **Arguments:** `id` Registered window identity.

Access the UI manager owned by a specific window.

**Example:**

```cpp
auto result = application.ui(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::UiManager& ui = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **ui**

#### `[[nodiscard]] Result<std::reference_wrapper<const UiManager>> ui() const noexcept`

- **Returns:** Borrowed const reference wrapper, or an unavailable-app/window access error, for FlowUi::UiManager.
- **Arguments:** none.

Access the UI manager.

**Example:**

```cpp
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.ui();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::UiManager& ui = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **ui**

#### `[[nodiscard]] Result<std::reference_wrapper<const UiManager>> ui(WindowId id) const noexcept`

- **Returns:** Borrowed immutable UI manager wrapper, or an app/window access error.
- **Arguments:** `id` Registered window identity.

Access the immutable UI manager owned by a specific window.

**Example:**

```cpp
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.ui(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::UiManager& ui = result->get();
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setWindowTitle**

#### `[[nodiscard]] Status setWindowTitle(std::string_view title) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `title` String view of the new title.

Set the native window title.

The initial window title is set from FlowUi::WindowConfig by default.

**Example:**

```cpp
const auto status = application.setWindowTitle("Inspector — ready");
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setWindowTitle**

#### `[[nodiscard]] Status setWindowTitle(WindowId id, std::string_view title) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `id` Registered window identity.; `title` New native-window title.

Set the native title of a specific window.

**Example:**

```cpp
const auto status = application.setWindowTitle(window, "Inspector — ready");
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **windowSize**

#### `[[nodiscard]] Result<std::pair<int, int>> windowSize() const noexcept`

- **Returns:** A (width, height) pair, or an app/window/thread access error.
- **Arguments:** none.

Return the window size in screen coordinates.

**Example:**

```cpp
auto result = application.windowSize();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto [width, height] = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **windowSize**

#### `[[nodiscard]] Result<std::pair<int, int>> windowSize(WindowId id) const noexcept`

- **Returns:** A (width, height) pair, or an app/window/thread access error.
- **Arguments:** `id` Registered window identity.

Return a specific window's size in screen coordinates.

**Example:**

```cpp
auto result = application.windowSize(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto [width, height] = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **framebufferSize**

#### `[[nodiscard]] Result<std::pair<int, int>> framebufferSize() const noexcept`

- **Returns:** A (width, height) pair, or an app/window/thread access error.
- **Arguments:** none.

Return the framebuffer size in pixels.

**Example:**

```cpp
auto result = application.framebufferSize();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto [framebufferWidth, framebufferHeight] = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **framebufferSize**

#### `[[nodiscard]] Result<std::pair<int, int>> framebufferSize(WindowId id) const noexcept`

- **Returns:** A (width, height) pair, or an app/window/thread access error.
- **Arguments:** `id` Registered window identity.

Return a specific window's framebuffer size in pixels.

**Example:**

```cpp
auto result = application.framebufferSize(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const auto [framebufferWidth, framebufferHeight] = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setWindowInputConfig**

#### `[[nodiscard]] Status setWindowInputConfig(const WindowInputConfig& config) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `config` Const reference to the WindowInputConfig struct to activate.

Apply window input configuration.

The initial window input configuration is set from FlowUi::WindowConfig by default.

**Example:**

```cpp
FlowUi::WindowInputConfig input{};
const auto status = application.setWindowInputConfig(input);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setWindowInputConfig**

#### `[[nodiscard]] Status setWindowInputConfig(WindowId id, const WindowInputConfig& config) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `id` Registered window identity.; `config` Input configuration to apply.

Apply input configuration to a specific window.

**Example:**

```cpp
FlowUi::WindowInputConfig input{};
const auto status = application.setWindowInputConfig(window, input);
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **windowInputConfig**

#### `[[nodiscard]] Result<WindowInputConfig> windowInputConfig() const noexcept`

- **Returns:** Active input configuration, or an app/window/thread access error.
- **Arguments:** none.

Return the current window input configuration.

**Example:**

```cpp
auto result = application.windowInputConfig();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowInputConfig inputConfiguration = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **windowInputConfig**

#### `[[nodiscard]] Result<WindowInputConfig> windowInputConfig(WindowId id) const noexcept`

- **Returns:** Active input configuration, or an app/window/thread access error.
- **Arguments:** `id` Registered window identity.

Return a specific window's current input configuration.

**Example:**

```cpp
auto result = application.windowInputConfig(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const FlowUi::WindowInputConfig inputConfiguration = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **nativeWindowHandle**

#### `[[nodiscard]] Result<void*> nativeWindowHandle() const noexcept`

- **Returns:** Borrowed native backend handle, or an app/window/thread access error.
- **Arguments:** none.

Return the backend native window handle.

**Note:** The concrete handle type depends on the active window backend.

**Example:**

```cpp
auto result = application.nativeWindowHandle();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
void* nativeHandle = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **nativeWindowHandle**

#### `[[nodiscard]] Result<void*> nativeWindowHandle(WindowId id) const noexcept`

- **Returns:** Borrowed native backend handle, or an app/window/thread access error.
- **Arguments:** `id` Registered window identity.

Return a specific window's backend-native handle.

**Example:**

```cpp
auto result = application.nativeWindowHandle(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
void* nativeHandle = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **supportsRawMouseMotion**

#### `[[nodiscard]] Result<bool> supportsRawMouseMotion() const noexcept`

- **Returns:** Raw-motion capability on success, or an app/window/thread access error.
- **Arguments:** none.

Return whether the window backend supports raw mouse motion.

**Example:**

```cpp
auto result = application.supportsRawMouseMotion();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool rawMotionSupported = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **supportsRawMouseMotion**

#### `[[nodiscard]] Result<bool> supportsRawMouseMotion(WindowId id) const noexcept`

- **Returns:** `Result<bool>`.
- **Arguments:** `id` Registered window identity.

Return whether a specific window supports raw mouse motion.

**Example:**

```cpp
auto result = application.supportsRawMouseMotion(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
const bool rawMotionSupported = *result;
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setClipboardText**

#### `[[nodiscard]] Status setClipboardText(std::string_view text) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `text` String view for the text to set in the system clipboard.

Set clipboard text through the window backend.

**Example:**

```cpp
const auto status = application.setClipboardText("Copied from FlowUi");
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **setClipboardText**

#### `[[nodiscard]] Status setClipboardText(WindowId id, std::string_view text) noexcept`

- **Returns:** `Status`; check success before continuing.
- **Arguments:** `id` Registered window identity.; `text` Text to place in the system clipboard.

Set clipboard text through a specific window backend.

**Example:**

```cpp
const auto status = application.setClipboardText(window, "Copied from FlowUi");
if (!status) {
    throw FlowUi::FlowUiException(status.error());
}
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **clipboardText**

#### `[[nodiscard]] Result<std::string> clipboardText() const noexcept`

- **Returns:** Owned clipboard text, or a structured app/window/thread/clipboard error.
- **Arguments:** none.

Read clipboard text through the window backend.

**Example:**

```cpp
auto result = application.clipboardText();
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
std::string text = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **clipboardText**

#### `[[nodiscard]] Result<std::string> clipboardText(WindowId id) const noexcept`

- **Returns:** Owned clipboard text, or a structured app/window/thread/clipboard error.
- **Arguments:** `id` Registered window identity.

Read clipboard text through a specific window backend.

**Example:**

```cpp
auto result = application.clipboardText(window);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
std::string text = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/App.hpp).

### **makeApplication**

#### `[[nodiscard]] Result<App> makeApplication(const AppConfig& cfg) noexcept`

- **Returns:** Sole owner of the initialized runtime and semantic main window, or a structured configuration, allocation, platform, resource, or rendering error.
- **Arguments:** `cfg` Configuration struct used for initialization.

Create a running FlowUi application from configuration.

Creates an App object, allocates its internal implementation, and initializes
the runtime managers.

**Note:** Runtime access and window/frame work must remain on the creating platform thread.

**Example:**

```cpp
FlowUi::AppConfig configuration{};
auto result = FlowUi::makeApplication(configuration);
if (!result) {
    throw FlowUi::FlowUiException(result.error());
}
FlowUi::App application = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/App.hpp).
