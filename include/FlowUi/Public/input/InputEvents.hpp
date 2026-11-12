#pragma once

#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Public/input/Key.hpp"
#include <cstdint>
#include <variant>

namespace FlowUi {

/** Stable device identity selected by the translator; zero resets every device. */
using InputDeviceId = std::uint64_t;
/** Distinguish independent producers that share a device identity. */
using InputSourceId = std::uint64_t;
/** Stable contact identity for a touch/pen lifetime. */
using InputContactId = std::uint64_t;
/** Native keyboard/mouse source; additional translators should choose other identities. */
inline constexpr InputDeviceId NativeInputDevice = 1;

/** Pointer family, independent of native SDK encodings. */
enum class PointerKind : std::uint8_t { Mouse, Touch, Pen, Virtual };
/** Buttons preserve FrameInput's first five slots. */
enum class PointerButton : std::uint8_t { Primary, Secondary, Middle, Back, Forward };
/** Lifecycle of a semantic action or contact. */
enum class InputPhase : std::uint8_t { Begin, Update, End, Cancel };
/** Logical UI intents. Directional intents project to named navigation keys. */
enum class NavigationAction : std::uint8_t {
	Left,
	Right,
	Up,
	Down,
	Next,
	Previous,
	Activate,
	Cancel
};

/** A physical key transition. Modifiers use Ctrl=1, Shift=2, Alt=4, Super=8. */
struct KeyEvent {
	InputDeviceId device = NativeInputDevice;
	WindowId target = InvalidWindowId;
	InputSourceId source = 0;
	Key key = Key::Space;
	bool pressed = false;
	std::uint8_t modifiers = 0;
};
/** Committed Unicode scalar value; never synthesized from physical key codes. */
struct TextInputEvent {
	InputDeviceId device = NativeInputDevice;
	WindowId target = InvalidWindowId;
	InputSourceId source = 0;
	char32_t codepoint = U'\0';
};
/** Pointer coordinates in window space; App performs the one layout conversion. */
struct PointerMoveEvent {
	InputDeviceId device = NativeInputDevice;
	WindowId target = InvalidWindowId;
	InputSourceId source = 0;
	InputContactId contact = 0;
	PointerKind kind = PointerKind::Mouse;
	float x = 0.0f;
	float y = 0.0f;
	bool inside = true;
};
/** Button transition; non-mouse contact capture persists through release/cancel. */
struct PointerButtonEvent {
	InputDeviceId device = NativeInputDevice;
	WindowId target = InvalidWindowId;
	InputSourceId source = 0;
	InputContactId contact = 0;
	PointerKind kind = PointerKind::Mouse;
	PointerButton button = PointerButton::Primary;
	bool pressed = false;
	InputPhase phase = InputPhase::Update;
};
/** Scroll magnitude in native window input units. */
struct ScrollInputEvent {
	InputDeviceId device = NativeInputDevice;
	WindowId target = InvalidWindowId;
	InputSourceId source = 0;
	float x = 0.0f;
	float y = 0.0f;
};
/** Observed host focus; losing focus cancels held contributions. */
struct FocusInputEvent {
	WindowId target = InvalidWindowId;
	bool focused = true;
};
/** Device cancellation/disconnection; zero device clears all contributions on the target. */
struct DeviceResetEvent {
	InputDeviceId device = 0;
	WindowId target = InvalidWindowId;
	InputSourceId source = 0;
};
/** Connection notification; disconnection also cancels the device's held input. */
struct DeviceConnectionEvent {
	InputDeviceId device = NativeInputDevice;
	WindowId target = InvalidWindowId;
	InputSourceId source = 0;
	bool connected = true;
};
/** Logical action with an explicit lifetime and optional analog magnitude. */
struct NavigationInputEvent {
	InputDeviceId device = NativeInputDevice;
	WindowId target = InvalidWindowId;
	InputSourceId source = 0;
	NavigationAction action = NavigationAction::Activate;
	InputPhase phase = InputPhase::Begin;
	float magnitude = 1.0f;
};
/** Ordered, allocation-free event value. Additional contacts remain visible in this stream. */
using InputEvent =
	std::variant<KeyEvent, TextInputEvent, PointerMoveEvent, PointerButtonEvent, ScrollInputEvent,
				 FocusInputEvent, DeviceResetEvent, DeviceConnectionEvent, NavigationInputEvent>;

} // namespace FlowUi
