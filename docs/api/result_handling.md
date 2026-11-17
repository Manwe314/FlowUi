# Results and borrowed references

`FlowUi::Result<T>` contains a successful `T` or a `FlowUiError`. `FlowUi::Status` is `Result<void>`. Include [Result.hpp](include/FlowUi/Public/errorManagement/Result.hpp), or a public header that uses these types. Each function reference documents its specific failure contract. `noexcept` applies to the call, not arbitrary work that the caller subsequently performs on its value.

## Propagate a checked result with ValueOrThrow

Include [Color.hpp](include/FlowUi/Public/Color.hpp), where the public helper is declared beside `Flow_Color`. Use `FlowUi::ValueOrThrow` when this operation deliberately has no local recovery choice:

```cpp
FlowUi::FontManager& fonts = FlowUi::ValueOrThrow(application.fonts()).get();
const Clay_Color color = FlowUi::ValueOrThrow(
    FlowUi::Flow_Color(std::string_view{"#20242cff"}));
FlowUi::ValueOrThrow(application.setWindowTitle(window, "Inspector"));
```

Failure throws `FlowUiException` with the original `FlowUiError`, preserving code, site, subject, auxiliary value and native code. Success for `Result<bool>` returns its bool, including false. Success for `Status` returns void. The helper does not report, retry, choose a fallback or clear a latched frame error. It is not `noexcept`; payload copy/move exceptions also propagate.

| Argument | Successful return |
|---|---|
| `Result<T>&` | Borrowed `T&` |
| `const Result<T>&` | Borrowed `const T&` |
| `Result<T>&&` | Owned `T` moved from the payload |
| `const Result<T>&&` | Owned copied `T` |
| `Status` | `void` |

References into an lvalue result remain valid only while that result and its payload do. Temporary results return owned payloads so no reference into a destroyed temporary escapes. A moved reference wrapper, pointer or view still borrows its underlying owner. To move an owned value out of a named result, pass `std::move(result)`; assigning from an lvalue without `auto&` otherwise copies the borrowed payload.

Direct operations such as `ui.toClayString(text)`, root `context.clayID()` and `fields.applyEdits(...)` already return their values and throw on operational failure. Call them directly rather than passing their plain return values to this Result helper. Named identities, theme queries and useful resource operations retain checked interfaces. Select an explicit fallback where it is meaningful, and use `ValueOrThrow` only for chosen propagation. [Migration details](ResultBoundaryMigration.md).

## Extract a manager into a normal reference

App manager accessors return `Result<std::reference_wrapper<T>>`. They can fail for an empty app, an unavailable service, or an invalid/closing window. Check first, then use `result->get()`:

```cpp
auto result = application.fonts();
if (!result)
    throw FlowUi::FlowUiException(result.error());
FlowUi::FontManager& fonts = result->get();
```

The `&` matters: `auto& fonts = result->get()` also works, while `auto fonts = result->get()` tries to copy the manager. Destroying the result does not destroy the manager or extend its lifetime. App-wide managers must not outlive their owning runtime; window-specific managers must not outlive their window or remain in use during its teardown. Do not retain a reference across operations that invalidate its owner. Moving the wrapper never transfers manager ownership.

Constness comes from the accessor overload and its target type, not the local result variable:

```cpp
const FlowUi::App& readOnlyApplication = application;
auto result = readOnlyApplication.actions();
if (!result)
    throw FlowUi::FlowUiException(result.error());
const FlowUi::ActionManager& actions = result->get();
```

A `const Result<std::reference_wrapper<T>>` still refers to mutable `T`. Use a const App or bind `const T&` when read-only access is intended. [App](docs/api/app.md) gives an example for every accessor, including build-gated services.

## Status has no payload

Check success before dependent work. A surrounding operation returning `Status` can propagate it unchanged:

```cpp
const auto status = application.setWindowTitle(window, "Inspector");
if (!status) {
    application.reportError(status.error());
    return status; // When the enclosing operation also returns Status.
}
```

Reporting records the error; it does not make a failed operation succeed. Choose retry, skip, or propagation according to that operation's contract. Read `.error()` only on failure.

## Move out an owned value

Returned strings, vectors, paths and App handles are owned by the result until moved or copied out. After checking, use `*result`, `result.value()` or `result->member`:

```cpp
std::filesystem::path path{"assets/logo.png"};
auto result = FlowUi::read_file_bytes(path);
if (!result)
    throw FlowUi::FlowUiException(result.error());
std::vector<unsigned char> bytes = std::move(*result);
```

A returned `std::span`, `std::string_view`, pointer or reference remains borrowed even when its wrapper is owned. For example, copy a successful `Result<std::string_view>` into `std::string` before its documented source is invalidated. Caller-side allocation, copying, moving or callbacks may throw; a function's `noexcept` declaration does not cover those later operations.

## Success and the contained value are separate

`if (result)` tests whether the operation succeeded. For `Result<bool>`, use `*result` to test the answer:

```cpp
auto result = application.shouldClose(window);
if (!result)
    throw FlowUi::FlowUiException(result.error());
const bool requestedClose = *result;
```

A successful `Result<const T*>` may contain `nullptr` when absence is documented. Check the result and then the pointer. Image or viewport lookup can succeed with an unavailable `TextureRef` under its missing-visual policy; success does not imply a live texture handle. The individual manager references explain their absence semantics.

## Explicit results and exceptions

Caller-recoverable requests and outer supervision use `Result<T>`/`Status`. Frame bridge and input authoring operations return direct values, booleans or `EditResult`, and propagate operational exceptions. Direct element authoring, action invocation and caller payload work can also propagate exceptions. Some theme and viewport operations combine a validation status with exceptions from arbitrary caller code or copying. Follow each signature and avoid adding `noexcept` to a caller that performs throwing work. `App::runFrame()` contains UI build failures and cancels the failed frame. A latched frame error takes precedence over a later unlatched callback exception instead of being replaced with a new allocation/foreign-error attribution. Each manager retains its first error; `UiManager::frameStatus()` selects UI, then input-field, then popup failure state. Managed-window callback containment follows the same precedence.

`ActionResult<T>` is a separate dispatch type. It uses `hasValue()` and dispatch errors such as `Unbound` or `ResultTypeMismatch`; invoked action exceptions propagate separately. See [actions](docs/api/action_manager.md).

## Portable Result interface

FlowUi aliases `std::expected<T, FlowUiError>` when available, otherwise supplies a compatibility implementation. Portable code uses `has_value()`, explicit `bool`, checked `value()`/`*`/`->`, failure-only `error()`, `value_or()` for copyable values, and `unexpectedError()`. Monadic functions such as `and_then()` are absent from the fallback. Wrong-alternative behavior differs between implementations; accessing an inactive value/error is never a recovery strategy. `value_or()` evaluates its argument before the call, may copy/convert, and discards the error when selecting the fallback.

See [values and errors](docs/api/values.md) for individual observers and overloads, and the [configuration glossary](docs/api/configuration-glossary.md) for recovery/reporting policy.
