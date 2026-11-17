# Input translation API

Include [InputTranslation.hpp](include/FlowUi/Public/input/InputTranslation.hpp) to author optional device translators. Native keyboard and mouse remain enabled. Events contain semantic window IDs and no backend handles. App owns the sink passed to a translator; it is already initialized. A standalone sink must be initialized before producers attach. Stop all worker producers before the sink is destroyed.

## Public API

### **InputSink**

Every emission is `[[nodiscard]] Status method(const Event&) noexcept`, uses a bounded queue, is safe for concurrent producers and does not allocate. Successful status means queue acceptance. Validation, uninitialized queue and overflow return errors. Overflow causes the engine to cancel affected held input before consuming subsequent input; handle the error and resynchronize the source rather than continuing a partial gesture. `InputSink` cannot be copied.

`initialize(std::size_t capacity = 4096) noexcept` reserves storage once. Zero capacity fails, repeated initialization fails, and allocation failure is returned. `capacity() const noexcept` returns zero before initialization and the actual fixed unread-event capacity afterward (which can exceed the requested reserve).

```cpp
FlowUi::InputSink sink;
const auto status = sink.initialize(1024);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

### **key**

#### `[[nodiscard]] Status key(const KeyEvent& event) noexcept`

Validates a supported `Key` and modifier bits (`Ctrl=1`, `Shift=2`, `Alt=4`, `Super=8`); release is a second event with `pressed=false`.

The caller supplies an initialized sink and a live target window. Device 42 represents an application translator and is distinct from `NativeInputDevice` (1).

```cpp
const FlowUi::KeyEvent event{
    .device = 42,
    .target = window,
    .key = FlowUi::Key::A,
    .pressed = true
};
const auto status = sink.key(event);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

### **text**

#### `[[nodiscard]] Status text(const TextInputEvent& event) noexcept`

Queues committed Unicode text. Surrogates and values above `0x10ffff` fail. Physical key events do not synthesize committed text.

The caller supplies an initialized sink and a live target window. Device 42 represents an application translator and is distinct from `NativeInputDevice` (1).

```cpp
const FlowUi::TextInputEvent event{
    .device = 42,
    .target = window,
    .codepoint = U'λ'
};
const auto status = sink.text(event);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

### **pointerMove**

#### `[[nodiscard]] Status pointerMove(const PointerMoveEvent& event) noexcept`

Coordinates are finite values in window space; App converts to layout coordinates once. Pointer kind must be supported.

The caller supplies an initialized sink and a live target window. Device 42 represents an application translator and is distinct from `NativeInputDevice` (1).

```cpp
const FlowUi::PointerMoveEvent event{
    .device = 42,
    .target = window,
    .kind = FlowUi::PointerKind::Virtual,
    .x = 80.0f,
    .y = 32.0f
};
const auto status = sink.pointerMove(event);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

### **pointerButton**

#### `[[nodiscard]] Status pointerButton(const PointerButtonEvent& event) noexcept`

Validates one of five buttons, pointer kind and phase. Send release/end or cancel for the same device/source/contact when appropriate.

The caller supplies an initialized sink and a live target window. Device 42 represents an application translator and is distinct from `NativeInputDevice` (1).

```cpp
const FlowUi::PointerButtonEvent event{
    .device = 42,
    .target = window,
    .kind = FlowUi::PointerKind::Virtual,
    .button = FlowUi::PointerButton::Primary,
    .pressed = true,
    .phase = FlowUi::InputPhase::Begin
};
const auto status = sink.pointerButton(event);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

### **scroll**

#### `[[nodiscard]] Status scroll(const ScrollInputEvent& event) noexcept`

Both scroll deltas must be finite and use native window-input units.

The caller supplies an initialized sink and a live target window. Device 42 represents an application translator and is distinct from `NativeInputDevice` (1).

```cpp
const FlowUi::ScrollInputEvent event{
    .device = 42,
    .target = window,
    .y = 1.0f
};
const auto status = sink.scroll(event);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

### **focus**

#### `[[nodiscard]] Status focus(const FocusInputEvent& event) noexcept`

Queues observed host focus; losing focus cancels held contributions when consumed.

The caller supplies an initialized sink and a live target window.

```cpp
const FlowUi::FocusInputEvent event{
    .target = window,
    .focused = false
};
const auto status = sink.focus(event);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

### **reset**

#### `[[nodiscard]] Status reset(const DeviceResetEvent& event) noexcept`

Queues cancellation of the matching device/source contributions. Device zero resets every device on the target.

The caller supplies an initialized sink and a live target window. Device 42 represents an application translator and is distinct from `NativeInputDevice` (1).

```cpp
const FlowUi::DeviceResetEvent event{
    .device = 42,
    .target = window
};
const auto status = sink.reset(event);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

### **connection**

#### `[[nodiscard]] Status connection(const DeviceConnectionEvent& event) noexcept`

Queues a device connection transition; disconnect cancels that contribution when consumed.

The caller supplies an initialized sink and a live target window. Device 42 represents an application translator and is distinct from `NativeInputDevice` (1).

```cpp
const FlowUi::DeviceConnectionEvent event{
    .device = 42,
    .target = window,
    .connected = false
};
const auto status = sink.connection(event);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

### **navigation**

#### `[[nodiscard]] Status navigation(const NavigationInputEvent& event) noexcept`

Validates finite magnitude, logical action and phase. Directional intents project to compatibility navigation keys.

The caller supplies an initialized sink and a live target window. Device 42 represents an application translator and is distinct from `NativeInputDevice` (1).

```cpp
const FlowUi::NavigationInputEvent event{
    .device = 42,
    .target = window,
    .action = FlowUi::NavigationAction::Activate,
    .phase = FlowUi::InputPhase::Begin,
    .magnitude = 1.0f
};
const auto status = sink.navigation(event);
if (!status)
    throw FlowUi::FlowUiException(status.error());
```

## InputTranslatorContext and InputTranslatorConcept

`InputTranslatorContext` borrows `targets` only for the translator call and supplies `deltaTimeSeconds`, `tick` and `eventCapacity` (default 4096). Each `InputRoutingTarget` contains `window`, `focused` (default false) and `enabled` (default true). `focusedTarget() const noexcept` chooses the first enabled focused window or `InvalidWindowId`; test for an absent destination before emitting targeted input.

`InputTranslatorConcept<T>` requires a nothrow default constructor/destructor; `Status initialize(const InputTranslatorContext&)`; `Status poll(const InputTranslatorContext&, InputSink&)`; and `void resetTarget(WindowId, InputSink&) noexcept` / `void shutdown() noexcept`. Initialize/poll may throw in a concrete translator: the pack contains those exceptions. A failing initializer must release its own partially acquired resources. Reset/shutdown must not throw across the host boundary. See [concept declaration](include/FlowUi/Public/input/InputTranslatorConcept.hpp).

## InputTranslatorPack<Translators...>

The default constructor is `noexcept`; copying is deleted. The destructor calls `shutdown()`. Composition is static and ownership stays inside the pack.

| Operation | Return and behavior |
| --- | --- |
| `initialize(const InputTranslatorContext&) noexcept` | `Status`; initialize in declaration order, roll back earlier successful acquisitions in reverse order on failure. A second initialization fails. |
| `poll(const InputTranslatorContext&, InputSink&) noexcept` | `Status`; requires initialization and stops at the first failed translator. Earlier emitted events remain queued. |
| `resetTarget(WindowId, InputSink&) noexcept` | `void`; delegates to all translators only while initialized, before a target disappears. |
| `shutdown() noexcept` | `void`; shuts down acquired translators in reverse order; repeat-safe. |
| `translators() noexcept` | Mutable `std::tuple<Translators...>&` for pre-initialization configuration. |
| `translators() const noexcept` | Borrowed const tuple reference. |

Direct consumers of a failed poll must reject/cancel the partial tick, reset affected targets and resynchronize external device state before reuse. App already cancels routed state and clears translated queues in its polling boundary. Reset/shutdown do not undo external SDK side effects. Do not replace or independently shut down tuple elements behind the pack's bookkeeping. App uses the source-build translator pack selected by `FLOWUI_ADDITIONAL_TRANSLATORS_HEADER` and `FLOWUI_ADDITIONAL_TRANSLATORS_TYPE`; see [build settings](docs/api/configuration-glossary.md#flowui_config_build).

Here `context` is the current polling context and `window` is a live routing target. The empty pack demonstrates lifecycle calls without a device SDK:

```cpp
FlowUi::InputSink sink;
if (const auto status = sink.initialize(context.eventCapacity); !status)
    throw FlowUi::FlowUiException(status.error());

FlowUi::InputTranslatorPack<> pack;
if (const auto status = pack.initialize(context); !status)
    throw FlowUi::FlowUiException(status.error());

if (const auto status = pack.poll(context, sink); !status) {
    pack.resetTarget(window, sink);
    pack.shutdown();
    throw FlowUi::FlowUiException(status.error());
}
pack.resetTarget(window, sink);
pack.shutdown();
```

## Key conversions

[Key.hpp](include/FlowUi/Public/input/Key.hpp) defines semantic keys independent of SDK enums. All helpers are `constexpr`, `[[nodiscard]]` and `noexcept`.

| Function | Contract |
| --- | --- |
| `int keyCode(Key)` | Numeric encoding of the supplied key; does not validate an arbitrary cast. |
| `std::optional<Key> keyFromCode(int)` | Valid supported key, or `std::nullopt` for unknown codes. |
| `std::optional<std::size_t> keySlot(Key)` | Validated slot in the compatibility key arrays, or `std::nullopt`. |

```cpp
constexpr int enterCode = FlowUi::keyCode(FlowUi::Key::Enter);
constexpr auto enterKey = FlowUi::keyFromCode(enterCode);
static_assert(enterKey && *enterKey == FlowUi::Key::Enter);
static_assert(FlowUi::keySlot(FlowUi::Key::Enter).has_value());
```

[InputEvents.hpp](include/FlowUi/Public/input/InputEvents.hpp) defines `InputEvent`, the variant of the nine event structs above. `InputDeviceId`, `InputSourceId` and `InputContactId` preserve independent device, producer and touch/pen contact identities. Keep an identity stable across the matching press, update, release and cancel events.
