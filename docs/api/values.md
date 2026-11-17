# Identities, errors, and results

These are public values and helpers used across the [App](docs/api/app.md), [manager APIs](docs/api/all_public_api.md), and element authoring APIs. Identity factories return direct scalar values and are `noexcept`; they do not return `Result`. Error values describe failures. `Result<T>` and `Status` carry a caller-visible success or error alternative.

Use the public headers linked below, or [Flow.hpp](include/FlowUi/Public/Flow.hpp) for the application and identity surface. Include [Error.hpp](include/FlowUi/Public/errorManagement/Error.hpp) for the full error/result surface. Examples use `<array>`, `<string>`, `<string_view>`, and `<utility>` where needed. The global/part examples use the public `FlowUi::kButton` tag.

## Identity values and their scope

[ElementIDTypes.hpp](include/FlowUi/Public/identificationTypes/ElementIDTypes.hpp), [ElementID.hpp](include/FlowUi/Public/identificationTypes/ElementID.hpp), [AppActionID.hpp](include/FlowUi/Public/identificationTypes/AppActionID.hpp), [TextureHandle.hpp](include/FlowUi/Public/identificationTypes/TextureHandle.hpp), and [WindowId.hpp](include/FlowUi/Public/identificationTypes/WindowId.hpp) declare these types.

| Value | Scope and invalid value |
|---|---|
| `FlowDefinitionID` | Stable authored element definition. `value == 0` is invalid. |
| `FlowElementID` | Resolved local element instance. Identity includes its parent and definition; `WindowId` supplies the outer storage/interaction scope. Zero is invalid. |
| `GlobalFlowID` | Definition-scoped, parent-independent element identity. The same numeric value still denotes separate instances in separate windows. Zero is invalid. |
| `FlowElementPart` | Definition-time semantic part declaration; `token == 0` is invalid. |
| `FlowElementPartID` | Part declaration resolved against a concrete owner instance and definition. Zero is invalid. |
| `LocalElementName` | Compile-time local-name token; default construction produces an invalid token. |
| `RuntimeElementName` | Explicit runtime-hashed local-name token. Default initialization produces an invalid token. |
| `IndexedElementName` | Numeric indexed/keyed local-name token. Default initialization produces an invalid token. |
| `AutoElementName` | File/line/column callsite token. Default initialization produces an invalid token. |
| `AppActionID` | App-level action identity. Value initialization (`AppActionID{}`) produces the invalid zero ID; plain default initialization does not initialize its fields. |
| `TextureHandle` | App-level slot index and generation. Either field being zero makes the handle invalid. |
| `WindowId` | `uint64_t` window identity. `InvalidWindowId == 0`, `MainWindowId == 1`. The semantic main ID is available through `App::mainWindowId()`. |

`InvalidFlowDefinitionID`, `InvalidFlowElementID`, `InvalidGlobalFlowID`, `InvalidFlowElementPart`, `InvalidFlowElementPartID`, `InvalidAppActionID`, and `InvalidTextureHandle` are value-initialized sentinels. `RootFlowScopeID` is the stable semantic parent for top-level local elements.

A nonzero identity does not prove an element/action/texture is registered, bound, or alive. Managers perform those checks. In developer builds, identity values can retain borrowed names or source paths. Keep runtime name/path bytes alive for all retained uses. Numeric equality and hashing ignore developer labels.

### Validity, equality, and hashes

All explicit validity conversions below are `[[nodiscard]] constexpr explicit operator bool() const noexcept`.

| Type | Validity test | Equality / ordering |
|---|---|---|
| `FlowDefinitionID` | `value != 0` | `operator==(FlowDefinitionID, FlowDefinitionID) noexcept`, defaulted. |
| `FlowElementID` | `value != 0` | `operator==(FlowElementID, FlowElementID) noexcept` compares `value`. |
| `GlobalFlowID` | `value != 0` | `operator==(GlobalFlowID, GlobalFlowID) noexcept` compares `value`. |
| `FlowElementPart` | `token != 0` | `operator==(FlowElementPart, FlowElementPart) noexcept` compares `token`. |
| `FlowElementPartID` | `value != 0` | `operator==(FlowElementPartID, FlowElementPartID) noexcept` compares `value`. |
| `LocalElementName` | `token != 0` | No named equality overload. |
| `RuntimeElementName` | `token != 0` | No named equality overload. |
| `IndexedElementName` | `token != 0` | `operator==(IndexedElementName, IndexedElementName) noexcept` compares `token`. |
| `AutoElementName` | `token != 0` | No named equality overload. |
| `AppActionID` | `value != 0` | `operator==(AppActionID, AppActionID) noexcept` compares `value`. |
| `TextureHandle` | `index != 0 && generation != 0` | Defaulted `operator<=>(const TextureHandle&) const noexcept` compares index then generation; equality is also generated. |

Every explicitly declared equality overload is `[[nodiscard]] constexpr` and is a free friend function. The texture three-way operator is `[[nodiscard]]` with deduced `auto` return type.

The public hash functors have `[[nodiscard]] constexpr std::size_t operator()(Id id) const noexcept`: `FlowDefinitionIDHash`, `FlowElementIDHash`, `GlobalFlowIDHash`, `FlowElementPartIDHash`, and `AppActionIDHash`. They hash their corresponding numeric identities; equal IDs hash equally. Hashes are container hashes, so do not substitute them for the full identity.

```cpp
constexpr auto first = FlowUi::DefinitionID("editor.document");
constexpr auto second = FlowUi::DefinitionID("editor.document");
const bool valid = static_cast<bool>(first);
const bool same = first == second;
const std::size_t hash = FlowUi::FlowDefinitionIDHash{}(first);
```

The same pattern applies to the other typed identities; for example:

```cpp
constexpr auto first = FlowUi::AppAction("editor.save");
constexpr auto second = FlowUi::AppAction("editor.save");
const bool valid = static_cast<bool>(first);
const bool same = first == second;
const std::size_t hash = FlowUi::AppActionIDHash{}(first);
```

```cpp
constexpr auto first = FlowUi::Part("label");
constexpr auto second = FlowUi::Part("label");
const bool valid = static_cast<bool>(first);
const bool same = first == second;
```

### Literal and runtime name factories

All free factories below are `[[nodiscard]]`; `N` is the literal array extent.

| Signature | Behavior |
|---|---|
| `constexpr LocalElementName() noexcept` | Invalid local-name token. |
| `template <std::size_t N> consteval LocalElementName(const char (&name)[N]) noexcept` | Hash an authored literal; developer builds borrow its literal label. |
| `template <std::size_t N> consteval FlowDefinitionID DefinitionID(const char (&name)[N]) noexcept` | Create a stable element-definition ID. |
| `constexpr RuntimeElementName RuntimeName(std::string_view name) noexcept` | Hash a runtime name; empty names produce invalid tokens. Developer builds borrow `name`. |
| `template <std::size_t N> consteval AppActionID AppAction(const char (&name)[N]) noexcept` | Hash an app action literal; an empty literal produces an invalid ID. |
| `template <std::size_t N> consteval FlowElementPart Part(const char (&name)[N]) noexcept` | Hash a definition-owned semantic part declaration. Resolve it against an owner with `PartID()`. |

Constructors do not carry `[[nodiscard]]`; the attribute applies to the free factory functions. Authored names should be stable across frames and builds when retained identity is desired. Runtime hashing does not allocate or intern a label.

```cpp
constexpr FlowUi::LocalElementName absent{};
constexpr FlowUi::LocalElementName saveButton{"toolbar/save"};
static_assert(!absent);
static_assert(static_cast<bool>(saveButton));
```

```cpp
std::string_view label = "toolbar/save";
const FlowUi::RuntimeElementName name = FlowUi::RuntimeName(label);
```

The caller owns the runtime label bytes. The example literal has static storage.

```cpp
constexpr FlowUi::FlowDefinitionID definition = FlowUi::DefinitionID("example.editor");
```

```cpp
constexpr FlowUi::AppActionID save = FlowUi::AppAction("application.save");
```

### Positional and data-keyed names

| Signature | Behavior |
|---|---|
| `constexpr IndexedElementName Indexed(LocalElementName name, uint64_t index) noexcept` | Compose the local token with a numeric position. An invalid base produces an invalid result. |
| `constexpr IndexedElementName Keyed(LocalElementName name, uint64_t key) noexcept` | Same numeric composition as `Indexed`; the application supplies a stable data key. |
| `constexpr explicit IndexedElementIDSequence(LocalElementName baseName, uint64_t firstIndex = 0) noexcept` | Start a local positional counter. |
| `constexpr IndexedElementIDSequence IndexedIDs(LocalElementName baseName, uint64_t firstIndex = 0) noexcept` | Convenience factory for that counter. |
| `constexpr IndexedElementName IndexedElementIDSequence::next() noexcept` | Return `Indexed(baseName, nextIndex)` and increment the counter, including for an invalid base. |
| `constexpr uint64_t IndexedElementIDSequence::nextIndex() const noexcept` | Observe the index the next call will consume. |

The free functions, `next()`, and `nextIndex()` are `[[nodiscard]]`. Recreate a sequence at the same starting position each frame for stable-order repeated UI. Use `Keyed()` with a stable model key when rows can reorder. The counter is `uint64_t`; wrapping it repeats positions.

```cpp
constexpr FlowUi::LocalElementName row{"records/row"};
const auto positional = FlowUi::Indexed(row, 0);
const std::uint64_t recordKey = 42;
const auto keyed = FlowUi::Keyed(row, recordKey);
const bool same = positional == keyed;
```

```cpp
FlowUi::IndexedElementIDSequence sequence{"records/row", 0};
const auto firstRow = sequence.next();
const auto secondRow = sequence.next();
const auto nextIndex = sequence.nextIndex();
auto continued = FlowUi::IndexedIDs("records/row", nextIndex);
```

### Callsite, global, and part identities

| Signature | Behavior |
|---|---|
| `constexpr AutoElementName AutoIDAt(std::string_view fileName, uint32_t line, uint32_t column) noexcept` | Hash source location; an empty filename produces an invalid token. Developer builds borrow the filename. |
| `constexpr AutoElementName AutoID(std::source_location location = std::source_location::current()) noexcept` | Available with standard `source_location`; captures the calling location through the default argument. |
| `constexpr AutoElementName AutoID(const char* fileName = __builtin_FILE(), uint32_t line = __builtin_LINE(), uint32_t column = 0) noexcept` | Compatibility spelling when standard `source_location` is unavailable. Null filename produces an invalid token. |
| `template <auto& Element, std::size_t N> consteval GlobalFlowID Global(const char (&name)[N]) noexcept` | Definition-scoped global identity; requires the element tag type to expose a nonzero `FlowDefinitionID definitionId`. |
| `template <auto& Element, std::size_t N> consteval GlobalFlowID GlobalID(const char (&name)[N]) noexcept` | Long-form alias with the same constraint and output as `Global`. |
| `template <typename OwnerElement> constexpr FlowElementPartID PartID(const OwnerElement&, FlowElementID owner, FlowElementPart part) noexcept` | Bind a semantic part to a local owner. Requires a valid typed definition; invalid owner or part produces an invalid result. |
| `template <typename OwnerElement> constexpr FlowElementPartID PartID(const OwnerElement& element, GlobalFlowID owner, FlowElementPart part) noexcept` | Same part resolution for a global owner. |

All functions in this table are `[[nodiscard]]`. The `AutoID` overloads are mutually exclusive build spellings. Automatic IDs identify a callsite, so repeated construction under one parent at the same callsite needs an explicit index/key. A global ID bypasses parent composition while retaining its element definition and window scope. `PartID` uses the element argument's type and does not retain the argument.

```cpp
const auto automatic = FlowUi::AutoID();
constexpr auto explicitLocation = FlowUi::AutoIDAt("screens/editor.cpp", 120, 8);
```

```cpp
constexpr auto saveButton = FlowUi::Global<FlowUi::kButton>("toolbar/save");
constexpr auto sameButton = FlowUi::GlobalID<FlowUi::kButton>("toolbar/save");
static_assert(saveButton == sameButton);
```

```cpp
constexpr auto label = FlowUi::Part("label");
const auto localPart = FlowUi::PartID(FlowUi::kButton, localOwner, label);
constexpr auto globalOwner = FlowUi::Global<FlowUi::kButton>("toolbar/save");
const auto globalPart = FlowUi::PartID(FlowUi::kButton, globalOwner, label);
```

### Clay ID bridge and texture packing

The three `[[nodiscard]] constexpr uint32_t FlowIDToClayID(Id id) noexcept` overloads accept `FlowElementID`, `GlobalFlowID`, and `FlowElementPartID`. They deterministically fold a Flow identity into Clay's 32-bit layout domain. Invalid zero maps to zero; a valid ID maps to a nonzero layout ID. Folding is not reversible and does not establish resource identity.

```cpp
const std::uint32_t localClayId = FlowUi::FlowIDToClayID(local);
const std::uint32_t globalClayId = FlowUi::FlowIDToClayID(global);
const std::uint32_t partClayId = FlowUi::FlowIDToClayID(part);
```

`TextureHandle::packed() const noexcept` is `[[nodiscard]] constexpr uint64_t`; it places generation in the high 32 bits and index in the low 32 bits. `TextureHandle::fromPacked(uint64_t value) noexcept` is `[[nodiscard]] static constexpr TextureHandle`; it reconstructs the fields without checking resource liveness or ownership. Keep the full generation when storing handles.

```cpp
const std::uint64_t packed = texture.handle.packed();
const auto restored = FlowUi::TextureHandle::fromPacked(packed);
const bool same = texture.handle == restored;
const auto ordering = texture.handle <=> restored;
```

### Resource keys and capacity target IDs

[ResourceKey.hpp](include/FlowUi/Public/identificationTypes/ResourceKey.hpp) declares a non-owning call-site aggregate: `name` defaults empty, `domain` defaults `ResourceDomain::Auto`, and `window` defaults `InvalidWindowId`. Receiving managers intern the name before returning. Auto selects the receiving manager's domain; zero window selects app scope for app-shared managers or the owning window for window-local managers. Resource domains are Auto, Image, Icon, Font, Ui, InputField, Viewport, Development, and Internal.

[MemoryCapacityTargets.hpp](include/FlowUi/Public/identificationTypes/MemoryCapacityTargets.hpp) declares `MemoryCapacityTargetId = uint64_t` and two `[[nodiscard]]` functions:

| Signature | Behavior |
|---|---|
| `constexpr MemoryCapacityTargetId memory_capacity_target_id(std::string_view name) noexcept` | Hash exact case-sensitive bytes with the registered target ID algorithm, without allocation or interning. Returns nonzero even for an empty name. |
| `consteval MemoryCapacityTargetId makeMemoryCapacityTargetId(std::string_view name) noexcept` | Compile-time spelling of the same algorithm. |

Hashing a key does not establish that it is registered. App creation validates settings against the [capacity catalog](docs/api/configuration-glossary.md#flowui_config_capacity_keys).

```cpp
constexpr auto target = FlowUi::makeMemoryCapacityTargetId(
    "flowui.tuning.input_fields.registry_entries");
std::string_view key = "flowui.tuning.input_fields.registry_entries";
const auto runtimeTarget = FlowUi::memory_capacity_target_id(key);
const bool same = target == runtimeTarget;
```

## Colors and ValueOrThrow

[Color.hpp](include/FlowUi/Public/Color.hpp) provides compile-time `Flow_Color` for literal `#RRGGBB`/`#RRGGBBAA` text and checked `Result<Clay_Color> Flow_Color(std::string_view) noexcept` for runtime text. Invalid runtime text returns `ColorInvalid`; invalid literal text is rejected during compilation.

The same public header declares `FlowUi::ValueOrThrow`. It returns a checked result's successful value or throws `FlowUiException` preserving the original error. Mutable/const lvalue results return borrowed `T&`/`const T&`; a temporary returns an owned moved value, and a const temporary returns an owned copied value. `ValueOrThrow(Status)` returns void on success. The helper never changes a failed frame into a valid one and does not report or select recovery. See [ownership and examples](docs/api/result_handling.md#propagate-a-checked-result-with-valueorthrow).

```cpp
const auto color = FlowUi::ValueOrThrow(
    FlowUi::Flow_Color(std::string_view{"#20242c"}));
```

## Compact errors, descriptors, and reporting

[ErrorValue.hpp](include/FlowUi/Public/errorManagement/ErrorValue.hpp) defines the trivially copyable 24-byte `FlowUiError`. It owns five numeric fields: `subject`, `auxiliary`, `nativeCode`, `code`, and `site`. Value initialization sets them to zero/None. It owns no dynamic message, source location, stack, or state snapshot. Those details can belong to development sidecars.

`[[nodiscard]] constexpr FlowUiError makeError(ErrorCode code, ErrorSite site, std::uint64_t subject = 0u, std::uint64_t auxiliary = 0u, std::uint32_t nativeCode = 0u) noexcept` copies those supplied fields without allocation or formatting. The detecting site determines how `subject` is interpreted; auxiliary/native values remain site/backend specific. Preserve all fields when propagating an error.

### FlowUiError observers

Every observer is `[[nodiscard]] constexpr` and `noexcept`.

| Member signature | Meaning |
|---|---|
| `bool hasError() const` | `code != ErrorCode::None`. |
| `explicit operator bool() const` | Same error-presence test. This truth test has the opposite meaning from a Result truth test. |
| `ErrorDescriptor descriptor() const` | Static code metadata. |
| `ErrorSiteDescriptor siteDescriptor() const` | Static detecting-operation/subject metadata. |
| `ErrorDomain domain() const` | Decode owning domain from the stable code value. |
| `ErrorSubjectKind subjectKind() const` | Subject interpretation supplied by the detecting site. |
| `std::string_view message() const` | Borrow the static, null-terminated code message. |

```cpp
const auto error = FlowUi::makeError(FlowUi::ErrorCode::InvalidWindowId,
                                     FlowUi::ErrorSite::AppAccessWindow, window);
const bool failed = error.hasError();
const bool containsError = static_cast<bool>(error);
const auto codeMetadata = error.descriptor();
const auto siteMetadata = error.siteDescriptor();
const auto domain = error.domain();
const auto subjectKind = error.subjectKind();
const std::string_view message = error.message();
```

### Descriptor helper functions

All five functions are `[[nodiscard]] constexpr` and `noexcept`.

| Signature | Meaning |
|---|---|
| `ErrorDescriptor describeError(ErrorCode code)` | Catalog descriptor. Unknown numeric codes retain their value and receive the fatal, process-boundary `UnknownErrorCode` descriptor. |
| `std::string_view errorName(ErrorCode code)` | Static code name; unknown codes return `UnknownErrorCode`. |
| `std::string_view errorMessage(ErrorCode code)` | Static, null-terminated message; unknown codes use the catalog fallback. |
| `ErrorSiteDescriptor describeErrorSite(ErrorSite site)` | Catalog site descriptor. Unknown numeric sites retain their value, use subject kind None, and name `UnknownErrorSite`. |
| `std::string_view errorSiteName(ErrorSite site)` | Static operation name or `UnknownErrorSite`. |

[ErrorDescriptor](include/FlowUi/Public/errorManagement/ErrorCodeTypes.hpp) contains `code`, `domain`, `category`, `boundary`, `resolvability`, `impact`, `resolutionPolicy`, `defaultResolution`, `name`, and `message`. [ErrorSiteDescriptor](include/FlowUi/Public/errorManagement/ErrorSites.hpp) contains `site`, `expectedSubject`, and `name`. Their text views refer to static catalog storage. Descriptor lookup itself performs no recovery and does not report or throw the described error.

The categories, domains, boundaries, impacts, resolutions, and subject kinds are declared in [ErrorTypes.hpp](include/FlowUi/Public/errorManagement/ErrorTypes.hpp). Recovery/reporting settings are described in the [configuration glossary](docs/api/configuration-glossary.md) and [error handling guide](docs/api/result_handling.md).

```cpp
const auto code = FlowUi::ErrorCode::InvalidWindowId;
const auto site = FlowUi::ErrorSite::AppAccessWindow;
const auto codeMetadata = FlowUi::describeError(code);
const auto siteMetadata = FlowUi::describeErrorSite(site);
const std::string_view name = FlowUi::errorName(code);
const std::string_view message = FlowUi::errorMessage(code);
const std::string_view operation = FlowUi::errorSiteName(site);
```

### FlowUiException

[Exception.hpp](include/FlowUi/Public/errorManagement/Exception.hpp) declares the typed exception used to carry a FlowUi-authored error to an enclosing containing boundary. It derives from `std::exception`. Ordinary public authoring APIs may throw; App `Result`/`Status` frame boundaries contain failures as documented by their API.

| Signature | Meaning |
|---|---|
| `explicit FlowUiException(FlowUiError error) noexcept` | Copy the compact value without allocating. |
| `[[nodiscard]] const FlowUiError& error() const noexcept` | Borrow the error for the lifetime of this exception. Copy it to retain it longer. |
| `[[nodiscard]] const char* what() const noexcept override` | Static, null-terminated message for the stored code. |

```cpp
const FlowUi::FlowUiException exception(error);
const FlowUi::FlowUiError& storedError = exception.error();
const char* message = exception.what(); // Static storage.
```

### ErrorSink

[ErrorPolicy.hpp](include/FlowUi/Public/errorManagement/ErrorPolicy.hpp) defines the non-owning `ErrorSink` aggregate with `userData = nullptr` and `callback = nullptr`.

| Signature | Meaning |
|---|---|
| `[[nodiscard]] constexpr explicit operator bool() const noexcept` | A callback is installed. |
| `void notify(const ErrorEventView& event) const noexcept` | Invoke it synchronously if installed; otherwise do nothing. |

`ErrorSinkCallback` is `void (*)(void* userData, const ErrorEventView& event) noexcept`. The borrowed context must remain alive while used. The event and `nativeMessage` views expire when the sink call returns; copy any data that needs to be retained. The callback must satisfy its nonthrowing contract.

[ErrorEventView](include/FlowUi/Public/errorManagement/ErrorEvents.hpp) contains `error`, borrowed `nativeMessage`, `nativeCategory`, `kind`, `resolution`, and `inspection`. Reporting observes an occurrence; it does not select recovery or make a failed Result successful.

```cpp
std::uint64_t errorCount = 0;
const FlowUi::ErrorSink sink{
    .userData = &errorCount,
    .callback = [](void* userData, const FlowUi::ErrorEventView&) noexcept {
        ++*static_cast<std::uint64_t*>(userData);
    },
};
if (sink)
    sink.notify(FlowUi::ErrorEventView{.error = error});
```

## Result<T>, Status, and unexpectedError

[Result.hpp](include/FlowUi/Public/errorManagement/Result.hpp) selects its implementation using the standard library feature macro. With `__cpp_lib_expected >= 202202L`, `Result<T>` is exactly `std::expected<T, FlowUiError>`. Otherwise FlowUi supplies its compatibility implementation. In both cases `Status` is `Result<void>`.

A Result is a value container, not an exception boundary: constructing/copying/moving a successful `T` can throw. The particular API producing it determines what failures it contains and whether that API is `noexcept`. Do not infer that all code working with Result is nonthrowing.

### Portable construction and propagation

Use `Result<T>{}` to default-construct a successful `T` when `T` is default constructible. Construct from `const T&` to copy a successful value, or `T&&` to move one. Copy/move Result operations follow the capabilities of the contained value and implementation. `Status{}` represents success without a payload.

`[[nodiscard]] constexpr unexpectedError(FlowUiError error) noexcept` wraps the error for construction or return of any failed `Result<T>` or `Status`. Its concrete return type is `std::unexpected<FlowUiError>` in the standard implementation and a compatibility carrier in the fallback. Use it through type deduction/return conversion; do not name the compatibility carrier or private support types.

Use a meaningful non-None error code for failures. A Result truth test describes its active alternative; it does not inspect `FlowUiError::hasError()`, a contained boolean, an empty string, a null pointer, or an invalid handle.

```cpp
const std::string title = "Inspector";
FlowUi::Result<std::string> copied{title};
FlowUi::Result<std::string> moved{std::string{"Inspector"}};
FlowUi::Result<std::string> failed = FlowUi::unexpectedError(FlowUi::makeError(
    FlowUi::ErrorCode::InvalidWindowConfiguration, FlowUi::ErrorSite::AppCreateWindow));
FlowUi::Result<std::string> emptyTitle{}; // Successful empty string.
```

```cpp
FlowUi::Status success{};
if (success.has_value())
    success.value(); // No payload.

FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
    FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
if (!failed) {
    FlowUi::FlowUiError& error = failed.error();
    const FlowUi::Status& readOnly = failed;
    const FlowUi::FlowUiError& observedError = readOnly.error();
}
```

To propagate a failed value Result as a different Result type or as Status, first check it, then return `unexpectedError(result.error())`. To propagate an already matching Status, return that Status. Neither propagation needs `.value()` or an exception.

```cpp
if (!result)
    return FlowUi::unexpectedError(result.error()); // Preserve the original failure.
```

### Common non-void access interface

These are the overloads supplied by the fallback and usable with the standard implementation. `T` is a non-void payload type. The standard alias can provide additional overloads depending on the installed C++ library; those extensions are not portable to FlowUi's fallback.

| Signature | Requires / result |
|---|---|
| `[[nodiscard]] bool has_value() const noexcept` | Observe success. |
| `[[nodiscard]] explicit operator bool() const noexcept` | Same success test. |
| `[[nodiscard]] T& value() &` | Success; borrow mutable payload. |
| `[[nodiscard]] const T& value() const&` | Success; borrow const payload. |
| `[[nodiscard]] T&& value() &&` | Success; move from payload. The returned reference still refers to this Result's storage. |
| `[[nodiscard]] T& operator*() &` | Success; borrow mutable payload. |
| `[[nodiscard]] const T& operator*() const&` | Success; borrow const payload. |
| `[[nodiscard]] T* operator->()` | Success; pointer into payload storage. |
| `[[nodiscard]] const T* operator->() const` | Success; const pointer into payload storage. |
| `[[nodiscard]] FlowUiError& error() &` | Failure; borrow mutable error. |
| `[[nodiscard]] const FlowUiError& error() const&` | Failure; borrow const error. |
| `template <typename U> [[nodiscard]] T value_or(U&& fallback) const&` | Copy successful value, otherwise convert fallback to T. Requires compatible construction/conversion. |

The fallback functions use the signatures above; its value/dereference/error operations are not declared `noexcept`. Standard-library signatures can additionally be `constexpr`/`noexcept` where the standard requires. Payload references/pointers expire with the Result, after replacement of its alternative, or according to the referenced object's lifetime when the payload is itself a non-owning wrapper.

```cpp
FlowUi::Result<std::string> title{std::string{"Inspector"}};
if (title.has_value()) {
    std::string& editable = title.value();
    const auto& readOnly = std::as_const(title);
    const std::string& observed = readOnly.value();
    std::string owned = std::move(title).value();
}
```

```cpp
FlowUi::Result<std::string> title{std::string{"Inspector"}};
if (title) {
    std::string& editable = *title;
    const auto& readOnly = std::as_const(title);
    const std::string& observed = *readOnly;
    const std::size_t length = title->size();
    const std::size_t observedLength = readOnly->size();
}
```

```cpp
FlowUi::Result<int> result = FlowUi::unexpectedError(FlowUi::makeError(
    FlowUi::ErrorCode::InvalidWindowId, FlowUi::ErrorSite::AppAccessWindow, window));
if (!result) {
    FlowUi::FlowUiError& error = result.error();
    const auto& readOnly = std::as_const(result);
    const FlowUi::FlowUiError& observedError = readOnly.error();
}
```

```cpp
const std::string title = result.value_or(std::string{"Untitled"});
```

`value_or` is appropriate only when the application deliberately chooses a fallback. Its fallback expression is constructed before the call, even when the Result succeeds. It does not report the original error; check/report it separately if needed.

### Result<void> / Status interface

| Signature | Requires / result |
|---|---|
| `Result() noexcept` | Successful no-value result. |
| Construction from `unexpectedError(error)` | Failed no-value result; no payload construction. |
| `[[nodiscard]] bool has_value() const noexcept` | Observe success. |
| `[[nodiscard]] explicit operator bool() const noexcept` | Same success test. |
| `void value() const` | Require/validate success; returns no value. |
| `[[nodiscard]] FlowUiError& error() &` | Failure; borrow mutable error. |
| `[[nodiscard]] const FlowUiError& error() const&` | Failure; borrow const error. |

Fallback `Result<void>::error()` overloads are `noexcept`, but the failure precondition remains. Status has no portable payload dereference/arrow or `value_or` interface. The Status example above demonstrates construction, checked access, and both mutable/const error overloads.

### Check the active alternative first

| Operation on the wrong alternative | Standard expected implementation | FlowUi compatibility implementation |
|---|---|---|
| Non-void `value()` on failure | Throws `std::bad_expected_access<FlowUiError>`. | Throws `std::bad_variant_access`. |
| Non-void `operator*` / `operator->` on failure | Violates the success precondition; do not use it as an error check. | Calls `value()` and throws `std::bad_variant_access`. |
| Non-void `error()` on success | Violates the failure precondition. | Throws `std::bad_variant_access`. |
| Void `value()` on failure | Throws `std::bad_expected_access<FlowUiError>`. | Throws `std::bad_variant_access`. |
| Void `error()` on success | Violates the failure precondition. | Returns an inactive/default stored error; that value is not a valid operation outcome. |

Portable code checks `if (result)` before success access and `if (!result)` before error access. Do not depend on one backend's wrong-alternative exception behavior. The fallback has no monadic `and_then`, `transform`, `or_else`, or `transform_error` API, and does not expose standard-only `emplace`, `swap`, or additional construction/comparison overloads.

### Comfortable borrowed references

For `Result<std::reference_wrapper<T>>`, the Result owns the wrapper, while the target object belongs to its manager/App/window. After checking, extract `T& value = result->get()`; preserve constness for `reference_wrapper<const T>`. The wrapper can leave scope without destroying the target, but it does not extend its lifetime. See the function-specific [App accessor examples](docs/api/app.md) for every checked manager getter.
