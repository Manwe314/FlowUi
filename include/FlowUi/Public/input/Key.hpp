#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace FlowUi {

/** FlowUi physical key identities. Numeric values preserve the established shortcut encoding. */
enum class Key : std::uint16_t {
	Space = 32,
	Apostrophe = 39,
	Comma = 44,
	Minus = 45,
	Period = 46,
	Slash = 47,
	Digit0 = 48,
	Digit1 = 49,
	Digit2 = 50,
	Digit3 = 51,
	Digit4 = 52,
	Digit5 = 53,
	Digit6 = 54,
	Digit7 = 55,
	Digit8 = 56,
	Digit9 = 57,
	Semicolon = 59,
	Equal = 61,
	A = 65,
	B = 66,
	C = 67,
	D = 68,
	E = 69,
	F = 70,
	G = 71,
	H = 72,
	I = 73,
	J = 74,
	K = 75,
	L = 76,
	M = 77,
	N = 78,
	O = 79,
	P = 80,
	Q = 81,
	R = 82,
	S = 83,
	T = 84,
	U = 85,
	V = 86,
	W = 87,
	X = 88,
	Y = 89,
	Z = 90,
	LeftBracket = 91,
	Backslash = 92,
	RightBracket = 93,
	GraveAccent = 96,
	World1 = 161,
	World2 = 162,
	Escape = 256,
	Enter = 257,
	Tab = 258,
	Backspace = 259,
	Insert = 260,
	Delete = 261,
	Right = 262,
	Left = 263,
	Down = 264,
	Up = 265,
	PageUp = 266,
	PageDown = 267,
	Home = 268,
	End = 269,
	CapsLock = 280,
	ScrollLock = 281,
	NumLock = 282,
	PrintScreen = 283,
	Pause = 284,
	F1 = 290,
	F2 = 291,
	F3 = 292,
	F4 = 293,
	F5 = 294,
	F6 = 295,
	F7 = 296,
	F8 = 297,
	F9 = 298,
	F10 = 299,
	F11 = 300,
	F12 = 301,
	F13 = 302,
	F14 = 303,
	F15 = 304,
	F16 = 305,
	F17 = 306,
	F18 = 307,
	F19 = 308,
	F20 = 309,
	F21 = 310,
	F22 = 311,
	F23 = 312,
	F24 = 313,
	F25 = 314,
	Keypad0 = 320,
	Keypad1 = 321,
	Keypad2 = 322,
	Keypad3 = 323,
	Keypad4 = 324,
	Keypad5 = 325,
	Keypad6 = 326,
	Keypad7 = 327,
	Keypad8 = 328,
	Keypad9 = 329,
	KeypadDecimal = 330,
	KeypadDivide = 331,
	KeypadMultiply = 332,
	KeypadSubtract = 333,
	KeypadAdd = 334,
	KeypadEnter = 335,
	KeypadEqual = 336,
	LeftShift = 340,
	LeftControl = 341,
	LeftAlt = 342,
	LeftSuper = 343,
	RightShift = 344,
	RightControl = 345,
	RightAlt = 346,
	RightSuper = 347,
	Menu = 348,
};

/** Return a key's stable integer encoding for shortcut and frame array APIs. */
[[nodiscard]] constexpr int keyCode(Key key) noexcept {
	return static_cast<int>(key);
}

/** Decode only named FlowUi keys; native providers must translate their own codes. */
[[nodiscard]] constexpr std::optional<Key> keyFromCode(int code) noexcept {
	switch (code) {
	case 32:
		return Key::Space;
	case 39:
		return Key::Apostrophe;
	case 44:
		return Key::Comma;
	case 45:
		return Key::Minus;
	case 46:
		return Key::Period;
	case 47:
		return Key::Slash;
	case 48:
		return Key::Digit0;
	case 49:
		return Key::Digit1;
	case 50:
		return Key::Digit2;
	case 51:
		return Key::Digit3;
	case 52:
		return Key::Digit4;
	case 53:
		return Key::Digit5;
	case 54:
		return Key::Digit6;
	case 55:
		return Key::Digit7;
	case 56:
		return Key::Digit8;
	case 57:
		return Key::Digit9;
	case 59:
		return Key::Semicolon;
	case 61:
		return Key::Equal;
	case 65:
		return Key::A;
	case 66:
		return Key::B;
	case 67:
		return Key::C;
	case 68:
		return Key::D;
	case 69:
		return Key::E;
	case 70:
		return Key::F;
	case 71:
		return Key::G;
	case 72:
		return Key::H;
	case 73:
		return Key::I;
	case 74:
		return Key::J;
	case 75:
		return Key::K;
	case 76:
		return Key::L;
	case 77:
		return Key::M;
	case 78:
		return Key::N;
	case 79:
		return Key::O;
	case 80:
		return Key::P;
	case 81:
		return Key::Q;
	case 82:
		return Key::R;
	case 83:
		return Key::S;
	case 84:
		return Key::T;
	case 85:
		return Key::U;
	case 86:
		return Key::V;
	case 87:
		return Key::W;
	case 88:
		return Key::X;
	case 89:
		return Key::Y;
	case 90:
		return Key::Z;
	case 91:
		return Key::LeftBracket;
	case 92:
		return Key::Backslash;
	case 93:
		return Key::RightBracket;
	case 96:
		return Key::GraveAccent;
	case 161:
		return Key::World1;
	case 162:
		return Key::World2;
	case 256:
		return Key::Escape;
	case 257:
		return Key::Enter;
	case 258:
		return Key::Tab;
	case 259:
		return Key::Backspace;
	case 260:
		return Key::Insert;
	case 261:
		return Key::Delete;
	case 262:
		return Key::Right;
	case 263:
		return Key::Left;
	case 264:
		return Key::Down;
	case 265:
		return Key::Up;
	case 266:
		return Key::PageUp;
	case 267:
		return Key::PageDown;
	case 268:
		return Key::Home;
	case 269:
		return Key::End;
	case 280:
		return Key::CapsLock;
	case 281:
		return Key::ScrollLock;
	case 282:
		return Key::NumLock;
	case 283:
		return Key::PrintScreen;
	case 284:
		return Key::Pause;
	case 290:
		return Key::F1;
	case 291:
		return Key::F2;
	case 292:
		return Key::F3;
	case 293:
		return Key::F4;
	case 294:
		return Key::F5;
	case 295:
		return Key::F6;
	case 296:
		return Key::F7;
	case 297:
		return Key::F8;
	case 298:
		return Key::F9;
	case 299:
		return Key::F10;
	case 300:
		return Key::F11;
	case 301:
		return Key::F12;
	case 302:
		return Key::F13;
	case 303:
		return Key::F14;
	case 304:
		return Key::F15;
	case 305:
		return Key::F16;
	case 306:
		return Key::F17;
	case 307:
		return Key::F18;
	case 308:
		return Key::F19;
	case 309:
		return Key::F20;
	case 310:
		return Key::F21;
	case 311:
		return Key::F22;
	case 312:
		return Key::F23;
	case 313:
		return Key::F24;
	case 314:
		return Key::F25;
	case 320:
		return Key::Keypad0;
	case 321:
		return Key::Keypad1;
	case 322:
		return Key::Keypad2;
	case 323:
		return Key::Keypad3;
	case 324:
		return Key::Keypad4;
	case 325:
		return Key::Keypad5;
	case 326:
		return Key::Keypad6;
	case 327:
		return Key::Keypad7;
	case 328:
		return Key::Keypad8;
	case 329:
		return Key::Keypad9;
	case 330:
		return Key::KeypadDecimal;
	case 331:
		return Key::KeypadDivide;
	case 332:
		return Key::KeypadMultiply;
	case 333:
		return Key::KeypadSubtract;
	case 334:
		return Key::KeypadAdd;
	case 335:
		return Key::KeypadEnter;
	case 336:
		return Key::KeypadEqual;
	case 340:
		return Key::LeftShift;
	case 341:
		return Key::LeftControl;
	case 342:
		return Key::LeftAlt;
	case 343:
		return Key::LeftSuper;
	case 344:
		return Key::RightShift;
	case 345:
		return Key::RightControl;
	case 346:
		return Key::RightAlt;
	case 347:
		return Key::RightSuper;
	case 348:
		return Key::Menu;
	default:
		return std::nullopt;
	}
}

/** Checked array slot; rejects invalid values cast into Key. */
[[nodiscard]] constexpr std::optional<std::size_t> keySlot(Key key) noexcept {
	return keyFromCode(keyCode(key)) ? std::optional<std::size_t>(static_cast<std::size_t>(key))
									 : std::nullopt;
}

} // namespace FlowUi
