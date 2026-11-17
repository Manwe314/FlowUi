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
	/** Stable producer device identity; NativeInputDevice denotes the native keyboard/mouse. */
	InputDeviceId device = NativeInputDevice;
	/** Semantic target window; use a live routed destination. */
	WindowId target = InvalidWindowId;
	/** Independent producer identity within the device; preserve it across matching events. */
	InputSourceId source = 0;
	/** Supported semantic key, independent of the external SDK encoding. */
	Key key = Key::Space;
	/** True for press, false for release. */
	bool pressed = false;
	/** Modifier mask: Control=1, Shift=2, Alt=4, Super=8; no other bits are accepted. */
	std::uint8_t modifiers = 0;
};
/** Committed Unicode scalar value; never synthesized from physical key codes. */
struct TextInputEvent {
	/** Stable producer device identity; NativeInputDevice denotes the native keyboard/mouse. */
	InputDeviceId device = NativeInputDevice;
	/** Semantic target window; use a live routed destination. */
	WindowId target = InvalidWindowId;
	/** Independent producer identity within the device; preserve it across matching events. */
	InputSourceId source = 0;
	/** Committed Unicode scalar; surrogate codepoints and values above 0x10ffff are rejected. */
	char32_t codepoint = U'\0';
};
/** Pointer coordinates in window space; App performs the one layout conversion. */
struct PointerMoveEvent {
	/** Stable producer device identity; NativeInputDevice denotes the native keyboard/mouse. */
	InputDeviceId device = NativeInputDevice;
	/** Semantic target window; use a live routed destination. */
	WindowId target = InvalidWindowId;
	/** Independent producer identity within the device; preserve it across matching events. */
	InputSourceId source = 0;
	/** Stable touch/pen contact identity for its complete lifetime. */
	InputContactId contact = 0;
	/** Pointer family; additional virtual devices need not impersonate a mouse. */
	PointerKind kind = PointerKind::Mouse;
	/** Finite horizontal window-space coordinate or scroll delta, according to this event type. */
	float x = 0.0f;
	/** Finite vertical window-space coordinate or scroll delta, according to this event type. */
	float y = 0.0f;
	/** Whether this pointer is inside the destination window. */
	bool inside = true;
};
/** Button transition; non-mouse contact capture persists through release/cancel. */
struct PointerButtonEvent {
	/** Stable producer device identity; NativeInputDevice denotes the native keyboard/mouse. */
	InputDeviceId device = NativeInputDevice;
	/** Semantic target window; use a live routed destination. */
	WindowId target = InvalidWindowId;
	/** Independent producer identity within the device; preserve it across matching events. */
	InputSourceId source = 0;
	/** Stable touch/pen contact identity for its complete lifetime. */
	InputContactId contact = 0;
	/** Pointer family; additional virtual devices need not impersonate a mouse. */
	PointerKind kind = PointerKind::Mouse;
	/** One of Primary, Secondary, Middle, Back or Forward. */
	PointerButton button = PointerButton::Primary;
	/** True for press, false for release. */
	bool pressed = false;
	/** Begin, Update, End or Cancel for the same contact/action lifetime. */
	InputPhase phase = InputPhase::Update;
};
/** Scroll magnitude in native window input units. */
struct ScrollInputEvent {
	/** Stable producer device identity; NativeInputDevice denotes the native keyboard/mouse. */
	InputDeviceId device = NativeInputDevice;
	/** Semantic target window; use a live routed destination. */
	WindowId target = InvalidWindowId;
	/** Independent producer identity within the device; preserve it across matching events. */
	InputSourceId source = 0;
	/** Finite horizontal scroll delta in native window input units. */
	float x = 0.0f;
	/** Finite vertical scroll delta in native window input units. */
	float y = 0.0f;
};
/** Observed host focus; losing focus cancels held contributions. */
struct FocusInputEvent {
	/** Semantic target window; use a live routed destination. */
	WindowId target = InvalidWindowId;
	/** Observed native focus; loss of focus cancels held input when consumed. */
	bool focused = true;
};
/** Device cancellation/disconnection; zero device clears all contributions on the target. */
struct DeviceResetEvent {
	/** Device to cancel; zero cancels all device contributions on the target. */
	InputDeviceId device = 0;
	/** Semantic target window; use a live routed destination. */
	WindowId target = InvalidWindowId;
	/** Independent producer identity within the device; preserve it across matching events. */
	InputSourceId source = 0;
};
/** Connection notification; disconnection also cancels the device's held input. */
struct DeviceConnectionEvent {
	/** Stable producer device identity; NativeInputDevice denotes the native keyboard/mouse. */
	InputDeviceId device = NativeInputDevice;
	/** Semantic target window; use a live routed destination. */
	WindowId target = InvalidWindowId;
	/** Independent producer identity within the device; preserve it across matching events. */
	InputSourceId source = 0;
	/** Observed device connection; disconnection cancels its held input when consumed. */
	bool connected = true;
};
/** Logical action with an explicit lifetime and optional analog magnitude. */
struct NavigationInputEvent {
	/** Stable producer device identity; NativeInputDevice denotes the native keyboard/mouse. */
	InputDeviceId device = NativeInputDevice;
	/** Semantic target window; use a live routed destination. */
	WindowId target = InvalidWindowId;
	/** Independent producer identity within the device; preserve it across matching events. */
	InputSourceId source = 0;
	/** Semantic navigation intent projected to compatibility input. */
	NavigationAction action = NavigationAction::Activate;
	/** Begin, Update, End or Cancel for the same contact/action lifetime. */
	InputPhase phase = InputPhase::Begin;
	/** Finite analog magnitude associated with the logical action. */
	float magnitude = 1.0f;
};
/** Ordered, allocation-free event value. Additional contacts remain visible in this stream. */
using InputEvent =
	std::variant<KeyEvent, TextInputEvent, PointerMoveEvent, PointerButtonEvent, ScrollInputEvent,
				 FocusInputEvent, DeviceResetEvent, DeviceConnectionEvent, NavigationInputEvent>;

} // namespace FlowUi
